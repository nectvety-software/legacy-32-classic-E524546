#!/usr/bin/env python3
"""Capture the ESP32-S3 ROM/UART0 boot log after a hardware reset.

UART0 is the only channel that carries the ROM banner, core ESP_LOGx output
and panic dumps on this board: `Serial` is redirected to the native USB CDC
(ARDUINO_USB_CDC_ON_BOOT=1), so [VQEAF]/[S3DIAG] app logs never appear here.

Usage: python _capture_boot_v240.py <port> <baud> <seconds> <outfile>
"""
import sys
import time

import serial

port, baud, secs, out = sys.argv[1], int(sys.argv[2]), float(sys.argv[3]), sys.argv[4]

# DTR/RTS toggling can raise PermissionError(13) on CH340; retry the whole
# open+reset+read cycle instead of giving up on the first failure.
last = None
for attempt in range(1, 6):
    try:
        with serial.Serial(port, baud, timeout=0.2) as ser:
            # RTS low -> EN low -> reset; then release. Classic auto-reset.
            ser.dtr = False
            ser.rts = True
            time.sleep(0.15)
            ser.rts = False
            time.sleep(0.05)

            buf = bytearray()
            end = time.time() + secs
            while time.time() < end:
                chunk = ser.read(4096)
                if chunk:
                    buf.extend(chunk)
            data = bytes(buf)
        with open(out, "wb") as fh:
            fh.write(data)
        print("captured %d bytes -> %s" % (len(data), out))
        sys.exit(0)
    except Exception as exc:  # noqa: BLE001 - transient CH340 driver errors
        last = exc
        print("attempt %d failed: %r" % (attempt, exc))
        time.sleep(1.0)

print("all attempts failed: %r" % (last,))
sys.exit(1)
