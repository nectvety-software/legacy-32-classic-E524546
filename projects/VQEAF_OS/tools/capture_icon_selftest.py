#!/usr/bin/env python3
"""Capture ESP32-S3 icon CRC/SPI diagnostic log over Serial (115200).
Or validate an existing log without installing pyserial using --parse-log.
Strict: requires all 24 variant lines, memory and timing and exact 192/192.
"""
import argparse
import json
from pathlib import Path
import re
import sys
from datetime import datetime, timezone
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build_reports'/'icon_device_v236'

VARIANT=re.compile(r'^\[ICONTEST\] VARIANT ([A-Za-z]+) size=(24|36) cases=(\d)/8$',re.M)
SUMMARY=re.compile(r'^\[ICONTEST\] SUMMARY passed=(\d+) expected=(\d+) errors=(\d+) result=(PASS|FAIL)$',re.M)
MEMORY=re.compile(r'^\[ICONTEST\] MEMORY internal_before=(\d+) internal_after=(\d+) psram_before=(\d+) psram_after=(\d+)$',re.M)
TIMING=re.compile(r'^\[ICONTEST\] TIMING decode_192_cases_us=(\d+) spi_draw_72_calls_us=(\d+)$',re.M)
NAMES={'WiFi','Bluetooth','Music','Files','Gallery','Internet','Shell',
       'Recovery','Settings','Themes','Apps','Library'}

def parse(data):
    groups={(name,int(sz)):int(p) for name,sz,p in VARIANT.findall(data)}
    expected={(name,sz) for name in NAMES for sz in (24,36)}
    summary=SUMMARY.findall(data)
    mem=MEMORY.findall(data)
    timing=TIMING.findall(data)
    board_mem_match=re.search(r'^\[VQEAF\]\[MEM\] flash=(\d+) psram=(\d+) free_heap=(\d+)',data,re.M)
    hw_ok=(board_mem_match is not None and int(board_mem_match[1])>=16*1024*1024 and
           int(board_mem_match[2])>=8*1024*1024)
    # Avoid success if log was truncated, accidentally concatenated with
    # another boot, or produced by an outdated diagnostic firmware.
    valid=(len(summary)==1 and len(mem)==1 and len(timing)==1 and
           set(groups)==expected and len(VARIANT.findall(data))==24 and
           all(v==8 for v in groups.values()) and
           tuple(summary[0])==('192','192','0','PASS') and
           '[ICONTEST] START v2.3.6 cases=192' in data and
           '[ICONTEST] FAIL' not in data)
    board_boot='[VQEAF][BUILD]' in data and '[VQEAF][MEM]' in data
    result={'status':('PASS' if hw_ok else 'HARDWARE_MISMATCH') if board_boot and valid else ('UNVERIFIED_ORIGIN' if valid else 'FAIL'),
            'evidence':'device boot log' if '[VQEAF][BUILD]' in data and '[VQEAF][MEM]' in data else 'unproven origin (may be host)',
            'variant_count':len(groups),'valid_variant_checks':sum(v for v in groups.values()),
            'summary':summary[-1] if summary else None,
            'memory':mem[-1] if mem else None,
            'timing_us':timing[-1] if timing else None,
            'logs_contains_board_boot_marker':board_boot,
            'n16r8_memory_claim_matches':hw_ok if board_boot else None,
            'warnings':[]}
    if not result['logs_contains_board_boot_marker']:
        result['warnings'].append('No [VQEAF][BUILD]/[VQEAF][MEM] pre-test markers; PASS tests alone do not prove physical-board execution.')
    if board_boot and not hw_ok:result['warnings'].append('Board flash/PSRAM values do not meet 16 MB/8 MB N16R8 or pre-test memory line is incomplete.')
    if not valid:result['warnings'].append('Missing/failed/truncated/duplicate device checks; inspect raw serial log.')
    return result

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    group=ap.add_mutually_exclusive_group(required=True)
    group.add_argument('--parse-log',type=Path,help='Validate saved UART log without pyserial')
    group.add_argument('--port',help='Serial device e.g. COM5 or /dev/ttyACM0')
    ap.add_argument('--timeout',type=int,default=90,help='Seconds to wait; press RESET once monitor has opened')
    args=ap.parse_args()
    OUT.mkdir(parents=True,exist_ok=True)
    if args.parse_log:
        data=args.parse_log.read_text(encoding='utf-8',errors='replace')
    else:
        try:import serial
        except ImportError:
            print('ERROR: Install pyserial, or capture pio device monitor -b 115200 and use --parse-log.',file=sys.stderr)
            return 2
        import time
        lines=[]
        print(f'Listening on {args.port} at 115200; press RESET on your ESP32-S3 now...')
        with serial.Serial(args.port,115200,timeout=0.5) as port:
            start=time.monotonic()
            while time.monotonic()-start < args.timeout:
                line=port.readline().decode('utf-8','replace')
                if not line:continue
                print(line,end='',flush=True)
                lines.append(line)
                if '[ICONTEST] SUMMARY ' in line:break
        data=''.join(lines)
    (OUT/'board_serial.log').write_text(data,encoding='utf-8')
    result=parse(data)
    result['captured_utc']=datetime.now(timezone.utc).isoformat(timespec='seconds')
    result['source']='saved log' if args.parse_log else 'serial:'+args.port
    (OUT/'report.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    rows=[f'# VQEAF OS icon device verification: {result["status"]}',
          '',f'Input: {result["source"]}',f'Variant checks: {result["variant_count"]}/24',
          f'CRC checks passed: {result["valid_variant_checks"]}/192',
          f'Device boot evidence: {result["logs_contains_board_boot_marker"]}',
          f'Summary: {result["summary"]}',f'Timing (microseconds): {result["timing_us"]}',
          f'Memory stats: {result["memory"]}',
          '',*['WARNING: '+w for w in result['warnings']],'']
    (OUT/'report.md').write_text('\n'.join(rows),encoding='utf-8')
    print('\nRESULT:',result['status'])
    print('Saved:',OUT/'report.md')
    return {'PASS':0,'UNVERIFIED_ORIGIN':2,'FAIL':1,'HARDWARE_MISMATCH':3}[result['status']]

if __name__=='__main__':sys.exit(main())
