# SD Card Manager App - User Guide

## Overview

The **SD Card Manager** is a comprehensive file browser and management application for PoChiTa OS. It provides full file navigation, viewing, and manipulation capabilities directly from your SD card.

**App Name:** SD Card Manager  
**Launch from:** Main Launcher > SD Card (system app)  
**Shortcut:** Red Power button → Home → Launch SD Card app

---

## Features

### 📁 File Operations
- **Browse directories** - Navigate through folders and subfolders
- **View file details** - Check file size, type, and properties
- **Copy/Paste** - Duplicate files between folders
- **Cut/Move** - Move files to different locations
- **Rename** - Change file and folder names
- **Delete** - Remove unwanted files and folders
- **Create folders** - Make new directories directly in the app

### 📄 File Viewing
- **Text files** (.txt, .log, .c, .cpp, .h, .ino) - View source code and text
- **Images** (.png, .jpg, .jpeg, .bmp) - Display photos and graphics
- **Lua scripts** (.lua) - View and run Lua code
- **JSON files** (.json) - View structured data
- **HTML files** (.htm, .html) - Browse web documents

### 🎮 ROM Support
The app supports launching emulator ROMs:

| Console | Extensions | Format |
|---------|------------|--------|
| **Nintendo** |
| NES | .nes | NES games |
| SNES | .snes, .smc, .sfc | Super Nintendo games |
| Game Boy | .gb | Original Game Boy |
| Game Boy Color | .gbc | Color Game Boy |
| Game Boy Advance | .gba | GBA games |
| **Sega** |
| Master System | .sms | SMS games |
| SG-1000 | .sg1000, .sc | SG-1000 games |
| Mega Drive/Genesis | .md, .gen, .bin | Sega Genesis games |
| Game Gear | .gg | Game Gear games |
| **Other** |
| ColecoVision | .col, .cv | ColecoVision games |
| PC Engine | .pce | TurboGrafx-16 |
| Atari Lynx | .lynx | Atari Lynx games |
| DOOM | .wad, .pwad | DOOM data files |

### 🐍 Script Execution
- **Lua scripts** - Run Lua programs stored on SD card
- **Execute-on-select** - Launch games and scripts with single button press

---

## Navigation Controls

### Button Layout (T9/Classic Phone Style)

```
        [UP]
        ▲
[MENU] ◄ ► [OPTION]
        ▼
       [DOWN]

        [SELECT] (Center)
[A] = Green/Call  [B] = Red/Back
```

### Basic Navigation

| Button | Action |
|--------|--------|
| **UP / DOWN** | Move selection up/down in file list |
| **SELECT** (Center) | Open file or folder |
| **MENU** (C key) | Show/hide options menu |
| **OPTION** (Left softkey) | **Reserved** |
| **B** (Back) | Go back to parent folder / Exit app |
| **A** (Green) | **Reserved** |

### Scrolling
- When the file list has more than 8 items, a scrollbar appears on the right side
- Press UP/DOWN to scroll smoothly through the list
- The currently selected item is highlighted in blue

---

## File Operations Guide

### Opening Files

1. **Navigate** to the file using UP/DOWN buttons
2. **Press SELECT** to open it
3. The app will:
   - **Folders** → Enter the directory
   - **Text files** → Display in text viewer
   - **Images** → Show image preview
   - **ROMs** → Launch the emulator
   - **Lua scripts** → Run the script

### Options Menu

Press **MENU** to open the options menu. Available options:

```
[1] Open        - Open/enter the selected item
[2] Run         - Execute a script or ROM
[3] Properties  - View file details
[4] Copy        - Copy file to clipboard
[5] Cut         - Cut file for moving
[6] Rename      - Rename file or folder
[7] Delete      - Remove file (with confirmation)
[8] Paste       - Paste previously copied/cut files
[9] New Folder  - Create a new directory
```

**Navigation in Options Menu:**
- Press **UP/DOWN** to move between options
- Press **SELECT** to execute selected option
- Press **B** or **MENU** to close menu

### Renaming Files

1. Open options menu (press **MENU**)
2. Navigate to **[6] Rename**
3. Press **SELECT**
4. Use on-screen keyboard to edit name:
   - Press keys like a phone (T9 input)
   - DELETE to remove character
   - **SELECT** to confirm
   - **B** to cancel
5. New name is saved immediately

### Creating New Folders

1. Open options menu (press **MENU**)
2. Navigate to **[9] New Folder**
3. Press **SELECT**
4. Type folder name using on-screen keyboard
5. Press **SELECT** to create
6. Folder appears immediately in the list

### Copy & Paste

**To copy a file:**
1. Select the file
2. Open options menu (MENU)
3. Choose **[4] Copy**
4. Navigate to destination folder
5. Open options menu
6. Choose **[8] Paste**

**To cut & move a file:**
1. Select the file
2. Open options menu (MENU)
3. Choose **[5] Cut**
4. Navigate to destination folder
5. Open options menu
6. Choose **[8] Paste**

### Deleting Files

1. Select file/folder
2. Open options menu (MENU)
3. Choose **[7] Delete**
4. Confirm deletion (if prompted)
5. File is removed

---

## Viewing Files

### Text Viewer

When you open a text file (.txt, .cpp, .h, etc.):

```
┌─────────────────────────────────┐
│ Item name                  1/50 │
├─────────────────────────────────┤
│ [Text content displayed here]   │
│ Line by line                    │
│ [More content...]               │
│                                 │
└─────────────────────────────────┘
```

**Controls in Text Viewer:**
- **DOWN** - Scroll down (10 pixels per press)
- **UP** - Scroll up (10 pixels per press)
- **B** or **MENU** - Exit text viewer, return to file list

### Image Viewer

When you open an image file (.png, .jpg, .bmp):

```
┌─────────────────────────────────┐
│ Image Name                      │
├─────────────────────────────────┤
│                                 │
│      [Image scaled to fit]      │
│      [the display]              │
│                                 │
└─────────────────────────────────┘
```

**Controls in Image Viewer:**
- **SELECT**, **A**, or **B** - Exit viewer, return to file list

### File Properties

Shows detailed information about selected file:

```
┌─────────────────────────────────┐
│ PROPERTIES                      │
├─────────────────────────────────┤
│ Name: [filename]                │
│ Type: [file type]               │
│ Size: [file size in bytes]      │
│ Path: [full path]               │
│ Created: [timestamp]            │
│                                 │
└─────────────────────────────────┘
```

**Controls:**
- **UP/DOWN** - Scroll through properties
- **B**, **MENU**, or **SELECT** - Exit properties view

---

## Folder Structure

The app maintains a predefined directory structure on your SD card:

```
/
├── /theme          - Theme and UI resources
├── /icons          - Icon files
├── /notes          - Notes app data
├── /system         - System files
│   └── /cache      - Temporary cache
├── /data           - User data
│   ├── /images     - Image collection
│   ├── /music      - Audio files
│   └── /documents  - Documents
└── /retro          - Emulator ROMs
    ├── /nes        - NES games
    ├── /snes       - SNES games
    ├── /gb         - Game Boy games
    ├── /gbc        - Game Boy Color games
    ├── /gba        - Game Boy Advance games
    ├── /sms        - Master System games
    ├── /md         - Mega Drive games
    ├── /gg         - Game Gear games
    └── /doom       - DOOM files
```

**Note:** These directories are automatically created on first run if they don't exist.

---

## Tips & Tricks

### Performance Tips
- Keep your SD card formatted as FAT32 for best compatibility
- Don't store too many files in one folder (limit 100-200 per directory)
- Clear the `/system/cache` folder regularly to free up space

### File Management Best Practices
- Keep original files backed up before moving/deleting
- Use **Cut** instead of **Copy** when reorganizing to save space
- Rename files to descriptive names for easy identification

### Lua Scripting
- Store Lua scripts in `/` root or organized folders
- Scripts can access SD card data and display on TFT
- Example: `pigeon_dance.lua` - Place in `/sd_card/` folder
- Press **SELECT** to run, or use **[2] Run** from options menu

### ROM Organization
- Organize ROMs by console type in `/retro/` subfolders
- Use consistent naming: `GameTitle (Region).ext`
- Example: `/retro/nes/Super Mario Bros (USA).nes`

### Text Editing
- Create/edit files using the on-screen keyboard
- Supports common text formats: .txt, .lua, .py, .json
- Large files (>100KB) may scroll slowly on display

---

## Troubleshooting

### "Insert SD Card" Message
- **Cause:** SD card not detected
- **Solution:** 
  - Power off device
  - Remove and reinsert SD card firmly
  - Power on and try again
  - Check SD card for damage

### File Won't Open
- **Cause:** Unsupported format or corrupted file
- **Solution:**
  - Check file extension matches actual format
  - Try viewing properties to confirm file type
  - Copy file again from source
  - Verify SD card integrity

### Slow Navigation
- **Cause:** Too many files in current folder
- **Solution:**
  - Organize files into subfolders
  - Delete unnecessary files
  - Consider using a faster SD card (UHS-I or UHS-II)

### Can't Delete/Rename File
- **Cause:** File might be read-only or system-protected
- **Solution:**
  - Check file is not in use by another app
  - Exit file manager and try again
  - Verify you have write permission to the folder

### ROM Won't Launch
- **Cause:** 
  - Wrong ROM format for console
  - Missing emulator core
  - Corrupted ROM file
- **Solution:**
  - Verify ROM extension matches console type
  - Re-download ROM from trusted source
  - Check console emulator is installed

---

## File Type Reference

### Displayable Files
| Type | Extensions | Viewer |
|------|-----------|--------|
| Text | .txt, .log, .c, .cpp, .h, .ino | Text Viewer |
| Images | .png, .jpg, .jpeg, .bmp | Image Viewer |
| Data | .json, .html, .htm | Text Viewer |
| Code | .lua, .py, .js, .css, .php | Text Viewer |

### Executable Files
| Type | Extensions | Action |
|------|-----------|--------|
| Lua Scripts | .lua | Execute Lua interpreter |
| Python | .py | Not yet supported |
| ROMs | .nes, .snes, .gb, etc. | Launch emulator |

### Folders
| Name | Purpose |
|------|---------|
| /theme | Stores theme and UI customization |
| /retro | Contains all emulator ROMs |
| /data | User-created files and data |

---

## Keyboard Shortcuts

| Key Combination | Action |
|-----------------|--------|
| **MENU** | Toggle options menu |
| **UP/DOWN** | Move selection / Scroll |
| **SELECT** | Confirm / Open / Execute |
| **B** | Back / Exit |
| **Long Press B** (1s) | Back to launcher |

---

## Storage Information

### Recommended SD Card Specs
- **Capacity:** 32GB - 256GB
- **Speed Class:** U3 or faster
- **Format:** FAT32 (for maximum compatibility)
- **File System:** Standard MBR partitioning

### Storage Usage Guide
- **Text files:** ~1-100 KB each
- **Images:** ~100 KB - 5 MB each
- **ROMs:** ~50 KB - 16 MB each
- **Lua scripts:** ~1-50 KB each

---

## Exiting the App

### Normal Exit
- Press **B** repeatedly to go back to root directory
- Then press **B** once more to exit to launcher

### Quick Exit
- Press and hold **B** for 1+ seconds
- Confirm exit in dialog

### Home Button
- Press **Red Power button** (fast)
- Returns immediately to launcher

---

## Version History

- **v1.0** - Initial release
  - File browsing and navigation
  - Text/image viewing
  - Basic file operations (copy, cut, paste, rename, delete)
  - ROM launching support
  - Lua script execution

---

## Support & Feedback

For issues, suggestions, or improvements:
- Check the troubleshooting section above
- Verify SD card is working with other devices
- Review file names for special characters
- Ensure sufficient free space on SD card

**Happy browsing!** 🎉

