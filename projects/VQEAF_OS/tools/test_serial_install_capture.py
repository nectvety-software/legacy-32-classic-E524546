#!/usr/bin/env python3
"""Offline simulated UART regression tests; no ESP32-S3 or pyserial required."""
import contextlib
import io
import json
from pathlib import Path
import tempfile
import unittest

from capture_install_reset import Recorder, choose_port, make_demo, main, clean_line


class Clock:
    def __init__(self): self.t=100.0
    def __call__(self):return self.t
    def move(self,seconds):self.t+=seconds

class StubPort:
    def __init__(self,device,vid=None):self.device=device;self.vid=vid;self.description=device;self.pid=0x1001

class FakeSerial:
    class tools:
        class list_ports:
            @staticmethod
            def comports():return [StubPort('COM5',0x303A),StubPort('COM10',None)]


class SerialCaptureTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.clock=Clock()
        self.folder=Path(self.tmp.name)/'capture'
        self.rec=Recorder(self.folder,console=False,clock=self.clock,wall=lambda:'2026-09-25T08:00:00.000+07:00')
        self.addCleanup(self.rec.close)

    def ev(self,kind):return [x for x in self.rec.events if x['kind']==kind]

    def test_raw_preserved_fragmentation_and_bad_utf8(self):
        data=[b'\xffhi \xe2', b'\x9d\x8c\r\n[QEAPP][INSTALL][10] heap=32000\r',b'\n']
        for chunk in data:self.rec.receive(chunk)
        self.rec.close()
        self.assertEqual((self.folder/'raw_uart.bin').read_bytes(),b''.join(data))
        self.assertEqual(self.rec.stats['serial_lines'],2)
        txt=(self.folder/'serial_timestamped.log').read_text()
        self.assertIn('hi',txt)
        self.assertIn('INSTALL][10]',txt)
        self.assertEqual(self.rec.last_phase,10)

    def test_simulated_boot_during_install(self):
        self.rec.mark('install: user tapped Install')
        self.rec.receive(b'[QEAPP][INSTALL][10] entry\n')
        self.clock.move(1)
        self.rec.receive(b'[QEAPP][INSTALL][50] copy_complete\n')
        self.clock.move(1)
        self.rec.receive(b'ESP-ROM:esp32s3-20210327\r\nrst:0xc (SW_CPU_RESET),boot:0x8\n')
        self.assertEqual(len(self.ev('BOOT_DURING_INSTALL_SUSPECTED')),1)
        self.assertEqual(self.rec.stats['boot_signatures'],1)
        self.assertEqual(self.ev('BOOT_DURING_INSTALL_SUSPECTED')[0]['last_phase'],50)
        self.assertEqual(self.rec.stats['suspected_install_reboots'],1)
        self.rec.close()
        report=(self.folder/'report.md').read_text()
        self.assertIn('Giai đoạn cài đặt cuối: **50**',report)

    def test_first_boot_and_explicit_reset_not_install_failure(self):
        self.rec.receive(b'ESP-ROM:esp32s3-20210327\r\n')
        self.clock.move(2.5)
        self.rec.mark('install: tapped')
        self.rec.mark('reset: I am testing deliberate reset')
        self.rec.receive(b'rst:0x1 (POWERON_RESET),boot:0x8\r\n')
        self.assertEqual(len(self.ev('BOOT_AFTER_MANUAL_RESET')),1)
        self.assertEqual(len(self.ev('BOOT_DURING_INSTALL_SUSPECTED')),0)

    def test_reconnect_is_not_boot(self):
        self.rec.connected('COM5')
        self.rec.disconnected('USB cable unplugged')
        self.rec.connected('COM10',reconnect=True)
        self.assertEqual(self.rec.stats['boot_signatures'],0)
        self.assertEqual(len(self.ev('PORT_RECONNECTED')),1)

    def test_fast_reboot_loop_two_rom_headers_counted(self):
        self.rec.mark('install: clicked')
        self.rec.receive(b'[QEAPP][INSTALL][20] verified\n')
        self.rec.receive(b'ESP-ROM:esp32s3-20210327\nrst:0xc (SW_CPU_RESET)\n')
        self.clock.move(.7)
        self.rec.receive(b'ESP-ROM:esp32s3-20210327\nrst:0xc (SW_CPU_RESET)\n')
        self.assertEqual(self.rec.stats['boot_signatures'],2)
        self.assertEqual(self.rec.stats['suspected_install_reboots'],1)

    def test_disconnect_separates_partial_line_from_next_boot(self):
        self.rec.receive(b'PANIC without newline')
        self.rec.disconnected('USB gone')
        self.rec.receive(b'ESP-ROM:esp32s3-20210327\n')
        self.assertEqual(self.rec.stats['boot_signatures'],1)

    def test_install_complete_followed_boot_not_reboot(self):
        self.rec.receive(b'[QEAPP][INSTALL][10] entry\n[QEAPP][INSTALL][100] install_complete\n')
        self.clock.move(2)
        self.rec.receive(b'ESP-ROM:esp32s3-20210327\n')
        self.assertEqual(len(self.ev('BOOT_DURING_INSTALL_SUSPECTED')),0)
        self.assertTrue(self.ev('INSTALL_COMPLETE'))

    def test_retain_backtrace_and_diagnostics(self):
        self.rec.receive(b'Guru Meditation Error: Core 0 panic\nBacktrace: 0x1234 0x4567\r\n')
        self.assertEqual(len(self.ev('PANIC_OR_ABORT_TEXT')),1)
        self.assertEqual(len(self.ev('BACKTRACE_TEXT')),1)
        self.assertIn(b'0x1234', (self.folder/'raw_uart.bin').read_bytes())

    def test_partial_line_persisted_even_without_newline(self):
        self.rec.receive(b'partial panic starting -')
        self.rec.close()
        self.assertIn('PARTIAL_SEGMENT', (self.folder/'serial_timestamped.log').read_text())
        self.assertEqual((self.folder/'raw_uart.bin').read_bytes(),b'partial panic starting -')

    def test_bounded_long_line_and_exact_raw(self):
        blob=b'x'*20000+b'\n'
        self.rec.receive(blob)
        self.rec.close()
        self.assertEqual((self.folder/'raw_uart.bin').read_bytes(),blob)
        self.assertEqual(self.rec.stats['serial_lines'],2)

    def test_boot_fallback_firmware_marker_when_usb_rom_unavailable(self):
        self.rec.connected('COM5')
        self.rec.mark('install: clicked')
        self.rec.receive(b'[QEAPP][INSTALL][40] staging\n')
        self.rec.disconnected('USB CDC restart')
        self.rec.connected('COM5',reconnect=True)
        self.rec.receive(b'[QEAPP][PREVIOUS_RESET] installer_phase=40\n')
        self.assertEqual(self.rec.stats['boot_signatures'],1)
        self.assertEqual(self.rec.stats['suspected_install_reboots'],1)
        self.assertEqual(self.ev('BOOT_DURING_INSTALL_SUSPECTED')[0]['evidence'].split(';')[0],
                         'FIRMWARE_PREVIOUS_RESET_MARKER')

    def test_previously_marker_after_rom_coalesced(self):
        self.rec.connected('COM5')
        self.rec.receive(b'ESP-ROM:esp32s3-20210327\n[QEAPP][PREVIOUS_RESET] installer_phase=55\n')
        self.assertEqual(self.rec.stats['boot_signatures'],1)
        self.assertEqual(len(self.ev('PREVIOUS_RESET_MARKER')),1)

    def test_previously_interrupted_marker(self):
        self.rec.receive(b'[QEAPP][PREVIOUS_RESET] installer_phase=55\r\n')
        self.assertEqual(len(self.ev('PREVIOUS_RESET_MARKER')),1)

    def test_port_auto_prefers_esp_303a(self):
        self.assertEqual(choose_port(FakeSerial,'auto'),'COM5')
        self.assertEqual(choose_port(FakeSerial,'COM8'),'COM8')

    def test_stamped_local_timezone_format(self):
        self.rec.receive(b'test line\n')
        self.assertIn('[2026-09-25T08:00:00.000+07:00]',(self.folder/'serial_timestamped.log').read_text())

    def test_demo_is_clearly_synthetic(self):
        with contextlib.redirect_stdout(io.StringIO()):
            rc=main(['--demo','--out',self.tmp.name])
        self.assertEqual(rc,0)
        d=Path(self.tmp.name)/'DEMO_SIMULATED_DO_NOT_TREAT_AS_HARDWARE'
        self.assertIn('SIMULATION_ONLY',(d/'events.jsonl').read_text())
        self.assertEqual(json.loads((d/'summary.json').read_text())['last_install_phase'],50)



# This separate class also exercises the live capture/reconnect orchestration,
# without requiring a physical board or the pyserial package.
class CaptureOrchestrationTest(unittest.TestCase):
    def test_reads_across_disconnect_and_reconnection(self):
        from types import SimpleNamespace
        from capture_install_reset import run_capture
        clock=Clock()
        opened=[]
        class SimException(Exception):pass
        class SimUart:
            def __init__(self,**kw):
                self.port=None;self.dtr=None;self.rts=None
                self.items = ([b'[QEAPP][INSTALL][10] entry\n',
                               b'[QEAPP][INSTALL][50] copy\n',SimException('USB lost')]
                              if not opened else
                              [b'ESP-ROM:esp32s3-20210327\n',
                               b'[QEAPP][PREVIOUS_RESET] installer_phase=50\n'])
                opened.append(self)
            def open(self):pass
            def read(self,n):
                clock.move(.16)
                if self.items:
                    x=self.items.pop(0)
                    if isinstance(x,Exception):raise x
                    return x
                return b''
            def close(self):pass
        serial=SimpleNamespace(Serial=SimUart,SerialException=SimException)
        tmp=tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        a=SimpleNamespace(out=Path(tmp.name), output=None, quiet=True,no_keyboard=True,
                          duration=1.6,port='COM5',baud=115200,reconnect=True,
                          retry_seconds=.4,idle_warning=100.,dtr=False,rts=False)
        # Keep one virtual capture session; the fake clock advances via read/sleep.
        with contextlib.redirect_stdout(io.StringIO()):
            folder=run_capture(a,serial,clock=clock,sleep=lambda sec:clock.move(sec))
        result=json.loads((folder/'summary.json').read_text())
        self.assertEqual(result['stats']['port_disconnects'],1)
        self.assertEqual(result['stats']['serial_connections'],2)
        self.assertEqual(result['stats']['suspected_install_reboots'],1)
        data=(folder/'raw_uart.bin').read_bytes()
        self.assertIn(b'[QEAPP][INSTALL][50]',data)
        self.assertIn(b'ESP-ROM:',data)
        self.assertEqual(opened[0].dtr,False)
        self.assertEqual(opened[0].rts,False)

if __name__=="__main__":unittest.main(verbosity=2)
