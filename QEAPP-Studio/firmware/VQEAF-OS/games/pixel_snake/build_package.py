#!/usr/bin/env python3
"""Build editable signed Pixel Snake config-only QEAPP/2 for the matching OS public key."""
import argparse,subprocess,sys
from pathlib import Path
HOME=Path(__file__).resolve().parent
ROOT=HOME.parent.parent
p=argparse.ArgumentParser(description='Build Pixel Snake QEAPP/2 (firmware built-in game handler required)')
p.add_argument('--sign-key',type=Path,required=True,help='Your private ECDSA P-256 PEM; keep off device')
p.add_argument('--version',default='1.0.0',help='Increase for every signed update')
p.add_argument('-o','--output',type=Path,default=HOME/'dist/snake_pixel.qeapp')
a=p.parse_args()
if not a.sign_key.is_file():p.error('Missing private signing key')
config=HOME/'snake.cfg'
source=config.read_bytes()
# Mirror the rigid firmware validator; actual C++ gate is run by test_pixel_snake.py.
head=b'VQEAF-SNAKE-1\n';lines=source.splitlines()
if len(source)>512 or not source.startswith(head):p.error('Invalid config header or length')
fields={}
for line in lines[1:]:
    if not line: continue
    if b'=' not in line:p.error('Expected key=value in snake.cfg')
    k,v=line.split(b'=',1)
    if k not in [b'speed_ms',b'wrap',b'palette'] or k in fields:p.error('Unknown or duplicate config field')
    fields[k]=v
if set(fields)!={b'speed_ms',b'wrap',b'palette'}:p.error('Missing required config fields')
try:
    speed=int(fields[b'speed_ms']); assert str(speed).encode()==fields[b'speed_ms']
    assert 85<=speed<=400
except (AssertionError,ValueError):p.error('speed_ms must be 85..400')
if fields[b'wrap'] not in [b'0',b'1'] or fields[b'palette'] not in [b'forest',b'night',b'amber']:
    p.error('wrap=0/1; palette=forest/night/amber')
args=[sys.executable,str(ROOT/'tools/build_qeapp.py'),'--id','snake_pixel','--name','Pixel Snake',
      '--version',a.version,'--type','text','--text',str(config),'--icon',str(HOME/'snake_icon_32.png'),
      '--sign-key',str(a.sign_key),'-o',str(a.output)]
print('Reminder: firmware must pin matching publisher public key to install this package.')
raise SystemExit(subprocess.call(args))
