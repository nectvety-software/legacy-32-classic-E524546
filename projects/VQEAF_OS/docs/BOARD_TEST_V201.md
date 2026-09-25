# ESP32-S3 v2.0.1: physical-board test protocol

**Evidence status:** Host regressions executed during preparation. No ESP32-S3
board, connected COM port or routable live DNS is available in this environment.
The physical-board cases below are **PENDING**, not claimed PASS. The package
provides instrumented source and a log collector for testing on your board.

Target: E524546 ESP32-S3 N16R8, ST7789 portrait 240×320, SD_MMC 1-bit,
115200 baud. The firmware stays the original Arduino-based Symbian S3 OS.

## Firmware additions

- Bench diagnostics: `diag help`, `diag tls valid`, `diag tls expired`,
  `diag tls wrong`, `diag tls self`, `diag tls host <domain>`, `diag sd status`, `diag sd rw` via Serial.
  The display Shell also supports `tlsdiag valid|expired|wrong|self` and
  `sddiag status|rw`; use `tlsdiag host <domain>` for a currently valid
  CA-covered public site if the default test endpoint changes. Detailed diagnostics are always logged to Serial.
- TLS probes reuse **production `TrustedTls::configure`** and embedded public
  CA roots. No `setInsecure` fallback. DNS failure or missing NTP returns
  `INCONCLUSIVE`. A successful accepted TLS session against the positive
  endpoint is `PASS`. A negative test is `PASS` only when ESP32 TLS reports
  certificate-verification error **-0x2700**; other connection failures are
  `INCONCLUSIVE`, not evidence of certificate validation.
- Tested hostnames: `valid-isrgrootx1.letsencrypt.org`,
  `expired.badssl.com`, `wrong.host.badssl.com`, `self-signed.badssl.com`.
  These public services may change their certificates/availability at any
  time. Use their observed live certificate chains for interpretation.
- `sddiag rw` uses **only** `/System/Temp/.s3_diag_scratch.bin`, creates the
  scratch file only when absent, writes 4096 deterministic bytes using one
  256-byte buffer, flushes, reopens, verifies all 4096 bytes/CRC32, then
  deletes the scratch. On a mismatch or failed deletion it retains the file
  for inspection. It never formats a card, never overwrites an existing file
  and never simulates removal during a write.
- SD remove/mount events are sent as `[S3DIAG][SD] event=REMOVED/MOUNTED`.
  The normal 8-second idle presence probe and bounded remount retries remain.

## Build and run on YOUR Windows PC

```powershell
cd SymbianS3_OS
pio run -t clean
pio run
pio run -t upload
pio device list
python -m pip install pyserial
python tools/board_test_capture.py --port COM7 --mode tls
python tools/board_test_capture.py --port COM7 --mode sd --cycles 10
```

Use your actual COM number. Close `pio device monitor` and any other Serial
Monitor while running the Python test collector. Open only one program on the
same COM port at a time. A command-line-only alternative is `pio device
monitor -b 115200` followed by the `diag ...` commands above; save its log.

### HTTPS acceptance criteria

1. Device is connected to WiFi; wait for real NTP time; verify with Home or
   Shell `date`. A clock value of `--:--` invalidates the test.
2. Positive case connects successfully with a verified chain anchored in one
   of firmware's 7 CA roots. Inspect `[S3DIAG][TLS] ... handshake=accept` and
   `verdict=PASS`. If the site is unavailable or its chain changed, report
   the test **INCONCLUSIVE** until an equivalent CA-covered host is selected.
3. Expired, wrong-host, and self-signed endpoints must **not connect**.
   Require certificate-specific mbedTLS error `-0x2700` (and successful
   positive case). Connection refused/DNS failure/network outage is not a
   meaningful negative-certificate test.
4. Separately visit a normal HTTPS page in Qeafbrowser and download a small
   trusted HTTPS file. Test browser HTTPS-to-HTTP redirect rejection and
   Shell `wget` rejection of plain HTTP. The direct TLS probe alone does not
   validate all browser HTTP paths.
5. Record heap before/after TLS with serial logs. This is not peak heap;
   heap extrema require continuous profiling during a handshake.

### microSD acceptance criteria

1. Back up the card; use a disposable spare for testing. **STOP Music and all
   downloads** before touching the slot. The board has no confirmed CD switch.
2. Execute `diag sd rw`; expect PASS (4096-byte readback and scratch removed).
3. Pull only an **idle** card, wait at least 8–20 seconds and check a real
   `event=REMOVED` event (not merely an empty Gallery screen).
4. Reinsert the card, wait up to 60–65 seconds for `event=MOUNTED`.
   Repeat `diag sd rw`, then open Gallery, Themes and signed Applications.
   Repeat **10 times**, looking for missing events or progressive heap loss.
5. Cold boot once with *no card*, then insert after Home loads, and verify
   remount. A separate tester trace is needed for this scenario.
6. Optionally run a controlled power-loss/cache-write test only on a spare
   card. A clean result from 4096-byte scratch I/O **does not** prove FAT
   transaction safety on power loss. Never remove during an active write.

### What to upload back for analysis

- The `board_results_tls.json` and `board_results_sd.json` files from
  `tools/board_test_capture.py`, or raw 115200 baud logs.
- PlatformIO complete build log, if it fails.
- SD media type/size and whether removed/mounted notifications fired.
- No passwords, auth tokens, private keys, or sensitive web URL content.

Do not label an unrun physical test PASS. Don't infer certificate rejection
solely from a failed TCP connection. This package is a reproducible procedure
for obtaining the real-hardware evidence safely.
