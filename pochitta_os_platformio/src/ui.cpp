#include "ui.h"
#include "board_config.h"

static const uint16_t BG=TFT_BLACK, FG=TFT_WHITE, CYAN=0x05FF, BLUE=0x249F, DIM=0x7BEF;
void UI::begin(){
  pinMode(TFT_BL,OUTPUT); digitalWrite(TFT_BL,LOW);
  delay(50);
  digitalWrite(TFT_BL,HIGH);
  delay(200);
  Serial.println("[UI] TFT init...");
  tft_.init();
  Serial.println("[UI] TFT init done");
  tft_.setRotation(DISPLAY_ROTATION); tft_.fillScreen(BG);
  tft_.setTextDatum(TL_DATUM); tft_.setTextFont(2);
  Serial.println("[UI] TFT ready");
}
void UI::battery(){
  int x=SCREEN_W-38,y=7; tft_.drawRect(x,y,28,11,FG); tft_.fillRect(x+28,y+3,3,5,FG);
  tft_.fillRect(x+2,y+2,18,7,TFT_GREEN);
}
void UI::header(const String& title){
  tft_.fillRect(0,0,SCREEN_W,52,BG); tft_.drawFastHLine(0,27,SCREEN_W,BLUE);
  tft_.setTextColor(CYAN,BG); tft_.drawString("WiFi " DEVICE_NAME,6,6,2);
  tft_.setTextColor(FG,BG); tft_.drawCentreString("12:00",SCREEN_W/2,6,2); tft_.drawRightString("65%",SCREEN_W-43,6,2); battery();
  tft_.setTextColor(CYAN,BG); tft_.drawCentreString(title,SCREEN_W/2,32,2);
}
void UI::footer(const String& left,const String& right){
  int y=SCREEN_H-31; tft_.fillRect(0,y,SCREEN_W,31,BG); tft_.drawFastHLine(0,y,SCREEN_W,BLUE);
  tft_.drawFastVLine(80,y,31,BLUE); tft_.drawFastVLine(160,y,31,BLUE);
  tft_.setTextColor(CYAN,BG); tft_.drawCentreString(left,40,y+8,2); tft_.drawCentreString(right,200,y+8,2);
}
void UI::menu(const String& title,const std::vector<MenuItem>& items,int selected,int scroll){
  tft_.fillScreen(BG); header(title); const int y0=58,rowH=42,maxRows=(SCREEN_H-y0-34)/rowH;
  for(int r=0;r<maxRows;r++){ int i=scroll+r; if(i>=(int)items.size()) break; int y=y0+r*rowH;
    if(i==selected){tft_.fillRect(5,y,SCREEN_W-10,rowH-4,BLUE);tft_.setTextColor(FG,BLUE);} else tft_.setTextColor(FG,BG);
    tft_.drawString(items[i].label,18,y+10,2);
    if(items[i].value.length()) tft_.drawRightString(items[i].value,SCREEN_W-22,y+10,2);
    else if(items[i].arrow) tft_.drawString(">",SCREEN_W-22,y+10,2);
  }
  footer();
}
void UI::info(const String& title,const std::vector<MenuItem>& rows){
  tft_.fillScreen(BG);header(title);int y=62;for(auto&r:rows){tft_.setTextColor(FG,BG);tft_.drawString(r.label,10,y,2);tft_.drawRightString(r.value,SCREEN_W-10,y,2);y+=30;}footer("","Back");
}
void UI::message(const String& title,const String& text){
  tft_.fillScreen(BG);header(title);tft_.setTextColor(FG,BG);tft_.setTextWrap(true);tft_.drawString(text,10,70,2);footer("OK","Back");
}
