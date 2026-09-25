#include "PixelSnakeConfig.h"
#include <string.h>
namespace PixelSnake {
bool parseConfig(const char *src,size_t len,Config &out){
  static const char header[]="VQEAF-SNAKE-1\n";
  if(!src||len<sizeof header-1||len>512||memcmp(src,header,sizeof header-1))return false;
  Config cfg={155,false,Config::Skin::Forest};
  bool gotSpeed=false,gotWrap=false,gotSkin=false;
  size_t pos=sizeof header-1;
  while(pos<len){
    char line[48];size_t k=0;
    while(pos<len&&src[pos]!='\n'){
      const char c=src[pos++];if(c=='\r'&&pos<len&&src[pos]=='\n')continue;
      if(c<32||c>126||k>=sizeof line-1)return false;
      line[k++]=c;
    }
    if(pos<len)pos++;
    line[k]=0;
    if(!k)continue;
    if(!strncmp(line,"speed_ms=",9)&&!gotSpeed){
      const char *v=line+9;if(!*v)return false;unsigned ms=0;
      for(;*v;v++){if(*v<'0'||*v>'9')return false;ms=ms*10u+unsigned(*v-'0');if(ms>2000)return false;}
      if(ms<85||ms>400)return false;
      cfg.speedMs=uint16_t(ms);gotSpeed=true;
    }else if(!strncmp(line,"wrap=",5)&&!gotWrap){
      if(!strcmp(line+5,"0"))cfg.wrap=false;
      else if(!strcmp(line+5,"1"))cfg.wrap=true;
      else return false;
      gotWrap=true;
    }else if(!strncmp(line,"palette=",8)&&!gotSkin){
      const char *v=line+8;
      if(!strcmp(v,"forest"))cfg.skin=Config::Skin::Forest;
      else if(!strcmp(v,"night"))cfg.skin=Config::Skin::Night;
      else if(!strcmp(v,"amber"))cfg.skin=Config::Skin::Amber;
      else return false;
      gotSkin=true;
    }else return false;
  }
  if(!gotSpeed||!gotWrap||!gotSkin)return false;
  out=cfg;return true;
}
}
