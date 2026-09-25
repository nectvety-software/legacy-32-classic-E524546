# VQEAF OS v2.4.2 — Host verification (2026-09-24)

Environment: Linux, Python 3, g++ (host), Python `cryptography` and OpenSSL. Test runner uses the project's source-level ESP32 host stubs; it does **not** run on the microcontroller.

| Test suite | Result |
|---|---|
| `python3 tools/verify_v242.py` | **17/17 passed**: theme runtime and Studio parser, HTTP chunked fuzz (1,000 malformed inputs), browser links/retry/chunked, legacy browser regression, signed installer, sample package signatures, firmware mock link, signing regression, boot/diag hooks. |
| `python3 tools/verify_v240.py` | **7/7 passed**: board config, UI integration, mock firmware link, signing/installer regression, icon and D-pad tests. |
| `tools/doctor_v242.py` with production trust key on bundled `welcome.qeapp`/`help_site.qeapp` | **2/2 signature verification passed** |
| `tools/doctor_v242.py --package games/pixel_snake/dist/snake_pixel_demo.qeapp --key-header src/services/QeappTrustKey.h` | **Expected failure**, showing signing ID mismatch between demo key `0x534E414B` and production key `0x31534351`. The demo game must not be installed in production without owner-controlled re-signing. |

**Not tested:** actual PlatformIO ESP32-S3 target compilation, LCD rendering on hardware, microSD insert/removal and actual writes, WiFi/TLS/HTTP from hardware, all current websites. Reproduce these with the steps in `docs/CORE_FIX_V242_VN.md` and attach the Serial Monitor 115200 log for unresolved issues.
