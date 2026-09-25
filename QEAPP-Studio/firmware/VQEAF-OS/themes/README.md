# VQEAF OS .vqeaf themes

Theme files use the `@vqeaf 1.0` DSL defined by VQEAF Theme Studio.
Copy a `.vqeaf` source file to `/System/Themes/` or `/Themes/` on microSD.
Built-in VQEAF Night is the safe fallback when SD is missing. Examples:
`vqeaf_night.vqeaf`, `vqeaf_day.vqeaf`, `amoled_red.vqeaf` and archived
`legacy_lime` (on disk `s60_green.vqeaf` for compatibility).
The optional top-level `launcher { ... }` block is only read by the firmware,
not necessarily preserved by VQEAF Theme Studio when exporting again.
The embedded renderer applies colors only; shell/keypad preview, resource
images, animations and vector decorations target the Android Theme Studio,
not the physical 240x320 LCD in this firmware.
See `docs/VQEAF_LAUNCHER_V21.md`.
