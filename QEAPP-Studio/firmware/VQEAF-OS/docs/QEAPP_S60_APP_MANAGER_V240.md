# VQEAF OS v2.4.0 — S60-style App Manager core (ESP32-S3)

This release implements a **package-management workflow inspired by classic
phone application managers**. It does **not** implement or ship Symbian, S60,
Symbian `.sis`, native ELF code loading, unrestricted Lua, or third-party
binary execution. The OS, icons and UI remain VQEAF originals.

## Device and compatibility

- ESP32-S3-WROOM-1 N16R8; ST7789 **240 x 320 portrait**, TFT_eSPI.
- GPIO is **unchanged** (see `include/BoardConfig.h` and the original hardware
  documents). Preserve the existing Game/T9 long-press SELECT behavior.
- Bundled Home/Menu pixel-art icons, `.vqeaf` theme loader and Qeafbrowser
  remain in the same firmware, including existing historical screens.
- Existing **signed QEAPP/2** binary format is unchanged; ECDSA P-256 and
  SHA-256 verification still run on install, catalog refresh and launch.
- **Supported external app types** are currently `web` (signed HTTPS URL)
  and `text` (signed bundled text). Other types are rejected.

## App lifecycle for signed packages

1. A signed `.qeapp` package is placed in `/System/Apps/Inbox/` on microSD,
   copied there from a PC or by the browser download manager.
2. **Menu → Applications → App installer → App inbox** scans up to 12 packages.
   Select **Details** to see the signed metadata, version, type, icon and
   allowed resource. The signature is verified before a user can install.
3. Choose **Install**, read the permission summary and explicitly confirm.
   The installer shows progress, checks storage and copies bounded chunks.
4. An app enters the signed installed catalog after verifying the staged
   receipt and verifying again after activation. The launcher also checks
   installed bytes again when opening the app (not only at boot).
5. A package **with the same ID and a higher numeric dotted version** is
   shown as **Update**. Current, equivalent and lower versions are rejected;
   the old signed version remains installed. All signed updates must use the
   publisher key pinned in the firmware.
6. Installed apps appear in the `Applications` screen and Retro-inspired
   Explorer tab. `web` launches Qeafbrowser; `text` opens Text Viewer.
   Existing built-in apps still use the normal OS task switcher.
7. **Uninstall** removes only known installer-owned files from the selected
   signed app folder. **Reset app data** (a separate explicit action on an
   installed app) removes only the three recognized app-data slots.

## Power-loss-aware update and recovery

The install directory is `/System/Apps/Installed/<id>/`.
Each transaction uses `/System/Apps/Installed/.stage-<id>/`, and updates use
`/System/Apps/Installed/.backup-<id>/`. All IDs are checked against the
QEAPP parser's `[a-z0-9_-]` allowlist (maximum 24 characters).

Before activation: source and stage are independently checked against the
publisher's signature. For an update, the old trusted directory is renamed
into `.backup-<id>` before staging becomes `<id>`. After new final verification,
the old version is deleted if its folder contains only the four installer
files. Failed activation immediately attempts rollback to the old version.

On reboot or SD reinsertion, catalog refresh searches for leftover backups:

- If final is signature-valid and its version is at least as new as the
  signed backup, discard the known-files-only backup.
- If final is missing or corrupt, restore the **verified backup** (but never
  erase unknown files to do so).
- An unapproved orphan `.stage-<id>` is never auto-installed. Clean it only
  if it contains exclusively installer-generated files.
- If cleanup or recovery encounters unexpected files, **leave them intact**,
  report the blocked transaction and use File Manager to investigate.

**FAT is not a power-fail atomic filesystem.** This protocol reduces the risk
of an incomplete update but cannot guarantee durability against total card
failure, missing flush, malicious replacement of the SD card or every reset
window. There is no hardware-backed secure boot, key revocation, third-party
publisher trust store or general code sandbox. Back up important SD data.

## App data foundation

A separate `QeappDataService` exposes only 3 fixed data slots per signed
installed app: `prefs.bin`, `state.bin`, `draft.bin` under
`/System/Apps/Data/<id>/`. This is **an OS-internal API for future runtimes**;
web/text packages cannot currently execute arbitrary code or access it.

- 16 KiB maximum per slot and 32 KiB maximum total per app.
- App ID must match a freshly reverified signed installed app.
- No free-form path parameters; saves go through StorageService's existing
  temporary/backup/rename atomic-write helper.
- Data is preserved across signed update and app uninstall by default. A
  separately confirmed **Reset app data** action can remove known slots while
  the app is still installed. Unknown files prevent automated deletion.
- A future Lua/DSL runtime requires its own capability model, resource limits
  and sandbox; this storage API alone is **not** a script sandbox.

## Publish a developer-owned signed package

Generate your publisher key **outside the firmware source tree**:

```powershell
py -3 tools/qeapp_keys.py --private C:\secure\publisher.pem `
  --header src/services/QeappTrustKey.h
```

Rebuild firmware to pin your public key. Sign an external text app:

```powershell
py -3 tools/build_qeapp.py --id welcome --name Welcome --version 1.0.0 `
  --type text --text C:\work\welcome.txt `
  --sign-key C:\secure\publisher.pem -o welcome.qeapp
```

For updates use exactly the same `--id` and trusted publisher key with a
higher `--version` (e.g. `1.1.0`). Never ship the private key, including inside
this ZIP, flashed firmware, microSD, or a Git repository. The default sample
signing key is a development fixture, not a production trust anchor.

## Verification and limitations

```powershell
cd VQEAF-OS
py -3 tools/verify_v240.py
py -3 tools/build_offline.py --dry-run --check-only

# PlatformIO installed + exact cached dependencies, on Windows:
pio run -e vqeaf_os
pio run -e vqeaf_os -t upload
pio device monitor -b 115200
```

The host suite compiles the **actual** installer, parser, signature verifier
and app-data source under OpenSSL/POSIX SD filesystem mocks and tests:
valid install, signed higher-version update, downgrade rejection, simulated
SD rename failure, interrupted update recovery, signed backup selection,
unexpected-file preservation, explicit app-data purge and copy progress.
It also compiles/links all available firmware translation units against the
Arduino/TFT **host mocks**. Neither is a genuine ESP32 cross-compile.

### Manual board checklist (not yet performed in this environment)

1. Verify cold boot and Free PSRAM/Flash readout at **115200 baud**.
2. Insert prepared SD; confirm Home/Menu (240x320) and theme from `.vqeaf`.
3. Install signed v1, open app, update to signed v2, ensure state and icon
   refresh without corruption. Lower version must be rejected.
4. Reboot between installs and after partial staging simulation; inspect
   `[QEAPP][RECOVERY]` Serial marker and the App installer Recovery option.
5. Remove/reinsert SD outside audio playback, test the signed catalog refresh;
   repeat low-space, missing-card and unexpected-file scenarios.
6. Check heap floor, UI responsiveness during a 256 KiB payload update and
   rollback behavior after real SD I/O errors. **Do not forcibly power off
   during a firmware flash or when the card is the only copy of user data.**

## Future phases, not included

- Per-app runtime service with true multitask process isolation for signed
  Lua/DSL apps. ESP32-S3 does not run unmodified Symbian/SIS apps.
- Runtime permission dialogs, per-app storage migration and publisher key
  rotation/revocation.
- Crash analytics for third-party runtime apps and OTA rollback validation.
