# ESP Browser v1.3 — Qeafivels / Opera Mini 4 Mode Test

## Target
- URL entered: `https://qeafivels.com/`
- Expected redirect: `https://www.qeafivels.com/`
- Device layout: 240×320, keypad/D-Pad, center OK.
- Render mode: single-column / fit-to-width, Opera Mini 4 inspired.

## Results
- PASS: URL accepted as HTTPS.
- PASS: HTTP redirect resolves to `https://www.qeafivels.com/`.
- PASS: `<title>` parsed; `Qeafivels Software` present.
- PASS: modern `script`, `style`, `template`, `iframe`, `object` payloads are suppressed from text rendering.
- PASS: D-Pad moves focus by content block/link.
- PASS: blue focus rectangle hugs the selected block/link.
- PASS: Page Overview opens from Option > Navg > Overview.
- PASS: Virtual Mouse opens from Option > Tool > Mouse.
- PASS: previous keypad focus regression still completes.

## Network note
The CI/container simulator cannot perform public DNS/TLS directly. Therefore the framebuffer screenshots use a semantic test fixture served through the same `http_get -> doc_parse -> render -> keypad` code path. The fixture verifies redirect handling and the browser UI/renderer; it is not claimed to be a pixel screenshot of the live Qeafivels site.

On ESP32-S3 hardware, `https://qeafivels.com/` uses `WiFiClientSecure` and follows the redirect to the `www` host. TLS currently uses `setInsecure()` for compatibility, so encryption is present but certificate authenticity is not validated.

## v1.3 changes
- Opera Mini 4.5/J2ME-inspired User-Agent.
- Mobile/WAP XHTML Accept header.
- `Accept-Encoding: identity` to avoid gzip decompression cost on ESP32.
- HTTPS lock icon in red title bar.
- Qeafivels Speed Dial entry.
- Feature-phone punctuation normalization for common UTF-8 dashes/quotes/bullets.
