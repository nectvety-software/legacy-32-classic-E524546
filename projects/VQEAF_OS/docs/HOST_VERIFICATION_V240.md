# VQEAF OS 2.4.0 — test report

Generated UTC: 2026-09-24T14:33:22.813139+00:00

| Test | Status | Exit |
|---|---|---:|
| `check_board_config.py` | PASS | 0 |
| `test_v14_wiring.py` | PASS | 0 |
| `test_v14_build.py` | PASS | 0 |
| `test_v15_signature.py` | PASS | 0 |
| `test_v24_app_manager.py` | PASS | 0 |
| `test_pixel_icons_v234.py` | PASS | 0 |
| `test_grid_nav.py` | PASS | 0 |

**PlatformIO ESP32-S3 cross-build:** NOT RUN by this script.

**Physical device installation:** NOT RUN.

**Firmware image (.bin):** NOT PRODUCED by these host-only tests.

Historical UI gate `test_v21.py` expects an obsolete literal code line;
`test_vqeaf_g3.py` expects an old unbundled PNG preview. These are not part
of this app-core release gate.
