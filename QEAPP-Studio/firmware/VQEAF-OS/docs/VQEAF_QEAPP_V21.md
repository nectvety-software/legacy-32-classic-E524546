# VQEAF OS v2.1 application format note

`*.qeapp` is the existing **signed binary QEAPP/2** package format, **not** a directory-with-extension, a ZIP, a VXP emulator payload or a Lua VM runtime.

- Fixed 116-byte QEAPP/2 header (`QEAPP2\\r\\n`), manifest, optional signed icon (32x32 RGB565 LE), optional bounded payload, fixed 76-byte ECDSA P-256 signature trailer; see `QEAPP_V15_SIGNING.md` for exact grammar, SHA-256 digests, publisher key and limitations.
- `type=web` opens a verified HTTPS URL in Qeafbrowser; `type=text` opens a bundled text file in TextViewer. Both retain existing execution limits.
- On SD, put packages under `/System/Apps/Inbox`. Installer verifies before registration and again on use. The launcher lists only verified installed entries.
- Theme Studio `.vqeaf` files belong under `/System/Themes`, not Apps inbox.
- Future `main.lua` extensions must have a separately designed sandbox, interpreter integration and versioned format; no arbitrary code is accepted in this release.

Public sample key is a demo trust anchor; rotate it to a developer-controlled key before production. Keep private key off SD, off firmware and off public repositories.
