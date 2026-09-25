// Real BrowserService against deterministic mocked HTTP; no live internet claim.
#define main original_browser_suite_main
#include "test_browser_v242.cpp"
#undef main
#include <fstream>
#include <iostream>
int main(int argc, char** argv){
  assert(argc==2);
  BrowserService b; assert(b.begin(nullptr));
  gFakeHttp.reset(200,-1,"<html><head><title>VQEAF Browser Test</title></head><body><h1>Qeafbrowser</h1><p>Host HTML loaded</p><a href=\"https://example.org/apps\">App downloads</a></body></html>");
  gFakeHttp.transferEncoding="";
  assert(b.load("https://example.org/start"));
  assert(b.linkCount()==1 && b.lineCount()>0 && b.status()==200);
  bool found=false;
  std::ofstream f(argv[1]); assert(f);
  f<<"{\"status\":"<<b.status()<<",\"title\":\""<<b.title()<<"\",\"url\":\""<<b.url()<<"\",\"lines\":[";
  for(int i=0;i<b.lineCount();i++){
    if(i){f<<",";}
    f<<"\""<<b.lineAt(i).text<<"\"";
    if(std::string(b.lineAt(i).text).find("Qeafbrowser")!=std::string::npos)found=true;
  }
  f<<"],\"link\":\""<<b.linkAt(0).url<<"\"}\n";
  assert(found);
  std::cout<<"BROWSER_LOADED|"<<b.status()<<"|"<<b.title()<<"|lines="<<b.lineCount()<<"|links="<<b.linkCount()<<"\n";
  gFakeHttp.reset(200,-1,"<html><body>Next page</body></html>");
  assert(b.openLink(0));
  assert(std::string(gFakeHttp.lastUrl)=="https://example.org/apps");
  std::cout<<"BROWSER_LINK_OPENED|https://example.org/apps\n";
}
