"""serial_watch.py - reset mach va doc log UART0 tren COM port.

Dung: python tools/serial_watch.py [PORT] [SECONDS]

UART0 (CH340) nhan duoc ROM banner, ESP_LOG cua core va ca Serial.* cua app
vi platformio.ini dat ARDUINO_USB_CDC_ON_BOOT=0 (Serial = UART0, khong phai USB
native). Vi vay log [G2]/[rg_*] phai hien tren dung port nay.

Hai cai bay da gap tren Windows + CH340:
  1. Dat ser.dtr/rts TRUOC open() -> OSError(22, 'A device which does not exist
     was specified.'). Phai open() truoc roi moi keo chan reset.
  2. Open co the fail vai lan lien tiep (CH340 dang re-enumerate sau khi esptool
     reset) -> phai retry.
"""
import sys
import time

import serial

PORT = sys.argv[1] if len(sys.argv) > 1 else "COM3"
SECS = float(sys.argv[2]) if len(sys.argv) > 2 else 20.0
BANNER = "ESP-ROM"


def open_port(port: str, retries: int = 12) -> serial.Serial:
    last: Exception | None = None
    for _ in range(retries):
        try:
            return serial.Serial(port, 115200, timeout=0.2)
        except Exception as exc:  # noqa: BLE001 - CH340 dang re-enumerate
            last = exc
            time.sleep(0.6)
    raise SystemExit(f"khong mo duoc {port}: {last}")


def read_for(ser: serial.Serial, secs: float) -> str:
    end = time.time() + secs
    buf = bytearray()
    errs = 0
    while time.time() < end:
        try:
            chunk = ser.read(4096)
        except (PermissionError, OSError) as exc:
            errs += 1
            if errs <= 3:
                print(f"# READ ERROR ({type(exc).__name__}: {exc})", file=sys.stderr)
            time.sleep(0.05)  # transient tren Windows, retry
            continue
        if chunk:
            buf += chunk
    if errs:
        print(f"# tong so loi doc: {errs} (port co the da rot khoi USB)", file=sys.stderr)
    return buf.decode("utf-8", errors="replace")


def hard_reset(ser: serial.Serial) -> None:
    """Pattern esptool classic: IO0=HIGH (DTR=False), keo EN thap roi tha."""
    ser.dtr = False
    ser.rts = True
    time.sleep(0.12)
    ser.dtr = False
    ser.rts = False
    time.sleep(0.05)


def main() -> int:
    ser = open_port(PORT)
    print(f"# {PORT} open OK", file=sys.stderr)

    out = ""
    for attempt in (1, 2, 3):
        hard_reset(ser)
        out = read_for(ser, SECS if attempt == 1 else 6.0)
        if BANNER in out:
            break
        print(f"# attempt {attempt}: khong thay banner, thu lai", file=sys.stderr)

    ser.close()
    sys.stdout.write(out if out else "<khong co du lieu tren UART0>\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
