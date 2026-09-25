// Tests real ThemeFileService scanner and palette application with in-memory SD shim.
#define main old_theme_test_main
#include "test_theme_runtime.cpp"
#undef main
#include <filesystem>
int main(int argc,char **argv){
  assert(argc==3);
  const std::string data=readFile(argv[1]);
  fakeThemeFiles.clear();
  fakeThemeFiles["/System/Themes/s60_green.vqeaf"]=data;
  fakeThemeFiles["/System/Themes/invalid.vqeaf"]="@vqeaf 1.0\n<theme>\npalette {\nscreen: \"#no\"\n}\n</theme>\n";
  StorageService sd; assert(sd.begin());
  ThemeFileService svc;
  assert(svc.scan(sd)==1);
  assert(svc.count()==1 && svc.find("/System/Themes/invalid.vqeaf")<0);
  ThemeColors palette=themeFor(ThemeId::Classic);
  LauncherStyle skin;
  String name,error;
  assert(svc.load(sd,"/System/Themes/s60_green.vqeaf",palette,name,error,&skin));
  assert(palette.bg==rgb(140,200,34));
  ThemeColors previous=palette;
  assert(!svc.load(sd,"/System/Themes/invalid.vqeaf",palette,name,error,&skin));
  assert(palette.bg==previous.bg);
  std::ofstream f(argv[2]); assert(f);
  const uint16_t *p=&previous.bg;
  f<<"{\"theme\":\"S60 Green\",\"colors\":[";
  for(int i=0;i<13;i++){if(i)f<<",";f<<p[i];}
  f<<"],\"verified\":true,\"invalid_theme_rejected\":true}\n";
  std::cout<<"THEME_SCANNED|1\nTHEME_APPLIED|S60 Green|"<<previous.bg<<"\nBAD_THEME_REJECTED|1\n";
  return 0;
}
