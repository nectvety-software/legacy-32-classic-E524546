#include "LauncherView.h"
#include <string.h>
#include "../core/UiVietnameseFont.h"

void LauncherView::clipped(const char *txt,int x,int y,int maxWidth,
                           uint16_t fg,uint16_t bg,int font,bool bold) {
  if(!txt||maxWidth<4)return;
  tft.setTextSize(1);tft.setTextFont(font);tft.setTextColor(fg,bg);
  auto width=[&](const String &s)->int {
    return UiVietnameseFont::hasUtf8(s.c_str())
      ? UiVietnameseFont::measure(s.c_str(),font==1?0:1)
      : tft.textWidth(s);
  };
  String s(txt);
  const size_t full=s.length();
  while(s.length() && width(s)>maxWidth) {
    size_t first=s.length()-1;
    while(first>0 && (((uint8_t)s[first]&0xC0)==0x80))--first;
    s.remove(first);
  }
  if(full>s.length() && s.length()>3) {
    while(s.length() && width(s+"~")>maxWidth) {
      size_t first=s.length()-1;
      while(first>0 && (((uint8_t)s[first]&0xC0)==0x80))--first;
      s.remove(first);
    }
    s+="~";
  }
  if(UiVietnameseFont::hasUtf8(s.c_str()))
    UiVietnameseFont::draw(tft,x,y,s.c_str(),fg,bg,font==1?0:1,maxWidth);
  else {
    tft.setCursor(x,y);tft.print(s);
    if(bold){tft.setTextColor(fg);tft.setCursor(x+1,y);tft.print(s);}
  }
}

void LauncherView::icon(const char *label,int x,int y,int size,
                        const LauncherStyle &skin,bool selected) {
  const uint16_t bg=selected?skin.tabAccent:skin.headerBg;
  const uint16_t fg=LauncherStyle::legibleOn(bg);
  tft.fillRect(x,y,size,size,bg);
  tft.drawRect(x,y,size,size,skin.border);
  tft.drawFastHLine(x+3,y+3,size-6,fg);
  char shortName[3]={'V','Q',0};
  if(label&&*label){shortName[0]=label[0];shortName[1]=label[1]?label[1]:0;}
  tft.setTextFont(2);tft.setTextSize(1);
  int textW=tft.textWidth(shortName);
  clipped(shortName,x+(size-textW)/2,y+(size-13)/2,size-4,fg,bg,2,true);
}

void LauncherView::status(const LauncherStyle &s,const String &tm,bool wifi,bool sd) {
  tft.fillRect(0,0,W,STATUS_H,s.headerBg);
  clipped("VQEAF OS",6,6,79,s.headerFg,s.headerBg,1,true);
  const int clockW=tft.textWidth(tm);
  clipped(tm.c_str(),(W-clockW)/2,7,57,s.headerFg,s.headerBg,1);
  if(wifi){ tft.fillRect(181,9,3,5,s.tabAccent);tft.fillRect(185,6,3,8,s.tabAccent);
            tft.fillRect(189,3,3,11,s.tabAccent); }
  else tft.drawLine(181,6,191,14,s.headerFg);
  if(sd)clipped("SD",200,7,18,s.headerFg,s.headerBg,1);
  // Battery charge is unavailable (no ADC defined in E524546 BOM).
  // Only outline is drawn, NOT a fake charge percentage.
  tft.drawRect(221,7,13,8,s.headerFg);tft.fillRect(234,9,2,4,s.headerFg);
  tft.drawFastHLine(0,STATUS_H-1,W,s.border);
}

void LauncherView::row(const LauncherStyle &s,const LauncherRow *item,int index,bool selected) {
  if(index<0 || index>=VISIBLE)return;
  const int y=LIST_Y+index*ROW_H;
  const uint16_t bg=selected?s.selectedBg:s.listBg;
  const uint16_t fg=selected?s.selectedFg:s.listFg;
  tft.fillRect(4,y,W-13,ROW_H-1,bg);
  if(selected){tft.fillRect(4,y,3,ROW_H-1,s.tabAccent);
               tft.drawFastHLine(5,y,W-15,s.border);}
  if(!item)return;
  // Left mini badge remains procedural; safe for arbitrary .qeapp metadata.
  const uint16_t miniBg=selected?s.tabAccent:s.previewBg;
  tft.fillRect(11,y+4,21,20,miniBg);
  tft.drawRect(11,y+4,21,20,s.border);
  char mark[3]={item->glyph&&item->glyph[0]?item->glyph[0]:'A',
                char(item->glyph&&item->glyph[1]?item->glyph[1]:0),0};
  clipped(mark,13,y+8,18,LauncherStyle::legibleOn(miniBg),miniBg,1,true);
  clipped(item->title,42,y+7,W-59,fg,bg,2,selected);
}

void LauncherView::scrollbar(const LauncherStyle &s,int total,int offset) {
  const int h=VISIBLE*ROW_H, x=W-6;
  tft.fillRect(x,LIST_Y,3,h,s.listBg);
  if(total<=VISIBLE)return;
  tft.fillRect(x,LIST_Y,2,h,s.border);
  int thumb=h*VISIBLE/total;
  if(thumb<13)thumb=13;
  if(thumb>h)thumb=h;
  int top=(h-thumb)*offset/(total-VISIBLE);
  tft.fillRect(x,LIST_Y+top,3,thumb,s.scrollbar);
}

void LauncherView::preview(const LauncherStyle &s,const LauncherRow *item,const uint16_t *icon565) {
  tft.fillRect(0,PREVIEW_Y,W,PREVIEW_H,s.previewBg);
  tft.drawFastHLine(0,PREVIEW_Y,W,s.border);
  tft.drawFastVLine(W-1,PREVIEW_Y,PREVIEW_H,s.border);
  if(!item){clipped("No applications",10,PREVIEW_Y+21,W-20,s.previewFg,s.previewBg,2);return;}
  if(icon565) {
    tft.fillRect(8,PREVIEW_Y+8,42,42,s.border);
    tft.pushImage(13,PREVIEW_Y+13,32,32,icon565);
  } else icon(item->glyph,9,PREVIEW_Y+10,39,s,true);
  clipped(item->title,57,PREVIEW_Y+8,W-65,s.previewFg,s.previewBg,2,true);
  clipped(item->summary,57,PREVIEW_Y+30,W-65,s.previewFg,s.previewBg,1);
  if(item->packageId)clipped("SIGNED .QEAPP",57,PREVIEW_Y+44,W-65,s.tabAccent,s.previewBg,1);
}

void LauncherView::content(const LauncherStyle &s,const LauncherRow *items,
                           int total,int offset,int selected,const uint16_t *icon565) {
  tft.fillRect(0,HEADER_Y+HEADER_H,W,PREVIEW_Y-(HEADER_Y+HEADER_H),s.listBg);
  for(int r=0;r<VISIBLE;++r){int i=offset+r;
    row(s,i>=0&&i<total ? &items[i] : nullptr,r,i<total&&i==selected);
  }
  scrollbar(s,total,offset);
  preview(s,total>0 && selected<total ? &items[selected]:nullptr,icon565);
}

void LauncherView::full(const LauncherStyle &s,const char *tabName,int tab,int tabCount,
                        const LauncherRow *items,int total,int offset,int selected,
                        const String &clock,bool wifi,bool sd,const uint16_t *icon565) {
  tft.fillScreen(s.background);
  status(s,clock,wifi,sd);
  tft.fillRect(0,HEADER_Y,W,HEADER_H,s.headerBg);
  tft.fillRect(0,HEADER_Y+HEADER_H-3,W,3,s.tabAccent);
  // Original tab emblem: 42 x 42 portrait analogue to a cover/logo header.
  // Procedural initials avoid redistributing Retro-Go artwork.
  static const char *const symbols[]={"HM","WB","AP","MD","SY","ST"};
  const char *symbol=(tab>=0 && tab<6)?symbols[tab]:"VQ";
  tft.fillRect(8,29,42,42,s.previewBg);
  tft.drawRect(8,29,42,42,s.border);
  tft.fillRect(10,31,38,3,s.tabAccent);
  clipped(symbol,16,43,30,s.previewFg,s.previewBg,2,true);
  clipped(tabName,59,34,131,s.headerFg,s.headerBg,2,true);
  clipped("<  LEFT     RIGHT  >",61,55,170,s.headerFg,s.headerBg,1);
  String pos=String(tab+1)+"/"+String(tabCount);
  clipped(pos.c_str(),196,38,42,s.headerFg,s.headerBg,1);
  // Small tab position dots, like a paged handheld launcher.
  const int start=(W-tabCount*13+4)/2;
  for(int i=0;i<tabCount;++i)tft.fillRect(start+i*13,HEADER_Y+47,i==tab?10:7,3,
                                        i==tab?s.tabAccent:s.border);
  content(s,items,total,offset,selected,icon565);
  tft.fillRect(0,FOOTER_Y,W,H-FOOTER_Y,s.footerBg);
  tft.drawFastHLine(0,FOOTER_Y,W,s.tabAccent);
  clipped("OPTION",6,FOOTER_Y+7,62,s.footerFg,s.footerBg,1,true);
  clipped("OK Open",91,FOOTER_Y+7,79,s.footerFg,s.footerBg,1,true);
  clipped("A Back",186,FOOTER_Y+7,53,s.footerFg,s.footerBg,1,true);
}

void LauncherView::dialog(const LauncherStyle &s,const char *title,
                          const char *const *choices,int count,int offset,int selected) {
  if(count<=0)return;
  const int visible=count<5?count:5;
  const int width=218;
  const int height=30+visible*28+8;
  const int x=11,y=(H-height)/2;
  tft.fillRect(x+3,y+4,width,height,0x0000);
  tft.fillRect(x,y,width,height,s.previewBg);
  tft.drawRect(x,y,width,height,s.border);
  tft.fillRect(x+1,y+1,width-2,28,s.headerBg);
  clipped(title,x+9,y+8,width-18,s.headerFg,s.headerBg,2,true);
  for(int row=0;row<visible;++row){int i=offset+row;if(i>=count)break;
    const int ry=y+32+row*28;
    uint16_t bg=i==selected?s.selectedBg:s.previewBg;
    uint16_t fg=i==selected?s.selectedFg:s.previewFg;
    tft.fillRect(x+4,ry,width-10,27,bg);
    if(i==selected)tft.fillRect(x+4,ry,3,27,s.tabAccent);
    clipped(choices[i],x+14,ry+6,width-28,fg,bg,2,i==selected);
  }
  if(count>visible){tft.fillRect(x+width-6,y+32,2,height-40,s.border);
    int thumb=(height-40)*visible/count;
    int top=(height-40-thumb)*offset/(count-visible);
    tft.fillRect(x+width-7,y+32+top,3,thumb,s.scrollbar);
  }
}
