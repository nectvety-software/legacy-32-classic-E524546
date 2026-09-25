"""Apply source-level pixel pass without disturbing services or GPIO."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
p=root/'src/core/SymbianUI.cpp'
s=p.read_text()
s=s.replace('#include <WiFi.h>','#include <WiFi.h>\n#include "UiVietnameseFont.h"\n#include "UiTypography.h"\n#include "UiIconCatalog.h"')
s=s.replace('''  return tft.textWidth(text);
}''','''  return UiVietnameseFont::hasUtf8(text.c_str())
    ? UiVietnameseFont::measure(text.c_str(), font == UiTypography::MICRO ? 0 : 1)
    : tft.textWidth(text);
}''',1)
s=s.replace('''  tft.setTextFont(font);
  tft.setTextSize(1);
  tft.setTextColor(fg, bg);
  tft.setCursor(x, y);
  tft.print(text);''','''  if (UiVietnameseFont::hasUtf8(text.c_str())) {
    UiVietnameseFont::draw(tft, x, y, text.c_str(), fg, bg,
                           font == UiTypography::MICRO ? 0 : 1, Board::SCREEN_W - x);
    return;
  }
  tft.setTextFont(font);
  tft.setTextSize(1);
  tft.setTextColor(fg, bg);
  tft.setCursor(x, y);
  tft.print(text);''',1)
# S60 reference selects warm lime when explicitly chosen; default theme remains separate.
s=s.replace('''void SymbianUI::drawS60MenuIcon(int x, int y, const String &kind, uint16_t bg) {''','''void SymbianUI::drawS60MenuIcon(int x, int y, const String &requestedKind, uint16_t bg) {
  const String kind = UiIconCatalog::canonical(requestedKind.c_str());''')
s=s.replace('''  } else if (kind == "Clk") {
    tft.fillCircle(x+18,y+18,13,white);''','''  } else if (kind == "Doc") {
    tft.fillRect(x+9,y+5,23,27,shadow);
    tft.fillRect(x+5,y+3,23,27,white);
    tft.drawRect(x+5,y+3,23,27,silver);
    for(int k=0;k<4;++k) tft.drawFastHLine(x+9,y+10+k*4,14,0x5D9F);
  } else if (kind == "Bell") {
    tft.fillCircle(x+18,y+13,7,0xFDE0);
    tft.fillRect(x+11,y+13,14,10,0xFDE0);
    tft.drawFastHLine(x+8,y+24,20,0xFDE0);
    tft.fillCircle(x+18,y+27,3,white);
  } else if (kind == "Lock") {
    tft.drawCircle(x+18,y+13,7,silver);
    tft.fillRect(x+9,y+14,18,16,0x4208);
    tft.drawRect(x+9,y+14,18,16,silver);
    tft.fillCircle(x+18,y+21,2,white);
  } else if (kind == "Br") {
    tft.fillCircle(x+18,y+18,11,0xFFE0);
    tft.drawFastHLine(x+16,y+2,4,0xFFE0);
    tft.drawFastVLine(x+16,y+31,4,0xFFE0);
    tft.drawFastHLine(x+1,y+16,4,0xFFE0);
    tft.drawFastHLine(x+31,y+16,4,0xFFE0);
  } else if (kind == "i") {
    tft.fillCircle(x+18,y+18,13,0x001F);
    tft.drawCircle(x+18,y+18,13,white);
    tft.fillCircle(x+18,y+11,2,white);
    tft.fillRect(x+17,y+16,3,11,white);
  } else if (kind == "Quick") {
    tft.fillRect(x+5,y+7,26,4,0x04FF);
    tft.fillRect(x+5,y+16,26,4,0x07E0);
    tft.fillRect(x+5,y+25,26,4,0xFFE0);
    tft.fillCircle(x+13,y+9,3,white);tft.fillCircle(x+24,y+18,3,white);
    tft.fillCircle(x+15,y+27,3,white);
  } else if (kind == "Clk") {
    tft.fillCircle(x+18,y+18,13,white);''')
start=s.index('void SymbianUI::drawIcon(')
end=s.index('void SymbianUI::listItem(',start)
# Eliminate duplicated 24x24 legacy icon renderer: same artwork across all screens and skins.
s=s[:start]+'''void SymbianUI::drawIcon(int x, int y, const String &kind, uint16_t color) {
  drawS60MenuIcon(x, y, kind, color);
}

'''+s[end:]
# replace selected focus top bar to screenshot corner streaks; guard row divider when redrawn
s=s.replace('''  tft.fillRect(x, y, w, h, bg);
  if (selected) {
    if (themeId == ThemeId::S60Green) {''','''  tft.fillRect(x, y, w, h, bg);
  // Restore background row seam after a focus-only cell refresh.
  if (row > 0 && !selected && themeId == ThemeId::S60Green)
    tft.drawFastHLine(x, y, w, 0xAEE9);
  if (selected) {
    if (themeId == ThemeId::S60Green) {''')
s=s.replace('''      tft.fillRect(x+3,y+3,w-6,2,0xF7FC);''','''      // Original reference frame: discrete white glints at the top corners.
      tft.fillRect(x+3,y+3,17,2,0xF7FC);
      tft.fillRect(x+w-20,y+3,17,2,0xF7FC);''')
s=s.replace('''  int tw = tft.textWidth(label);
  textBold(x + max(2, (w - tw) / 2), y + 44, label, 1,''','''  int tw = textWidth(label, UiTypography::CAPTION);
  textBold(x + max(2, (w - tw) / 2), y + UiTypography::MENU_CAPTION_Y, label, UiTypography::CAPTION,''')
s=s.replace('''  tft.setTextFont(2);
  tft.setTextSize(1);
  const int textX = (themeId == ThemeId::S60Green || themeId == ThemeId::AmoledRed || themeId == ThemeId::External) ? 48 : 42;''','''  tft.setTextFont(UiTypography::BODY);
  tft.setTextSize(1);
  const int textX = 48;''')
s=s.replace('''  if (ttl.length() > 20) ttl = ttl.substring(0, 19) + "~";
  textBold(textX, y + 3, ttl, 2, labelInk, bg);''','''  ttl = fitTextPixels(ttl, UiTypography::BODY, Board::SCREEN_W - textX - 8);
  textBold(textX, y + 3, ttl, UiTypography::BODY, labelInk, bg);''')
s=s.replace('''    if (line.length() > 30) line = line.substring(0, 29) + "~";
    tft.print(line);''','''    line = fitTextPixels(line, UiTypography::MICRO, Board::SCREEN_W - textX - 8);
    if (UiVietnameseFont::hasUtf8(line.c_str()))
      UiVietnameseFont::draw(tft,textX+1,y+24,line.c_str(),colors.dim,bg,0,Board::SCREEN_W-textX-9);
    else tft.print(line);''')
s=s.replace('''  while (firstLine.length() > 1 && tft.textWidth(firstLine) > 204) {
    firstLine.remove(firstLine.length() - 1);
  }
  tft.print(firstLine);''','''  firstLine = fitTextPixels(firstLine, UiTypography::MICRO, 204);
  if (UiVietnameseFont::hasUtf8(firstLine.c_str()))
    UiVietnameseFont::draw(tft,18,140,firstLine.c_str(),
      themeId==ThemeId::S60Green?TFT_BLACK:colors.text,statusPanel,0,204);
  else tft.print(firstLine);''')
s=s.replace('''  while (clipped.length() > 1 && tft.textWidth(clipped) > 204) {
    clipped.remove(clipped.length() - 1);
  }
  tft.setCursor(18, 140);
  tft.print(clipped);''','''  clipped = fitTextPixels(clipped, UiTypography::MICRO, 204);
  tft.setCursor(18, 140);
  if (UiVietnameseFont::hasUtf8(clipped.c_str()))
    UiVietnameseFont::draw(tft,18,140,clipped.c_str(),ink,panel,0,204);
  else tft.print(clipped);''')
# configure Home icon focus-only rendering and delta method
start=s.index('void SymbianUI::idleShortcuts(')
end=s.index('void SymbianUI::lockScreen(',start)
s=s[:start]+'''void SymbianUI::idleShortcutTile(int i, bool selected) {
  static const char *labels[] = {"WiFi", "Music", "Files"};
  static const char *icons[]  = {"Wi", "Mus", "Dir"};
  if (i < 0 || i >= 3) return;
  const VqeafLayout::Rect r=VqeafLayout::homeShortcut(i);
  const uint16_t bg=selected ? colors.selected : colors.panel;
  tft.fillRect(r.x,r.y,r.w,r.h,bg);
  tft.drawRect(r.x,r.y,r.w,r.h,selected?colors.border:colors.dim);
  if (selected) {
    tft.drawFastHLine(r.x+2,r.y+2,17, colors.chrome);
    tft.drawFastHLine(r.x+r.w-19,r.y+2,17,colors.chrome);
  }
  drawIcon(r.x+18,r.y+6,icons[i],bg);
  tft.setTextSize(1);
  const uint16_t fg=selected?selectedInk:colors.text;
  const int w=textWidth(labels[i],UiTypography::CAPTION);
  textBold(r.x+(r.w-w)/2,r.y+47,labels[i],UiTypography::CAPTION,fg,bg);
}
void SymbianUI::idleShortcuts(int shortcutIndex) {
  for(int i=0;i<3;++i)idleShortcutTile(i,i==shortcutIndex);
  const VqeafLayout::Rect h=VqeafLayout::HOME_HINT;
  const uint16_t hintBg=themeId==ThemeId::S60Green?0x4BE5:
    (themeId==ThemeId::External?colors.chrome:0x1082);
  tft.fillRect(h.x,h.y,h.w,h.h,hintBg);
  tft.setTextFont(UiTypography::MICRO);
  tft.setTextColor(themeId==ThemeId::S60Green?TFT_WHITE:colors.chromeText,hintBg);
  tft.setCursor(h.x+5,UiTypography::HOME_HINT_Y);
  tft.print("Hold MENU: tasks  Hold OPT: settings");
}
void SymbianUI::idleShortcutDelta(int previous, int next) {
  if(previous==next)return;
  idleShortcutTile(previous,false);
  idleShortcutTile(next,true);
}

'''+s[end:]
s=s.replace('''  tft.setCursor(4, 5); tft.print("General");''','''  textBold(UiTypography::HEADER_TITLE_X,UiTypography::HEADER_TITLE_Y,"General",
           UiTypography::TITLE,colors.chromeText,colors.chrome,true);''')
s=s.replace('''  textBold(4, 5, cut, 2, ink, bar, true);''','''  textBold(UiTypography::HEADER_TITLE_X,UiTypography::HEADER_TITLE_Y,cut,
           UiTypography::TITLE,ink,bar,true);''')
# Remove UTF-8 breakage from common clip path; called in Home/list/menu/chrome.
pos=s.index('String SymbianUI::timeText(')
s=s[:pos]+'''String SymbianUI::fitTextPixels(const String &label,uint8_t font,int maxPx) {
  if(maxPx<=0)return String();
  if(textWidth(label,font)<=maxPx)return label;
  String cut=label;
  while(cut.length() && textWidth(cut+"~",font)>maxPx) {
    cut.remove(cut.length()-1);
    // Never leave an incomplete UTF-8 sequence at the new string boundary.
    while(cut.length() && (((uint8_t)cut[cut.length()-1]&0xC0)==0x80))
      cut.remove(cut.length()-1);
  }
  return cut.length()?cut+"~":String();
}

'''+s[pos:]
# Better label middle truncation: raw truncate by bytes can cut multibyte. replace.
s=s.replace('''  if (label.length() > 11) label = label.substring(0, 10) + "~";''','''  label = fitTextPixels(label, UiTypography::CAPTION, w - 6);''')
p.write_text(s)
p=root/'src/core/SymbianUI.h';s=p.read_text();s=s.replace('''  void idleShortcuts(int shortcutIndex);''','''  void idleShortcuts(int shortcutIndex);
  void idleShortcutDelta(int previous, int next); // only two affected Home tiles''');s=s.replace('''  int textWidth(const String &text, uint8_t font = 1);''','''  int textWidth(const String &text, uint8_t font = 1);
  String fitTextPixels(const String &label, uint8_t font, int maxPx);''');s=s.replace('''  void drawWallpaper();''','''  void drawWallpaper();
  void idleShortcutTile(int i,bool selected);''');p.write_text(s)
p=root/'src/main.cpp';s=p.read_text();s=s.replace('''if (e.key == Key::Left) { idleShortcut = (idleShortcut + 2) % 3; ui.idleShortcuts(idleShortcut); }''','''if (e.key == Key::Left) { const int old=idleShortcut; idleShortcut = (idleShortcut + 2) % 3; ui.idleShortcutDelta(old,idleShortcut); }''').replace('''else if (e.key == Key::Right) { idleShortcut = (idleShortcut + 1) % 3; ui.idleShortcuts(idleShortcut); }''','''else if (e.key == Key::Right) { const int old=idleShortcut; idleShortcut = (idleShortcut + 1) % 3; ui.idleShortcutDelta(old,idleShortcut); }''');p.write_text(s)
p=root/'src/apps/LauncherGrid.cpp';s=p.read_text().replace('#include "BoardConfig.h"','#include "BoardConfig.h"\n#include "../core/UiIconCatalog.h"');s=s.replace('''static const char *launcherIcon[]  = {"Wi","BLE","Mus","Dir","Pic","Web","Term","Rec","Set","Th","App","Col"};''','''static const char *const (&launcherIcon)[12] = UiIconCatalog::GRID_IDS;''');p.write_text(s)
# New first-install visual default: preserve all existing user choices.
p=root/'src/services/SettingsStore.cpp';s=p.read_text().replace('''// One-time first-install default is VQEAF Night; existing choices survive.''','''// First-install follows the user's screenshot-style Lime reference.
  // Existing v2.x settings remain untouched (including user-selected Night).''').replace('''static_cast<uint8_t>(ThemeId::Classic));''','''static_cast<uint8_t>(ThemeId::S60Green));''',1).replace('''    cfg.theme = ThemeId::Classic;
    prefs.putUChar("theme", static_cast<uint8_t>(cfg.theme));
    prefs.putUChar("themeRev", 2);''','''    cfg.theme = ThemeId::S60Green;
    prefs.putUChar("theme", static_cast<uint8_t>(cfg.theme));
    prefs.putUChar("themeRev", 3);''',1);p.write_text(s)
p=root/'src/services/SettingsStore.h';s=p.read_text().replace('''  ThemeId theme = ThemeId::Classic;''','''  ThemeId theme = ThemeId::S60Green;''');p.write_text(s)
print('Patched UI + Home focus + font + icon normalization + first-install Lime')
