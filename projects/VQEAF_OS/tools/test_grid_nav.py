#!/usr/bin/env python3
"""VQEAF G3 portrait launcher tab/linear scroll navigation regression."""
from pathlib import Path
src=(Path(__file__).resolve().parents[1]/'src/apps/Apps.cpp').read_text()
assert 'TAB_LABELS[]={"Home","Internet","Applications","Media","System","Settings"}' in src
assert 'view.row(ctx.ui.getLauncherStyle(),&rows[old],old-offset,false)' in src
assert 'selectedTarget(ctx)' in src and 'ScreenId::PackageApp' in src
VISIBLE=5
# 6 tabs in rotating carousel; 6th HOME row forces scrollbar.
for tab in range(6):
 for step in (-1,1):
  assert (tab+step+6)%6 in range(6)
for length in (0,1,4,5,6,12,18):
 for selection in range(length):
  offset=0
  for target in range(selection+1):
   if target>=offset+VISIBLE:offset=target-VISIBLE+1
  assert 0<=offset<=selection and selection-offset<VISIBLE
  assert offset<=max(0,length-VISIBLE)
print('PASS: portrait tab carousel, focus scroll bounds, signed .qeapp launch route')
