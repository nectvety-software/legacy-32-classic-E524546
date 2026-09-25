#pragma once
// VQEAF OS v2.2 | Portrait UI contract. Must stay in sync with docs/pixel_atlas.json.
// Header-only, pure C++11: PC tests can verify geometry without board libraries.
namespace VqeafLayout {
struct Rect { int x,y,w,h; constexpr int right() const {return x+w;} constexpr int bottom() const {return y+h;} };
constexpr int W=240, H=320;
constexpr int TITLEBAR_H=27, DIVIDER_Y=27, CONTENT_TOP=28;
constexpr int FOOTER_Y=298, FOOTER_H=22;
constexpr int LIST_ROW_H=42, LIST_VISIBLE=6;
constexpr int GRID_COLS=3, GRID_ROWS=4, GRID_W=78, GRID_H=66, ICON_BOX=36;
constexpr int STATUS_ZONE_W=80, STATUS_ICON_W=11, STATUS_ICON_GAP=6, STATUS_RIGHT_PAD=6;
constexpr Rect HEADER{0,0,W,TITLEBAR_H}, DIVIDER{0,DIVIDER_Y,W,1};
constexpr Rect CONTENT{0,CONTENT_TOP,W,FOOTER_Y-CONTENT_TOP};
constexpr Rect FOOTER{0,FOOTER_Y,W,FOOTER_H};
constexpr Rect HOME_CLOCK{12,43,216,78}, HOME_NETWORK{12,132,216,46};
constexpr Rect HOME_HINT{8,266,224,20};
constexpr Rect MENU_GRID{1,28,234,264};
constexpr Rect MENU_SCROLL{236,34,2,252};
constexpr Rect LIST_SCROLL{235,30,3,264};
constexpr Rect CALC_RESULT{12,40,216,48};
constexpr Rect CALC_KEYS{8,100,224,180};
constexpr Rect STOPWATCH_FACE{10,52,220,74};
constexpr Rect BROWSER_ADDRESS{4,29,232,26};
constexpr Rect BROWSER_VIEW{4,59,232,234};
constexpr Rect SHELL_TERMINAL{2,28,236,228};
constexpr Rect SHELL_INPUT{3,256,234,38};
constexpr Rect gridCell(int c,int r) {return {1+c*GRID_W,CONTENT_TOP+r*GRID_H,GRID_W-2,GRID_H};}
constexpr Rect gridIcon(int c,int r) {return {1+c*GRID_W+(GRID_W-2-ICON_BOX)/2,CONTENT_TOP+r*GRID_H+3,ICON_BOX,ICON_BOX};}
constexpr Rect homeShortcut(int i) {return {8+76*i,191,72,67};}
constexpr Rect listRow(int i) {return {2,CONTENT_TOP+1+LIST_ROW_H*i,W-7,LIST_ROW_H-1};}
constexpr Rect softkey(int i) {return {80*i,FOOTER_Y,80,FOOTER_H};}
constexpr bool inScreen(Rect r) {return r.x>=0&&r.y>=0&&r.w>0&&r.h>0&&r.right()<=W&&r.bottom()<=H;}
static_assert(HEADER.bottom()==DIVIDER_Y&&DIVIDER.bottom()==CONTENT_TOP, "header seam");
static_assert(CONTENT.bottom()==FOOTER_Y&&FOOTER.bottom()==H, "footer seam");
static_assert(inScreen(gridCell(2,3))&&gridCell(2,3).bottom()<=FOOTER_Y, "grid overflow");
static_assert(inScreen(listRow(5))&&listRow(5).bottom()<=FOOTER_Y, "list overflow");
static_assert(inScreen(homeShortcut(2))&&inScreen(CALC_KEYS)&&inScreen(STOPWATCH_FACE), "screen overflow");
}
