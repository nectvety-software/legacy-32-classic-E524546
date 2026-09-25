#define PIXEL_SNAKE_TEST 1
#include "../../src/apps/PixelSnakeLogic.h"
#include "../../src/apps/PixelSnakeRender.h"
#include "../../src/apps/PixelSnakeConfig.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>
#include <string>
#include <fstream>

using namespace PixelSnake;
struct Frame: Canvas {
 uint16_t pixels[240*320] = {};
 void fill(int x,int y,int w,int h,uint16_t c) override {
  assert(x>=0&&y>=0&&w>0&&h>0&&x+w<=240&&y+h<=320);
  for(int row=y;row<y+h;row++)for(int col=x;col<x+w;col++)pixels[row*240+col]=c;
 }
 void save(const char *path){std::ofstream f(path,std::ios::binary);f<<"P6\n240 320\n255\n";
  for(uint16_t s:pixels){uint8_t rgb[3]={uint8_t(((s>>11)&31)*255/31),uint8_t(((s>>5)&63)*255/63),uint8_t((s&31)*255/31)};f.write(reinterpret_cast<char*>(rgb),3);}
 }
};
int main(int argc,char **argv){
 Config conf;const char valid[]="VQEAF-SNAKE-1\nspeed_ms=155\nwrap=0\npalette=forest\n";
 assert(parseConfig(valid,strlen(valid),conf));assert(conf.speedMs==155&&!conf.wrap);
 const char malicious[]="VQEAF-SNAKE-1\nspeed_ms=99\nwrap=1\npalette=forest\nexec=shell\n";
 assert(!parseConfig(malicious,strlen(malicious),conf));
 const char tooFast[]="VQEAF-SNAKE-1\nspeed_ms=0\nwrap=1\npalette=forest\n";
 assert(!parseConfig(tooFast,strlen(tooFast),conf));
 const char overflowing[]="VQEAF-SNAKE-1\nspeed_ms=999999999\nwrap=0\npalette=forest\n";
 assert(!parseConfig(overflowing,strlen(overflowing),conf));
 Game g;g.reset(777);
 assert(g.phase()==Phase::Ready&&g.length()==4&&g.score()==0);
 for(int i=0;i<g.length();i++)assert(!(g.segment(i).x==g.food().x&&g.segment(i).y==g.food().y));
 assert(!g.turn(Dir::Left)); // immediate reversal blocked
 assert(g.turn(Dir::Up));assert(!g.turn(Dir::Left)); // one direction per tick
 g.start();assert(g.step()==Result::Moved);assert(g.segment(0).y==8);
 Point body[4]={{6,6},{5,6},{4,6},{3,6}};
 g.testState(body,4,Dir::Right,{7,6},false);
 assert(g.step()==Result::Ate&&g.score()==10&&g.length()==5);
 g.testState(body,4,Dir::Right,{15,17},false);
 for(int i=0;i<9;i++){Result r=g.step();assert(r!=Result::Lost);} // x6->15
 assert(g.step()==Result::Lost&&g.phase()==Phase::Lost);
 Point edge[4]={{15,6},{14,6},{13,6},{12,6}};
 g.testState(edge,4,Dir::Right,{5,5},true);
 assert(g.step()==Result::Moved&&g.segment(0).x==0); // wrap
 // A legitimate move onto the position of the OLD tail is allowed.
 Point ring[4]={{3,3},{3,4},{2,4},{2,3}};
 g.testState(ring,4,Dir::Left,{15,17},false);
 assert(g.step()==Result::Moved&&g.segment(0).x==2&&g.segment(0).y==3);
 // Head into retained body is a real self-collision.
 Point collide[5]={{3,3},{3,4},{2,4},{2,3},{1,3}};
 g.testState(collide,5,Dir::Left,{15,17},false);
 assert(g.step()==Result::Lost);
 // Determinism and no food spawn on snake over repeated seeds.
 for(uint32_t seed=1;seed<512;seed++){
   Game a,b;a.reset(seed);b.reset(seed);assert(a.food().x==b.food().x&&a.food().y==b.food().y);
   for(uint16_t i=0;i<a.length();i++)assert(!(a.food().x==a.segment(i).x&&a.food().y==a.segment(i).y));
 }
 // Fuzz-derived endurance: random legal turns must not corrupt body geometry.
 uint32_t rng=0x794a613u;
 for(int j=0;j<90;j++){
   g.reset(j*17+1,(j%2)==0);g.start();
   for(int i=0;i<250;i++){
      rng ^= rng<<13;rng ^= rng>>17;rng ^= rng<<5;
      if((rng&7)==0)g.turn(Dir((rng>>8)&3));
      Result r=g.step();
      assert(g.length()>0&&g.length()<=CAPACITY);
      for(uint16_t n=0;n<g.length();n++){
         Point p=g.segment(n);assert(p.x<W&&p.y<H);
         for(uint16_t v=0;v<n;v++){Point q=g.segment(v);assert(p.x!=q.x||p.y!=q.y);}
      }
      if(r==Result::Lost||r==Result::Won)break;
   }
 }
 // Render actual shared production painter to 240x320 frame buffer.
 g.reset(32332);Frame f;Colors c;
 f.fill(0,0,240,27,0x2b63);Renderer::text(f,10,8,"PIXEL SNAKE",0xffff,2);
 f.fill(0,298,240,22,0xaee9);Renderer::text(f,7,306,"MENU",0x0000);Renderer::text(f,95,306,"START",0x0000);Renderer::text(f,187,306,"BACK",0x0000);
 Renderer::scene(f,g,c,20);
 if(argc>1)f.save(argv[1]);
 g.start();g.turn(Dir::Up);g.step();
 Frame live;live.fill(0,0,240,27,0x2b63);Renderer::text(live,10,8,"PIXEL SNAKE",0xffff,2);
 live.fill(0,298,240,22,0xaee9);Renderer::text(live,7,306,"MENU",0x0000);Renderer::text(live,95,306,"PAUSE",0x0000);Renderer::text(live,187,306,"BACK",0x0000);
 Renderer::scene(live,g,c,20);
 if(argc>2)live.save(argv[2]);
 printf("PASS: config strict, turn queue, growth, wall, wrap, tail collision, self-collision, deterministic RNG, frame bounds 240x320\n");
 return 0;
}
