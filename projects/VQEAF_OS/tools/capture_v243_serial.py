#!/usr/bin/env python3
"""Capture VQEAF OS 115200 serial data during field retest. Read-only."""
import argparse
from datetime import datetime
from pathlib import Path
from time import monotonic

def main():
  a=argparse.ArgumentParser(description=__doc__)
  a.add_argument('--port',required=True,help='COM5, /dev/ttyACM0, etc')
  a.add_argument('--baud',type=int,default=115200)
  a.add_argument('--seconds',type=int,default=150)
  a.add_argument('--output',type=Path,default=Path('build_reports/device/v243_serial.log'))
  args=a.parse_args()
  try:
    import serial
  except ImportError:
    a.error('Install pyserial first: py -3 -m pip install pyserial')
  args.output.parent.mkdir(parents=True,exist_ok=True)
  print('Capture started; reproduce both installs and theme apply, then press Ctrl+C.')
  count=0
  with serial.Serial(args.port,args.baud,timeout=0.5) as port, args.output.open('w',encoding='utf-8') as out:
    started=monotonic()
    try:
      while monotonic()-started<args.seconds:
        data=port.readline()
        if not data:continue
        line=data.decode('utf-8',errors='replace').strip()
        stamped=datetime.now().astimezone().isoformat(timespec='seconds')+' '+line
        print(stamped,flush=True);out.write(stamped+'\n');out.flush();count+=1
    except KeyboardInterrupt:pass
  print(f'Saved {count} lines to {args.output}')
  if count==0:print('No serial data received. Confirm USB CDC on boot, baud, COM port and press RESET.')
  return int(count==0)
if __name__=='__main__':raise SystemExit(main())
