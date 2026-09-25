# VQEAF OS — system icons v1.0

12 original, theme-aware (focus via parent UI) icon assets for **240×320 portrait ESP32-S3**; **two authoritative resolutions**: **24×24** and **36×36**. The RGB565 PNG and firmware RLE color roles are generated from a single source; transparent safe insets are checked automatically.

- `assets/png/{24,36}/`: 24 **transparent PNGs** in total.
- `assets/svg/{24,36}/`: 24 **pixel-identical scalable SVGs**.
- `assets/svg/geometry/`: 12 editable geometric references (not pixel-identical in all SVG rasterizers).
- `assets/raw565/{24,36}/`: 24 optional little-endian raw RGB565 files, color key `0xF81F`.
- `assets/atlas_24.png`, `assets/atlas_36.png`: packed transparent spritesheets with atlas JSON mappings.
- `firmware/VqeafIcon*`: pure C++11 RLE renderer and static flash data. Designed for TFT_eSPI, no additional Arduino dependency.
- `tests/test_icon_pack.py`: rebuild, compile with a minimal TFT stub, and verify 24 bit-exact RGB565 image pairs.
- `docs/ICON_SPEC.md`: geometry, stroke, contrast, focus mapping, and integration requirements.

**Existing firmware:** install only the three `VqeafIcon*` files in `src/core`, then follow the minimal integration steps in `docs/ICON_SPEC.md` or use the accompanying integrated source ZIP. There is no change to the `.vqeaf` file grammar or signed `.qeapp` specification.

**Optional integration patch:** `patches/SymbianUI_v23_icons.patch` is against the original VQEAF OS v2.3 source. Place the three firmware files in `src/core` then apply the patch from the firmware project root (`git apply <path-to-patch>`). If your project has changed since v2.3, inspect the diff instead of forcing it. The matching fully integrated v2.3.1 firmware ZIP is provided separately.
