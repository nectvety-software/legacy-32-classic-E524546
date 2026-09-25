# UI v0.4 — Portrait S60 Home + Dialogs + Shortcuts

## Native portrait target

- Physical display: ST7789 240×320.
- Runtime rotation: `tft.setRotation(0)`.
- UI coordinate system: 240×320; no landscape assumptions remain in current v0.4 code.

## Screen structure

- Top chrome: 32 px for title and status indicators.
- Main list: up to 6 rows × 40 px.
- Launcher: 3×3 grid, 78×67 px cells.
- Grid hint panel: y=239..289.
- Softkey bar: y=292..319.

## Idle / standby screen

After boot, the system enters an S60-inspired standby screen instead of opening the application menu immediately.

- Pixel gradient/skyline wallpaper generated with TFT primitives.
- Profile label `General`.
- Large clock + date.
- WiFi and microSD state.
- Three selectable quick-launch tiles: WiFi, Music, Files.
- LEFT/RIGHT selects a tile; START/SELECT opens it.
- MENU opens the 3×3 application menu.

## Global keypad shortcuts

- Short MENU: open application menu; from Menu it returns to Idle.
- Hold MENU: go directly to Idle/Home.
- Hold OPTION: open Settings.
- Hold START: open Music.
- Hold SELECT: open WiFi.
- B: Back/right-softkey outside the text editor.
- SELECT: alternate OK key outside the text editor.
- D-pad retains hold-to-repeat navigation.

The WiFi text editor intentionally keeps its own mapping: B=delete, SELECT=space, MENU=done, A=cancel.

## Dialogs

`SymbianUI::dialog()` draws a centered S60-style modal with:

- warm title strip,
- bordered popup body,
- two selectable buttons,
- drop shadow.

File deletion now uses this modal; LEFT/RIGHT changes Delete/Cancel and START/SELECT confirms.

## Screen transitions

`SymbianUI::transitionOut()` provides a short center-closing wipe using only direct TFT drawing. It does not allocate a framebuffer and therefore keeps RAM use low.
