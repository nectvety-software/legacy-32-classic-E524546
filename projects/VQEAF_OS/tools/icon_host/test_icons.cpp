#include "VqeafIconRenderer.h"
#include "VqeafIconData.h"
#include <TFT_eSPI.h>
#include <cassert>
#include <cstdio>
#include <string>
#include <fstream>
static void savePPM(TFT_eSPI &tft,int x,int y,int size,const std::string &p){
  std::ofstream f(p.c_str(),std::ios::binary);
  f<<"P6\n"<<size<<" "<<size<<"\n255\n";
  for(int yy=y; yy<y+size; ++yy) for(int xx=x;xx<x+size;++xx){
    uint16_t c=tft.fb[yy*240+xx];
    char rgb[3]={char(((c>>11)&31)*255/31),char(((c>>5)&63)*255/63),char((c&31)*255/31)};
    f.write(rgb,3);
  }
}
int main(int argc,char **argv){
  assert(argc==2);
  const std::string dir(argv[1]);
  TFT_eSPI t;
  const uint16_t lime=0xAEE9;
  auto pal=VqeafIcons::Palette::standard();
  assert(VqeafIcons::fromLegacy("Wi")==VqeafIcons::Id::WiFi);
  assert(VqeafIcons::fromLegacy("BLE")==VqeafIcons::Id::Bluetooth);
  assert(VqeafIcons::fromLegacy("Typo")==VqeafIcons::Id::Count);
  assert(!VqeafIcons::draw(t,VqeafIcons::Id::Count,5,5,24,lime,pal));
  assert(!VqeafIcons::draw(t,VqeafIcons::Id::WiFi,5,5,31,lime,pal));
  for(int s=24;s<=36;s+=12)for(int i=0;i<12;++i){
    auto id=static_cast<VqeafIcons::Id>(i);
    assert(VqeafIcons::draw(t,id,10,10,s,lime,pal,true));
    for(int yy=0;yy<s;++yy)for(int xx=0;xx<s;++xx){
      if(xx<(s==36?4:3)||yy<(s==36?4:3)||xx>=s-(s==36?4:3)||yy>=s-(s==36?4:3))
        assert(t.fb[(10+yy)*240+(10+xx)]==lime);
    }
    std::string p=dir+"/"+VqeafIcons::name(id)+"_"+std::to_string(s)+".ppm";
    savePPM(t,10,10,s,p);
  }
  auto before=t.fb;
  VqeafIcons::focusFrame(t,3,3,70,64,0xFFFF,0xB6EE);
  for(int y=0;y<320;++y)for(int x=0;x<240;++x)if(t.fb[y*240+x]!=before[y*240+x])
    assert(x>=3&&x<73&&y>=3&&y<67);
  puts("PASS: 12x24, 12x36, RLE bounds, safe inset, focus bounds, invalid-size guard.");
}
