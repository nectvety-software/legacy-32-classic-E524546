# Signed application packages — QEAPP/2 (v1.5)

## What the signature protects

- Same bounded declarative `web` (HTTPS URL) / `text` (read-only bundled text)
  app types as v1.4. It does **not** run native/ELF/VXP/Lua/script payloads.
- The ESP32 firmware embeds exactly ONE trusted ECDSA **P-256** public key,
  `src/services/QeappTrustKey.h` (65-byte uncompressed SEC1 point + numeric ID).
  The private key remains on the developer computer; the firmware never stores it.
- A valid signature proves only that the package was signed by the current
  embedded publisher key and has not changed. It is **not** malware scanning,
  application sandboxing, certificate-chain validation or revocation.

## Signed QEAPP/2 binary layout

- Offsets 0..115 are the bounded v1.4 header with new 8-byte magic
  `QEAPP2\r\n`. The header includes three lengths and SHA-256 hashes of the
  exact manifest, icon and payload bytes. See historical `QEAPP_V14.md` for
  the inner sections; lengths 1..2048 / 0 or 2048 / 0..262144 respectively.
- Follow with exact `manifest || icon || payload` bytes.
- Append a fixed **76-byte** trailer:
  `QSIGP256` (8 ASCII) || `key-id` (4 bytes little-endian) ||
  `ECDSA signature r` (32 bytes big-endian) || `s` (32 bytes big-endian).
- Signed digest: `SHA256(116-byte header || manifest || icon || payload)`.
  ECDSA uses P-256 + SHA-256. The trailer is not part of the digest. The
  key ID selects the single pinned key (not an external/untrusted public key).
  Each section's SHA-256 is checked independently before ECDSA verification.
- File length must match these fields and the 76-byte trailer *exactly*.
  `QEAPP1` is always rejected, even when old section hashes match.

## Publish YOUR OWN applications

Requirements: Python 3; `pip install cryptography`. Pillow is needed only
when an optional 32×32 PNG icon is supplied.

**Step 1: provision your private key locally and inject its public half**:

```sh
python3 tools/qeapp_keys.py \
  --private "$HOME/private-qeapp-publisher.pem" \
  --header src/services/QeappTrustKey.h \
  --key-id 0x31534351
```

The command refuses to overwrite an existing private key and sets restrictive
file permissions on POSIX. Store the PEM key securely, with separate backups.
**Do not copy it to a microSD card, upload it to GitHub, or include it in
firmware releases.** Use a secure signing host for production.

**Step 2: rebuild and flash your firmware** to pin your new public key:

```sh
pio run -t clean && pio run && pio run -t upload
```

**Step 3: sign an app with the matching private key**:

```sh
python3 tools/build_qeapp.py --sign-key "$HOME/private-qeapp-publisher.pem" \
  --key-id 0x31534351 --id welcome --name Welcome --version 1.0.0 \
  --type text --text sd/System/Apps/Inbox/welcome.txt \
  --icon sd/System/Apps/Inbox/welcome_icon.png \
  -o welcome.qeapp
```

Copy `welcome.qeapp` to `/System/Apps/Inbox/` on the device's SD card. Open
**Menu > Applications > App installer > App inbox**. Inspect the package;
**Signature verified** shows the app name, version, type and icon. Press
**Install** and confirm. An invalid signature displays **Package rejected**
and the reason (for example `Unknown signing key ID` or
`Digital signature verification failed`); no app is installed.

The demo public key included with this source was generated solely to
validate the two included signed sample packages; **its private key is not
shipped**. Before publishing your own apps, generate and pin YOUR OWN key;
the demo packages then become untrusted until rebuilt with your key.

## Install, boot, use-time checks, uninstall

1. Scan the package and check magic/lengths, manifest SHA-256, icon SHA-256,
   payload SHA-256, and ECDSA P-256 signature **before installation**.
2. On confirmation, reopen it and hash-check each copied section while writing
   into `/System/Apps/Installed/.stage-<id>/` in fixed 512-byte chunks.
   Save its exact signed header+trailer as `receipt.bin` (192 bytes).
3. Reinspect the source and atomically rename the completed staging directory
   to `/System/Apps/Installed/<id>/`; verify the installed sections and the
   receipt's signature again. If post-copy verification fails, remove only
   known installer files and do not register an app.
4. Boot/catalog refresh reads each installed receipt and independently hashes
   every installed section before trusting the app. Immediately before
   application launch, `get(id)` repeats this verification to detect modified
   SD content; invalid installed apps do not enter the trusted catalog.
5. Uninstall deletes only known `manifest.ini`, `icon.rgb565`, `payload.txt`,
   `receipt.bin` and the app directory. Unexpected files block automatic
   deletion. A corrupted installed app may need *manual* recovery using
   File Manager after a backup (it is omitted from the trusted catalog).

**Note:** The SD card itself is not protected from physical modification, and
there is no hardware-backed secure boot or anti-rollback in this project.
There remains a narrow SD swap/race opportunity between final verification
and content actually consumed by the browser/text viewer. Packages are still
restricted to declarative web and text types. Treat the system as a
signed-install prototype, not a secure mobile app sandbox. Publishing a new
public key requires flashing firmware and re-signing apps with that key.

## Tests

```sh
python3 tools/test_v15_signature.py
python3 tools/test_v14_build.py
```

The first compiles the **actual C++ parser / installer / signature verifier**
using OpenSSL on the host. It generates ephemeral test keys, signs 13 packages,
then tests full install/uninstall, catalog bounds, original binary hashing,
foreign publisher, unsigned QEAPP/1, invalid signature, forged section hashes,
corrupted payload and damaged installed receipt. Test private keys are kept in
an automatically deleted temporary directory. The second uses the previous
Arduino/TFT **mock** harness; real mbedTLS link/hardware requires `pio run`.

**Transport caveat:** a signed `web` app authenticates the bundled HTTPS URL,
not the website's subsequent responses. Qeafbrowser currently uses
`WiFiClientSecure::setInsecure()` for compatibility, so its TLS connection
cannot authenticate the remote website's certificate. Do not use this browser
for sensitive logins until proper CA verification and trusted time are added.

**FAT caveat:** staging plus rename avoids half-written app folders under
normal conditions, but FAT/SD does not promise a fully power-fail-atomic
rename. The signed receipt and boot-time verification reject a partially
installed or corrupted directory after reboot.
