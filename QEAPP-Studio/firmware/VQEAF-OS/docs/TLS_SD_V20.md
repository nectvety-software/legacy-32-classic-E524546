# Symbian S3 OS v2.0.0: microSD recovery and certificate validation

Hardware target: E524546 **ESP32-S3 N16R8**, ST7789 **240 x 320 portrait**, SD_MMC one-bit. These are firmware services, not a Linux kernel.

## SD card lifecycle

- At boot, `StorageService::begin()` configures the documented SD pins and verifies both card metadata and a readable `/` FAT directory. A mounted card gets the existing `/System`, `/Media`, `/Documents`, cache, installed-app, themes, logs and download directory layout.
- When the main loop is idle, `StorageService::tick` checks read-only card metadata and the root directory at most every **8 seconds**. On failure the OS records `offline`, notifies the user and avoids treating a stale mount as usable. It waits **5 seconds** and then performs bounded remount retries at **15-second** intervals.
- **A music file is an open SD_MMC handle.** `tick` receives `idle=false` while Music is playing *or paused*, so it never calls `SD_MMC.end()` under that file. A removed card may therefore not be reported until playback stops/resumes and the next scheduled check. Stopping playback before ejecting is required. Other operations are synchronous on the main UI loop, so `tick` does not run in parallel with a download/installer or File Manager operation.
- On successful remount the OS creates missing standard folders, refreshes signed app metadata and reports an event. Apps already displaying deleted/stale media return to Idle on detected loss.
- `StorageService::writeAtomic` validates paths, checks free space when available, writes to `.tmp`, flushes/closes and compares bytes from a newly opened file in **256-byte chunks** before moving any existing file to `.bak` and renaming the complete staging file. `recoverAtomicFile` handles missing live files and lingering `.bak`/`.tmp` on later reads.
- Browser/Shell downloads use separate `.part` files, reject unknown lengths and compare finalized staging length against the server's `Content-Length` before renaming to the final path. This does not detect a malicious server supplying wrong bytes; QEAPP/2 cryptographic signature checks are still mandatory for app installation.

**Hardware limits:** The board documentation does not specify a card-detect switch or a resettable SD-slot power transistor. Driver-reported `cardType()` / `cardSize()` and root reads may be stale or block when electrical contacts fail. SD_MMC/FAT is **not transactional**: power loss during FAT metadata changes can still corrupt the card even when the application uses `.tmp/.bak` and flush. Save critical files externally and avoid removal under active transfers.

Shell: `sd` (mount status/error count), `df`, `mount`, `cache status`, `dmesg`. No automatic destructive filesystem repair or blind formatting is performed.

## HTTPS trust policy

- `TrustedTls` owns the compiled-in CA PEM string in read-only firmware flash. Browser HTTPS and Shell `wget` call `WiFiClientSecure::setCACert()`; Espressif's TLS stack validates the certificate chain and requested hostname during its connection handshake. Neither code path uses `setInsecure()` or accepts arbitrary unsigned trust anchors from microSD.
- `time(nullptr)` must be at least 2024-01-01 UTC; otherwise the user sees **"HTTPS: clock unset; connect WiFi and wait for NTP"**. The OS already requests NTP asynchronously after the WiFi station connects. Time set by unprotected NTP is a practical dependency and is **not authenticated time**; do not treat this as protection against a powerful on-path time attacker.
- TLS failures surface as `HTTPS verification or network failed` or setup errors. The low-level ESP32 error may describe a failed handshake, wrong hostname, chain, expired cert or network issue; the generic UI message intentionally does not falsely diagnose the exact cause. Clock-unset has a distinct message.
- Every HTTPS redirect hop is validated. HTTPS pages **cannot downgrade to HTTP**. Browser downloads and Shell `wget` accept only HTTPS URLs, bounded response sizes and declared `Content-Length`. Servers using **chunked streaming without a Content-Length cannot be downloaded** by this release (legacy HTML browsing support is separate).
- Previously cached insecure pages from v1.9 use different filenames; v2.0 HTTPS uses `tls20_*.htm` and plaintext HTTP uses `plain20_*.htm`. Offline pages are not silently substituted over **online HTTPS errors**. Plain HTTP page browsing is still allowed for legacy sites, is visibly not authenticated, and should never handle private credentials.

## Embedded public trust anchors

The embedded **public root certificates** were copied from the host Mozilla CA certificate bundle. Subject identities and SHA-256 DER fingerprints are checked verbatim by `tools/test_v20_storage_tls.py`:

| Certificate | SHA-256 DER fingerprint |
|---|---|
| ISRG Root X1 | `96bcec06264976f37460779acf28c5a7cfe8a3c0aae11a8ffcee05c0bddf08c6` |
| GTS Root R1 | `d947432abde7b7fa90fc2e6b59101b1280e0e1c7e4e40fa3c6887fff57a7f4cf` |
| GTS Root R2 | `8d25cd97229dbf70356bda4eb3cc734031e24cf00fafcfd32dc76eb5841c7ea8` |
| GTS Root R3 | `34d8a73ee208d9bcdb0d956520934b4e40e69482596e8b6f73c8426b010a6f48` |
| GTS Root R4 | `349dfa4058c5e263123b398ae795573c4e1313c83fe68f93556cd5e8031b3c7d` |
| DigiCert Global Root G2 | `cb3ccbb76031e5e0138f8dd39a23f9de47ffc35e43c1144cea27d46a5ab1cb5f` |
| GlobalSign Root CA | `ebd41040e4bb3ec742c9e381d31ef2a41a48b6685c96e7cef3c1df6cd4331c99` |

**Maintenance:** This small CA list will not work with every current/future website. Update the public certificates and test SHA-256 fingerprints via a trusted build, then publish and flash a **new firmware** when a root expires or a new trust anchor is needed. Root CA files are public verification material; never put your QEAPP/2 private signing key in the firmware or SD card. QEAPP/2 signature verification is independent of TLS website identity verification.

## Reproducible testing

```sh
python tools/test_v20_storage_tls.py
python tools/test_v14_build.py
```

The first command compiles and exercises **real production** C++ logic under host stubs, injects a corrupted SD staging write and a remove/reinsert cycle, verifies the complete CA bundle with OpenSSL against every known root, ensures an unrelated locally generated root **fails** verification, checks source policy and runs the full previous regression suite. The second performs a complete mock-API C++11 host link.

**Required on-board checks (not performed by this distribution):**
1. `pio run -t clean; pio run; pio run -t upload`, then monitor **115200 baud**, confirm 240x320 portrait menu and stable audio.
2. Remove/insert an *idle* SD (never during a write), observe notification and `sd` status, then re-open Gallery, Themes and Installed apps. Repeat ten times. Separately test boot without SD, insertion and reboot.
3. Connect WiFi, wait for real NTP clock, browse a currently valid CA-covered `https://` website and download a small known-size file. Try a deliberately expired, hostname-mismatched or untrusted certificate test endpoint: it must fail with no installed file. Try an HTTPS-to-HTTP redirect: it must fail. Test absence of NTP: should show a clock-required error.
4. Power off during **a disposable-card cache write** only, then reboot and verify `.bak` recovery where possible. Do not use valuable data for destructive power-failure experiments.
5. Observe free heap/PSRAM during HTTPS handshake, SD retries, WAV playback and downloads over 30-60 minutes. TLS can require substantial temporary RAM; host compilation does not measure the board's handshake heap peak.
