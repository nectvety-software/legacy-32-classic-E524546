import sys
from luaparser import ast

files = ['data/main.lua', 'data/conf.lua', 'data/src/player.lua',
         'data/src/doodle.lua', 'data/src/os.lua', 'data/src/apps.lua']
bad = 0
for f in files:
    src = open(f, encoding='utf-8').read()
    try:
        ast.parse(src)
        print('SYNTAX OK  ', f)
    except Exception as e:
        bad += 1
        print('SYNTAX FAIL', f)
        print(str(e)[:2000])
sys.exit(1 if bad else 0)
