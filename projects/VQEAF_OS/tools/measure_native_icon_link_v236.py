#!/usr/bin/env python3
"""Supplemental linked HOST icon-harness size; NOT full ESP32-S3 firmware."""
from pathlib import Path
import json,re,shutil,subprocess,tempfile
R=Path(__file__).resolve().parents[1]
OUT=R/'build_reports'/'firmware_icon_size_v236'
OUT.mkdir(parents=True,exist_ok=True)
assert shutil.which('g++') and shutil.which('size'), 'g++ + binutils size required'
results={}
with tempfile.TemporaryDirectory(prefix='vqeaf-host-link-') as folder:
    for version,flag in [('baseline','-DVQEAF_ICON_BASELINE=1'),('optimized',None)]:
        binary=Path(folder)/version
        args=['g++','-std=c++11','-Os','-flto','-ffunction-sections',
              '-fdata-sections','-Wl,--gc-sections',
              '-Isrc/core','-Itools/icon_host']
        if flag:args.append(flag)
        args+=['src/core/VqeafIconRenderer.cpp',
               'tools/icon_host/dump_all_pixels.cpp','-o',str(binary)]
        proc=subprocess.run(args,cwd=R,text=True,capture_output=True,check=True)
        (OUT/f'native_{version}_build.log').write_text(' '.join(args)+'\n'+proc.stdout+'\n'+proc.stderr)
        proc=subprocess.run(['size',str(binary)],text=True,capture_output=True,check=True)
        (OUT/f'native_{version}_size.log').write_text(proc.stdout)
        parts=proc.stdout.splitlines()[-1].split()
        assert len(parts)>=5
        results[version]=dict(text=int(parts[0]),data=int(parts[1]),bss=int(parts[2]),total=int(parts[3]))
results['delta_baseline_minus_optimized']={key:results['baseline'][key]-results['optimized'][key]
                                            for key in ('text','data','bss','total')}
results['scope']='host executable with icon renderer + host dump harness; NOT whole ESP32 firmware'
(OUT/'native_linked_size.json').write_text(json.dumps(results,indent=2)+'\n')
print(json.dumps(results,indent=2))
