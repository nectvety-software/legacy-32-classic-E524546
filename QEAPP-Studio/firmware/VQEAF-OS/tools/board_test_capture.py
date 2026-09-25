#!/usr/bin/env python3
"""Drive v2.0.1 diag commands on a physically connected ESP32-S3.

Requires pyserial and a dedicated UART/USB serial port, 115200 baud.
Does not flash the device. Its PASS verdicts are evidence from this serial
session, never a claim about a simulation. No WiFi credentials are logged.
"""
import argparse
import datetime as dt
import json
import re
import sys
import time
from pathlib import Path


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--port', required=True, help='ESP32-S3 serial port, e.g. COM7')
    ap.add_argument('--mode', choices=['tls', 'sd'], required=True)
    ap.add_argument('--cycles', type=int, default=10, help='Manual SD remove/insert cycles, default 10')
    ap.add_argument('--output', default='', help='Result JSON path (default: board_results_<mode>.json)')
    args = ap.parse_args()
    if args.cycles < 1 or args.cycles > 30:
        ap.error('cycles must be 1..30')
    try:
        import serial
    except ImportError:
        sys.exit('Install pyserial: python -m pip install pyserial')
    path = Path(args.output or ('board_results_' + args.mode + '.json')).absolute()
    session = {
        'port': args.port, 'mode': args.mode,
        'started_at_utc': dt.datetime.now(dt.timezone.utc).isoformat(),
        'note': 'Live UART evidence; no tests assumed to pass in advance',
        'cases': [], 'raw_serial': []
    }
    with serial.Serial(port=args.port, baudrate=115200, timeout=0.25, write_timeout=3,
                       rtscts=False, dsrdtr=False) as ser:
        # Some ESP32 USB-UART bridges reset when opened; wait for startup.
        print('Connected. Wait for the board to boot; stop playback/downloads first.')
        time.sleep(3)
        pending = bytearray()
        def flush():
            path.write_text(json.dumps(session, indent=2, ensure_ascii=False), encoding='utf-8')
        def read_line(timeout_s=0.25):
            deadline = time.monotonic() + timeout_s
            while time.monotonic() < deadline:
                b = ser.read(1)  # Preserve every byte; do not discard extra lines in a bulk read.
                for ch in b:
                    if ch in (10, 13):
                        if pending:
                            line = pending.decode('utf-8','replace')
                            pending.clear()
                            if line:
                                session['raw_serial'].append(line)
                                if len(session['raw_serial']) > 8000:
                                    session['raw_serial'] = session['raw_serial'][-8000:]
                                print(line)
                                return line
                    elif len(pending) < 400:
                        pending.append(ch)
                time.sleep(.02)
            return ''
        def send(command):
            ser.write((command + '\n').encode('ascii'))
            ser.flush()
            print('>>',command)
        def wait(marker,seconds):
            end = time.monotonic() + seconds
            while time.monotonic() < end:
                line=read_line(0.4)
                if line and marker in line:
                    return line
            return ''
        def diag(cmd,marker,seconds=45):
            send(cmd)
            result=wait(marker,seconds)
            return result
        if args.mode=='tls':
            print('Before continuing: WiFi must be connected and clock synchronized via NTP.')
            print('Use the normal WiFi app. DO NOT enter your password into this script.')
            input('Press Enter when the clock shows a real date/time: ')
            for case in ('valid','expired','wrong','self'):
                result = diag('diag tls '+case,'[S3DIAG][TLS] verdict=',45)
                verdict = re.search(r'verdict=(PASS|FAIL|INCONCLUSIVE)',result)
                session['cases'].append({'case':case,'result':verdict.group(1) if verdict else 'TIMEOUT','line':result})
                flush()
            good = session['cases'][0]['result']=='PASS'
            negatives = all(v['result']=='PASS' for v in session['cases'][1:])
            print('\nSummary:', 'PASS' if good and negatives else 'INCOMPLETE / FAILED')
            print('Negative PASS requires firmware TLS error -0x2700 and valid positive control.')
        else:
            print('USE A DISPOSABLE/SELF-BACKED-UP MICROSD CARD. STOP MUSIC/DOWNLOADS.')
            print('Never remove the SD while it is writing. Read/write tests only before removal or after remount.')
            input('Boot with SD inserted. Press Enter after Home screen appears: ')
            boot=diag('diag sd status','[S3DIAG][SD] status',5)
            session['cases'].append({'case':'boot-status','line':boot})
            for i in range(args.cycles):
                print(f'\nSD cycle {i+1}/{args.cycles}')
                pre=diag('diag sd rw','[S3DIAG][SD] test=rw',12)
                if 'result=PASS' not in pre:
                    print('Pre-test failed; do not pull card; fix SD first.')
                    session['cases'].append({'cycle':i+1,'pre':pre,'result':'FAILED_PRE'})
                    flush();break
                input('Now REMOVE the idle microSD. Press Enter after physically removing it: ')
                removed=wait('[S3DIAG][SD] event=REMOVED',25)
                input('Now REINSERT the microSD. Press Enter after fully inserting it: ')
                mounted=wait('[S3DIAG][SD] event=MOUNTED',65)
                post=diag('diag sd rw','[S3DIAG][SD] test=rw',15) if mounted else ''
                ok=bool(removed and mounted and 'result=PASS' in post)
                session['cases'].append({'cycle':i+1,'pre':pre,'removed':removed,'mounted':mounted,'post':post,'result':'PASS' if ok else 'FAIL'})
                flush()
                if not ok:
                    print('Hotplug not verified in this round. Stop and inspect the log.')
                    break
            print('Check that Gallery/Themes/installed apps reopen manually after the last remount.')
            print('Boot-without-card scenario requires a separate manual cold boot / insertion log.')
    session['finished_at_utc']=dt.datetime.now(dt.timezone.utc).isoformat()
    path.write_text(json.dumps(session,indent=2,ensure_ascii=False),encoding='utf-8')
    print('Saved:',path)

if __name__=='__main__':main()
