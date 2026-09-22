#include "FileManager.h"
#include "ui_utils.h"
#include "../lua_interpreter.h"
#include "../retro_go.h"
#include "../openrhynn_game.h"
#include "../nes_emulator.h"
#include "../ai_chat.h"
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>

void installApp(String name, String path);
extern SystemMode currentMode;
extern void drawLauncherContent();

FileManager fileManager;
PNG png;

namespace {
constexpr const char *REFRESH_ENTRY_NAME = "[SYS] Refresh / Remount SD";
constexpr const char *FORMAT_ENTRY_NAME = "[SYS] Format / Reset SD";

// VFS mount point of the SD card (see mountSDCard() in main.cpp: SD.begin(...,
// "/sd", ...)). The Arduino SD wrapper maps the card root "/" to "/sd/" WITH a
// trailing slash, and opendir("/sd/") fails on this ESP-IDF FAT VFS while
// subdirectories such as "/sd/music" work -- that is exactly why MUSIC lists
// audio fine but FILES showed an empty root. Enumerating through the POSIX VFS
// with the mount point and no trailing slash lists the root reliably too.
constexpr const char *SD_MOUNT_POINT = "/sd";

// Build the absolute POSIX/VFS path for a card-relative path ("/" -> "/sd",
// "/music" -> "/sd/music"), stripping any duplicate or trailing slashes so we
// never produce "/sd//x", "/sd/x/" or the broken "/sd/" root form.
String sdVfsPath(const String &cardPath) {
  String p = SD_MOUNT_POINT;
  if (cardPath.length() && cardPath != "/") {
    String sub = cardPath;
    if (!sub.startsWith("/")) sub = "/" + sub;
    while (sub.length() > 1 && sub.endsWith("/")) sub.remove(sub.length() - 1);
    p += sub;
  }
  return p;
}

// True when the card root can actually be enumerated through the POSIX VFS.
// Used instead of SD.open("/") because that maps to the broken "/sd/" form.
bool sdRootEnumerable() {
  DIR *d = opendir(SD_MOUNT_POINT);
  if (!d) return false;
  closedir(d);
  return true;
}
}

// --- Callbacks ---
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h,
                uint16_t *bitmap) {
  if (y >= tft.height())
    return 0;
  tft.pushImage(x, y, w, h, bitmap);
  return 1;
}

// PNG Draw Callback
// PNG Draw Callback
int png_draw(PNGDRAW *pDraw) {
  uint16_t lineBuffer[320];
  png.getLineAsRGB565(pDraw, lineBuffer, PNG_RGB565_BIG_ENDIAN, 0xffffffff);
  tft.pushImage(0, pDraw->y, pDraw->iWidth, 1, lineBuffer);
  return 1;
}

struct DrawContext {
  int x;
  int y;
};

int png_draw_attr(PNGDRAW *pDraw) {
  DrawContext *ctx = (DrawContext *)pDraw->pUser;
  uint16_t lineBuffer[320];
  png.getLineAsRGB565(pDraw, lineBuffer, PNG_RGB565_BIG_ENDIAN, 0xffffffff);
  tft.pushImage(ctx->x, ctx->y + pDraw->y, pDraw->iWidth, 1, lineBuffer);
  return 1;
}

// Minimal uncompressed BMP viewer: BI_RGB 24/32-bit, top-down or bottom-up,
// centered within the given area. Pixels are pushed big-endian so it shares the
// same tft.setSwapBytes(false) path as the JPG/PNG decoders (correct colors).
static void drawBmpCentered(const String &path, int ax, int ay, int aw, int ah) {
  File f = SD.open(path);
  if (!f) return;
  uint8_t hdr[54];
  if (f.read(hdr, 54) != 54 || hdr[0] != 'B' || hdr[1] != 'M') {
    f.close();
    return;
  }
  auto rd32 = [](const uint8_t *p) -> uint32_t {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
  };
  uint32_t dataOff = rd32(hdr + 10);
  int32_t  bw      = (int32_t)rd32(hdr + 18);
  int32_t  bh      = (int32_t)rd32(hdr + 22);
  uint16_t bpp     = (uint16_t)(hdr[28] | (hdr[29] << 8));
  uint32_t comp    = rd32(hdr + 30);
  if (comp != 0 || (bpp != 24 && bpp != 32) || bw <= 0 || bh == 0) {
    f.close();
    return;
  }
  bool    topDown = bh < 0;
  int32_t h       = topDown ? -bh : bh;
  int32_t w       = bw;
  int     bytesPP = bpp / 8;
  uint32_t rowSize = (((uint32_t)bpp * (uint32_t)w + 31u) / 32u) * 4u;
  uint8_t  *row  = (uint8_t *)malloc(rowSize);
  uint16_t *line = (uint16_t *)malloc((size_t)w * 2);
  if (!row || !line) {
    free(row);
    free(line);
    f.close();
    return;
  }
  int drawX = ax + (aw - (int)w) / 2;
  int drawY = ay + (ah - (int)h) / 2;
  for (int32_t r = 0; r < h; r++) {
    int32_t srcRow = topDown ? r : (h - 1 - r);
    f.seek(dataOff + (uint32_t)srcRow * rowSize);
    if ((uint32_t)f.read(row, rowSize) != rowSize) break;
    for (int32_t x = 0; x < w; x++) {
      uint8_t b  = row[x * bytesPP + 0];
      uint8_t g  = row[x * bytesPP + 1];
      uint8_t rr = row[x * bytesPP + 2];
      uint16_t c = (uint16_t)(((rr & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
      line[x] = (uint16_t)((c >> 8) | (c << 8));  // store big-endian
    }
    tft.pushImage(drawX, drawY + r, w, 1, line);
  }
  free(row);
  free(line);
  f.close();
}

// -----------------

bool FileManager::ensureDirectories() {
    const char* dirs[] = {
        "/theme", 
        "/icons", 
        "/notes", 
        "/system", 
        "/system/cache", 
        "/data", 
        "/data/images", 
        "/data/music", 
        "/data/documents",
        "/retro",
        "/retro/nes",
        "/retro/snes",
        "/retro/gb",
        "/retro/gbc",
        "/retro/gba",
        "/retro/sms",
        "/retro/md",
        "/retro/gg",
        "/retro/doom",
        "/games",
        "/games/openrhynn",
        "/roms",
        "/roms/nes",
        "/music",
        "/downloads",
        "/backup"
    };

    bool ok = true;
    for (const char *dir : dirs) {
        if (!SD.exists(dir) && !SD.mkdir(dir)) {
            ok = false;
        }
    }
    return ok;
}

// -----------------

FileType FileManager::identifyType(String filename) {
  if (filename.endsWith("/"))
    return FT_FOLDER;
  String name = filename;
  name.toLowerCase();
  if (name.endsWith(".jpg") || name.endsWith(".jpeg") ||
      name.endsWith(".bmp") || name.endsWith(".png"))
    return FT_IMAGE;
  if (name.endsWith(".txt") || name.endsWith(".log") ||
      name.endsWith(".ino") || name.endsWith(".cpp") || name.endsWith(".h"))
    return FT_TEXT;
  if (name.endsWith(".lua"))
    return FT_LUA;
  if (name.endsWith(".py"))
    return FT_PYTHON;
  if (name.endsWith(".html") || name.endsWith(".htm"))
    return FT_HTML;
  if (name.endsWith(".json"))
    return FT_JSON;
  if (name.endsWith(".js") || name.endsWith(".gs") ||
      name.endsWith(".php") || name.endsWith(".css"))
    return FT_SCRIPT;
    
  // Nintendo
  // NOTE: .bin is routed to the NES emulator (per project decision). OpenRhynn
  // .bin packages are still detected earlier by name/content before type use.
  if (name.endsWith(".nes") || name.endsWith(".bin"))
    return FT_NES;
  if (name.endsWith(".snes") || name.endsWith(".smc") || name.endsWith(".sfc"))
    return FT_SNES;
  if (name.endsWith(".gb"))
    return FT_GB;
  if (name.endsWith(".gbc"))
    return FT_GBC;
  if (name.endsWith(".gba"))
    return FT_GBA;
    
  // Sega
  if (name.endsWith(".sms"))
    return FT_SMS;
  if (name.endsWith(".sg1000") || name.endsWith(".sc"))
    return FT_SG1000;
  if (name.endsWith(".md") || name.endsWith(".gen"))
    return FT_MD;
  if (name.endsWith(".gg"))
    return FT_GG;
    
  // Coleco & NEC
  if (name.endsWith(".col") || name.endsWith(".cv"))
    return FT_COLECO;
  if (name.endsWith(".pce"))
    return FT_PCENGINE;
    
  // Atari
  if (name.endsWith(".lynx"))
    return FT_ATARI_LYNX;
    
  // DOOM
  if (name.endsWith(".wad") || name.endsWith(".pwad"))
    return FT_DOOM;
    
  // Legacy/Generic ROM
  if (name.endsWith(".img") || name.endsWith(".rom"))
    return FT_ROM;
    
  return FT_FILE;
}

String FileManager::getIcon(FileType type) {
  switch (type) {
    case FT_FOLDER: return "[DIR]";
    case FT_SYSTEM: return "[SYS]";
    case FT_IMAGE: return "[IMG]";
    case FT_TEXT: return "[TXT]";
    case FT_LUA: return "[LUA]";
    case FT_PYTHON: return "[PY]";
    case FT_HTML: return "[HTM]";
    case FT_JSON: return "[JSON]";
    case FT_SCRIPT: return "[SCR]";
    
    // Nintendo
    case FT_NES: return "[NES]";
    case FT_SNES: return "[SNES]";
    case FT_GB: return "[GB]";
    case FT_GBC: return "[GBC]";
    case FT_GBA: return "[GBA]";
    
    // Sega
    case FT_SMS: return "[SMS]";
    case FT_SG1000: return "[SG1K]";
    case FT_MD: return "[MD]";
    case FT_GG: return "[GG]";
    
    // Coleco & NEC
    case FT_COLECO: return "[COL]";
    case FT_PCENGINE: return "[PCE]";
    
    // Atari & DOOM
    case FT_ATARI_LYNX: return "[LYNX]";
    case FT_DOOM: return "[DOOM]";
    
    case FT_ROM: return "[ROM]";
    case FT_MODEL: return "[AI]";
    case FT_FILE: return "[FILE]";
    default: return "[?]";
  }
}

void FileManager::listFiles() {
  fileList.clear();

  // Enumerate through the POSIX VFS (opendir/readdir) instead of SD.open(). The
  // Arduino SD root "/" maps to "/sd/" whose opendir() fails on this FAT VFS,
  // so SD.open("/") returned an invalid File and the root always looked empty.
  const String vfsPath = sdVfsPath(currentPath);
  DIR *dir = opendir(vfsPath.c_str());
  if (!dir) {
    Serial.printf("SD: opendir('%s') failed for path '%s'\n", vfsPath.c_str(),
                  currentPath.c_str());
    // Keep the system entries at the root so the user can always trigger a
    // manual remount instead of being stranded on an empty screen.
    if (currentPath == "/") {
      fileList.push_back({REFRESH_ENTRY_NAME, FT_SYSTEM, 0});
      fileList.push_back({FORMAT_ENTRY_NAME, FT_SYSTEM, 0});
    }
    return;
  }

  if (currentPath != "/") {
    fileList.push_back({"..", FT_FOLDER, 0});
  }

  struct dirent *ent;
  while ((ent = readdir(dir)) != nullptr) {
    String name = ent->d_name;
    if (name.isEmpty() || name == "." || name == "..") continue;
    // Some VFS versions return a full path; reduce it to the final component.
    int slash = name.lastIndexOf('/');
    if (slash >= 0) name = name.substring(slash + 1);
    if (name.isEmpty() || name.startsWith(".")) continue;

    // d_type can be DT_UNKNOWN on FAT; stat the entry to be sure of the type
    // and to obtain its size.
    bool directory = (ent->d_type == DT_DIR);
    size_t size = 0;
    String full = vfsPath + "/" + name;
    struct stat st;
    if (stat(full.c_str(), &st) == 0) {
      directory = S_ISDIR(st.st_mode);
      size = static_cast<size_t>(st.st_size);
    }
FileType type = directory ? FT_FOLDER : identifyType(name);
    // A .bin is usually a NES ROM or OpenRhynn package, but the PLE TinyLM
    // model formats (magic 0x504C4531 / 0x4C494C4B) are AI apps. Peek the
    // header so model files get the FT_MODEL type and their own launcher.
    if (!directory) {
      String lower = name;
      lower.toLowerCase();
      if (lower.endsWith(".bin")) {
        String probe = currentPath;
        if (!probe.endsWith("/")) probe += "/";
        probe += name;
        File mf = SD.open(probe, FILE_READ);
        if (mf) {
          uint32_t magic = 0;
          if (mf.read((uint8_t *)&magic, 4) == 4)
            if (magic == AI_MAGIC_PLE || magic == AI_MAGIC_VN)
              type = FT_MODEL;
          mf.close();
        }
      }
    }
    fileList.push_back({name, type, size});
    Serial.printf("SD: %s %s (%u bytes)\n", directory ? "DIR " : "FILE",
                  name.c_str(), static_cast<unsigned>(size));
  }
  closedir(dir);

  if (currentPath == "/") {
    fileList.push_back({REFRESH_ENTRY_NAME, FT_SYSTEM, 0});
    fileList.push_back({FORMAT_ENTRY_NAME, FT_SYSTEM, 0});
  }
  Serial.printf("SD: listed '%s' -> '%s': %u entries\n", currentPath.c_str(),
                vfsPath.c_str(), static_cast<unsigned>(fileList.size()));
}

bool FileManager::refreshStorage(bool showResult) {
  // SD/FAT directory data is cached by the mounted VFS. A real SD.end() is
  // required after the card has been edited on a PC; calling SD.begin() again
  // while mounted merely returns the old session.
  SD.end();
  sdMounted = false;
  delay(80);

  const uint32_t frequencies[] = {20000000, 10000000, 4000000, 1000000};
  uint32_t mountedAt = 0;   // fastest speed that can actually READ the root
  uint32_t fallbackAt = 0;  // speed that mounts + opens root but reads nothing
  for (uint32_t frequency : frequencies) {
    Serial.printf("SD: mounting at %u Hz\n", static_cast<unsigned>(frequency));
    if (!mountSDCard(false, frequency)) {
      SD.end();
      delay(80);
      continue;
    }

    // Mounting can succeed at an SPI speed the wiring cannot reliably READ at.
    // That failure mode looks like a healthy mount with a permanently empty
    // file list. Verify the root directory is genuinely enumerable before
    // accepting this speed; ensureDirectories() guarantees the system folders
    // exist, so a readable card must return at least one entry here. The check
    // goes through the POSIX mount point ("/sd") because SD.open("/") maps to
    // the broken "/sd/" form whose opendir() fails on this FAT VFS.
    ensureDirectories();
    DIR *root = opendir(SD_MOUNT_POINT);
    const bool isDir = (root != nullptr);
    bool hasEntry = false;
    if (root) {
      struct dirent *probe;
      while ((probe = readdir(root)) != nullptr) {
        const char *n = probe->d_name;
        if (n[0] == '\0' || strcmp(n, ".") == 0 || strcmp(n, "..") == 0) continue;
        hasEntry = true;
        break;
      }
      closedir(root);
    }

    if (isDir && hasEntry) {
      mountedAt = frequency;  // fully readable: keep this mount live
      break;
    }
    if (isDir && fallbackAt == 0) {
      fallbackAt = frequency;  // opens but no content (maybe an empty card)
    }
    Serial.printf("SD: %u Hz mounted but root not readable; trying slower\n",
                  static_cast<unsigned>(frequency));
    SD.end();
    delay(80);
  }

  // No speed produced readable content. If some speed at least opened the root
  // as a directory, the card is simply empty (or read-only) rather than absent,
  // so remount there instead of reporting a missing card.
  if (mountedAt == 0 && fallbackAt != 0) {
    if (mountSDCard(false, fallbackAt)) mountedAt = fallbackAt;
  }

  if (mountedAt == 0) {
    fileList.clear();
    statusMessage = "SD Card Not Found";
    viewerMode = VM_MESSAGE;
    if (showResult) drawMessage();
    Serial.println("SD: remount failed at every SPI speed");
    return false;
  }

  sdMounted = true;
  if (currentPath.isEmpty() || !SD.exists(currentPath)) currentPath = "/";
  ensureDirectories();
  selectedIndex = 0;
  scrollOffset = 0;
  listFiles();
  viewerMode = VM_LIST;
  statusMessage = "";
  Serial.printf("SD: remounted at %u Hz, %u visible entries\n",
                static_cast<unsigned>(mountedAt),
                static_cast<unsigned>(fileList.size()));

  if (showResult) drawList();
  return true;
}

void FileManager::checkSD() {
  static unsigned long lastCheck = 0;
  if (millis() - lastCheck < 1000) // Check every second instead of 500ms
    return;
  lastCheck = millis();

  // First try to detect if card is present
  uint8_t cardType = SD.cardType();
  
  if (cardType == CARD_NONE) {
    // No card detected
    if (sdMounted) {
      // Card was removed
      sdMounted = false;
      statusMessage = "SD Card Removed";
      fileList.clear();
      viewerMode = VM_MESSAGE;
      SD.end(); // Clean up SD interface
      drawMessage();
      Serial.println("SD Card removed");
    } else {
      // Still no card, try to reinitialize (hot-plug support)
      Serial.println("Attempting to reinitialize SD card...");
      currentPath = "/";
      if (refreshStorage(false)) drawList();
    }
  } else {
    // Card is present
    if (!sdMounted) {
      // Card inserted or recovered. Force a new VFS/FAT session instead of
      // reusing the stale mount left from before removal.
      currentPath = "/";
      if (refreshStorage(false)) {
        drawList();
        Serial.println("SD Card mounted successfully");
      } else {
        statusMessage = "SD Card Error";
        viewerMode = VM_MESSAGE;
        drawMessage();
        Serial.println("Failed to mount SD Card");
      }
    }
  }
}

void FileManager::update(bool &exitToHome) {
  checkSD();
  if (!sdMounted && viewerMode != VM_MESSAGE) {
    statusMessage = "Insert SD Card";
    viewerMode = VM_MESSAGE;
    drawMessage();
    return;
  }

  // Typing/saving a text file's contents.
  if (isEditingText) {
    int res = keyboard.handleInput(editBuffer);
    if (res == 1) {  // OK -> write to SD
      bool ok = writeTextFile(editTargetPath, editBuffer);
      isEditingText = false;
      showOptions = false;
      listFiles();
      statusMessage = ok ? "File saved" : "Save failed";
      viewerMode = VM_MESSAGE;
      drawMessage();
    } else if (res == 2) {  // Cancel -> discard
      isEditingText = false;
      showOptions = false;
      drawList();
    }
    return;
  }

  // Naming a new folder / new file, or renaming an entry.
  if (isCreatingFolder || isRenaming || isCreatingFile) {
    int res = keyboard.handleInput(newFolderName);
    if (res == 1) { // OK
      statusMessage = "";
      if (validEntryName(newFolderName)) {
        String path = currentPath;
        if (!path.endsWith("/"))
          path += "/";
        path += newFolderName;
        bool operationOk = false;
        if (isCreatingFolder) {
          operationOk = !SD.exists(path) && SD.mkdir(path);
        } else if (isCreatingFile) {
          if (!SD.exists(path)) operationOk = writeTextFile(path, "");
          if (operationOk) {
            // Chain straight into the editor so the file can be filled in.
            listFiles();
            isCreatingFile = false;
            isEditingText = true;
            editTargetPath = path;
            editBuffer = "";
            keyboard.begin();
            tft.fillScreen(BG_COLOR);
            keyboard.draw(true);
            return;
          }
        } else if (isRenaming) {
          if (selectedIndex < fileList.size()) {
            String oldPath = currentPath;
            if (!oldPath.endsWith("/"))
              oldPath += "/";
            oldPath += fileList[selectedIndex].name;
            operationOk = oldPath == path ||
                          (!SD.exists(path) && SD.rename(oldPath, path));
          }
        }
        listFiles();
        if (!operationOk) statusMessage = "Operation failed";
      } else {
        statusMessage = "Invalid or empty name";
      }
      isCreatingFolder = false;
      isRenaming = false;
      isCreatingFile = false;
      showOptions = false;
      if (statusMessage.length()) {
        viewerMode = VM_MESSAGE;
        drawMessage();
      } else {
        drawList();
      }
    } else if (res == 2) { // Cancel
      isCreatingFolder = false;
      isRenaming = false;
      isCreatingFile = false;
      showOptions = false;
      drawList();
    }
    return;
  }


  if (viewerMode == VM_TEXT) {
    if (buttonManager.isJustPressed(KEY_DOWN)) {
      textScrollY += 10;
      drawTextViewer();
    }
    if (buttonManager.isJustPressed(KEY_UP)) {
      textScrollY -= 10;
      if (textScrollY < 0)
        textScrollY = 0;
      drawTextViewer();
    }
    if (buttonManager.isJustPressed(KEY_A)) {
      viewerMode = VM_LIST;
      drawList();
    }
    return;
  }

  if (viewerMode == VM_PROPERTIES) {
    if (buttonManager.isJustPressed(KEY_DOWN)) {
      textScrollY += 10;
      drawProperties();
    }
    if (buttonManager.isJustPressed(KEY_UP)) {
      textScrollY -= 10;
      if (textScrollY < 0)
        textScrollY = 0;
      drawProperties();
    }
    if (buttonManager.isJustPressed(KEY_A)) {
      viewerMode = VM_LIST;
      drawList();
    }
    return;
  }

  if (viewerMode == VM_FORMAT_MENU) {
    if (buttonManager.isJustPressed(KEY_UP) && formatMenuIndex > 0) {
      --formatMenuIndex;
      drawFormatMenu();
    } else if (buttonManager.isJustPressed(KEY_DOWN) && formatMenuIndex < 1) {
      ++formatMenuIndex;
      drawFormatMenu();
    } else if (buttonManager.isJustPressed(KEY_START)) {
      formatRepartition = formatMenuIndex == 1;
      formatArmed = false;
      formatStatus = "Press RIGHT to arm";
      viewerMode = VM_FORMAT_CONFIRM;
      drawFormatConfirm();
    } else if (buttonManager.isJustPressed(KEY_A)) {
      viewerMode = VM_LIST;
      drawList();
    }
    return;
  }

  if (viewerMode == VM_FORMAT_CONFIRM) {
    if (buttonManager.isJustPressed(KEY_RIGHT)) {
      formatArmed = true;
      formatStatus = "Armed";
      drawFormatConfirm();
    } else if (buttonManager.isJustPressed(KEY_LEFT)) {
      formatArmed = false;
      formatStatus = "Disarmed";
      drawFormatConfirm();
    } else if ((buttonManager.isJustPressed(KEY_START)) && formatArmed) {
      formatStatus = formatRepartition ? "Preparing FAT32 partition"
                                       : "Deleting files and folders";
      drawFormatProgress();
      formatSuccess = formatStorage();
      formatArmed = false;
      viewerMode = VM_FORMAT_RESULT;
      drawFormatResult();
    } else if (buttonManager.isJustPressed(KEY_A)) {
      formatArmed = false;
      viewerMode = VM_FORMAT_MENU;
      drawFormatMenu();
    }
    return;
  }

  if (viewerMode == VM_FORMAT_RESULT) {
    if (buttonManager.isJustPressed(KEY_START) ||
        buttonManager.isJustPressed(KEY_A)) {
      viewerMode = VM_LIST;
      drawList();
    }
    return;
  }

  if (viewerMode == VM_IMAGE || viewerMode == VM_MESSAGE) {
    if (buttonManager.isJustPressed(KEY_A)) {
      if (viewerMode == VM_MESSAGE && !sdMounted) {
        // Retry SD card initialization when B is pressed
        if (buttonManager.isJustPressed(KEY_START)) {
          statusMessage = "Retrying...";
          drawMessage();
          delay(500);
          
          // Try a full remount, including lower SPI speeds for marginal cards.
          currentPath = "/";
          if (refreshStorage(false)) {
            drawList();
            return;
          } else {
            statusMessage = "SD Card Not Found";
            drawMessage();
          }
        }
        return;
      }
      viewerMode = VM_LIST;
      drawList();
    }
    return;
  }

  if (fileList.empty() && sdMounted) {
    static bool loaded = false;
    if (!loaded) {
      listFiles();
      loaded = true;
    }
  }

if (showOptions) {
    // Open, Edit, Run, Properties, Copy, Cut, Rename, Delete, Add to menu,
    // then optional Paste, plus New File and New Folder at the tail.
    const bool hasClipboard = !clipboardPath.isEmpty();
    const int baseCount = 9;
    const int idxPaste = hasClipboard ? baseCount : -1;
    const int idxNewFile = baseCount + (hasClipboard ? 1 : 0);
    const int idxNewFolder = idxNewFile + 1;
    const int maxOpt = idxNewFolder;

    if (buttonManager.isJustPressed(KEY_UP) && optionIndex > 0) {
      optionIndex--;
      drawOptionMenu();
    }
    if (buttonManager.isJustPressed(KEY_DOWN) && optionIndex < maxOpt) {
      optionIndex++;
      drawOptionMenu();
    }

    if (buttonManager.isJustPressed(KEY_START)) {
      if (optionIndex == 0) // Open
        openSelected();
      else if (optionIndex == 1) { // Edit text file
        editSelected();
        showOptions = false;
        return;
      } else if (optionIndex == 2) // Run
        runSelectedFile();
      else if (optionIndex == 3) { // Properties
        viewerMode = VM_PROPERTIES;
        textScrollY = 0;
        drawProperties();
        showOptions = false;
        return;
      } else if (optionIndex == 4) // Copy
        copySelected();
      else if (optionIndex == 5) // Cut
        cutSelected();
      else if (optionIndex == 6) { // Rename
        renameSelected();
      } else if (optionIndex == 7) { // Delete
        deleteSelected();
      } else if (optionIndex == 8) { // Add file to launcher
        if (selectedIndex < fileList.size() &&
            fileList[selectedIndex].type != FT_FOLDER &&
            fileList[selectedIndex].name != "..") {
          String path = currentPath;
          if (!path.endsWith("/")) path += "/";
          path += fileList[selectedIndex].name;
          String label = fileList[selectedIndex].name;
          int dot = label.lastIndexOf('.');
          if (dot > 0) label = label.substring(0, dot);
          installApp(label, path);
          statusMessage = "Added to launcher";
          viewerMode = VM_MESSAGE;
          drawMessage();
          showOptions = false;
          return;
        }
      } else if (optionIndex == idxPaste) {
        pasteToCurrent();
      } else if (optionIndex == idxNewFile) {
        isCreatingFile = true;
        newFolderName = "";
        keyboard.begin();
        tft.fillScreen(BG_COLOR);
        keyboard.draw(true);
        return;
      } else if (optionIndex == idxNewFolder) {
        isCreatingFolder = true;
        newFolderName = "";
        keyboard.begin();
        tft.fillScreen(BG_COLOR);
        keyboard.draw(true);
        return;
      }

      if (!isCreatingFolder && !isRenaming && !isCreatingFile &&
          !isEditingText && viewerMode != VM_PROPERTIES) {
        showOptions = false;
        drawList();
      }
    }

    if (buttonManager.isJustPressed(KEY_A)) {
      showOptions = false;
      drawList();
    }
    return;
  }


  // Physical OPTION key is a fast refresh shortcut from any folder. This is
  // especially useful after moving the card between the device and a PC.
  if (buttonManager.isJustPressed(KEY_OPTION)) {
    refreshStorage(true);
    return;
  }

  int oldSelectedIndex = selectedIndex;
  int oldScrollOffset = scrollOffset;
  const int visibleItems = 6;
  bool moved = false;
  if (buttonManager.isJustPressed(KEY_DOWN) &&
      selectedIndex < fileList.size() - 1) {
    selectedIndex++;
    if (selectedIndex >= scrollOffset + visibleItems)
      scrollOffset++;
    moved = true;
  }
  if (buttonManager.isJustPressed(KEY_UP) && selectedIndex > 0) {
    selectedIndex--;
    if (selectedIndex < scrollOffset)
      scrollOffset--;
    moved = true;
  }

  if (moved) {
    if (scrollOffset == oldScrollOffset) {
      int oldRow = oldSelectedIndex - scrollOffset;
      int newRow = selectedIndex - scrollOffset;
      if (oldRow >= 0 && oldRow < visibleItems)
        drawListRow(oldRow, oldSelectedIndex, false);
      if (newRow >= 0 && newRow < visibleItems)
        drawListRow(newRow, selectedIndex, true);
    } else {
      drawList();
    }
  }

  // SELECT/A: Default Action
  if (buttonManager.isJustPressed(KEY_START)) {
    if (!fileList.empty()) {
        FileEntry &entry = fileList[selectedIndex];
        if (entry.name == "..") openSelected(); // Up dir
        else if (entry.type == FT_FOLDER) openSelected(); // Enter dir
        else if (entry.type == FT_SCRIPT || entry.type == FT_ROM ||
                 entry.type == FT_MODEL) runSelectedFile();
        else openSelected(); // View Text/Image or Unknown
    } else {
        // Empty list? New folder maybe?
        // showOptions = true; optionIndex = 6; drawOptionMenu();
    }
  }

  // MENU: Context Menu
  if (buttonManager.isJustPressed(KEY_OPTION)) { // Context key (Symbian map moi)
      if (!fileList.empty() && fileList[selectedIndex].name != ".." &&
          fileList[selectedIndex].type != FT_SYSTEM) {
          showOptions = true;
          optionIndex = 0;
          drawOptionMenu();
      }
  }

  // BACK
  if (buttonManager.isJustPressed(KEY_A)) { // Removed KEY_SELECT from back
    if (currentPath == "/") {
      exitToHome = true;
    } else {
      int lastSlash = currentPath.lastIndexOf('/', currentPath.length() - 2);
      if (lastSlash != -1)
        currentPath = currentPath.substring(0, lastSlash + 1);
      else
        currentPath = "/";
      selectedIndex = 0;
      scrollOffset = 0;
      listFiles();
      drawList();
    }
  }
}

  // Scrollbar uses ui_utils helper
void FileManager::drawList() {
  SymbianUI::drawChrome("Files", "Options", "Back");

  // Compact path strip under the title.
  tft.fillRect(0, 53, UiLayout::WIDTH, 21, SymbianUI::BG);
  tft.drawFastHLine(0, 73, UiLayout::WIDTH, SymbianUI::DIVIDER);
  tft.setTextColor(SymbianUI::DIM, SymbianUI::BG);
  tft.setTextDatum(ML_DATUM);
  String displayPath = currentPath;
  if (displayPath.length() > 26) displayPath = ".." + displayPath.substring(displayPath.length() - 24);
  tft.drawString(displayPath, 8, 63, 1);
  
  // Item count
  tft.setTextDatum(MR_DATUM);
  tft.drawString(String(fileList.size()) + " items", 232, 63, 1);
  
  int listStartY = 76;

  if (fileList.empty()) {
    tft.fillRect(0, listStartY, UiLayout::WIDTH,
                 UiLayout::FOOTER_Y - listStartY, SymbianUI::BG);
    tft.setTextColor(TFT_SILVER, SymbianUI::BG);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Empty folder", 120, 118, 2);

    // An empty root almost always means the directory could not be READ, not
    // that the card is blank. Surface the live SD state on-screen so the cause
    // can be identified without a serial cable.
    uint8_t ct = SD.cardType();
    const char *ctName = (ct == CARD_NONE)  ? "NONE"
                         : (ct == CARD_MMC) ? "MMC"
                         : (ct == CARD_SD)  ? "SD"
                         : (ct == CARD_SDHC) ? "SDHC"
                                             : "UNKNOWN";
    File dbg = SD.open(currentPath);
    const bool openOk = static_cast<bool>(dbg);
    const bool dirOk = openOk && dbg.isDirectory();
    if (dbg) dbg.close();

    tft.setTextColor(SymbianUI::DIM, SymbianUI::BG);
    tft.drawString(String("path: ") + currentPath, 120, 150, 1);
    tft.drawString(String("mount: ") + (sdMounted ? "yes" : "no") +
                       "   type: " + ctName,
                   120, 166, 1);
    tft.drawString(String("open: ") + (openOk ? "ok" : "FAIL") +
                       "   isDir: " + (dirOk ? "yes" : "no"),
                   120, 182, 1);
    tft.setTextColor(SymbianUI::ACCENT, SymbianUI::BG);
    tft.drawString("Press Options for Refresh/Remount", 120, 206, 1);
    return;
  }

  int visibleItems = 6;
  
  // Adjust scrollOffset if needed
  if (selectedIndex < scrollOffset) scrollOffset = selectedIndex;
  if (selectedIndex >= scrollOffset + visibleItems) scrollOffset = selectedIndex - visibleItems + 1;

  for (int i = 0; i < visibleItems; i++) {
    int idx = i + scrollOffset;
    drawListRow(i, idx, idx == selectedIndex);
  }
  
  // Scrollbar
  int totalItems = fileList.size();
  if (totalItems > visibleItems) {
    drawScrollBar(235, listStartY, 4, 204, totalItems, scrollOffset,
                  visibleItems, SymbianUI::ACCENT);
  }
  
  SymbianUI::drawSoftkeys("Options", "Back");
}

void FileManager::drawListRow(int rowIndex, int idx, bool sel) {
  int listStartY = 76;
  int itemHeight = 32;
  int itemGap = 2;
  int yPos = listStartY + (rowIndex * (itemHeight + itemGap));

  const int x = 4;
  const int w = UiLayout::WIDTH - 8;

  if (idx >= fileList.size()) {
    tft.fillRect(x, yPos, w, itemHeight + itemGap, SymbianUI::BG);
    return;
  }

  FileEntry &entry = fileList[idx];
  // Rounded amber card when selected, thin gray outline otherwise.
  tft.fillRect(x, yPos, w, itemHeight + itemGap, SymbianUI::BG);
  SymbianUI::drawRoundCard(x, yPos, w, itemHeight, sel);

  uint16_t txt = sel ? SymbianUI::ON_ACCENT : SymbianUI::FG;
  int cy = yPos + itemHeight / 2;

  tft.setTextDatum(ML_DATUM);
  String icon = getIcon(entry.type);
  tft.setTextColor(txt);
  tft.drawString(icon, x + 10, cy, 2);

  String name = entry.name;
  if (name.length() > 18) name = name.substring(0, 15) + "...";
  tft.setTextColor(txt);
  tft.drawString(name, x + 42, cy, 2);
}

void FileManager::drawOptionMenu() {
  SymbianUI::drawChrome("File options", "Select", "Back");

  // Base options always available (must match update() dispatch indices).
  const char *baseOpts[] = {"Open", "Edit",   "Run",    "Properties", "Copy",
                            "Cut",  "Rename", "Delete", "Add to menu"};
  const int baseCount = 9;

  bool hasClipboard = !clipboardPath.isEmpty();
  int totalOpts = baseCount + (hasClipboard ? 1 : 0) + 2; // + Paste? + New File + New Folder

  constexpr int visible = 7;
  int first = constrain(optionIndex - visible / 2, 0, max(0, totalOpts - visible));
  for (int row = 0; row < visible; row++) {
    int i = first + row;
    if (i >= totalOpts) break;
    String label;
    if (i < baseCount) {
      label = baseOpts[i];
    } else if (i == baseCount && hasClipboard) {
      label = "Paste";
    } else if (i == baseCount + (hasClipboard ? 1 : 0)) {
      label = "New File";
    } else {
      label = "New Folder";
    }

    SymbianUI::drawListRow(62 + row * 32, 29, label, optionIndex == i);
  }
}

void FileManager::openSelected() {
  if (selectedIndex >= fileList.size())
    return;
  FileEntry entry = fileList[selectedIndex];

  if (entry.type == FT_SYSTEM && entry.name == REFRESH_ENTRY_NAME) {
    refreshStorage(true);
    return;
  }

  if (entry.type == FT_SYSTEM && entry.name == FORMAT_ENTRY_NAME) {
    formatMenuIndex = 0;
    formatArmed = false;
    viewerMode = VM_FORMAT_MENU;
    drawFormatMenu();
    return;
  }

  if (entry.name == "..") {
    int lastSlash = currentPath.lastIndexOf('/', currentPath.length() - 2);
    if (lastSlash != -1)
      currentPath = currentPath.substring(0, lastSlash + 1);
    else
      currentPath = "/";
    selectedIndex = 0;
    scrollOffset = 0;
    listFiles();
    drawList();
    return;
  }

  if (entry.type == FT_FOLDER) {
    String newPath = currentPath;
    if (!newPath.endsWith("/"))
      newPath += "/";
    newPath += entry.name;
    newPath += "/";
    currentPath = newPath;
    selectedIndex = 0;
    scrollOffset = 0;
    listFiles();
    drawList();
    return;
  }

  // Emulator / ROM images are launched, not viewed. Hand them to the runner
  // (which shows the console splash and dispatches to the right emulator).
  if (entry.type == FT_NES || entry.type == FT_SNES || entry.type == FT_GB ||
      entry.type == FT_GBC || entry.type == FT_GBA || entry.type == FT_SMS ||
      entry.type == FT_SG1000 || entry.type == FT_MD || entry.type == FT_GG ||
      entry.type == FT_COLECO || entry.type == FT_PCENGINE ||
      entry.type == FT_ATARI_LYNX || entry.type == FT_DOOM ||
      entry.type == FT_ROM) {
    runSelectedFile();
    drawList();
    return;
  }

  String fullPath = currentPath;
  if (!fullPath.endsWith("/"))
    fullPath += "/";
  fullPath += entry.name;
  currentFileView = fullPath;

  if (entry.type == FT_TEXT || entry.type == FT_LUA || entry.type == FT_PYTHON || 
      entry.type == FT_SCRIPT || entry.type == FT_HTML || entry.type == FT_JSON) {
    File f = SD.open(fullPath);
    if (f) {
      textBuffer = "";
      int c = 0;
      while (f.available() && c < 2000) {
        textBuffer += (char)f.read();
        c++;
      }
      f.close();
      viewerMode = VM_TEXT;
      textScrollY = 0;
      drawTextViewer();
    }
  } else if (entry.type == FT_IMAGE) {
    viewerMode = VM_IMAGE;
    drawImageViewer();
  } else {
    statusMessage = "Unsupported File";
    viewerMode = VM_MESSAGE;
    drawMessage();
  }
}

// Write (create or overwrite) a text file on the SD card.
bool FileManager::writeTextFile(const String &path, const String &content) {
  File f = SD.open(path, FILE_WRITE);
  if (!f)
    return false;
  f.print(content);
  f.close();
  return true;
}

// Load a text-like file into the on-screen keyboard so it can be edited and
// saved back to the SD card.
void FileManager::editSelected() {
  if (selectedIndex >= fileList.size())
    return;
  FileEntry &entry = fileList[selectedIndex];

  if (entry.type == FT_FOLDER || entry.name == "..") {
    statusMessage = "Cannot edit folder";
    viewerMode = VM_MESSAGE;
    drawMessage();
    return;
  }

  if (!(entry.type == FT_TEXT || entry.type == FT_LUA ||
        entry.type == FT_PYTHON || entry.type == FT_SCRIPT ||
        entry.type == FT_HTML || entry.type == FT_JSON)) {
    statusMessage = "Not a text file";
    viewerMode = VM_MESSAGE;
    drawMessage();
    return;
  }

  String path = currentPath;
  if (!path.endsWith("/"))
    path += "/";
  path += entry.name;

  editBuffer = "";
  File f = SD.open(path);
  if (f) {
    int c = 0;
    while (f.available() && c < 2000) {
      editBuffer += (char)f.read();
      c++;
    }
    f.close();
  }

  editTargetPath = path;
  isEditingText = true;
  keyboard.begin();
  tft.fillScreen(BG_COLOR);
  keyboard.draw(true);
}

void FileManager::renameSelected() {
  isRenaming = true;
  newFolderName = fileList[selectedIndex].name;
  keyboard.begin();
  tft.fillScreen(BG_COLOR);
  keyboard.draw(true);
}

void FileManager::copySelected() {
  if (fileList[selectedIndex].name == "..")
    return;
  String fullPath = currentPath;
  if (!fullPath.endsWith("/"))
    fullPath += "/";
  fullPath += fileList[selectedIndex].name;

  clipboardPath = fullPath;
  isCutOperation = false;
  statusMessage = "Copied to Clipboard";
  viewerMode = VM_MESSAGE;
  drawMessage();
}

void FileManager::cutSelected() {
  if (fileList[selectedIndex].name == "..")
    return;
  String fullPath = currentPath;
  if (!fullPath.endsWith("/"))
    fullPath += "/";
  fullPath += fileList[selectedIndex].name;

  clipboardPath = fullPath;
  isCutOperation = true;
  statusMessage = "Cut to Clipboard";
  viewerMode = VM_MESSAGE;
  drawMessage();
}

void FileManager::pasteToCurrent() {
  if (clipboardPath.isEmpty())
    return;

  String dest = currentPath;
  if (!dest.endsWith("/"))
    dest += "/";

  int lastSlash = clipboardPath.lastIndexOf('/');
  String filename = clipboardPath.substring(lastSlash + 1);
  dest += filename;

  if (dest == clipboardPath || SD.exists(dest)) {
    statusMessage = dest == clipboardPath ? "Same source/destination"
                                          : "Destination exists";
    viewerMode = VM_MESSAGE;
    drawMessage();
    return;
  }

  bool ok = false;
  if (isCutOperation) {
    ok = SD.rename(clipboardPath, dest);
    if (ok) clipboardPath = "";
  } else {
    ok = recursiveCopy(clipboardPath, dest);
  }

  if (!ok) {
    statusMessage = "Paste failed";
    viewerMode = VM_MESSAGE;
    drawMessage();
    return;
  }
  listFiles();
  drawList();
}

void FileManager::deleteSelected() {
  if (fileList.empty())
    return;
  FileEntry entry = fileList[selectedIndex];
  if (entry.name == ".." || entry.type == FT_SYSTEM)
    return;

  String path = currentPath;
  if (!path.endsWith("/"))
    path += "/";
  path += entry.name;

  bool deleted = false;
  if (entry.type == FT_FOLDER) {
    recursiveDelete(path.c_str());
    deleted = !SD.exists(path);
  } else {
    deleted = SD.remove(path);
  }

  listFiles();
  if (!deleted) {
    statusMessage = "Delete failed";
    viewerMode = VM_MESSAGE;
    drawMessage();
  } else {
    drawList();
  }
}

void FileManager::drawImageViewer() {
  tft.fillScreen(SymbianUI::BG);
  
  String name = currentFileView;
  name.toLowerCase();

  // Content area between the title bar and the softkey footer.
  const int AX = 0;
  const int AY = 54;
  const int AW = UiLayout::WIDTH;             // 240
  const int AH = UiLayout::FOOTER_Y - AY;    // 296 - 54 = 242

  // All decoders below produce big-endian RGB565 buffers, so the panel must
  // NOT byte-swap again (otherwise red/blue get swapped -> wrong colors).
  tft.setSwapBytes(false);

  if (name.endsWith(".jpg") || name.endsWith(".jpeg")) {
    uint16_t iw = 0, ih = 0;
    TJpgDec.setCallback(tft_output);
    TJpgDec.setSwapBytes(true);
    if (TJpgDec.getSdJpgSize(&iw, &ih, currentFileView) == JDR_OK && iw && ih) {
      // Pick the smallest hardware scale (1/2/4/8) that fits the content area.
      uint8_t scale = 1;
      while (scale < 8 &&
             ((iw / scale) > (uint16_t)AW || (ih / scale) > (uint16_t)AH))
        scale <<= 1;
      TJpgDec.setJpgScale(scale);
      int dw = iw / scale, dh = ih / scale;
      int dx = AX + (AW - dw) / 2;
      int dy = AY + (AH - dh) / 2;
      if (dx < 0) dx = 0;
      if (dy < AY) dy = AY;
      TJpgDec.drawSdJpg(dx, dy, currentFileView);
    } else {
      TJpgDec.setJpgScale(1);
      TJpgDec.drawSdJpg(AX, AY, currentFileView);
    }
  } else if (name.endsWith(".png")) {
    int rc = png.open(
        currentFileView.c_str(),
        // Open (void *)
        [](const char *filename, int32_t *size) -> void * {
          File *f = new File(SD.open(filename));
          if (f && *f) {
            *size = f->size();
            return (void *)f;
          }
          delete f;
          return NULL;
        },
        // Close (void *)
        [](void *handle) -> void {
          if (handle) {
            ((File *)handle)->close();
            delete (File *)handle;
          }
        },
        // Read (PNGFILE *)
        [](PNGFILE *handle, uint8_t *buffer, int32_t length) -> int32_t {
          if (!handle || !handle->fHandle)
            return 0;
          return ((File *)(handle->fHandle))->read(buffer, length);
        },
        // Seek (PNGFILE *)
        [](PNGFILE *handle, int32_t position) -> int32_t {
          if (!handle || !handle->fHandle)
            return 0;
          return ((File *)(handle->fHandle))->seek(position);
        },
        (PNG_DRAW_CALLBACK *)png_draw_attr);

    if (rc == PNG_SUCCESS) {
      // PNGdec cannot downscale, so just center it (larger images are clipped).
      DrawContext ctx;
      ctx.x = AX + (AW - png.getWidth()) / 2;
      ctx.y = AY + (AH - png.getHeight()) / 2;
      if (ctx.y < AY) ctx.y = AY;
      tft.startWrite();
      png.decode(&ctx, 0);
      tft.endWrite();
      png.close();
    }
  } else if (name.endsWith(".bmp")) {
    drawBmpCentered(currentFileView, AX, AY, AW, AH);
  }
  // Redraw system chrome after the decoder so full-size images cannot cover it.
  SymbianUI::drawStatusBar();
  SymbianUI::drawTitle("Image viewer");
  SymbianUI::drawSoftkeys("", "Back");
}

void FileManager::drawTextViewer() {
  SymbianUI::drawChrome("Text viewer", "", "Back");
  
  // File name
  int lastSlash = currentFileView.lastIndexOf('/');
  String fileName = (lastSlash != -1) ? currentFileView.substring(lastSlash + 1) : currentFileView;
  tft.fillRect(0, 53, 240, 20, SymbianUI::BG);
  tft.drawFastHLine(0, 72, 240, SymbianUI::DIVIDER);
  tft.setTextColor(SymbianUI::DIM, SymbianUI::BG);
  tft.setTextDatum(ML_DATUM);
  if (fileName.length() > 30) fileName = ".." + fileName.substring(fileName.length() - 27);
  tft.drawString(fileName, 8, 63, 1);
  
  tft.setTextColor(SymbianUI::FG, SymbianUI::BG);
  tft.setCursor(8, 78 - textScrollY);
  tft.print(textBuffer);
}

void FileManager::drawMessage() {
  SymbianUI::drawMessageScreen("Files", SymbianUI::ICON_INFO,
                               statusMessage, "Check storage or file type",
                               "Retry", "Back", SymbianUI::WARN);
}

void FileManager::drawFormatMenu() {
  SymbianUI::drawChrome("SD format options", "Select", "Back");
  tft.fillRect(0, 53, UiLayout::WIDTH, UiLayout::FOOTER_Y - 53, SymbianUI::BG);
  SymbianUI::drawSectionLabel(58, "Choose operation");
  SymbianUI::drawListRow(82, 54, "Factory reset", formatMenuIndex == 0,
                         "Keep FAT32", SymbianUI::ICON_FOLDER);
  SymbianUI::drawListRow(138, 54, "Repartition FAT32", formatMenuIndex == 1,
                         "Repair card", SymbianUI::ICON_SYSTEM);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(SymbianUI::DIM, SymbianUI::BG);
  tft.drawString(formatMenuIndex == 0
                     ? "Erase files and restore folders"
                     : "New MBR partition and FAT32 filesystem",
                 120, 218, 1);
  SymbianUI::drawSoftkeys("Select", "Back");
}

void FileManager::drawFormatConfirm() {
  SymbianUI::drawChrome(formatRepartition ? "Repartition FAT32" : "Factory reset",
                        formatArmed ? "Erase" : "Arm", "Cancel");
  tft.fillRect(0, 53, UiLayout::WIDTH, UiLayout::FOOTER_Y - 53, SymbianUI::BG);
  const uint16_t tone = formatArmed ? TFT_RED : SymbianUI::WARN;
  tft.drawRoundRect(10, 68, 220, 166, 7, tone);
  SymbianUI::drawIcon(SymbianUI::ICON_SYSTEM, 108, 82, tone);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_RED, SymbianUI::BG);
  tft.drawString("ALL SD DATA WILL BE LOST", 120, 126, 2);
  tft.setTextColor(SymbianUI::FG, SymbianUI::BG);
  tft.drawString("Files, games and saves are erased.", 120, 151, 1);
  tft.drawString(formatRepartition ? "A new MBR + FAT32 will be created."
                                   : "Default folders are then restored.",
                 120, 168, 1);
  tft.setTextColor(SymbianUI::DIM, SymbianUI::BG);
  tft.drawString(formatRepartition ? "Use this to repair PC card errors."
                                   : "The FAT32 partition is not changed.",
                 120, 190, 1);
  tft.setTextColor(tone, SymbianUI::BG);
  tft.drawString(formatArmed ? "ARMED - press A/SELECT" : "Press RIGHT to arm", 120, 216, 2);
  SymbianUI::drawSoftkeys(formatArmed ? "Erase" : "Arm", "Cancel");
}

void FileManager::drawFormatProgress() {
  SymbianUI::drawChrome(formatRepartition ? "Creating FAT32" : "Formatting SD", "", "");
  tft.fillRect(0, 53, UiLayout::WIDTH, UiLayout::FOOTER_Y - 53, SymbianUI::BG);
  SymbianUI::drawIcon(SymbianUI::ICON_SYSTEM, 108, 91, SymbianUI::WARN);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(SymbianUI::WARN, SymbianUI::BG);
  tft.drawString(formatRepartition ? "Writing MBR and FAT32..."
                                   : "Deleting SD contents...",
                 120, 148, 2);
  tft.setTextColor(SymbianUI::FG, SymbianUI::BG);
  tft.drawString("Removed: " + String(formatDeletedItems) + " items", 120, 177, 2);
  tft.setTextColor(SymbianUI::DIM, SymbianUI::BG);
  tft.drawString("Do not remove the card", 120, 203, 1);
}

void FileManager::drawFormatResult() {
  const uint16_t tone = formatSuccess ? TFT_GREEN : SymbianUI::WARN;
  SymbianUI::drawMessageScreen("SD format result", SymbianUI::ICON_SYSTEM,
                               formatSuccess ? "Completed" : "Completed with errors",
                               formatStatus + " - " + String(formatDeletedItems) + " removed",
                               "OK", "Back", tone);
}

bool FileManager::removeStorageTree(const String &path, uint8_t depth) {
  if (depth > 20) {
    formatStatus = "Directory nesting too deep";
    return false;
  }

  File node = SD.open(path, FILE_READ);
  if (!node) return false;
  const bool directory = node.isDirectory();
  node.close();

  if (!directory) {
    const bool removed = SD.remove(path);
    if (removed) ++formatDeletedItems;
    return removed;
  }

  bool ok = true;
  while (true) {
    File parent = SD.open(path, FILE_READ);
    if (!parent || !parent.isDirectory()) {
      if (parent) parent.close();
      ok = false;
      break;
    }
    File child = parent.openNextFile();
    if (!child) {
      parent.close();
      break;
    }
    String childPath = child.path();
    if (!childPath.startsWith("/")) {
      childPath = path + (path.endsWith("/") ? "" : "/") + child.name();
    }
    const bool childDirectory = child.isDirectory();
    child.close();
    parent.close();

    const bool childOk = childDirectory ? removeStorageTree(childPath, depth + 1)
                                        : SD.remove(childPath);
    if (childOk) {
      if (!childDirectory) ++formatDeletedItems;
    } else {
      ok = false;
      formatStatus = "Cannot remove " + childPath;
      break;
    }

    if ((formatDeletedItems & 7U) == 0U) {
      drawFormatProgress();
      yield();
      delay(1);
    }
  }

  if (path != "/") {
    const bool removed = SD.rmdir(path);
    if (removed) ++formatDeletedItems;
    else ok = false;
  }
  return ok;
}

bool FileManager::formatStorage() {
  if (!sdMounted || SD.cardType() == CARD_NONE) {
    formatStatus = "microSD is not mounted";
    return false;
  }

  clipboardPath = "";
  isCutOperation = false;
  formatDeletedItems = 0;
  bool ok = formatRepartition ? repartitionFat32() : removeStorageTree("/");
  const String operationStatus = formatStatus;

  if (!sdMounted) return false;

  formatStatus = "Restoring POCHITA folders";
  drawFormatProgress();
  if (!ensureDirectories()) ok = false;

  File readme = SD.open("/README.txt", FILE_WRITE);
  if (readme) {
    readme.println("POCHITA OS SD card factory layout");
    readme.println("Apps and games: /games");
    readme.println("ROM files: /roms");
    readme.println("Music: /music or /data/music");
    readme.println("Copy OpenRhynn.bin back after a format to reinstall the game.");
    readme.close();
  } else {
    ok = false;
  }

  currentPath = "/";
  selectedIndex = 0;
  scrollOffset = 0;
  listFiles();
  if (ok) {
    formatStatus = formatRepartition ? "FAT32 partition verified"
                                     : "Factory folders restored";
  } else {
    formatStatus = operationStatus.length() ? operationStatus
                                            : "Format completed with errors";
  }
  return ok;
}

bool FileManager::repartitionFat32() {
  constexpr uint32_t partitionStart = 2048;  // 1 MiB alignment at 512 B/sector.
  const uint32_t totalSectors = static_cast<uint32_t>(SD.numSectors());
  if (totalSectors <= partitionStart + 131072UL) {
    formatStatus = "Card is too small for FAT32";
    return false;
  }

  uint8_t mbr[512]{};
  auto writeLe32 = [](uint8_t *target, uint32_t value) {
    target[0] = static_cast<uint8_t>(value);
    target[1] = static_cast<uint8_t>(value >> 8);
    target[2] = static_cast<uint8_t>(value >> 16);
    target[3] = static_cast<uint8_t>(value >> 24);
  };
  auto readLe32 = [](const uint8_t *source) -> uint32_t {
    return static_cast<uint32_t>(source[0]) |
           (static_cast<uint32_t>(source[1]) << 8) |
           (static_cast<uint32_t>(source[2]) << 16) |
           (static_cast<uint32_t>(source[3]) << 24);
  };

  // Windows-compatible MBR with one active, 1 MiB-aligned FAT32-LBA partition.
  writeLe32(mbr + 440, static_cast<uint32_t>(micros()));
  uint8_t *partition = mbr + 446;
  partition[0] = 0x80;
  partition[1] = 0xFE;
  partition[2] = 0xFF;
  partition[3] = 0xFF;
  partition[4] = 0x07;  // Temporary; f_mkfs changes this to FAT32 LBA (0x0C).
  partition[5] = 0xFE;
  partition[6] = 0xFF;
  partition[7] = 0xFF;
  writeLe32(partition + 8, partitionStart);
  writeLe32(partition + 12, totalSectors - partitionStart);
  mbr[510] = 0x55;
  mbr[511] = 0xAA;

  BYTE *work = static_cast<BYTE *>(malloc(FF_MAX_SS));
  if (!work) {
    formatStatus = "Not enough memory for FAT32";
    return false;
  }

  constexpr BYTE logicalDrive = 0;
  constexpr const char *drive = "0:";
  const BYTE oldPhysicalDrive = VolToPart[logicalDrive].pd;
  const BYTE oldPartition = VolToPart[logicalDrive].pt;

  FRESULT result = f_mount(nullptr, drive, 0);
  if (result == FR_OK && !SD.writeRAW(mbr, 0)) result = FR_DISK_ERR;

  // Force logical drive 0 to partition 1 so f_mkfs preserves the aligned MBR.
  if (result == FR_OK) {
    VolToPart[logicalDrive].pd = logicalDrive;
    VolToPart[logicalDrive].pt = 1;
    result = f_mkfs(drive, FM_FAT32, 0, work, FF_MAX_SS);
  }
  VolToPart[logicalDrive].pd = oldPhysicalDrive;
  VolToPart[logicalDrive].pt = oldPartition;
  free(work);

  SD.end();
  sdMounted = false;
  delay(120);

  if (result != FR_OK) {
    formatStatus = "FAT32 error code " + String(static_cast<int>(result));
    // Best-effort recovery prevents a failed format from permanently stranding
    // the card. The result remains an error so the UI never claims success.
    if (mountSDCard(true)) sdMounted = true;
    return false;
  }

  if (!mountSDCard(false)) {
    formatStatus = "FAT32 created but remount failed";
    return false;
  }
  sdMounted = true;

  uint8_t checkMbr[512]{};
  uint8_t bootSector[512]{};
  bool verified = SD.readRAW(checkMbr, 0) && checkMbr[510] == 0x55 &&
                  checkMbr[511] == 0xAA;
  const uint32_t bootLba = verified ? readLe32(checkMbr + 454) : 0;
  verified = verified && bootLba >= 63 && SD.readRAW(bootSector, bootLba) &&
             std::memcmp(bootSector + 82, "FAT32   ", 8) == 0 &&
             (checkMbr[450] == 0x0B || checkMbr[450] == 0x0C);
  if (!verified) {
    formatStatus = "FAT32 verification failed";
    return false;
  }

  formatStatus = "FAT32 partition verified";
  return true;
}

void FileManager::drawProperties() {
  SymbianUI::drawChrome("Properties", "", "Back");

  if (selectedIndex >= fileList.size())
    return;
  FileEntry entry = fileList[selectedIndex];

  String fullPath = currentPath;
  if (!fullPath.endsWith("/"))
    fullPath += "/";
  fullPath += entry.name;

  int y = 66 - textScrollY;

  // Helper to check y boundary for simple text
  auto drawLine = [&](String label, String value) {
    if (y > -20 && y < 240) {
      tft.setTextColor(SymbianUI::ACCENT, SymbianUI::BG);
      tft.drawString(label, 10, y, 2);
      tft.setTextColor(SymbianUI::FG, SymbianUI::BG);
      tft.drawString(value, 80, y, 2);
    }
    y += 20;
  };

  drawLine("File:", entry.name);
  drawLine("Type:", getIcon(entry.type));
  drawLine("Size:", String(entry.size) + " B");
  drawLine("Path:", currentPath);

  y += 10;

  if (entry.type == FT_IMAGE) {
    if (y > -200 && y < 240) {
      tft.drawString("Preview:", 10, y, 2);
    }
    y += 20;

    // Image Preview Logic
    // We draw centered, max width 200, max height 120 ?
    int imgX = 60;
    int imgY = y;

    String lowerName = entry.name;
    lowerName.toLowerCase();

    // To properly preview, we usually need to draw it.
    // TJpgDec can scale. 1, 2, 4, 8.
    if (lowerName.endsWith(".jpg") || lowerName.endsWith(".jpeg")) {
      TJpgDec.setJpgScale(4); // 1/4 scale for thumbnail
      TJpgDec.setSwapBytes(true);
      TJpgDec.setCallback(tft_output);
      TJpgDec.drawSdJpg(imgX, imgY, fullPath);
      // Assume standard size or small enough?
      // If we dont know size, y increment is guess.
      y += 100;
    } else if (lowerName.endsWith(".png")) {
      // PNG render full size is slow and big.
      // We'll try to render it but might overflow.
      // Just rendering as is for now, maybe clipped by screen.
      // To scale PNG is hard without library support.
      // We will just draw it.
      int rc = png.open(
          fullPath.c_str(),
          [](const char *filename, int32_t *size) -> void * {
            File *f = new File(SD.open(filename));
            if (f && *f) {
              *size = f->size();
              return (void *)f;
            }
            delete f;
            return NULL;
          },
          [](void *handle) -> void {
            if (handle) {
              ((File *)handle)->close();
              delete (File *)handle;
            }
          },
          [](PNGFILE *handle, uint8_t *buffer, int32_t length) -> int32_t {
            if (!handle || !handle->fHandle)
              return 0;
            return ((File *)(handle->fHandle))->read(buffer, length);
          },
[](PNGFILE *handle, int32_t position) -> int32_t {
            if (!handle || !handle->fHandle)
              return 0;
            return ((File *)(handle->fHandle))->seek(position);
          },
          [](PNGDRAW *pDraw) -> int {
            return png_draw_attr(pDraw);
          }); // Pass the function pointer

      if (rc == PNG_SUCCESS) {
        DrawContext ctx = {imgX, imgY};
        tft.startWrite();
        png.decode(&ctx, 0);
        tft.endWrite();
        png.close();
        y += 100; // Guess height
      }
    }
  }

  tft.setTextColor(THEME_COLOR);
  // End
}

void FileManager::recursiveDelete(const char *path) {
  File root = SD.open(path);
  if (!root)
    return;
  if (!root.isDirectory()) {
    root.close();
    SD.remove(path);
    return;
  }
  File file = root.openNextFile();
  while (file) {
    String entryPath = String(path);
    if (!entryPath.endsWith("/"))
      entryPath += "/";
    entryPath += file.name();
    if (file.isDirectory()) {
      file.close();
      recursiveDelete(entryPath.c_str());
    } else {
      file.close();
      SD.remove(entryPath);
    }
    file = root.openNextFile();
  }
  root.close();
  SD.rmdir(path);
}

bool FileManager::validEntryName(const String &name) const {
  if (name.isEmpty() || name == "." || name == "..") return false;
  return name.indexOf('/') < 0 && name.indexOf('\\') < 0 &&
         name.indexOf(':') < 0;
}

bool FileManager::recursiveCopy(const String &source,
                                const String &destination) {
  File src = SD.open(source, FILE_READ);
  if (!src) return false;

  if (src.isDirectory()) {
    if (!SD.mkdir(destination)) {
      src.close();
      return false;
    }

    File child = src.openNextFile();
    while (child) {
      String childName = child.name();
      int slash = childName.lastIndexOf('/');
      if (slash >= 0) childName = childName.substring(slash + 1);
      child.close();

      String childSource = source + (source.endsWith("/") ? "" : "/") + childName;
      String childDestination = destination + "/" + childName;
      if (!recursiveCopy(childSource, childDestination)) {
        src.close();
        recursiveDelete(destination.c_str());
        return false;
      }
      child = src.openNextFile();
    }
    src.close();
    return true;
  }

  SD.remove(destination);
  File dst = SD.open(destination, FILE_WRITE);
  if (!dst) {
    src.close();
    return false;
  }

  uint8_t buffer[512];
  bool ok = true;
  while (src.available()) {
    int count = src.read(buffer, sizeof(buffer));
    if (count <= 0 || dst.write(buffer, count) != static_cast<size_t>(count)) {
      ok = false;
      break;
    }
  }
  src.close();
  dst.close();
  if (!ok) SD.remove(destination);
  return ok;
}

void FileManager::launchPath(const String &path) {
    if (!sdMounted || !SD.exists(path)) {
        statusMessage = "App file not found";
        viewerMode = VM_MESSAGE;
        drawMessage();
        return;
    }

    // Native POCHITA game package. launchPath() is used by installed launcher
    // entries, so a confirmed exit returns directly to the S60 home screen.
    if (isOpenRhynnGame(path) || isOpenRhynnFilename(path)) {
        runOpenRhynnGame(path);
        currentMode = MODE_LAUNCHER;
        drawLauncherContent();
        return;
    }

    int slash = path.lastIndexOf('/');
    String fileName = slash >= 0 ? path.substring(slash + 1) : path;
    currentPath = slash > 0 ? path.substring(0, slash) : "/";
    selectedIndex = 0;
    scrollOffset = 0;
    viewerMode = VM_LIST;
    listFiles();

    for (int i = 0; i < fileList.size(); ++i) {
        if (fileList[i].name == fileName) {
            selectedIndex = i;
            scrollOffset = max(0, i - 2);
            FileType type = fileList[i].type;
bool executable = type == FT_LUA || type == FT_PYTHON ||
                              type == FT_NES || type == FT_SNES ||
                              type == FT_GB || type == FT_GBC ||
                              type == FT_GBA || type == FT_SMS ||
                              type == FT_SG1000 || type == FT_MD ||
                              type == FT_GG || type == FT_COLECO ||
                              type == FT_PCENGINE || type == FT_ATARI_LYNX ||
                              type == FT_DOOM || type == FT_ROM ||
                              type == FT_MODEL;
            if (executable) {
                runSelectedFile();
                drawList();
            } else {
                openSelected();
            }
            return;
        }
    }

    statusMessage = "App entry unavailable";
    viewerMode = VM_MESSAGE;
    drawMessage();
}

// Jump straight into a directory (used by shortcut launchers such as the
// Retro system menu). Falls back to root when the folder is missing.
void FileManager::openDirectory(const String &path) {
    if (!sdMounted) {
        statusMessage = "No SD Card";
        viewerMode = VM_MESSAGE;
        drawMessage();
        return;
    }
    currentPath = (path.length() > 0 && SD.exists(path)) ? path : "/";
    selectedIndex = 0;
    scrollOffset = 0;
    viewerMode = VM_LIST;
    listFiles();
    drawList();
}

void FileManager::runSelectedFile() {
    if (selectedIndex >= fileList.size()) return;
    FileEntry entry = fileList[selectedIndex];
    
    String fullPath = currentPath;
    if (!fullPath.endsWith("/")) fullPath += "/";
    fullPath += entry.name;

    // A POCHITA native .bin is an SD package handled by the built-in engine,
    // not a Mega Drive ROM. Direct File Manager launch returns to this folder.
    if (isOpenRhynnGame(fullPath) || isOpenRhynnFilename(fullPath)) {
        runOpenRhynnGame(fullPath);
        drawList();
        return;
    }
    
    tft.fillScreen(COLOR_BLACK);
    tft.setTextColor(COLOR_WHITE);
    tft.setTextDatum(MC_DATUM);
    
    if (entry.type == FT_LUA) {
        tft.setTextColor(TFT_CYAN);
        tft.drawString("[ LUA ]", UiLayout::CENTER_X, 40, 2);
        tft.setTextColor(TFT_YELLOW);
        tft.drawString(UiLayout::ellipsize(entry.name, 24), UiLayout::CENTER_X, 65, 2);
        
        tft.setTextColor(TFT_SILVER);
        tft.drawString("Running script...", UiLayout::CENTER_X, 100, 1);
        
        delay(300);
        
        tft.fillScreen(COLOR_BLACK);  // clean canvas for the script
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.setTextDatum(MC_DATUM);
        
        String result = lua.runScript(fullPath);
        
        if (result.length() == 0) {
            // Graphics script: keep the script's final frame, add a footer.
            tft.setTextColor(TFT_WHITE, TFT_BLACK);
            tft.setTextDatum(MC_DATUM);
            tft.fillRect(0, 296, UiLayout::WIDTH, 24, TFT_BLACK);
            tft.setTextColor(THEME_COLOR);
            tft.drawString("Press Back to exit", UiLayout::CENTER_X, 308, 1);
            while (true) {
                buttonManager.update();
                if (buttonManager.isJustPressed(KEY_A)) break;
                delay(10);
            }
        } else {
            tft.fillScreen(COLOR_BLACK);
            
            tft.setTextColor(TFT_WHITE);
            tft.setTextDatum(MC_DATUM);
            tft.drawString("[ OUTPUT ]", UiLayout::CENTER_X, 20, 2);
            
            int y = 50;
            tft.setTextColor(TFT_GREEN);
            tft.setTextDatum(ML_DATUM);
            
            int start = 0;
            for (int i = 0; i <= result.length(); i++) {
                if (i == result.length() || result[i] == '\n') {
                    String line = result.substring(start, i);
                    if (line.length() > 0) {
                        if (y < 230) {
                            tft.drawString(line, 10, y, 2);
                            y += 20;
                        }
                    }
                    start = i + 1;
                }
            }
            
            tft.setTextColor(THEME_COLOR);
            tft.drawString("Press Back to exit", UiLayout::CENTER_X, 210, 1);
            
            while (true) {
                buttonManager.update();
                if (buttonManager.isJustPressed(KEY_A)) break;
                delay(10);
            }
        }
    }
    else if (entry.type == FT_PYTHON) {
        tft.drawString("[ PYTHON ]", UiLayout::CENTER_X, 60, 2);
        tft.setTextColor(TFT_YELLOW);
        tft.drawString(UiLayout::ellipsize(entry.name, 24), UiLayout::CENTER_X, 90, 2);
        
        tft.setTextColor(TFT_SILVER);
        tft.drawString("Python interpreter", UiLayout::CENTER_X, 130, 1);
        tft.drawString("not available.", UiLayout::CENTER_X, 145, 1);
        
        tft.setTextColor(THEME_COLOR);
        tft.drawString("Press Back to exit", UiLayout::CENTER_X, 180, 1);
        
        while (true) {
            buttonManager.update();
            if (buttonManager.isJustPressed(KEY_A)) break;
            delay(10);
        }
    }
    else if (entry.type == FT_SCRIPT || entry.type == FT_HTML || entry.type == FT_JSON) {
        tft.drawString("[ SCRIPT ]", UiLayout::CENTER_X, 60, 2);
        tft.setTextColor(TFT_YELLOW);
        tft.drawString(UiLayout::ellipsize(entry.name, 24), UiLayout::CENTER_X, 90, 2);
        
        tft.setTextColor(TFT_SILVER);
        tft.drawString("View in Text Viewer?", UiLayout::CENTER_X, 130, 1);
        
        tft.setTextColor(THEME_COLOR);
        tft.drawString("OK: View   Back: Return", UiLayout::CENTER_X, 160, 1);
        
        bool waiting = true;
        while (waiting) {
            buttonManager.update();
            if (buttonManager.isJustPressed(KEY_START)) {
                viewerMode = VM_TEXT;
                textScrollY = 0;
                currentFileView = fullPath;
                
                File f = SD.open(fullPath);
                if (f) {
                    textBuffer = "";
                    int c = 0;
                    while (f.available() && c < 2000) {
                        textBuffer += (char)f.read();
                        c++;
                    }
                    f.close();
                    drawTextViewer();
                }
                waiting = false;
            }
            if (buttonManager.isJustPressed(KEY_A)) {
                waiting = false;
            }
            delay(10);
        }
    }
    else if (entry.type == FT_MODEL) {
        tft.setTextColor(TFT_CYAN);
        tft.drawString("[ AI MODEL ]", UiLayout::CENTER_X, 40, 2);
        tft.setTextColor(TFT_YELLOW);
        tft.drawString(UiLayout::ellipsize(entry.name, 24), UiLayout::CENTER_X, 65, 2);

        tft.setTextColor(TFT_SILVER);
        tft.drawString("Loading model from SD...", UiLayout::CENTER_X, 100, 1);

        delay(300);

        tft.fillScreen(COLOR_BLACK);
        runAiChat(fullPath);
        if (!aiTookLaunch()) drawList();
        return;
    }
    else if (entry.type == FT_NES || entry.type == FT_SNES || entry.type == FT_GB || 
             entry.type == FT_GBC || entry.type == FT_GBA || entry.type == FT_SMS || 
             entry.type == FT_SG1000 || entry.type == FT_MD || entry.type == FT_GG ||
             entry.type == FT_COLECO || entry.type == FT_PCENGINE || entry.type == FT_ATARI_LYNX ||
             entry.type == FT_DOOM || entry.type == FT_ROM) {
        
        String consoleName = "";
        uint16_t consoleColor = TFT_WHITE;
        
        switch (entry.type) {
            case FT_NES: consoleName = "NES"; consoleColor = TFT_RED; break;
            case FT_SNES: consoleName = "SNES"; consoleColor = 0xF81F; break;
            case FT_GB: consoleName = "GAMEBOY"; consoleColor = 0x39C4; break;
            case FT_GBC: consoleName = "GBC"; consoleColor = 0xA5D6; break;
            case FT_GBA: consoleName = "GBA"; consoleColor = 0x7E3F; break;
            case FT_SMS: consoleName = "MASTER SYSTEM"; consoleColor = 0x05B8; break;
            case FT_SG1000: consoleName = "SG-1000"; consoleColor = 0x05B8; break;
            case FT_MD: consoleName = "MEGA DRIVE"; consoleColor = 0x0421; break;
            case FT_GG: consoleName = "GAME GEAR"; consoleColor = 0x05B8; break;
            case FT_COLECO: consoleName = "COLECOVISION"; consoleColor = 0xFBC0; break;
            case FT_PCENGINE: consoleName = "PC ENGINE"; consoleColor = 0xF003; break;
            case FT_ATARI_LYNX: consoleName = "ATARI LYNX"; consoleColor = 0xFD20; break;
            case FT_DOOM: consoleName = "DOOM"; consoleColor = 0xF800; break;
            default: consoleName = "ROM"; consoleColor = TFT_WHITE; break;
        }
        
        tft.fillScreen(COLOR_BLACK);
        tft.setTextDatum(MC_DATUM);
        
        tft.setTextColor(consoleColor);
        tft.drawString("[ " + consoleName + " ]", UiLayout::CENTER_X, 30, 2);
        
        tft.setTextColor(TFT_YELLOW);
        tft.drawString(UiLayout::ellipsize(entry.name, 24), UiLayout::CENTER_X, 55, 2);
        
        tft.setTextColor(TFT_SILVER);
        tft.drawString("Ready to play!", UiLayout::CENTER_X, 85, 1);
        tft.drawString("Retro-Go required", UiLayout::CENTER_X, 100, 1);
        
        if (entry.type == FT_NES) {
            tft.setTextColor(TFT_CYAN);
            tft.drawString("Arrows: move   A/B: action", UiLayout::CENTER_X, 116, 1);
            tft.drawString("START/SEL: sys  MENU: pause", UiLayout::CENTER_X, 131, 1);
        }
        
        tft.setTextColor(0x07E0);
        tft.drawString("OK: Start   Back: Return", UiLayout::CENTER_X, 140, 1);
        
        tft.setTextColor(0x18E3);
        tft.drawString("Select: Run Game", UiLayout::CENTER_X, 170, 1);
        tft.drawString("Menu: Options", UiLayout::CENTER_X, 185, 1);
        
        while (true) {
            buttonManager.update();
            if (buttonManager.isJustPressed(KEY_START)) {
                if (entry.type == FT_NES) {
                    runNesEmulator(fullPath);
                } else {
                    runRetroGoEmulator(fullPath, entry.type);
                }
                break;
            }
            if (buttonManager.isJustPressed(KEY_A)) {
                break;
            }
            delay(10);
        }
    }
    else {
        tft.setTextColor(TFT_RED);
        tft.drawString("Unsupported", UiLayout::CENTER_X, 100, 2);
        tft.setTextColor(TFT_SILVER);
        tft.drawString("Cannot run this file", UiLayout::CENTER_X, 130, 1);
        
        delay(1500);
    }
    
    drawList();
}
