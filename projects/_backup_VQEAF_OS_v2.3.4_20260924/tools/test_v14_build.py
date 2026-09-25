#!/usr/bin/env python3
"""Host-only GNU++11 syntax/link diagnostic for all firmware C++ units.

This verifies source compatibility and duplicate symbols with our mock API;
it does NOT compile ESP-IDF libraries, flash hardware, or test live key/LCD I/O.
"""
import subprocess
import tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parent.parent
STUB=ROOT/"tools/host_stubs"
FLAGS=["-std=c++11","-fpermissive","-DARDUINO","-DQEAPP_HOST_STUB_CRYPTO=1","-DTFT_DC=47","-DTFT_CS=14","-DTFT_RST=3",
       "-I"+str(STUB),"-I"+str(ROOT/"include"),"-I"+str(ROOT/"src")]

def run(cmd):
    p=subprocess.run(cmd,capture_output=True,text=True)
    if p.returncode:
        print(p.stdout);print(p.stderr);raise SystemExit(p.returncode)

def main():
    files=sorted((ROOT/"src").rglob("*.cpp"))
    with tempfile.TemporaryDirectory(prefix="s3-v14-build-") as tmp:
        objs=[]
        for i,f in enumerate(files):
            obj=Path(tmp)/(str(i)+".o")
            run(["g++",*FLAGS,"-c",str(f),"-o",str(obj)])
            objs.append(str(obj))
            print("PASS:",f.relative_to(ROOT))
        run(["g++",*FLAGS,*objs,str(STUB/"host_globals.cpp"),
             str(STUB/"host_entry.cpp"),"-o",str(Path(tmp)/"host_firmware")])
    assert len(files)>=19, f"Expected at least v1.4 baseline 19 translation units; found {len(files)}"
    print(f"PASS: {len(files)}/{len(files)} host C++11 translation units + full host link")
    print("NOTE: mock Arduino/ESP32 APIs; PlatformIO target build not verified")
if __name__=="__main__":main()
