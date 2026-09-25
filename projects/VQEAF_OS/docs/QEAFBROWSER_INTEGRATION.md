# Qeafbrowser integration in Symbian S3 OS v0.7

Source studied: `legacy-32-classic-E524546/projects/Qeafbrowser_v1.7`.

The standalone Qeafbrowser firmware owns its full display loop and uses LovyanGFX. Symbian S3 OS already owns TFT_eSPI, its keypad state machine, chrome, app lifecycle, and task switcher, so embedding the standalone `main.cpp` directly would duplicate hardware ownership and waste RAM/flash. v0.7 instead integrates the browser engine as an **in-OS adapter**.

The adapter carries over the small-device design used by Qeafbrowser:

- HTTP and HTTPS (`WiFiClientSecure`)
- explicit relative redirect resolution, maximum 6 hops
- `setInsecure()` compatibility mode matching the source project
- Opera Mini/J2ME-style User-Agent
- `Accept-Encoding: identity`
- fixed 84-line / 24-link render pools
- fixed 8-entry in-RAM back history
- 32 KB page body buffer allocated from PSRAM only during fetch/parse
- no JavaScript engine, no CSS layout engine, no DOM heap
- HTML title/link extraction, case-preserving `href`, image `alt` fallback
- D-pad link focus and on-screen URL input
- default home `https://qeafivels.com/`

The full Qeafbrowser thumbnail/overview code is not duplicated into the OS browser app in v0.7. Gallery reuses the decoder choices proven by Qeafbrowser (`TJpg_Decoder 1.1.0`, `PNGdec 1.1.6`) but keeps image viewing separate from web-page rendering to control memory pressure.

TLS currently uses `setInsecure()`: transport is encrypted but server certificates are not authenticated. Do not treat this browser as suitable for passwords or sensitive sign-in until a CA store/bundle is added.
