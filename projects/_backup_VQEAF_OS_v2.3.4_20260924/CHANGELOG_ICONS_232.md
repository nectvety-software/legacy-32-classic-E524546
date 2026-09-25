# v2.3.2 — System icons integrated across Home / Menu

- One typed 12-entry canonical Menu bitmap table and 3-entry Home shortcut table.
- Main Menu now renders the 36x36 assets by semantic slot rather than label-string inference; Home shares those exact glyph bytes.
- 24x24 lists retain their optimized independent versions; old app icon fallback and signed .qeapp artwork preserved.
- .vqeaf still controls selected tile/focus/chrome without mutating icon bitmap payloads.
- New C++ production-UI framebuffer parity test covers 12 Menu icons, 3 Home icons, all 12 list icon variants and focus-only dirty updates.
- Exact-sized PC-rendered previews and verification logs included. Board/PlatformIO test pending.
