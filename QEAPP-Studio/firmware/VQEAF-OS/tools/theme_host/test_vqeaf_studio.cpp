#include <cassert>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <cstring>
#include "../../src/services/ThemeFileService.h"
std::map<std::string,std::string> fakeThemeFiles;
static fs::FS disk;
bool StorageService::begin(){ok=true;return true;}
fs::FS &StorageService::fs(){return disk;}
int StorageService::scanMedia(const String &root,const char *const *extensions,
 int extCount,FsEntry *out,int limit,int depth){
 int n=0;std::string base=root.c_str();for(auto &i:fakeThemeFiles){
  auto &s=i.first;
  if(base!="/" && s.compare(0,base.size()+1,base+"/")!=0)continue;
  std::string tail=base=="/"?s.substr(1):s.substr(base.size()+1);
  if(tail.empty() || std::count(tail.begin(),tail.end(),'/')>depth)continue;
  bool match=false;for(int k=0;k<extCount;++k){size_t len=strlen(extensions[k]);
   if(s.size()>=len&&s.compare(s.size()-len,len,extensions[k])==0)match=true;}
  if(!match)continue;if(n>=limit)break;
  out[n++]=FsEntry(s.substr(s.find_last_of('/')+1),s,false,i.second.size());
 }
 return n;
}
static std::string readText(const char *filename){std::ifstream f(filename);
 assert(f);return std::string((std::istreambuf_iterator<char>(f)),std::istreambuf_iterator<char>());
}
int main(int argc,char**argv){
 assert(argc>=2);
 std::string sample=readText(argv[1]);
 fakeThemeFiles["/Themes/vqeaf_night.vqeaf"]=sample;
 // A canonical Theme Studio file can contain big base64 payloads AFTER palette.
 // Streaming palette parser must seek the closing tag and never read the image.
 std::string big="@vqeaf 1.0\n<theme id=\"base64\" name=\"Large Studio\">\npalette {\n"
 "screen: \"#0A2030\"\nkeyText: \"#FFFFFF\"\naccent: \"#AACCFF\"\nglow: \"#00000000\"\n}\n"
 "<component id=\"screen\" type=\"panel\">\n shape { fill: $palette.screen }\n"
 "</component>\n<resource id=\"frame_background\" type=\"image\">\n"
 "data: \"data:image/webp;base64,"+std::string(125000,'A')+"\"\n</resource>\n</theme>\n";
 fakeThemeFiles["/Themes/large.vqeaf"]=big;
 StorageService sd;assert(sd.begin());ThemeFileService svc;
 int n=svc.scan(sd);assert(n==2);
 ThemeColors c=themeFor(ThemeId::Classic);LauncherStyle skin;
 String name,err;
 assert(svc.load(sd,"/Themes/vqeaf_night.vqeaf",c,name,err,&skin));
 assert(name=="VQEAF Night");
 assert(skin.headerBg==0x1128 /* #102540 RGB565 */);
 assert(skin.selectedFg==0xFFFF);
 assert(svc.load(sd,"/Themes/large.vqeaf",c,name,err,&skin));
 assert(name=="Large Studio");
 assert(c.bg!=0x0000 && skin.tabAccent==c.accent);
 // Truncated and huge files are rejected without touching the previous skin.
 LauncherStyle before=skin;ThemeColors old=c;
 fakeThemeFiles["/Themes/large.vqeaf"].resize(20000);
 assert(!svc.load(sd,"/Themes/large.vqeaf",c,name,err,&skin));
 assert(c.bg==old.bg && skin.tabAccent==before.tabAccent);
 std::cout<<"PASS VQEAF Studio .vqeaf: launcher fields, 125KB data URI skip, malformed fallback\n";
}
