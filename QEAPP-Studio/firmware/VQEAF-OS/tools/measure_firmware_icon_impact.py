#!/usr/bin/env python3
"""Build BOTH entire ESP32-S3 firmware images with identical PlatformIO options,
changing only VQEAF_ICON_BASELINE (frozen v234) vs optimized production renderer.
Use firmware.bin sizes as the primary REAL Flash image comparison, and ELF
sections as a secondary diagnostic. If PlatformIO/toolchain is missing,
produce UNAVAILABLE (never substitute asset or host figures for firmware data).
"""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
from datetime import datetime, timezone

ROOT=Path(__file__).resolve().parents[1]
DEST=ROOT/'build_reports'/'firmware_icon_size_v236'
BASE='vqeaf_size_baseline'
OPT='vqeaf_size_optimized'
APP_SLOT=0x640000

def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()

def command(args, logname, offline=False, timeout=1800):
    env=os.environ.copy()
    env['PLATFORMIO_SETTING_ENABLE_TELEMETRY']='No'
    env['PLATFORMIO_DISABLE_PROGRESSBAR']='1'
    if offline:
        # Prevent ordinary proxy-mediated downloads. A disconnected machine is
        # required for a strong 'offline' guarantee; pre-caching still needed.
        env.update(HTTP_PROXY='http://127.0.0.1:9',HTTPS_PROXY='http://127.0.0.1:9',
                   ALL_PROXY='http://127.0.0.1:9',NO_PROXY='')
    try:
        proc=subprocess.run(args,cwd=ROOT,env=env,text=True,
              stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=timeout)
        output=proc.stdout
        rc=proc.returncode
    except subprocess.TimeoutExpired as e:
        output=(e.stdout or b'')
        if isinstance(output,bytes): output=output.decode('utf-8','replace')
        output+='\nTIMEOUT after %ss\n'%timeout
        rc=124
    (DEST/logname).write_text('COMMAND: '+repr(args)+'\n\n'+output,encoding='utf-8')
    return rc,output

def locate_size():
    core=Path(os.environ.get('PLATFORMIO_CORE_DIR',str(Path.home()/'.platformio')))
    folders=list((core/'packages').glob('toolchain-xtensa-esp32s3/bin/*size*'))
    folders+=list((core/'packages').glob('toolchain-xtensa-esp-elf/bin/*size*'))
    for tool in folders:
        if tool.is_file() and ('size' in tool.stem):return str(tool)
    return shutil.which('xtensa-esp32s3-elf-size') or shutil.which('xtensa-esp-elf-size')

def sections(tool, elf, name):
    if not tool:return None
    rc,out=command([tool,'-A',str(elf)],name+'_elf_sections.log',timeout=40)
    if rc:return None
    entries={}
    for line in out.splitlines():
        v=re.match(r'^\s*(\.[-_.A-Za-z0-9]+)\s+(\d+)\s+(?:\d+|0x[0-9a-fA-F]+)',line)
        if v:entries[v.group(1)]=int(v.group(2))
    return entries or None

def error_hint(text):
    s=text.lower()
    if 'no module named platformio' in s or 'not recognized as' in s:return 'Install PlatformIO Core and run from the firmware project root.'
    if 'could not install' in s or 'download' in s or 'unknown package' in s:return 'Platform/toolchain/library missing: prime PlatformIO cache while online first.'
    if 'fatal error:' in s or 'compilation terminated' in s:return 'C/C++ compile error: inspect first fatal error and include paths.'
    if 'undefined reference' in s or 'collect2:' in s:return 'Linker error: inspect undefined symbols and pinned dependency versions.'
    if 'region' in s and 'overflowed' in s:return 'Linker region overflow: review partitions, rodata and enabled modules.'
    return 'See preserved platformio_*.log and rerun the failing PlatformIO environment.'

def render_report(r):
    status=r['status']
    lines=['# VQEAF OS v2.3.6 — real whole-firmware icon size measurement','',
           'Generated: '+r['utc'],'', 'Status: **'+status+'**','',
           'Comparison: identical firmware sources / build_flags, switching only ',
           '`VQEAF_ICON_BASELINE=1` (frozen v2.3.4 decoder + arrays) versus optimized v2.3.6.','',
           '| Metric | Baseline | Optimized | Difference (baseline − optimized) |',
           '|---|---:|---:|---:|']
    b=r.get('baseline',{}) or {};o=r.get('optimized',{}) or {};d=r.get('difference',{}) or {}
    def row(label,key,suffix=' B'):
        lines.append(f'| {label} | {b.get(key,"—")}{suffix if key in b else ""} | '
                     f'{o.get(key,"—")}{suffix if key in o else ""} | '
                     f'{d.get(key,"not measured")}{suffix if key in d else ""} |')
    row('Entire firmware.bin size','firmware_bin_bytes')
    row('ELF .text+.rodata+.data subset (when tool available)','elf_flash_section_subset')
    lines+=['',f'App partition size (per OTA slot): **{APP_SLOT:,} B**.',
            'Image size is NOT the same metric as occupied chips/partition bytes or running RAM.',
            'The ELF subset is supplementary and may exclude other loadable sections.','',
            'Static icon-only payload: **8,260 B → 6,443 B (−1,817 B, 22.00%)**.',
            'This 1,817 B is NOT a measured whole-firmware result.','',
            '## Execution']
    for k,v in r.get('checks',{}).items():lines.append(f'- {k}: {v}')
    if r.get('error'):
        lines+=['','## Problem',r['error'],'',r.get('suggestion','')]
    lines+=['','## Raw evidence','`build_reports/firmware_icon_size_v236/` contains complete logs, '
             '`report.json`, `report.md`, and SHA-256 fingerprints.','',
            '**No ESP32 board was connected or flashed by this script.**','']
    (DEST/'report.md').write_text('\n'.join(lines),encoding='utf-8')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--report-only',action='store_true',help='Do not invoke PlatformIO; publish honest unavailable report.')
    parser.add_argument('--reuse-existing',action='store_true',help='Measure preexisting env artifacts; marked not freshly built.')
    parser.add_argument('--offline',action='store_true',help='Best-effort block HTTP proxy use; install deps beforehand.')
    parser.add_argument('--pio',default='',help='Full path to PlatformIO executable if not in PATH.')
    args=parser.parse_args()
    DEST.mkdir(parents=True,exist_ok=True)
    meta={
       'utc':datetime.now(timezone.utc).isoformat(timespec='seconds'),
       'subject':'full VQEAF OS ESP32-S3 firmware.bin delta',
       'board':'vqeaf_s3_n16r8','baseline_environment':BASE,'optimized_environment':OPT,
       'source_baseline_sha256':sha(ROOT/'docs/verification/v234_baseline/VqeafIconData.h'),
       'source_optimized_sha256':sha(ROOT/'src/core/VqeafIconData.h'),
       'payload_bytes':{'baseline':8260,'optimized':6443,'saved':1817},
       'app_slot_bytes':APP_SLOT,'status':'UNAVAILABLE','checks':{},
       'measurement_origin':'not measured'
    }
    pio=[args.pio] if args.pio else ([shutil.which('pio')] if shutil.which('pio') else [])
    if not pio and not args.report_only and not args.reuse_existing:
        try:
            if subprocess.run([sys.executable,'-m','platformio','--version'],
                stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,timeout=5).returncode==0:
                pio=[sys.executable,'-m','platformio']
        except (OSError,subprocess.TimeoutExpired):pass
    if args.report_only:
        meta.update(error='Measurement intentionally not requested in --report-only mode.',
                    suggestion='Run: python tools/measure_firmware_icon_impact.py on a PC with PlatformIO and target toolchain.')
    elif not pio and not args.reuse_existing:
        meta.update(error='PlatformIO not installed / not accessible in the current environment.',
                    suggestion='Install PlatformIO and required packages; execute the script to produce actual target firmware.bin data.')
    elif not args.reuse_existing:
        rc,output=command(pio+['--version'],'platformio_version.log',args.offline,60)
        meta['checks']['platformio_version']='PASS' if rc==0 else 'FAIL'
        if rc:
            meta.update(error='PlatformIO failed its version check.',suggestion=error_hint(output))
        else:
            for label in (BASE,OPT):
                rc,out=command(pio+['run','-e',label,'-t','clean'],label+'_clean.log',args.offline)
                meta['checks'][label+'_clean']='PASS' if rc==0 else 'FAIL'
                if rc:
                    meta.update(error=f'PlatformIO clean failed for {label}.',suggestion=error_hint(out))
                    break
                rc,out=command(pio+['run','-e',label],label+'_build.log',args.offline)
                meta['checks'][label+'_build']='PASS' if rc==0 else 'FAIL'
                if rc:
                    meta.update(error=f'PlatformIO build failed for {label}.',suggestion=error_hint(out))
                    break
    if args.reuse_existing:
        meta['checks']['reuse_existing']='UNVERIFIED provenance: generated artifacts might not match current source'
    if (all(meta['checks'].get(label+'_build')=='PASS' for label in (BASE,OPT)) or args.reuse_existing):
        tool=locate_size()
        products=[]
        for label in (BASE,OPT):
            build=ROOT/'.pio'/'build'/label
            binary=build/'firmware.bin'; elf=build/'firmware.elf'
            if not binary.is_file() or not elf.is_file():
                meta.update(error=f'Missing firmware.bin / firmware.elf in {build}',
                            suggestion='Build both target environments; do not use self-test firmware for size comparison.')
                break
            dat={ 'firmware_bin_bytes':binary.stat().st_size,
                 'firmware_bin_sha256':sha(binary), 'firmware_elf_sha256':sha(elf) }
            sec=sections(tool,elf,label)
            if sec:
                dat['elf_sections']=sec
                # ESP32 commonly places code/const in .flash.text/.flash.rodata,
                # older ESP IDF in .irom0.text/.drom0.rodata etc. Record full
                # sections and a conservative subset; bin remains authority.
                names=('.text','.rodata','.data','.flash.text','.flash.rodata',
                       '.irom0.text','.drom0.rodata','.dram0.data','.iram0.text')
                dat['elf_flash_section_subset']=sum(sec.get(k,0) for k in names)
            meta['baseline' if label==BASE else 'optimized']=dat
            products.append(dat)
        if len(products)==2:
            baseline,optimized=products
            delta=baseline['firmware_bin_bytes']-optimized['firmware_bin_bytes']
            meta['difference']={'firmware_bin_bytes':delta}
            if ('elf_flash_section_subset' in baseline and 'elf_flash_section_subset' in optimized):
                meta['difference']['elf_flash_section_subset']=(
                  baseline['elf_flash_section_subset']-optimized['elf_flash_section_subset'])
            meta['whole_firmware_saved_percent']=(100*delta/baseline['firmware_bin_bytes']
                                if baseline['firmware_bin_bytes'] else 0)
            meta['status']='PASS' if not args.reuse_existing else 'REUSED_ARTIFACTS_UNVERIFIED'
            meta['measurement_origin']='fresh PlatformIO target build' if not args.reuse_existing else 'reuse-existing'
            meta.pop('error',None)
            meta.pop('suggestion',None)
    (DEST/'report.json').write_text(json.dumps(meta,indent=2,ensure_ascii=False)+'\n',encoding='utf-8')
    render_report(meta)
    print('Status:',meta['status'])
    print('Report:',DEST/'report.md')
    if meta.get('error'):print('Reason:',meta['error'])
    if 'difference' in meta:
        print('Whole firmware.bin saved bytes:',meta['difference']['firmware_bin_bytes'])
    return 0 if meta['status'] in ('PASS','REUSED_ARTIFACTS_UNVERIFIED') or args.report_only else 2

if __name__=='__main__':sys.exit(main())
