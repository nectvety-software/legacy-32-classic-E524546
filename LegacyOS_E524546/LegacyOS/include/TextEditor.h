#pragma once
#include <Arduino.h>
#include <SD.h>
#include "UITheme.h"

// ═══════════════════════════════════════════════════════════
//  TextEditor - Basic text editor with virtual D-pad keyboard
// ═══════════════════════════════════════════════════════════

namespace AppTextEditor {
  static String   content = "";
  static String   filename = "untitled.txt";
  static int      cursorPos = 0;
  static int      scrollLine = 0;
  static bool     modified = false;
  static bool     showKeyboard = false;
  static int      kbRow = 0, kbCol = 0;
  static bool     kbShift = false;
  
  // Virtual keyboard layout
  static const char* kbRows[] = {
    "1234567890-=",
    "qwertyuiop[]",
    "asdfghjkl;'",
    "zxcvbnm,./"
  };
  static const char* kbRowsShift[] = {
    "!@#$%^&*()_+",
    "QWERTYUIOP{}",
    "ASDFGHJKL:\"",
    "ZXCVBNM<>?"
  };
  
  std::vector<String> _getLines() {
    std::vector<String> lines;
    String cur = "";
    for (char c : content) {
      if (c == '\n') { lines.push_back(cur); cur = ""; }
      else cur += c;
    }
    lines.push_back(cur);
    return lines;
  }
  
  void renderEditor(AppContext& ctx) {
    auto* spr = ctx.display->getContent();
    spr->fillSprite(CLR_BG);
    
    // Title bar
    spr->fillRect(0, 0, 240, 18, CLR_GRAY3);
    spr->setTextColor(CLR_WHITE); spr->setTextSize(1); spr->setTextDatum(ML_DATUM);
    spr->drawString((modified ? "* " : "  ") + filename, 4, 10);
    spr->setTextDatum(MR_DATUM);
    spr->setTextColor(CLR_GRAY2);
    spr->drawString(String(content.length()) + "b", 234, 10);
    
    // Text area
    spr->fillRect(0, 19, 240, 230, CLR_BLACK);
    auto lines = _getLines();
    
    // Find cursor line/col
    int cLine = 0, cCol = 0;
    int pos = 0;
    for (int i = 0; i < (int)content.length() && pos < cursorPos; i++, pos++) {
      if (content[i] == '\n') { cLine++; cCol = 0; }
      else cCol++;
    }
    
    // Adjust scroll
    if (cLine < scrollLine) scrollLine = cLine;
    if (cLine >= scrollLine + 12) scrollLine = cLine - 11;
    
    // Render visible lines
    spr->setTextColor(CLR_WHITE); spr->setTextSize(1); spr->setTextDatum(ML_DATUM);
    for (int l = 0; l < 12 && (l + scrollLine) < (int)lines.size(); l++) {
      int ly = 22 + l * 18;
      int lineNum = l + scrollLine;
      
      // Line numbers
      char ln[5]; snprintf(ln, 5, "%3d", lineNum+1);
      spr->setTextColor(CLR_GRAY3);
      spr->drawString(ln, 2, ly);
      
      // Line content
      spr->setTextColor(CLR_WHITE);
      spr->drawString(lines[lineNum], 26, ly);
      
      // Cursor
      if (lineNum == cLine) {
        int cx = 26 + cCol * 6;
        static bool curBlink = false;
        static uint32_t bt = 0;
        if (millis() - bt > 400) { curBlink = !curBlink; bt = millis(); }
        if (curBlink) spr->drawLine(cx, ly-1, cx, ly+10, CLR_PRIMARY);
      }
    }
    
    // Line/col indicator
    spr->fillRect(0, 250, 240, 14, CLR_BG2);
    char posStr[24];
    snprintf(posStr, 24, "Ln:%d Col:%d | %d lines", cLine+1, cCol+1, (int)lines.size());
    spr->setTextColor(CLR_GRAY2); spr->setTextDatum(ML_DATUM);
    spr->drawString(posStr, 4, 257);
    
    // Help
    spr->fillRect(0, 265, 240, 35, CLR_BG2);
    spr->setTextColor(CLR_GRAY2); spr->setTextDatum(MC_DATUM);
    spr->drawString("SELECT:Keyboard  OPTION:Save", 120, 272);
    spr->drawString("START:Enter  B:Del  A:Exit", 120, 284);
    
    ctx.display->pushContent();
  }
  
  void renderKeyboard(AppContext& ctx) {
    auto* spr = ctx.display->getContent();
    // Show text area (top half)
    spr->fillRect(0, 0, 240, 130, CLR_BLACK);
    auto lines = _getLines();
    spr->setTextColor(CLR_WHITE); spr->setTextSize(1); spr->setTextDatum(ML_DATUM);
    for (int l = 0; l < 7 && (l + scrollLine) < (int)lines.size(); l++) {
      spr->drawString(lines[l + scrollLine], 2, 4 + l * 18);
    }
    
    // Keyboard (bottom half)
    spr->fillRect(0, 130, 240, 170, CLR_BG2);
    spr->drawLine(0, 130, 240, 130, CLR_GRAY3);
    
    const char** rows = kbShift ? kbRowsShift : kbRows;
    
    for (int r = 0; r < 4; r++) {
      int rowLen = strlen(rows[r]);
      int startX = (240 - rowLen * 18) / 2;
      
      for (int c = 0; c < rowLen; c++) {
        int x = startX + c * 18;
        int y = 135 + r * 34;
        bool sel = (r == kbRow && c == kbCol);
        
        spr->fillRoundRect(x, y, 16, 28, 3, sel ? CLR_PRIMARY : CLR_GRAY3);
        spr->setTextColor(CLR_WHITE); spr->setTextDatum(MC_DATUM);
        char ch[2] = {rows[r][c], 0};
        spr->drawString(ch, x+8, y+14);
      }
    }
    
    // Special keys
    spr->fillRoundRect(5,  264, 40, 22, 3, kbShift ? CLR_PRIMARY : CLR_GRAY3);
    spr->fillRoundRect(50, 264, 60, 22, 3, CLR_GRAY3);
    spr->fillRoundRect(115,264, 40, 22, 3, CLR_GRAY3);
    spr->fillRoundRect(160,264, 35, 22, 3, CLR_GRAY3);
    spr->fillRoundRect(200,264, 35, 22, 3, CLR_APP_RED);
    
    spr->setTextColor(CLR_WHITE); spr->setTextDatum(MC_DATUM);
    spr->drawString("SHF", 25, 274);
    spr->drawString("SPACE", 80, 274);
    spr->drawString("ENT", 135, 274);
    spr->drawString("DEL", 177, 274);
    spr->drawString("ESC", 217, 274);
    
    ctx.display->pushContent();
  }
  
  void handleKeyboardInput(AppContext& ctx) {
    const char** rows = kbShift ? kbRowsShift : kbRows;
    int rowLen = strlen(rows[kbRow]);
    
    if (ctx.input->up())    { if (kbRow > 0) { kbRow--; kbCol = min(kbCol, (int)strlen(rows[kbRow])-1); }}
    if (ctx.input->down())  { if (kbRow < 3) { kbRow++; kbCol = min(kbCol, (int)strlen(rows[kbRow])-1); }}
    if (ctx.input->left())  { if (kbCol > 0) kbCol--; }
    if (ctx.input->right()) { if (kbCol < rowLen-1) kbCol++; }
    
    if (ctx.input->start()) {
      // Insert character
      char ch = rows[kbRow][kbCol];
      content = content.substring(0, cursorPos) + ch + content.substring(cursorPos);
      cursorPos++;
      modified = true;
    }
    if (ctx.input->menu()) { // Space
      content = content.substring(0, cursorPos) + " " + content.substring(cursorPos);
      cursorPos++; modified = true;
    }
    if (ctx.input->option()) { // Enter
      content = content.substring(0, cursorPos) + "\n" + content.substring(cursorPos);
      cursorPos++; modified = true;
    }
    if (ctx.input->b()) { // Delete
      if (cursorPos > 0) {
        content = content.substring(0, cursorPos-1) + content.substring(cursorPos);
        cursorPos--; modified = true;
      }
    }
    if (ctx.input->a())  { kbShift = !kbShift; }
    if (ctx.input->select()) { showKeyboard = false; }
  }
  
  void handleEditorInput(AppContext& ctx) {
    if (ctx.input->left())  { if (cursorPos > 0) cursorPos--; }
    if (ctx.input->right()) { if (cursorPos < (int)content.length()) cursorPos++; }
    if (ctx.input->up()) {
      // Move cursor up one line
      int nl = content.lastIndexOf('\n', max(0, cursorPos-1));
      if (nl >= 0) cursorPos = max(0, nl - 1);
    }
    if (ctx.input->down()) {
      int nl = content.indexOf('\n', cursorPos);
      if (nl >= 0) cursorPos = min((int)content.length(), nl + 1);
    }
    if (ctx.input->select()) { showKeyboard = true; kbRow = 1; kbCol = 0; }
    if (ctx.input->b()) { // Backspace
      if (cursorPos > 0) {
        content = content.substring(0, cursorPos-1) + content.substring(cursorPos);
        cursorPos--; modified = true;
      }
    }
    if (ctx.input->option()) { // Save
      if (ctx.fs->sdMounted) {
        ctx.fs->writeFile("/saves/" + filename, content);
        modified = false;
        ctx.display->showToast("Saved: " + filename);
      } else {
        ctx.display->showToast("No SD card!");
      }
    }
    if (ctx.input->b()) { ctx.os->setState(OSState::HOME_SCREEN); }
  }
  
  void start(AppContext& ctx) {
    content = ""; cursorPos = 0; scrollLine = 0;
    modified = false; showKeyboard = false;
    filename = "untitled.txt";
    kbRow = 1; kbCol = 0; kbShift = false;
  }
  
  void loop(AppContext& ctx) {
    if (showKeyboard) {
      renderKeyboard(ctx);
      handleKeyboardInput(ctx);
    } else {
      renderEditor(ctx);
      handleEditorInput(ctx);
    }
  }
}
