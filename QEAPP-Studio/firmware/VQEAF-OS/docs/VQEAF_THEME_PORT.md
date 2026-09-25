# Theme Studio `.vqeaf` support on ESP32-S3

This reader supports `@vqeaf 1.x` source from VQEAF Theme Studio. It imports **only color tokens**, deliberately not Android PhoneShell frame assets, virtual keypad textures, animation, embedded WebP data or vector paths. Those pieces describe the external emulator phone frame, not the real ESP32-S3's physical shell.

Install `.vqeaf` files in `SD:/System/Themes/` (preferred) or `SD:/Themes/`, then select Settings -> Themes from the launcher. Up to **16 themes** are enumerated. File size limit: **512 KiB**, header/palette inspection limit: **16 KiB**, maximum inspected header line **319 bytes**. Files must contain a complete `</theme>` ending. To avoid huge RAM allocations, base64 data following the palette is not decoded and only a bounded tail seek validates the closing tag. Partial invalid colors never change the applied theme.

Mandatory base Studio palette fields: `screen`, `keyText`, `accent`, hex strings `#RGB`, `#ARGB`, `#RRGGBB`, `#AARRGGBB` (alpha accepted but RGB565 display is opaque). The existing firmware also accepts `key`, `keyPressed`, `keyBorder`, `subText`, `shellTop`, `shellBottom`, `glow`, `panel`, `border`, `titlebar`, `chromeText` in palette, mapping them to the existing RGB565 UI. Unknown fields are safely ignored.

`launcher` is a small **optional firmware-only extension** placed after the flat palette and before the first `<component>`/`<resource>`. The stock Theme Studio may discard this extension when it imports and re-exports a file, so the OS always derives a functional launcher palette from the standard Studio palette if the block is missing. Supported fields are `background`, `foreground`, `headerBg`, `headerFg`, `tabAccent`, `listBg`, `listFg`, `selectedBg`, `selectedFg`, `previewBg`, `previewFg`, `scrollbar`, `footerBg`, `footerFg`, `border`. Every color is a quoted VQEAF hex color, and unprovided tokens inherit safe defaults.

Example excerpt (see full `themes/vqeaf_night.vqeaf`):

```vqeaf
@vqeaf 1.0
<theme id="vqeaf_night" name="VQEAF Night">
 palette {
   screen: "#080F1A"
   keyText: "#F0F7FF"
   accent: "#55D2C6"
 }
 launcher {
   headerBg: "#102540"
   tabAccent: "#55D2C6"
   selectedBg: "#235481"
   selectedFg: "#FFFFFF"
 }
</theme>
```

Note: the sample above illustrates tokens only; the complete supplied theme file is the **buildable, multiline** example for the bounded parser. The size limit means some future oversized exports (>512 KiB) must be optimized before import. The firmware never runs theme text as code. Full VQEAF 1.0 AST/inheritance/animation rendering is **not** implemented.
