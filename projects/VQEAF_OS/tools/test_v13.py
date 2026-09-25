#!/usr/bin/env python3
from pathlib import Path
import subprocess, shutil, tempfile
ROOT=Path(__file__).resolve().parents[1]
def read(p): return (ROOT/p).read_text(encoding='utf-8')
def check(name, cond):
    if not cond: raise AssertionError(name)
    print('PASS:',name)

uih=read('src/core/SymbianUI.h'); uic=read('src/core/SymbianUI.cpp')
sth=read('src/services/StorageService.h'); stc=read('src/services/StorageService.cpp')
bh=read('src/services/BrowserService.h'); bc=read('src/services/BrowserService.cpp')
apps=read('src/apps/Apps.cpp'); main=read('src/main.cpp'); shell=read('src/services/ShellService.cpp')
theme=read('src/services/ThemeFileService.cpp')
board=read('include/BoardConfig.h')
check('portrait 240x320', 'SCREEN_W = 240' in board and 'SCREEN_H = 320' in board)
check('Nokia-like bold helper', 'void SymbianUI::textBold' in uic and 'drawn twice one pixel apart' in uic)
check('bold title/list/grid/softkeys', all(x in uic for x in ('textBold(4, 5, cut','textBold(textX, y + 3','textBold(x + max(2','textBold(4, y + 3, left')))
check('no custom font asset dependency', 'LOAD_FONT2=1' in read('platformio.ini'))
for p in ('CACHE_WEB','CACHE_THUMBS','THEMES','APPS_INSTALLED','APPS_INBOX','DOWNLOADS','LOGS','TEMP','MUSIC','PICTURES','DOCUMENTS'):
    check('storage path '+p, ('StoragePaths::'+p) in stc or (p+' =') in sth)
check('layout created after SD mount', 'if (ok) ensureSystemLayout();' in stc)
check('atomic SD write', '.tmp' in stc and 'SD_MMC.rename(tmp, path)' in stc)
check('bounded web cache', '16, 512UL * 1024UL' in bc and 'pruneFlatDirectory' in bc)
check('offline browser cache', 'loadCache(url' in bc and 'Offline cache' in bc and 'pageFromCache' in bh)
check('streamed browser downloads', 'uint8_t buf[512]' in bc and 'MAX_DOWNLOAD = 4 * 1024 * 1024' in bc)
check('download routing themes', 'StoragePaths::THEMES' in bc and 'endsWith(".vqeaf")' in bc)
check('download routing app inbox', 'StoragePaths::APPS_INBOX' in bc and 'endsWith(".qeapp")' in bc)
check('download routing general', 'StoragePaths::DOWNLOADS' in bc)
check('part-file finalize', '.part' in bc and '.rename(tmpPath, savedPath)' in bc)
check('browser UI download action', 'Download link' in apps and 'ctx.browser.download' in apps)
check('browser cache marker', 'pageFromCache()' in apps and 'd.print("C")' in apps)
check('Files quick folders', all(x in apps for x in ('"Downloads", "Themes", "App inbox"','StoragePaths::APPS_INBOX')))
check('Applications App inbox', '"App inbox"' in apps and 'Downloaded app packages' in apps)
check('media preferred roots', all(x in apps for x in ('StoragePaths::MUSIC','StoragePaths::PICTURES','StoragePaths::DOCUMENTS')))
check('theme preferred system folder', 'StoragePaths::THEMES, "/Themes", "/"' in theme)
check('shell layout/cache commands', 'commandLayout()' in shell and 'cache [status|prune|clear]' in shell and 'cmd == "cache"' in shell)
check('browser initialized after SD mount', main.find('bool sdOk = storage.begin();') < main.find('browserService.begin(sdOk ? &storage : nullptr)'))
check('v1.3 version', 'v1.3.0 SD Platform' in main and 'Symbian S3 OS v1.3.0' in read('README.md'))
for rel in ['sd/System/Themes/amoled_red.vqeaf','sd/System/Themes/s60_green.vqeaf','sd/System/Apps/Inbox/README.txt','docs/SD_STORAGE_V13.md','docs/BROWSER_DOWNLOADS_V13.md']:
    check('artifact '+rel, (ROOT/rel).exists())
# Structural brace sanity. Not a compiler, but catches edit corruption before packaging.
for p in sorted((ROOT/'src').rglob('*')):
    if p.suffix in {'.cpp','.h'} and p.name != 'ThemeFileService.cpp':
        txt=p.read_text(encoding='utf-8')
        check('balanced '+str(p.relative_to(ROOT)), txt.count('{')==txt.count('}'))
# Re-run the existing real C++ VQEAF loader test because ThemeFileService changed scan roots.
gcc=shutil.which('g++')
if gcc:
    with tempfile.TemporaryDirectory() as tmp:
        binary=Path(tmp)/'vqeaf_test'
        stub=ROOT/'tools/theme_host'
        subprocess.run([gcc,'-std=gnu++11','-Wall','-Wextra','-I'+str(stub),'-I'+str(ROOT/'src/services'),'-I'+str(ROOT/'src/core'),str(stub/'test_theme_runtime.cpp'),str(ROOT/'src/services/ThemeFileService.cpp'),'-o',str(binary)],check=True)
        subprocess.run([str(binary),str(ROOT/'sd/Themes/amoled_red.vqeaf'),str(ROOT/'sd/Themes/s60_green.vqeaf')],check=True)
    check('native VQEAF loader regression',True)
print('v1.3 gates complete')
