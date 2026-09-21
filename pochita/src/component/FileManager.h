#ifndef FILE_MANAGER_H
#define FILE_MANAGER_H

// #include "../Shared.h" // Removed
#include "Config.h"
#include "UIManager.h"
#include "ButtonManager.h"
#include "Keyboard.h"
#include <PNGdec.h>
#include <SD.h>
#include <TJpg_Decoder.h>
#include <ff.h>
#include <dirent.h>

enum FileType { FT_FILE, FT_FOLDER, FT_SYSTEM, FT_IMAGE, FT_ROM, FT_SCRIPT, FT_TEXT, FT_LUA, FT_PYTHON, FT_HTML, FT_JSON, 
                 FT_NES, FT_SNES, FT_GB, FT_GBC, FT_GBA, FT_SMS, FT_SG1000, FT_MD, FT_GG, FT_COLECO, FT_PCENGINE, FT_ATARI_LYNX, FT_DOOM,
                 FT_MODEL };

enum ViewerMode { VM_LIST, VM_TEXT, VM_IMAGE, VM_MESSAGE, VM_PROPERTIES, VM_INSTALLING,
                  VM_FORMAT_MENU, VM_FORMAT_CONFIRM, VM_FORMAT_RESULT };

struct FileEntry {
  String name;
  FileType type;
  size_t size;
};

class FileManager {
public:
  void begin() { 
      currentPath = "/"; 
      
      // Initialize ButtonManager if not already done
      buttonManager.begin();

      // The SD card is already mounted at boot and is read successfully by the
      // rest of the system (ROM loading, settings, MUSIC). Prefer that live
      // mount. NOTE: the card root must be probed through the POSIX mount point
      // "/sd" -- SD.open("/") maps to "/sd/" whose opendir() fails on this FAT
      // VFS, which is exactly what made FILES show an empty list while every
      // other subsystem still worked.
      bool live = false;
      if (SD.cardType() != CARD_NONE) {
          DIR *root = opendir("/sd");
          if (root) { closedir(root); live = true; }
      }

      if (live) {
          Serial.println("SD: using live boot mount in FileManager");
          sdMounted = true;
          statusMessage = "";
          ensureDirectories();
          listFiles();
          return;
      }

      // Live mount unusable -> fall back to a full multi-speed remount. The
      // manual "[SYS] Refresh / Remount SD" entry and hot-plug detection still
      // force this path when the card is swapped or edited on a PC.
      if (refreshStorage(false)) {
          Serial.println("SD Card ready in FileManager (remounted)");
          sdMounted = true;
          statusMessage = "";
          ensureDirectories();
          listFiles();
      } else {
          Serial.println("SD Card not found in FileManager::begin()");
          sdMounted = false;
          statusMessage = "No SD Card";
      }
  }
  
  bool ensureDirectories();

  void update(bool &exitToHome);
  void drawList();
  void drawListRow(int rowIndex, int idx, bool sel);
  void drawOptionMenu();

  // Viewer Methods
  void drawTextViewer();
  void drawImageViewer();
  void drawProperties();
  void drawMessage();

  // Action Methods Declared Here
  void openSelected();
  void renameSelected();
  void copySelected();
  void cutSelected();
  void pasteToCurrent();
  void deleteSelected();
  void runSelectedFile(); // New
  void launchPath(const String &path);
  void openDirectory(const String &path);
  void editSelected();  // Edit a text file's contents on-device

private:
  String currentPath = "/";
  std::vector<FileEntry> fileList;
  int selectedIndex = 0;
  int scrollOffset = 0;

  // UI States
  ViewerMode viewerMode = VM_LIST;
  String statusMessage = "";

  // Clipboard
  String clipboardPath = "";
  bool isCutOperation = false;

  // Text Viewer
  String textBuffer;
  int textScrollY = 0;
  String currentFileView = "";
  FileType currentViewType = FT_FILE;

  // Options Menu
  bool showOptions = false;
  int optionIndex = 0;
  bool isCreatingFolder = false;
  bool isRenaming = false;
  bool isCreatingFile = false;   // naming a brand-new text file
  bool isEditingText = false;    // typing/saving a text file's contents
  String newFolderName = "";
  String editBuffer = "";        // working copy of the edited file contents
  String editTargetPath = "";    // SD path being written on save
  Keyboard keyboard;

  bool formatArmed = false;
  bool formatSuccess = false;
  bool formatRepartition = false;
  int formatMenuIndex = 0;
  uint32_t formatDeletedItems = 0;
  String formatStatus = "Ready";

  void listFiles();
  void checkSD();
  bool refreshStorage(bool showResult = false);
  bool sdMounted = true;
  // Removed duplicate private declarations of actions
  void recursiveDelete(const char *path);
  bool recursiveCopy(const String &source, const String &destination);
  bool validEntryName(const String &name) const;
  bool writeTextFile(const String &path, const String &content);
  FileType identifyType(String filename);
  String getIcon(FileType type);

  // Image Helpers
  void drawBmp(String filename);
  void drawPng(String filename);
  void drawFormatConfirm();
  void drawFormatMenu();
  void drawFormatProgress();
  void drawFormatResult();
  bool removeStorageTree(const String &path, uint8_t depth = 0);
  bool formatStorage();
  bool repartitionFat32();
};

extern FileManager fileManager;

#endif
