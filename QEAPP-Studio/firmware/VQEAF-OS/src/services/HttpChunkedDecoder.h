#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>

// Bounded HTTP/1.1 Transfer-Encoding: chunked decoder, C++11 / no heap.
// next() returns 0..255 or -1 for EOF/timeout. Sink receives at most 512 B.
// Network timeout, TLS verification and SD publication are caller concerns.
class HttpChunkedDecoder {
public:
  template <typename NextByte, typename Sink>
  static bool decodeTo(NextByte next, Sink sink, uint32_t maxBytes,
                       uint32_t &written, const char *&error) {
    written=0;
    error="Malformed chunked response";
    uint8_t block[512];
    while (true) {
      uint32_t size=0;
      size_t digits=0, lineLen=0;
      bool extension=false, cr=false;
      while (true) {
        int ch=next();
        if(ch<0 || ++lineLen>64)return false;
        if(ch=='\r') {cr=true;continue;}
        if(ch=='\n') {
          if(!cr || !digits)return false;
          break;
        }
        if(cr)return false;
        if(ch==';'&&digits) {extension=true;continue;}
        if(extension) {
          if(ch<0x20||ch>0x7e)return false;
          continue;
        }
        int d=(ch>='0'&&ch<='9')?ch-'0':
              (ch>='A'&&ch<='F')?ch-'A'+10:
              (ch>='a'&&ch<='f')?ch-'a'+10:-1;
        if(d<0||++digits>8)return false;
        size=(size<<4)|uint32_t(d);
      }
      if(size==0) {
        size_t totalTrailer=0,current=0;
        while(true) {
          int ch=next();
          if(ch<0||++totalTrailer>1024)return false;
          if(ch=='\r') {
            if(next()!='\n')return false;
            if(current==0) {
              if(!written) {error="Empty HTML response";return false;}
              error="";return true;
            }
            current=0;continue;
          }
          if(ch=='\n'||++current>256)return false;
        }
      }
      if(size>maxBytes-written) {
        error="Downloaded content exceeds size limit";
        return false;
      }
      uint32_t left=size;
      while(left) {
        size_t take=left>sizeof(block)?sizeof(block):left;
        for(size_t i=0;i<take;i++) {
          int ch=next();if(ch<0)return false;
          block[i]=uint8_t(ch);
        }
        if(!sink(block,take)) {error="Destination write failed";return false;}
        written+=take;left-=take;
      }
      if(next()!='\r'||next()!='\n')return false;
    }
  }

  template <typename NextByte>
  static bool decode(NextByte next,char *dst,size_t cap,size_t &written,
                     const char *&error) {
    written=0;error="Malformed chunked response";
    if(!dst || cap<2) return false;
    dst[0]=0;
    uint32_t total=0;
    auto sink=[&](const uint8_t*block,size_t n)->bool {
      memcpy(dst+total,block,n);
      return true;
    };
    bool ok=decodeTo(next,sink,uint32_t(cap-1),total,error);
    written=total;
    dst[written]=0;
    // Make decoder API stable for screen buffers. Disk callers use decodeTo().
    if(!ok && error && !strcmp(error,"Downloaded content exceeds size limit"))
      error="Page exceeds 32 KB limit";
    return ok;
  }
};
