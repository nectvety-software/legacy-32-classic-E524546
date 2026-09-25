#!/usr/bin/env python3
"""Run real v2.3.4 pixel art Home/Menu source route + layout regression suite."""
import subprocess,sys
from pathlib import Path
r=Path(__file__).resolve().parents[1]
sys.exit(subprocess.call([sys.executable,str(r/'tools/test_icon_integration_v232.py')],cwd=r))
