#!/usr/bin/env python3
"""Offline, dependency-free checks for the exact VQEAF OS ESP32-S3 N16R8 board.

These validate wiring, manifest, CSV layout and pinned build configuration.
Passing is NOT equivalent to compiling against Arduino/ESP-IDF or flashing hardware.
"""
from __future__ import annotations
from pathlib import Path
import configparser
import csv
import json
import re

ROOT = Path(__file__).resolve().parent.parent
EXPECTED = {
    'TFT_LEDK_PIN':39,'TFT_DC_PIN':47,'TFT_CS_PIN':14,'TFT_SCL_PIN':48,
    'TFT_SDA_PIN':12,'TFT_RST_PIN':3,
    'KEY_MENU':18,'KEY_UP':7,'KEY_A':15,'KEY_LEFT':45,'KEY_START':17,
    'KEY_RIGHT':6,'KEY_OPTION':8,'KEY_DOWN':46,'KEY_B':5,'KEY_SELECT':16,
    'SD_D3':10,'SD_CMD':11,'SD_CLK':13,'SD_D0':9,
    'TFT_ROTATION':0,'SCREEN_W':240,'SCREEN_H':320,
}
PIN_FLAGS={'TFT_WIDTH':240,'TFT_HEIGHT':320,'TFT_MOSI':12,'TFT_SCLK':48,
           'TFT_CS':14,'TFT_DC':47,'TFT_RST':3,'TFT_BL':39}


def read_sizes(s):
    s=s.strip().lower()
    if s.endswith('m'): return int(s[:-1],0)*1024*1024
    if s.endswith('k'): return int(s[:-1],0)*1024
    return int(s,0)


def check():
    ini = configparser.ConfigParser(interpolation=None)
    ini.read(ROOT/'platformio.ini', encoding='utf-8')
    assert ini['platformio']['default_envs'].strip()=='vqeaf_os'
    e=ini['env:vqeaf_os']
    assert e['platform'].replace(' ','')=='espressif32@6.10.0'
    assert e['board'].strip()=='vqeaf_s3_n16r8'
    assert e['framework'].strip()=='arduino'
    assert e['board_build.filesystem'].strip()=='littlefs'
    assert e['board_build.arduino.memory_type'].strip()=='qio_opi'
    assert e['board_build.flash_mode'].strip()=='qio'
    assert e['board_build.psram_type'].strip()=='opi'
    assert e['board_upload.flash_size'].strip()=='16MB'
    assert e['monitor_speed'].strip()=='115200'
    b=json.loads((ROOT/'boards/vqeaf_s3_n16r8.json').read_text())
    assert b['build']['mcu']=='esp32s3'
    assert b['build']['variant']=='esp32s3'
    assert b['build']['arduino']['memory_type']=='qio_opi'
    assert b['upload']['flash_size']=='16MB'
    assert b['build']['flash_mode']=='qio'
    assert b['upload']['maximum_ram_size']==327680  # internal RAM field from upstream board
    assert b['upload']['maximum_size']==int(e['board_upload.maximum_size'])
    print('PASS board: project-local ESP32-S3 N16R8 QIO flash + OPI PSRAM')

    board_config=(ROOT/'include/BoardConfig.h').read_text()
    for name, pin in EXPECTED.items():
        m=re.search(r'constexpr\s+(?:int|uint8_t)\s+'+name+r'\s*=\s*(\d+)\s*;',board_config)
        assert m and int(m.group(1))==pin,(name,pin, m.group(1) if m else None)
    all_pins=[EXPECTED[k] for k in EXPECTED if k.startswith(('TFT_','KEY_','SD_')) and k not in ('TFT_ROTATION',)]
    assert len(all_pins)==len(set(all_pins)), 'duplicate GPIO'
    flags=dict((k,int(v)) for k,v in re.findall(r'^\s*-D\s+(TFT_\w+)=(\d+)\s*$',e['build_flags'],re.M))
    for name,pin in PIN_FLAGS.items(): assert flags.get(name)==pin,(name,flags.get(name),pin)
    for define in ('BOARD_HAS_PSRAM','VQEAF_BOARD_N16R8','USER_SETUP_LOADED','ST7789_DRIVER'):
        assert re.search(r'-D\s+'+define+r'(?:=1)?(?:\n|$)', e['build_flags']),define
    print('PASS pins: LCD (SPI2), all 10 buttons, SDMMC 1-bit; portrait 240x320')

    libs=e['lib_deps']
    for lib in ('bodmer/TFT_eSPI@2.5.43','h2zero/NimBLE-Arduino@2.3.6',
                'bodmer/TJpg_Decoder@1.1.0','bitbank2/PNGdec@1.1.6'):
        assert lib in libs, f'missing pin: {lib}'
    assert '[env:esp32-s3-st7789]' not in (ROOT/'platformio.ini').read_text()
    print('PASS platform: pinned espressif32 6.10.0 and four pinned libraries')

    path=ROOT/'partitions/vqeaf_16mb_ota.csv'
    assert e['board_build.partitions'].strip()==path.relative_to(ROOT).as_posix()
    regions=[]
    for line in path.read_text().splitlines():
        if not line.strip() or line.lstrip().startswith('#'):continue
        c=next(csv.reader([line],skipinitialspace=True))
        assert len(c)>=5,line
        name,kind,sub,offset,size=[v.strip() for v in c[:5]]
        off=read_sizes(offset); size=read_sizes(size)
        assert size>0 and off>=0
        if kind=='app':assert off%0x10000==0
        else:assert off%0x1000==0
        assert off+size<=16*1024*1024, f'partition exceeds flash {name}'
        regions.append((off,off+size,name,kind,sub))
    regions.sort()
    for old,new in zip(regions,regions[1:]):
        assert old[1]<=new[0],f'overlap {old[2]} vs {new[2]}'
    r={v[2]:v for v in regions}
    assert len(regions)==6
    assert r['app0'][3:] == ('app','ota_0') and r['app1'][3:]==('app','ota_1')
    assert r['app0'][1]-r['app0'][0]==0x640000
    assert r['app1'][1]-r['app1'][0]==0x640000
    assert r['coredump'][1]==0x1000000
    print('PASS partitions: NVS, OTA data, 2x 6.25MiB app slots, flash FS, coredump; no overlaps')
    main=(ROOT/'src/main.cpp').read_text()
    assert 'core/BuildSanity.h' in main
    assert 'Board::TFT_ROTATION' in (ROOT/'src/core/SymbianUI.cpp').read_text()
    assert '[VQEAF][MEM]' in main
    print('PASS build guards: C++11 pin asserts and serial memory diagnostics installed')
    return len(regions)

if __name__=='__main__':
    try: check()
    except Exception as e:
        print(f'FAIL board preflight: {type(e).__name__}: {e}')
        raise SystemExit(1)
