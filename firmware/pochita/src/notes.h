#ifndef NOTES_H
#define NOTES_H

#include "component/Display.h"
#include "component/Config.h"
#include "component/Keyboard.h"
#include "component/ui_utils.h"
#include <FS.h>
#include <FS.h>

void drawNotesApp();
void loopNotes();

bool noteMenuOpen = false;
int noteMenuIndex = 0;
String currentNoteFile = "";
String noteContent = "Hello Pochita!_";
bool noteModified = false;
bool isSymbolsMode = false;

void drawNoteBody() {
    tft.fillRect(0, SymbianUI::HEADER_H, UiLayout::WIDTH,
                 UiLayout::FOOTER_Y - SymbianUI::HEADER_H, TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextDatum(TL_DATUM);
    tft.setTextWrap(true, true);
    tft.setCursor(UiLayout::MARGIN, SymbianUI::HEADER_H + 7);
    String visible = noteContent;
    if (visible.endsWith("__")) visible.remove(visible.length() - 1);
    tft.print(visible);
    tft.setTextWrap(false, false);
}

void openNoteFromSD(String path);
void saveNoteAs();

void saveNote(String path) {
    if (!path.startsWith("/")) path = "/" + path;
    
    if (!path.startsWith("/notes/")) {
        if (path.startsWith("/")) path = "/notes" + path; 
        else path = "/notes/" + path;
    }

    File f = SD.open(path, FILE_WRITE);
    if (f) {
        String content = noteContent;
        if (content.endsWith("_")) content.remove(content.length()-1);
        f.print(content);
        f.close();
        tft.fillRect(20, 100, 200, 40, 0x2104);
        tft.setTextColor(TFT_GREEN);
        tft.drawCentreString("SAVED!", UiLayout::CENTER_X, 120, 2);
        currentNoteFile = path;
        noteModified = false;
        delay(800);
    } else {
        tft.fillRect(20, 100, 200, 40, TFT_RED);
        tft.setTextColor(TFT_WHITE);
        tft.drawCentreString("SAVE FAILED", UiLayout::CENTER_X, 120, 2);
        delay(800);
    }
}

void openNoteFromSD(String path) {
    File f = SD.open(path);
    if (f) {
        noteContent = "";
        while (f.available()) {
            char c = f.read();
            if (c == '\n') noteContent += "\r\n";
            else noteContent += c;
        }
        f.close();
        noteContent += "_";
        currentNoteFile = path;
        noteModified = false;
    } else {
        tft.fillRect(20, 100, 200, 40, TFT_RED);
        tft.setTextColor(TFT_WHITE);
        tft.drawCentreString("OPEN FAILED", UiLayout::CENTER_X, 120, 2);
        delay(800);
    }
}

void listNotesForOpen() {
    File root = SD.open("/notes");
    std::vector<String> noteFiles;
    
    if (root && root.isDirectory()) {
        File file = root.openNextFile();
        while (file) {
            String name = file.name();
            if (!name.startsWith(".") && !file.isDirectory()) {
                noteFiles.push_back("/notes/" + name);
            }
            file = root.openNextFile();
        }
    }
    
    if (noteFiles.empty()) {
        SymbianUI::drawMessageScreen("Open note", SymbianUI::ICON_FILE,
                                     "No notes found", "Create a note first");
        delay(1000);
        return;
    }
    
    int selected = 0;
    int scrollOffset = 0;
    bool selecting = true;
    
    SymbianUI::drawChrome("Open note", "Open", "Cancel");
    
    for (int i = 0; i < 6; i++) {
        int idx = i + scrollOffset;
        if (idx >= noteFiles.size()) break;
        String fname = noteFiles[idx];
        int lastSlash = fname.lastIndexOf('/');
        if (lastSlash != -1) fname = fname.substring(lastSlash + 1);
        SymbianUI::drawListRow(60 + i * 36, 34, fname,
                               idx == selected, "", SymbianUI::ICON_FILE);
    }
    
    while (selecting) {
        buttonManager.update();
        
        int oldSel = selected;
        int oldScroll = scrollOffset;
        
        if (buttonManager.isJustPressed(KEY_UP)) {
            selected--;
            if (selected < 0) selected = noteFiles.size() - 1;
            if (selected < scrollOffset) scrollOffset = selected;
        }
        if (buttonManager.isJustPressed(KEY_DOWN)) {
            selected++;
            if (selected >= noteFiles.size()) selected = 0;
            if (selected >= scrollOffset + 6) scrollOffset = selected - 5;
        }
        
        if (selected != oldSel || scrollOffset != oldScroll) {
            if (scrollOffset != oldScroll) {
                for (int i = 0; i < 6; i++) {
                    int idx = i + scrollOffset;
                    if (idx >= noteFiles.size()) {
                        tft.fillRect(4, 60 + i * 36, UiLayout::WIDTH - 8, 34, SymbianUI::BG);
                        continue;
                    }
                    String fname = noteFiles[idx];
                    int lastSlash = fname.lastIndexOf('/');
                    if (lastSlash != -1) fname = fname.substring(lastSlash + 1);
                    SymbianUI::drawListRow(60 + i * 36, 34, fname,
                                           idx == selected, "", SymbianUI::ICON_FILE);
                }
            } else {
                auto drawSelRow = [&](int idx) {
                    int row = idx - scrollOffset;
                    if (row < 0 || row >= 6) return;
                    String fname = noteFiles[idx];
                    int lastSlash = fname.lastIndexOf('/');
                    if (lastSlash != -1) fname = fname.substring(lastSlash + 1);
                    SymbianUI::drawListRow(60 + row * 36, 34, fname,
                                           idx == selected, "", SymbianUI::ICON_FILE);
                };
                drawSelRow(oldSel);
                drawSelRow(selected);
            }
        }
        
        if (buttonManager.isJustPressed(KEY_START)) {
            openNoteFromSD(noteFiles[selected]);
            selecting = false;
        }
        
        if (buttonManager.isJustPressed(KEY_A)) {
            selecting = false;
        }
        
        delay(50);
    }
}

void saveNoteAs() {
    String filename = "";
    keyboard.begin();
    keyboard.active = true;
    keyboard.draw(true);
    
    bool gettingName = true;
    while (gettingName) {
        buttonManager.update();
        int result = keyboard.handleInput(filename);
        
        if (result == 1) {
            if (filename.length() > 0) {
                String path = filename;
                if (!path.endsWith(".txt")) path += ".txt";
                saveNote(path);
                gettingName = false;
            }
        } else if (result == 2) {
            gettingName = false;
        }
    }
    
    keyboard.active = false;
    drawNotesApp();
}

extern void drawNotesApp();

void drawNotesApp() {
  SymbianUI::drawChrome("Notes", "Edit", "Options");
  drawNoteBody();
  
  if (!keyboard.active && !noteMenuOpen) {
      tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
      tft.setTextDatum(ML_DATUM);
      SymbianUI::drawSoftkeys("Edit", "Options");
      
      if (noteModified) {
          tft.setTextColor(TFT_YELLOW, TFT_BLACK);
          tft.setTextDatum(MR_DATUM);
          tft.drawString("*", 205, 11, 2);
      }
  }
  
  if (noteMenuOpen) {
      SymbianUI::drawChrome("Note options", "Select", "Back");
      const char* items[] = {"New", "Open", "Save", "Save As", "Close"};
      for (int i=0; i<5; i++) {
          SymbianUI::drawListRow(66 + i * 42, 36, items[i],
                                 i == noteMenuIndex);
      }
  }
}

void loopNotes() {
    if (noteMenuOpen) {
        if (buttonManager.isJustPressed(KEY_UP)) { noteMenuIndex--; if (noteMenuIndex < 0) noteMenuIndex = 4; drawNotesApp(); }
        if (buttonManager.isJustPressed(KEY_DOWN)) { noteMenuIndex++; if (noteMenuIndex > 4) noteMenuIndex = 0; drawNotesApp(); }
        
        if (buttonManager.isJustPressed(KEY_START)) {
            if (noteMenuIndex == 0) {
                noteContent = "_";
                currentNoteFile = "";
                noteModified = false;
                noteMenuOpen = false;
                drawNotesApp();
            }
            else if (noteMenuIndex == 1) {
                noteMenuOpen = false;
                listNotesForOpen();
                drawNotesApp();
            }
            else if (noteMenuIndex == 2) {
                if (currentNoteFile != "") {
                    saveNote(currentNoteFile);
                } else {
                    saveNoteAs();
                }
                noteMenuOpen = false;
                drawNotesApp();
            }
            else if (noteMenuIndex == 3) {
                noteMenuOpen = false;
                saveNoteAs();
            }
            else if (noteMenuIndex == 4) {
                noteMenuOpen = false;
                currentMode = MODE_LAUNCHER;
                drawLauncherContent();
                return;
            }
        }
        
        if (buttonManager.isJustPressed(KEY_A)) {
            noteMenuOpen = false;
            drawNotesApp();
        }
        return;
    }

    if (keyboard.active) {
        int result = keyboard.handleInput(noteContent);
        if (result == 1 || result == 2) {
            keyboard.active = false;
            noteModified = true;
            if (result == 1 && noteContent.length() > 0) {
                if (!noteContent.endsWith("_")) noteContent += "_";
            }
            drawNotesApp();
        } else if (result == 0) {
            noteModified = true;
        }
        return;
    }

  if (buttonManager.isJustPressed(KEY_OPTION) || digitalRead(KEY_OPTION) == LOW) {
      noteMenuOpen = !noteMenuOpen;
      drawNotesApp();
      delay(300);
  }
  
  if (digitalRead(KEY_OPTION) == LOW) { 
    if (noteContent.length() > 0) {
      if (noteContent.endsWith("_")) noteContent.remove(noteContent.length()-1);
      if (noteContent.length() > 0) noteContent.remove(noteContent.length() - 1);
      noteContent += "_";
      noteModified = true;
      
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      tft.setTextDatum(TL_DATUM);
      drawNoteBody();
      delay(150);
    }
  }
  
  if (isSelectPressed()) {
      keyboard.begin();
      keyboard.active = true;
      keyboard.draw(true);
      delay(300); 
  }
}

#endif
