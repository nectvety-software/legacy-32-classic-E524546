> HISTORICAL v1.4 documentation (unsigned QEAPP/1). Current v1.5 firmware
> rejects this format. See `QEAPP_V15_SIGNING.md` and repackage/re-sign apps.

# QEAPP/1 — Symbian S3 OS v1.4 app package

This format is **specific to this firmware**. A `.qeapp` is **not** an ESP32
native binary, Linux package, APK, VXP/MRE application or unrestricted script.
Supported *declarative* app types:

- `web`: HTTPS link shown as an installed app, opens within Qeafbrowser.
- `text`: bundled UTF-8/ASCII text opened by the system Text Viewer.

No executable code is loaded or run from downloaded packages. An untrusted
package can still carry an untrusted URL or misleading text.

## Binary format

All multi-byte integers are little-endian. Exactly 116-byte header:

| Offset | Size | Contents |
|--:|--:|---|
| 0 | 8 | ASCII `QEAPP1\r\n` magic |
| 8 | 4 | manifest length (1–2048) |
| 12 | 4 | optional icon length: 0 or 2048 |
| 16 | 4 | payload length: 0–262144 |
| 20 | 32 | SHA-256 of manifest |
| 52 | 32 | SHA-256 of icon, SHA-256(empty) when omitted |
| 84 | 32 | SHA-256 of payload, SHA-256(empty) when omitted |

Immediately after header: manifest bytes, icon bytes (RGB565 LE 32×32),
then payload bytes. The total file length must equal header + sections.

Manifest uses ASCII `key=value` lines. Required: `id`, `name`,
`version`, `type`. `id` is 1..24 lower-case ASCII `[a-z0-9_-]`;
`name` max 40 printable ASCII characters; `version` max 19 digits/dots.
`type` is `web` or `text`. A web app additionally requires
`entry=https://...` and no payload; a text app requires a nonempty
text payload and no `entry`. Unknown/duplicate keys are rejected.

## Create a package on the PC

Use Python 3 and install Pillow only if you supply an icon:

```sh
python tools/build_qeapp.py --id welcome --name Welcome --version 1.0.0 \
  --type text --text sd/System/Apps/Inbox/welcome.txt \
  --icon sd/System/Apps/Inbox/welcome_icon.png -o welcome.qeapp
python tools/build_qeapp.py --id help_site --name 'Help website' \
  --version 1.0.0 --type web --url https://qeafivels.com/ -o help_site.qeapp
```

Copy to microSD: `/System/Apps/Inbox/`. On-device, open **Menu →
Applications → App installer** or open `.qeapp` in File Manager. Select
a package to inspect its *verified* name, version, icon and type; choose
Install and confirm. Installed entries show in Applications.
To remove, switch App installer to **Installed apps**, open details,
choose Uninstall and confirm. Only known installer-owned files are removed.
If any unexpected file exists in an app directory, uninstall stops without
removing that unexpected file.

## Install and restore behavior

1. Check package magic/length/manifest; hash all three sections in 512-byte
   streaming chunks. Hashes help detect accidental damage but **are not a
   signature** and cannot prove who built the package.
2. Reopen input, revalidate manifest and stream/copy each section into a
   staging directory under `/System/Apps/Installed/.stage-<id>`. Hash copied
   bytes again as they are written.
3. On complete success rename staging directory to
   `/System/Apps/Installed/<id>`. An existing ID cannot be overwritten.
4. At boot/App list entry, rescan at most 12 installed apps from SD.
   Only validated safe IDs/manifest pairs become menu entries.
5. A stale stage directory after power loss is left untouched; remove it
   manually after inspecting via File Manager or Shell and retry.

A plain `SHA-256` cannot provide authenticity or trust. Signature
verification with pinned public keys and a permissions model would be a
separate feature before general third-party executable app support.

## Device configuration

Portrait ST7789 240×320; keep Menu S60 theme and compact WiFi/battery bar.
The installer is SD-backed: if the card is missing the App installer displays
a storage error; it cannot install internally without SD. `web` apps require
working WiFi and the browser's existing TLS settings; do not enter sensitive
credentials until Qeafbrowser validates CA certificates.
