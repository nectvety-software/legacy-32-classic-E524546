// Host-only mutation stress against ACTUAL ThemeFileService.cpp; no ESP32 claim.
#define main unused_theme_test_main
#include "../theme_host/test_theme_runtime.cpp"
#undef main
#include <random>
#include <chrono>
static void checkTheme(ThemeFileService &svc,StorageService &sd,const std::string &s,unsigned &ok,unsigned &reject){
  fakeThemeFiles["/Themes/fuzz.vqeaf"]=s;
  ThemeColors out=themeFor(ThemeId::Black),snapshot=out;
  String name("fuzz"),error;
  bool success=svc.load(sd,"/Themes/fuzz.vqeaf",out,name,error);
  if(success){
    assert(error.length()==0);
    ++ok;
  }else {
    assert(out.bg==snapshot.bg && out.accent==snapshot.accent && out.text==snapshot.text);
    assert(error.length()>0);
    ++reject;
  }
}
int main(){
  fakeThemeFiles.clear();
  StorageService sd;assert(sd.begin());ThemeFileService svc;
  const std::string seed="@vqeaf 1.0\n<theme name=\"Fuzz\">\npalette {\nscreen: \"#123456\"\nkeyText: \"#FFFFFF\"\naccent: \"#309040\"\n}\n</theme>\n";
  std::mt19937 gen(0x243c0de);unsigned accept=0,reject=0;
  checkTheme(svc,sd,seed,accept,reject);assert(accept==1);
  for(int i=0;i<5000;++i){
    std::string s=seed;
    int mutations=1+(gen()%20);
    for(int j=0;j<mutations;++j){
      if((gen()%6)==0 && s.size()<60000) {
        int pos=gen()%(s.size()+1);
        s.insert(pos,1,char(gen()%256));
      } else if((gen()%8)==0 && !s.empty()) {
        s.erase(s.begin()+(gen()%s.size()));
      } else if(!s.empty())s[gen()%s.size()]=char(gen()%256);
    }
    checkTheme(svc,sd,s,accept,reject);
  }
  // Long base64-like lines, too many lines, empty, >512 KiB, traversal.
  for(size_t n: {size_t(0),size_t(1),size_t(319),size_t(320),size_t(16384),size_t(65536),size_t(524288),size_t(524289)}) {
    std::string s=seed+std::string(n,'Z');
    checkTheme(svc,sd,s,accept,reject);
  }
  ThemeColors base=themeFor(ThemeId::Black);String name,error;
  assert(!svc.load(sd,"/Themes/../fuzz.vqeaf",base,name,error));
  assert(!svc.load(sd,"C:\\bad.vqeaf",base,name,error));
  std::cout<<"PASS theme mutation UBSan: 5000 deterministic cases plus oversized/path; accept="<<accept<<" rejected="<<reject<<"\n";
}
