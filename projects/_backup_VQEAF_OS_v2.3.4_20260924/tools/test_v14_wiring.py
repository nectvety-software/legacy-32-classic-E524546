#!/usr/bin/env python3
"""Source integration checks complementing the executable host installer tests."""
from pathlib import Path
import re
R=Path(__file__).resolve().parent.parent
read=lambda f:(R/f).read_text()
main=read('src/main.cpp')
apps=read('src/apps/Apps.cpp')
apph=read('src/apps/Apps.h')
svc=read('src/services/AppInstallerService.cpp')
format_cpp=read('src/services/QeappFormat.cpp')
browser=read('src/services/BrowserService.cpp')
storage=read('src/services/StorageService.cpp')
checks={
  'portrait width 240': re.search(r'SCREEN_W\s*=\s*240',read('include/BoardConfig.h')) is not None,
  'portrait height 320': re.search(r'SCREEN_H\s*=\s*320',read('include/BoardConfig.h')) is not None,
  'installer registered in main': 'static AppInstallerService appInstaller;' in main and 'appInstaller.begin(storage)' in main,
  'installer screen routes input': 'appInstallerApp.handle(appCtx, e)' in main,
  'installer listed in Applications': '"App installer"' in apps,
  'dynamic installed app list': 'APP_COUNT+ctx.installer.count()' in apps,
  'app launch route exists': 'ScreenId::PackageApp' in main and 'pendingPackageId' in main,
  'web/text isolated dispatch': 'pendingBrowserUrl = meta.entry' in main and 'pendingOpenPath = appInstaller.installedPath' in main,
  'file manager opens qeapp': 'lower.endsWith(".qeapp")' in apps and 'ScreenId::AppInstaller' in apps,
  'browser routes downloaded qeapp': 'lower.endsWith(".qeapp")' in browser and 'APPS_INBOX' in browser,
  'sd installed paths': 'StoragePaths::APPS_INSTALLED' in storage,
  'bounded file copy': 'uint8_t buffer[512]' in svc and 'MAX_INSTALLED' in read('src/services/AppInstallerService.h'),
  'SHA256 manifest/icon/payload': all(tok in svc for tok in ('manifestHash','iconHash','payloadHash','SHA-256')),
  'atomic stage then rename': 'fs.rename(stage,finalDir)' in svc,
  'strict package IDs': 'Unsafe app id' in format_cpp,
  'no native package execution': all(x not in svc for x in ('system(', 'exec(', 'dlopen(', 'loadELF(')),
  'check installed limit': 'used >= MAX_INSTALLED' in svc,
  'S60 theme preserved': 'S60Green' in read('src/core/Theme.h'),
  'full package documentation': (R/'docs/QEAPP_V14.md').exists(),
  'sample packages present': all((R/f'sd/System/Apps/Inbox/{n}.qeapp').exists() for n in ('welcome','help_site')),
}
for name,ok in checks.items():
    if not ok: raise AssertionError(name)
    print('PASS:',name)
print(f'v1.4 system integration: {len(checks)}/{len(checks)} PASS')
