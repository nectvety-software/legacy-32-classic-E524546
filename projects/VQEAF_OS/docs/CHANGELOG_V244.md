# VQEAF OS v2.4.4 — Installer reboot investigation candidate

## Functional changes

- Moved QEAPP manifest parsing and verified directory manifest from loopTask
  stack to checked, bounded heap allocations; OOM fails safely.
- Moved recovery transaction ID snapshots from stack to bounded heap.
- Deferred Inbox signatures until an item is selected; install remains strict.
- Moved UI icon buffers into static screen objects rather than local stack.
- Aborts installation early when internal heap/contiguous block is critically low.
- Logs install phases, internal free heap, largest block, task stack watermark;
  best-effort RTC prior-phase marker printed at next boot with ESP reset reason.
- Added device capture script, compile-only ESP32 instrumentation mock,
  and host regression gate.

## Security invariants

- All packages still must pass ECDSA P-256 verification with firmware trust key.
- No key replacement, signature bypass or arbitrary code loader.
- Existing update / backup / rollback and install receipts unchanged.
- Existing theme `.vqeaf` parser and board pin mapping untouched.

## Limits

No physical board available to reproduce reboot. Suspected stack pressure,
SD power brownout and watchdog remain hypotheses until complete 115200 log.
