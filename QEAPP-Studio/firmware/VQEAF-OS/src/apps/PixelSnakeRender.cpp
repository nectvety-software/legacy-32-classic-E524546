#include "PixelSnakeRender.h"
#include <string.h>
namespace PixelSnake {
// Explicitly hand-authored 5x7 bitmap font. Columns are LSB-top.
struct Glyph {char c;uint8_t col[5];};
static const Glyph FONT[]={
{' ',{0,0,0,0,0}},{'A',{0x7e,0x11,0x11,0x11,0x7e}},
{'B',{0x7f,0x49,0x49,0x49,0x36}},{'C',{0x3e,0x41,0x41,0x41,0x22}},
{'D',{0x7f,0x41,0x41,0x22,0x1c}},{'E',{0x7f,0x49,0x49,0x49,0x41}},
{'F',{0x7f,0x09,0x09,0x09,0x01}},{'G',{0x3e,0x41,0x49,0x49,0x7a}},
{'H',{0x7f,0x08,0x08,0x08,0x7f}},{'I',{0x41,0x41,0x7f,0x41,0x41}},
{'J',{0x20,0x40,0x41,0x3f,0x01}},{'K',{0x7f,0x08,0x14,0x22,0x41}},
{'L',{0x7f,0x40,0x40,0x40,0x40}},{'M',{0x7f,0x02,0x04,0x02,0x7f}},
{'N',{0x7f,0x02,0x04,0x08,0x7f}},{'O',{0x3e,0x41,0x41,0x41,0x3e}},
{'P',{0x7f,0x09,0x09,0x09,0x06}},{'Q',{0x3e,0x41,0x51,0x21,0x5e}},
{'R',{0x7f,0x09,0x19,0x29,0x46}},{'S',{0x26,0x49,0x49,0x49,0x32}},
{'T',{0x01,0x01,0x7f,0x01,0x01}},{'U',{0x3f,0x40,0x40,0x40,0x3f}},
{'V',{0x1f,0x20,0x40,0x20,0x1f}},{'W',{0x7f,0x20,0x18,0x20,0x7f}},
{'X',{0x63,0x14,0x08,0x14,0x63}},{'Y',{0x07,0x08,0x70,0x08,0x07}},
{'Z',{0x61,0x51,0x49,0x45,0x43}},
{'0',{0x3e,0x45,0x49,0x51,0x3e}},{'1',{0,0x42,0x7f,0x40,0}},
{'2',{0x42,0x61,0x51,0x49,0x46}},{'3',{0x21,0x41,0x45,0x4b,0x31}},
{'4',{0x18,0x14,0x12,0x7f,0x10}},{'5',{0x27,0x45,0x45,0x45,0x39}},
{'6',{0x3c,0x4a,0x49,0x49,0x30}},{'7',{0x01,0x71,0x09,0x05,0x03}},
{'8',{0x36,0x49,0x49,0x49,0x36}},{'9',{0x06,0x49,0x49,0x29,0x1e}},
{':',{0,0x36,0x36,0,0}},{'-',{0x08,0x08,0x08,0x08,0x08}},
{'/',{0x20,0x10,0x08,0x04,0x02}},{'.',{0,0x40,0x40,0,0}},
{'!',{0,0,0x5f,0,0}},{'+',{0x08,0x08,0x3e,0x08,0x08}}
};
static const uint8_t *glyph(char c){
 for(const Glyph &g:FONT)if(g.c==c)return g.col;
 return FONT[0].col;
}
void Renderer::text(Canvas &canvas,int x,int y,const char *s,uint16_t color,int scale){
 if(!s)return;
 for(;*s;s++,x+=6*scale){const uint8_t *bits=glyph(*s);
   for(int cx=0;cx<5;cx++)for(int cy=0;cy<7;cy++)
     if(bits[cx]&(1u<<cy))canvas.fill(x+cx*scale,y+cy*scale,scale,scale,color);
 }
}
void Renderer::background(Canvas &c,const Colors &p){
 c.fill(0,28,240,270,p.shadow);
 c.fill(6,32,228,22,p.panel);
 c.fill(X-3,Y-3,BW+6,BH+6,p.border);
 c.fill(X-1,Y-1,BW+2,BH+2,p.light);
 for(int y=0;y<H;y++)for(int x=0;x<W;x++)
   c.fill(X+x*TILE,Y+y*TILE,TILE,TILE,((x+y)&1)?p.alt:p.ground);
 c.fill(6,281,228,13,p.panel);
 text(c,12,284,"D-PAD MOVE",p.ink);
 text(c,144,284,"OK PAUSE",p.ink);
}
void Renderer::hud(Canvas &c,const Game &g,uint16_t high,const Colors &p){
 char s[]="SCORE 0000";uint16_t score=g.score();
 for(int i=9;i>=6;i--){s[i]='0'+score%10;score/=10;}
 c.fill(9,36,122,14,p.panel);text(c,12,39,s,p.light);
 char h[]="HI 0000";for(int i=6;i>=3;i--){h[i]='0'+high%10;high/=10;}
 c.fill(150,36,84,14,p.panel);text(c,151,39,h,p.ink);
}
void Renderer::cell(Canvas &c,Point cell,const Game &g,const Colors &p){
 const int x=X+cell.x*TILE,y=Y+cell.y*TILE;
 c.fill(x,y,TILE,TILE,((cell.x+cell.y)&1)?p.alt:p.ground);
 if(cell.x==g.food().x&&cell.y==g.food().y){
   c.fill(x+4,y+1,3,2,p.light);c.fill(x+5,y+2,2,2,p.dark);
   c.fill(x+2,y+4,8,6,p.fruit);c.fill(x+3,y+3,6,8,p.fruit);
   c.fill(x+3,y+5,2,2,p.fruitLight);return;
 }
 for(uint16_t i=0;i<g.length();i++){
  Point s=g.segment(i);if(s.x!=cell.x||s.y!=cell.y)continue;
  c.fill(x+1,y+1,10,10,p.dark);
  c.fill(x+2,y+1,8,8,p.body);
  c.fill(x+3,y+2,6,3,p.light);
  if(!i){
    switch(g.direction()){
    case Dir::Right:c.fill(x+8,y+3,2,2,p.border);c.fill(x+8,y+7,2,2,p.border);break;
    case Dir::Left:c.fill(x+2,y+3,2,2,p.border);c.fill(x+2,y+7,2,2,p.border);break;
    case Dir::Up:c.fill(x+3,y+2,2,2,p.border);c.fill(x+7,y+2,2,2,p.border);break;
    case Dir::Down:c.fill(x+3,y+8,2,2,p.border);c.fill(x+7,y+8,2,2,p.border);break;
    }
  }
  return;
 }
}
void Renderer::scene(Canvas &c,const Game &g,const Colors &p,uint16_t high){
 background(c,p);
 for(uint16_t i=0;i<g.length();i++)cell(c,g.segment(i),g,p);
 cell(c,g.food(),g,p);
 hud(c,g,high,p);
 overlay(c,g.phase(),p);
}
void Renderer::overlay(Canvas &c,Phase phase,const Colors &p){
 if(phase==Phase::Running)return;
 c.fill(39,131,162,68,p.border);
 c.fill(42,134,156,62,p.panel);
 const char *heading=phase==Phase::Ready?"PIXEL SNAKE":phase==Phase::Paused?"PAUSED":phase==Phase::Won?"YOU WIN!":"GAME OVER";
 const char *detail=phase==Phase::Ready?"OK TO START":phase==Phase::Paused?"OK RESUME":phase==Phase::Won?"OK NEW GAME":"OK RETRY";
 int first=int(strlen(heading))*6;
 int second=int(strlen(detail))*6;
 text(c,120-first/2,145,heading,p.light);
 text(c,120-second/2,171,detail,p.ink);
}
}
