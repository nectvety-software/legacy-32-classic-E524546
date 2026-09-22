# SD Card Manager - Technical Documentation

## Architecture Overview

The SD Card Manager is implemented across three main files:

- **`component/FileManager.h`** - Header with class definition and interface
- **`component/FileManager.cpp`** - Implementation of file operations and UI rendering
- **`src/sdcard.h`** - App integration point with PoChiTa OS

---

## Class Definition: `FileManager`

### Core Data Members

```cpp
class FileManager {
private:
  String currentPath;              // Current directory path
  std::vector<FileEntry> fileList; // Current directory contents
  int selectedIndex;               // Currently selected item (0-based)
  int scrollOffset;                // Scroll position in list
  
  ViewerMode viewerMode;           // Current view mode (LIST, TEXT, IMAGE, etc.)
  String statusMessage;            // Status display message
  
  String clipboardPath;            // Path to copied/cut file
  bool isCutOperation;             // Is clipboard a cut (not copy)?
  
  bool isCreatingFolder;           // Keyboard input state
  bool isRenaming;                 // Rename in progress
  String newFolderName;            // Input buffer
  
  String textViewContent;          // Cached text file content
  int textScrollY;                 // Vertical scroll position
  
  bool sdMounted;                  // SD card status
  int optionIndex;                 // Selected option in menu
  bool showOptions;                // Options menu visible?
};
```

### Enum Types

**FileType** - Identifies file categories:
```cpp
enum FileType { 
  FT_FILE, FT_FOLDER, FT_IMAGE, FT_ROM, FT_SCRIPT, FT_TEXT, FT_LUA, 
  FT_PYTHON, FT_HTML, FT_JSON,
  FT_NES, FT_SNES, FT_GB, FT_GBC, FT_GBA, FT_SMS, FT_SG1000, FT_MD, 
  FT_GG, FT_COLECO, FT_PCENGINE, FT_ATARI_LYNX, FT_DOOM 
};
```

**ViewerMode** - Application state machine:
```cpp
enum ViewerMode { 
  VM_LIST,        // File list view (main)
  VM_TEXT,        // Text file viewer
  VM_IMAGE,       // Image viewer
  VM_MESSAGE,     // Status message (SD missing, etc.)
  VM_PROPERTIES,  // File properties
  VM_INSTALLING   // (Reserved for future use)
};
```

### Key Methods

#### Initialization
```cpp
void begin()        // Initialize file manager, check SD card
void ensureDirectories()  // Create required folder structure
```

#### Navigation & Update
```cpp
void update(bool &exitToHome)  // Main input & logic loop (called each frame)
void listFiles()               // Scan current directory
FileType identifyType(String filename)  // Determine file type from extension
```

#### Rendering
```cpp
void drawList()              // Main file list display (8 visible items)
void drawListRow(int rowIndex, int idx, bool sel)  // Render single row
void drawOptionMenu()        // Show context menu
void drawTextViewer()        // Display text file
void drawImageViewer()       // Display image
void drawProperties()        // Show file details
void drawMessage()           // Show status message
```

#### File Operations
```cpp
void openSelected()       // Open file or enter folder
void renameSelected()     // Rename file/folder (keyboard input)
void copySelected()       // Copy to clipboard
void cutSelected()        // Cut to clipboard (move operation)
void pasteToCurrent()     // Paste clipboard contents
void deleteSelected()     // Remove file (with confirmation)
void runSelectedFile()    // Execute script/ROM
```

#### Utility Methods
```cpp
String getIcon(FileType type)        // Get file type label [DIR], [LUA], etc.
String getFileSize(size_t bytes)     // Format size as "1.2 MB"
String getFormattedDate(File &f)     // Get file timestamp
bool confirmAction(String msg)       // Show Y/N dialog
```

---

## File List Display

### Layout

```
╔═══════════════════════════╦═══╗
║ 12:00           [Path...] │ 5 ║  (Status bar + path)
║ items                     ║   ║
╟───────────────────────────╢───╢
║ Row 0: [DIR] folder/      ║   ║  (8 visible items)
║ Row 1: [TXT] document.txt ║ █ ║
║ Row 2: [LUA] script.lua→ ║ █ ║  (← selected)
║ Row 3: [ROM] game.nes     ║ █ ║
║ Row 4: [IMG] photo.png    ║ █ ║
║ Row 5: [FILE] data.bin    ║   ║
║ Row 6: (empty)            ║   ║
║ Row 7: (empty)            ║   ║
╟───────────────────────────╢───╢
║ UP/DWN:Move SEL:Open ...  │   ║  (Help text)
╚═══════════════════════════╩═══╝
```

### Display Constants

```cpp
const int listStartY = 72;      // Top of list (after headers)
const int itemHeight = 28;      // Pixel height per item
const int itemGap = 4;          // Spacing between items
const int visibleItems = 8;     // Max items shown
const int scrollbarWidth = 4;
const int scrollbarX = 235;
```

### Rendering Optimization

The `drawList()` function uses these optimizations:

1. **Partial redraws** - Only changed rows are redrawn when selecting within visible area
2. **Smart scrolling** - Full redraw only when scroll offset changes
3. **Scrollbar calculation** - Proportional bar size/position

```cpp
if (scrollOffset == oldScrollOffset) {
  // Selection changed but not scrolled - only redraw two rows
  drawListRow(oldRow, oldSelectedIndex, false);
  drawListRow(newRow, selectedIndex, true);
} else {
  // Scrolled - redraw entire list
  drawList();
}
```

---

## Input Handling

### Button State Management

The `update()` function uses the global `buttonManager` instance:

```cpp
buttonManager.isJustPressed(KEY_X)  // Returns true once per press
buttonManager.isPressed(KEY_X)      // Returns true while held
```

### State Machine Flow

```
┌─────────────────────────────────┐
│  START: Check SD Card           │
│  If not mounted → Show message  │
└──────────┬──────────────────────┘
           │
     ┌─────▼──────────────────┐
     │ Keyboard input active? │ ── [Yes] ──> Keyboard mode (rename/create)
     └─────┬──────────────────┘
           │ No
     ┌─────▼──────────────────┐
     │ Which viewer mode?     │
     └─┬───────┬────────┬─────┘
       │       │        │
    ┌──▼─┐ ┌──▼──┐ ┌───▼───┐
    │LIST│ │TEXT │ │ IMAGE │
    └──┬─┘ └────┘ └───────┘
       │
   ┌───▼────────────────────┐
   │ Options menu shown?    │
   ├───────────────┬────────┤
   │ Yes: Navigate│ No: Nav
   │      options │      list
   └───────────────┴────────┘
```

---

## File Navigation Algorithm

### Selection Movement (UP/DOWN)

```cpp
if (buttonManager.isJustPressed(KEY_DOWN) && 
    selectedIndex < fileList.size() - 1) {
  selectedIndex++;
  
  // Auto-scroll if needed
  if (selectedIndex >= scrollOffset + visibleItems)
    scrollOffset++;
  
  moved = true;
}
```

### Viewport Management

- `selectedIndex` - File list position (0 to fileList.size()-1)
- `scrollOffset` - First visible item index
- **Visible range:** [scrollOffset, scrollOffset + visibleItems)

**Constraint:** Always keep selected item visible:
```cpp
if (selectedIndex < scrollOffset) 
  scrollOffset = selectedIndex;
if (selectedIndex >= scrollOffset + visibleItems) 
  scrollOffset = selectedIndex - visibleItems + 1;
```

---

## File Type Identification

### Extension Matching Logic

```cpp
String name = filename.toLowerCase();

if (name.endsWith(".jpg")) return FT_IMAGE;
if (name.endsWith(".lua")) return FT_LUA;
if (name.endsWith(".nes")) return FT_NES;
// ... etc
```

### ROM Detection

ROMs are grouped by console. Directories created for each:

```
/retro/nes/         (.nes)
/retro/snes/        (.snes, .smc, .sfc)
/retro/gb/          (.gb)
/retro/gbc/         (.gbc)
/retro/gba/         (.gba)
/retro/sms/         (.sms)
/retro/md/          (.md, .gen, .bin)
/retro/gg/          (.gg)
/retro/pcengine/    (.pce)
/retro/coleco/      (.col, .cv)
/retro/lynx/        (.lynx)
/retro/doom/        (.wad, .pwad)
```

---

## Text File Viewing

### Implementation

Text files are entire loaded into memory:

```cpp
void FileManager::drawTextViewer() {
  // Load file on first display
  if (textViewContent.isEmpty()) {
    File f = SD.open(currentPath + "/" + fileList[selectedIndex].name);
    while (f.available()) {
      textViewContent += (char)f.read();
    }
    f.close();
  }
  
  // Draw scrollable text
  int startLine = textScrollY / 10;
  tft.drawString(textViewContent, x, y, fontSize);
}
```

### Scrolling Control

```cpp
if (buttonManager.isJustPressed(KEY_DOWN)) {
  textScrollY += 10;  // 10 pixels per press
  drawTextViewer();
}
if (buttonManager.isJustPressed(KEY_UP)) {
  textScrollY = max(0, textScrollY - 10);
  drawTextViewer();
}
```

---

## Image Viewer

### Supported Formats

- **PNG** - Decoded via PNGdec library
- **JPEG** - Decoded via TJpg_Decoder library
- **BMP** - Native TFT_eSPI support
- **GIF** - Not yet supported

### Rendering

```cpp
void FileManager::drawImageViewer() {
  String path = currentPath + "/" + fileList[selectedIndex].name;
  
  if (imagePath.endsWith(".png")) {
    png.open(path.c_str(), png_draw);  // PNG callback
    png.decode(0, 0, 0);
  }
  else if (imagePath.endsWith(".jpg")) {
    TJpgDec.drawJpg(0, 0, path.c_str());  // JPEG decoder
  }
  else if (imagePath.endsWith(".bmp")) {
    tft.drawBitmap(0, 0, path.c_str());  // BMP direct draw
  }
}
```

---

## Clipboard Operations

### Copy Operation

```cpp
void FileManager::copySelected() {
  if (selectedIndex < fileList.size()) {
    clipboardPath = currentPath + "/" + fileList[selectedIndex].name;
    isCutOperation = false;
  }
}
```

### Paste Operation

```cpp
void FileManager::pasteToCurrent() {
  String srcPath = clipboardPath;
  String dstPath = currentPath;
  
  if (isCutOperation) {
    SD.rename(srcPath, dstPath);  // Move
  } else {
    copyFile(srcPath, dstPath);   // Copy (recursive for folders)
  }
  
  clipboardPath = "";  // Clear clipboard
  listFiles();         // Refresh display
  drawList();
}
```

### Copy vs Cut

- **Copy** - Duplicates file, keeps original
- **Cut** - Moves file to new location, removes from source
- **Clipboard** - Stores one file/folder path at a time

---

## Keyboard Input (Rename/Create)

Uses the global `keyboard` object (Keyboard.cpp):

```cpp
void FileManager::update(bool &exitToHome) {
  if (isCreatingFolder || isRenaming) {
    int res = keyboard.handleInput(newFolderName);
    
    if (res == 0) {
      // Still editing - show live preview
      drawKeyboardPreview(newFolderName);
    }
    else if (res == 1) {
      // User pressed OK
      if (isCreatingFolder) {
        SD.mkdir(currentPath + "/" + newFolderName);
      } else if (isRenaming) {
        SD.rename(currentPath + "/" + oldName, 
                  currentPath + "/" + newFolderName);
      }
      listFiles();
      isCreatingFolder = false;
      isRenaming = false;
      drawList();
    }
    else if (res == 2) {
      // User pressed Cancel
      isCreatingFolder = false;
      isRenaming = false;
      drawList();
    }
    return;  // Don't process list nav while editing
  }
}
```

---

## Performance Considerations

### Memory Usage

- **File list vector** - ~40 bytes per entry (typical folder: 100-200 entries)
- **Text cache** - Entire file content (limit: ~200KB practical)
- **Image cache** - None (streamed to display)

### Rendering Performance

- **Full list redraw** - ~50-100ms (240x320 TFT)
- **Single row redraw** - ~5-10ms
- **Partial update strategy** - Reduces flicker and lag

### Optimization Tips

1. Avoid large text files (>200KB) - load may freeze UI
2. Keep folders organized (< 200 items per directory)
3. Disable serial debugging in production
4. Use FAT32 SD cards for fastest access

---

## Integration with PoChiTa OS

### App Registration

In `src/app_registry.h`:

```cpp
AppItem sdcardApp = {
  "SD Card",
  APP_SYSTEM,
  1,  // System ID
  ""
};
```

### Launch Sequence

1. **pochita.ino main loop** detects `MODE_APP_SDCARD`
2. Calls `initSD()` from `src/sdcard.h`
3. `initSD()` calls `fileManager.begin()`
4. Calls `drawFileManager()`
5. Main loop calls `loopFileManager()` each frame
6. `loopFileManager()` updates display and handles input

### Context Switching

```cpp
// Launch SD Card app
if (selectedApp == SDCARD) {
  currentMode = MODE_APP_SDCARD;
  initSD();
  drawFileManager();
}

// In main loop
else if (currentMode == MODE_APP_SDCARD) {
  loopFileManager();
}

// Exit to launcher
void loopFileManager() {
  fileManager.update(exitToHome);
  if (exitToHome) {
    currentMode = MODE_LAUNCHER;
    drawLauncherContent();
  }
}
```

---

## Error Handling

### SD Card Detection

```cpp
void FileManager::checkSD() {
  if (!SD.begin(SD_CS)) {
    if (sdMounted) {
      sdMounted = false;
      viewerMode = VM_MESSAGE;
      statusMessage = "Insert SD Card";
    }
  }
}
```

### File Operation Failures

```cpp
void FileManager::deleteSelected() {
  if (!SD.remove(filePath)) {
    statusMessage = "Delete failed";
    drawMessage();
  }
}
```

---

## Future Enhancements

### Planned Features
- [ ] File search/filter
- [ ] Favorite locations
- [ ] File sorting (by name, size, date)
- [ ] Archive support (.zip, .rar)
- [ ] FTP/WiFi file transfer
- [ ] Hex viewer for binary files
- [ ] Audio player for .mp3, .wav
- [ ] Video playback

### Known Limitations
- No network file access
- No multi-file operations
- Single-threaded (blocking file I/O)
- Limited text file size (~200KB practical limit)
- No NTFS/exFAT support (FAT32 only)

---

## Code Style & Conventions

### Naming
- `camelCase` for variables and methods
- `PascalCase` for classes and enums
- `UPPERCASE` for constants
- Prefix `is_` or `show_` for booleans

### File Organization
- Public methods in header
- Private data members
- Implementation in .cpp file
- Callbacks at file top

### Comments
- Function purpose above declaration
- Complex logic inline
- TODO/FIXME markers for future work

---

## Testing Checklist

Before release, verify:

- [ ] Navigation works with UP/DOWN smoothly
- [ ] Selection highlight follows correctly
- [ ] Scrollbar position accurate for list size
- [ ] Create folder dialog appears and works
- [ ] Rename preserves file content
- [ ] Copy/Paste creates duplicate files
- [ ] Cut/Paste removes original
- [ ] Delete works with confirmation
- [ ] Text viewer scrolls without freezing
- [ ] Images display without corruption
- [ ] ROM launching works for each console type
- [ ] Lua scripts execute correctly
- [ ] SD card removal handled gracefully
- [ ] All button combinations tested

---

**Last Updated:** April 2026  
**Maintainer:** PoChiTa Team  
**Version:** 1.0

