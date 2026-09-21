# SD Card Manager - Quick Reference Guide

## Control Summary

```
          ▲ UP
          │
◄ MENU ━━━◇━━━ OPTION ►
          │
          ▼ DOWN
          
      SELECT (Center)
      
[A] Green    [B] Back
```

---

## Quick Commands

| Action | Keys |
|--------|------|
| **Move up** | UP |
| **Move down** | DOWN |
| **Open/Enter** | SELECT |
| **Go back** | B |
| **Options Menu** | MENU |
| **Exit app** | Hold B (1s) |

---

## Options Menu

Press **MENU** on any file:

```
[1] Open       - Open file/folder
[2] Run        - Execute script/ROM
[3] Properties - View details
[4] Copy       - Copy to clipboard
[5] Cut        - Move to clipboard
[6] Rename     - Change name
[7] Delete     - Remove file
[8] Paste      - Paste clipboard
[9] New Folder - Create directory
```

---

## Supported File Types

### View
- **Text:** .txt, .cpp, .h, .ino, .py, .js, .lua
- **Images:** .png, .jpg, .bmp
- **Data:** .json, .html, .log

### Run
- **Scripts:** .lua
- **ROMs:** .nes, .snes, .gb, .gbc, .gba, .sms, .md, .gg, .pce, .lynx, .wad

---

## File Operations

**Copy file:**
1. Select → MENU → [4] Copy
2. Navigate → MENU → [8] Paste

**Move file:**
1. Select → MENU → [5] Cut
2. Navigate → MENU → [8] Paste

**Rename:**
1. Select → MENU → [6] Rename
2. Edit name → SELECT ✓

**Delete:**
1. Select → MENU → [7] Delete
2. Confirm

---

## Folder Structure

```
/ (Root)
├── /retro       ← ROMs go here
│   ├── /nes, /snes, /gb, /gba...
├── /data        ← Your files
├── /theme       ← UI themes
└── /notes       ← App data
```

---

## Keyboard Input

When renaming or creating folders:

Press number keys like old T9 phone:
- **2:** ABC  **3:** DEF  **4:** GHI  **5:** JKL
- **6:** MNO  **7:** PQRS  **8:** TUV  **9:** WXYZ
- **0:** Space, **1:** Punctuation

Example: Press 4 to cycle A→B→C, then 4 again for next letter

---

## Tips

✓ Organize ROMs by console in `/retro/` subfolders  
✓ Keep file names short (< 32 characters)  
✓ Use FAT32 format for SD card  
✓ Don't store >200 files in one folder  
✓ Back up important files before deleting  

---

## Emulator ROM Locations

| Console | Path | Extension |
|---------|------|-----------|
| NES | /retro/nes | .nes |
| SNES | /retro/snes | .snes, .smc |
| Game Boy | /retro/gb | .gb |
| GB Color | /retro/gbc | .gbc |
| GBA | /retro/gba | .gba |
| Sega Master | /retro/sms | .sms |
| Mega Drive | /retro/md | .md, .gen |
| Game Gear | /retro/gg | .gg |
| PC Engine | /retro/pce | .pce |

---

## Troubleshooting

| Problem | Solution |
|---------|----------|
| "No SD Card" | Reinsert SD; power cycle |
| File won't open | Check format, try again |
| Slow scrolling | Use fewer files per folder |
| ROM won't run | Verify extension matches |

---

**Last Updated:** April 2026

