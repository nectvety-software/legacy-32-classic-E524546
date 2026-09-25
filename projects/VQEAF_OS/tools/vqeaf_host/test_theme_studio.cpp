#include <cassert>
#include <cstring>
#include <iostream>
#include <string>
#include <map>
#include <fstream>
#include <sstream>
#include "services/ThemeFileService.h"
std::map<std::string,std::string> fakeThemeFiles;
fs::FS disk;
bool StorageService::begin(){ok=true;return true;}
fs::FS &StorageService::fs(){return disk;}
int StorageService::scanMedia(const String &root,const char *const *ext,int extCount,
                              FsEntry *out,int cap,int depth){
 int n=0;
 for(const auto &entry:fakeThemeFiles){
    std::string base(root.c_str());
    if(base!="/" && entry.first.compare(0,base.length()+1,base+"/")!=0)continue;
    const std::string tail=base=="/"?entry.first.substr(1):entry.first.substr(base.length()+1);
    if(std::count(tail.begin(),tail.end(),'/')>depth)continue;
    bool match=false;
    for(int i=0;i<extCount;++i){const std::string e(ext[i]);
       if(entry.first.size()>=e.size()&&entry.first.compare(entry.first.size()-e.size(),e.size(),e)==0)match=true;
    }
    if(!match || n>=cap)continue;
    out[n++]=FsEntry(entry.first.substr(entry.first.find_last_of('/')+1),entry.first,false,entry.second.length());
 }
 return n;
}
static std::string load(const char*name){std::ifstream f(name,std::ios::binary);std::ostringstream o;o<<f.rdbuf();return o.str();}
static uint16_t rgb(int r,int g,int b){return uint16_t(((r&0xf8)<<8)|((g&0xfc)<<3)|(b>>3));}
int main(int argc,char **argv) {
  assert(argc>3);
  fakeThemeFiles["/Themes/night.vqeaf"]=load(argv[1]);
  fakeThemeFiles["/Themes/day.vqeaf"]=load(argv[2]);
  fakeThemeFiles["/Themes/reference_lime.vqeaf"]=load(argv[3]);
  // Large Theme Studio embedded image (>260k base64-like line) is ignored
  // without buffering it in RAM, while checking complete closing tag.
  fakeThemeFiles["/Themes/studio_large.vqeaf"] =
      "@vqeaf 1.0\n<theme id=\"studio\" name=\"Official-like test\">\n"
      " palette {\n screen: \"#02050A\"\n keyText: \"#EAF0FF\"\n accent: \"#66D5C4\"\n glow: \"#00000000\"\n}\n"
      "<resource id=\"frame_background\" type=\"image\">\n"
      "data: \"data:image/webp;base64,"+std::string(280000,'X')+"\"\n</resource>\n</theme>";
  fakeThemeFiles["/Themes/rgba.vqeaf"] =
      "@vqeaf 1.0\n<theme id=\"rgba\" name=\"RGBA\">\npalette {\n"
      "screen: \"#123\"\nkeyText: \"#FABC\"\naccent: \"#AA336699\"\n"
      "glow: \"#00000000\"\n}\n</theme>";
  fakeThemeFiles["/Themes/invalid.vqeaf"] =
      "@vqeaf 1.0\n<theme name=\"oops\">\npalette {\nscreen: \"#HI1234\"\n"
      "keyText: \"#FFFFFF\"\naccent: \"#55FF00\"\n}\n</theme>";
  StorageService sd;assert(sd.begin());ThemeFileService service;assert(service.scan(sd)==5);
  assert(service.find("/Themes/invalid.vqeaf")<0);
  ThemeColors c=themeFor(ThemeId::Classic);
  LauncherStyle skin;String name,error;
  assert(service.load(sd,"/Themes/night.vqeaf",c,name,error,&skin));
  assert(name=="VQEAF Night");assert(skin.background==rgb(8,15,26));
  assert(skin.selectedBg==rgb(35,84,129));assert(c.accent==rgb(85,210,198));
  assert(service.load(sd,"/Themes/day.vqeaf",c,name,error,&skin));
  assert(skin.background!=0);
  assert(service.load(sd,"/Themes/reference_lime.vqeaf",c,name,error,&skin));
  assert(name=="VQEAF Reference Lime");
  assert(skin.headerBg==rgb(41,109,24));
  assert(skin.selectedBg==rgb(222,242,156));
  assert(skin.footerBg==rgb(180,222,115));
  assert(service.load(sd,"/Themes/studio_large.vqeaf",c,name,error,&skin));
  assert(c.bg==rgb(2,5,10));
  assert(service.load(sd,"/Themes/rgba.vqeaf",c,name,error,&skin));
  assert(c.bg==rgb(17,34,51));assert(c.accent==rgb(51,102,153));
  const ThemeColors saved=c;
  assert(!service.load(sd,"/Themes/invalid.vqeaf",c,name,error,&skin));
  assert(c.bg==saved.bg);assert(c.accent==saved.accent);
  assert(!service.load(sd,"/Themes/../escape.vqeaf",c,name,error,&skin));
  std::cout<<"PASS VQEAF: native Studio palette, extension, big base64 streaming, RGBA, fallback\n";
}
