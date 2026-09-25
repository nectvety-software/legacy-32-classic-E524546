#pragma once
#include "Arduino.h"
#include <memory>
#include <string>
#include <vector>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
class File {
  struct Handle {
    FILE *fp=nullptr;DIR *dir=nullptr;std::string path;size_t len=0;bool directory=false;
    ~Handle(){if(fp)fclose(fp);if(dir)closedir(dir);}
  };
  std::shared_ptr<Handle> h;
  static std::string virtualRoot;
  static std::string displayed(const std::string &abs){return abs.find(virtualRoot)==0?abs.substr(virtualRoot.size()):abs;}
public:
  File(){}explicit File(const std::string &path,const char *mode){
    struct stat st;if(!stat(path.c_str(),&st)&&S_ISDIR(st.st_mode)){
      if(DIR *dir=opendir(path.c_str())){h.reset(new Handle);h->dir=dir;h->path=path;h->directory=true;}
    }else if(FILE *f=fopen(path.c_str(),mode)){
      h.reset(new Handle);h->fp=f;h->path=path;struct stat stf;if(!stat(path.c_str(),&stf))h->len=stf.st_size;
    }
  }
  static void setRoot(const std::string &r){virtualRoot=r;}
  explicit operator bool()const{return bool(h);}
  bool isDirectory()const{return h&&h->directory;}
  uint64_t size()const{return h?h->len:0;}
  const char *name()const{static std::string name;name=h?displayed(h->path):"";return name.c_str();}
  int read(uint8_t *out,size_t n){return h&&h->fp?(int)fread(out,1,n,h->fp):0;}
  size_t readBytes(char *out,size_t n){return read(reinterpret_cast<uint8_t*>(out),n);}
  bool seek(size_t pos){return h&&h->fp&&fseek(h->fp,pos,SEEK_SET)==0;}
  size_t write(const uint8_t *b,size_t n){if(!h||!h->fp)return 0;size_t r=fwrite(b,1,n,h->fp);if(ftell(h->fp)>=0)h->len=ftell(h->fp);return r;}
  void flush(){if(h&&h->fp)fflush(h->fp);}
  void close(){h.reset();}
  File openNextFile(){
    if(!h||!h->dir)return File();
    dirent *de;
    while((de=readdir(h->dir)))if(strcmp(de->d_name,".")&&strcmp(de->d_name,"..")){
      return File(h->path+"/"+de->d_name,FILE_READ);
    }
    return File();
  }
};
namespace fs {
class FS {
  std::string root;
  std::string resolve(const String &path){return root+std::string(path.c_str());}
public:
  explicit FS(const std::string &r=""):root(r){File::setRoot(r);}
  File open(const String &p,const char *mode=FILE_READ){return File(resolve(p),mode);}
  bool exists(const String &p){struct stat st;return stat(resolve(p).c_str(),&st)==0;}
  bool mkdir(const String &p){return ::mkdir(resolve(p).c_str(),0777)==0;}
  bool rmdir(const String &p){return ::rmdir(resolve(p).c_str())==0;}
  bool rename(const String &src,const String &dst){return ::rename(resolve(src).c_str(),resolve(dst).c_str())==0;}
  bool remove(const String &p){return ::unlink(resolve(p).c_str())==0;}
};
}
