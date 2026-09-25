#include "PixelSnakeApp.h"
#include <WiFi.h>
#include <string.h>
// A small adapter: all pixel art is rendered by the SAME pure C++ renderer
// used by preview generation and host pixel-regression tests.
namespace {
class ScreenCanvas : public PixelSnake::Canvas {
 public:
  explicit ScreenCanvas(TFT_eSPI &t):tft(t){}
  void fill(int x,int y,int w,int h,uint16_t rgb565) override { tft.fillRect(x,y,w,h,rgb565); }
 private:TFT_eSPI &tft;
};
static PixelSnake::Colors skinFor(PixelSnake::Config::Skin skin,ThemeColors t){
 PixelSnake::Colors p;
 if(skin==PixelSnake::Config::Skin::Night){
   p.ground=0x1084;p.alt=0x18e5;p.border=0x0000;p.body=0x767f;p.light=0xcfff;
   p.dark=0x1c10;p.fruit=0xfb2f;p.fruitLight=0xfdb6;p.panel=t.chrome;
   p.shadow=t.bg;p.ink=t.chromeText;
 }else if(skin==PixelSnake::Config::Skin::Amber){
   p.ground=0x3184;p.alt=0x39c5;p.border=0x1040;p.body=0xfd20;p.light=0xffb0;
   p.dark=0x9980;p.fruit=0xf841;p.fruitLight=0xffa7;p.panel=t.chrome;
   p.shadow=t.bg;p.ink=t.chromeText;
 }else{
   p.panel=t.chrome;p.shadow=t.bg;p.ink=t.chromeText;
 }
 return p;
}
}
void PixelSnakeApp::loadHigh(AppContext &ctx){
 high_=0;uint8_t data[8]={};size_t n=0;String err;
 if(ctx.appData.load("snake_pixel",QeappDataService::Slot::State,data,sizeof data,n,err)&&
    n==8&&memcmp(data,"SNK1",4)==0){
   high_=uint16_t(data[4]) | (uint16_t(data[5])<<8);
   if(high_>PixelSnake::CAPACITY*10)high_=0;
 }
}
void PixelSnakeApp::saveHigh(AppContext &ctx){
 if(game_.score()<=high_)return;
 high_=game_.score();
 uint8_t data[8]={'S','N','K','1',uint8_t(high_&0xff),uint8_t(high_>>8),0,0};
 String err;ctx.appData.save("snake_pixel",QeappDataService::Slot::State,data,sizeof data,err);
}
bool PixelSnakeApp::enter(AppContext &ctx){
 valid_=false;
 Qeapp::Meta signedApp;
 if(!ctx.installer.get("snake_pixel",signedApp)||strcmp(signedApp.type,"text"))return false;
 String path=ctx.installer.installedPath("snake_pixel")+"/payload.txt";
 File f=ctx.storage.fs().open(path,FILE_READ);
 if(!f||f.isDirectory()||f.size()>512){if(f)f.close();return false;}
 char buf[513];const size_t count=size_t(f.size());
 bool ok=f.read(reinterpret_cast<uint8_t*>(buf),count)==int(count);f.close();
 if(!ok||!PixelSnake::parseConfig(buf,count,config_))return false;
 valid_=true;colors_=skinFor(config_.skin,ctx.ui.c());loadHigh(ctx);
 game_.reset(uint32_t(millis())^0x424cdf11u,config_.wrap);lastStep_=millis();
 return true;
}
void PixelSnakeApp::paintTFT(AppContext &ctx,bool full){
 if(!valid_)return;
 if(full){
   ctx.ui.chrome("Pixel Snake",WiFi.status()==WL_CONNECTED,false,ctx.storage.mounted(),ctx.settings.data().hour12);
 }
 ScreenCanvas canvas(ctx.ui.display());
 PixelSnake::Renderer::scene(canvas,game_,colors_,high_>game_.score()?high_:game_.score());
 if(full)ctx.ui.softkeys("MENU","OK / Pause","Back");
}
void PixelSnakeApp::draw(AppContext &ctx){paintTFT(ctx,true);}
void PixelSnakeApp::redrawDelta(AppContext &ctx,PixelSnake::Point beforeHead,PixelSnake::Point beforeTail,PixelSnake::Result change){
 ScreenCanvas canvas(ctx.ui.display());
 if(change==PixelSnake::Result::Moved||change==PixelSnake::Result::Ate){
   PixelSnake::Renderer::cell(canvas,beforeHead,game_,colors_);
   PixelSnake::Renderer::cell(canvas,beforeTail,game_,colors_);
   PixelSnake::Renderer::cell(canvas,game_.segment(0),game_,colors_);
   if(change==PixelSnake::Result::Ate){
      PixelSnake::Renderer::cell(canvas,game_.food(),game_,colors_);
      PixelSnake::Renderer::hud(canvas,game_,high_>game_.score()?high_:game_.score(),colors_);
   }
 }else if(change==PixelSnake::Result::Lost||change==PixelSnake::Result::Won){
   if(change==PixelSnake::Result::Won){
     PixelSnake::Renderer::cell(canvas,beforeHead,game_,colors_);
     PixelSnake::Renderer::cell(canvas,beforeTail,game_,colors_);
     PixelSnake::Renderer::cell(canvas,game_.segment(0),game_,colors_);
   }
   saveHigh(ctx);
   PixelSnake::Renderer::overlay(canvas,game_.phase(),colors_);
   PixelSnake::Renderer::hud(canvas,game_,high_,colors_);
 }
}
void PixelSnakeApp::tick(AppContext &ctx){
 if(!valid_||game_.phase()!=PixelSnake::Phase::Running)return;
 const uint32_t now=millis();if(uint32_t(now-lastStep_)<config_.speedMs)return;
 // One fixed simulation step; no backlog bursts after a slow SD/WiFi task.
 lastStep_=now;
 const auto oldHead=game_.segment(0),oldTail=game_.segment(game_.length()-1);
 auto result=game_.step();
 redrawDelta(ctx,oldHead,oldTail,result);
}
ScreenId PixelSnakeApp::handle(AppContext &ctx,const KeyEvent &e){
 if(!valid_)return ScreenId::Applications;
 if(!e.pressed||e.longPress)return ScreenId::Snake;
 using namespace PixelSnake;
 if(e.key==Key::A||e.key==Key::B){saveHigh(ctx);return ScreenId::Applications;}
 if(e.key==Key::Start){
   if(game_.phase()==Phase::Lost||game_.phase()==Phase::Won){
      saveHigh(ctx);game_.reset(uint32_t(millis())^0x271d41a3u,config_.wrap);
   }
   if(game_.phase()==Phase::Running)game_.pause();else game_.start();
   lastStep_=millis();draw(ctx);return ScreenId::Snake;
 }
 if(e.key==Key::Option){
    if(game_.phase()==Phase::Running)game_.pause();else if(game_.phase()==Phase::Paused)game_.start();
    lastStep_=millis();draw(ctx);return ScreenId::Snake;
 }
 if(e.key==Key::Up)game_.turn(Dir::Up);
 else if(e.key==Key::Down)game_.turn(Dir::Down);
 else if(e.key==Key::Left)game_.turn(Dir::Left);
 else if(e.key==Key::Right)game_.turn(Dir::Right);
 return ScreenId::Snake;
}
