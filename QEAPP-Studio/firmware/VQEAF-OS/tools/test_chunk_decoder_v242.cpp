#include "../src/services/HttpChunkedDecoder.h"
#include <cassert>
#include <cstring>
#include <string>
#include <cstdio>

struct Guard {char before[8];char payload[32];char after[8];};
static bool decode(const std::string &wire,Guard &buf,size_t &length,const char *&error){
 size_t index=0;
 auto next = [&]() -> int {return index<wire.size() ? (unsigned char)wire[index++] : -1;};
 return HttpChunkedDecoder::decode(next,buf.payload,sizeof(buf.payload),length,error);
}
int main(){
 Guard b={};size_t n;const char *error="";
 assert(decode("5\r\nHello\r\n1;ext=yes\r\n!\r\n0\r\nX-a: b\r\n\r\n",b,n,error));
 assert(n==6&&std::strcmp(b.payload,"Hello!")==0);
 assert(!decode("20\r\n"+std::string(32,'x')+"\r\n0\r\n\r\n",b,n,error));
 assert(std::strcmp(error,"Page exceeds 32 KB limit")==0);
 assert(!decode("0\r\n\r\n",b,n,error));
 assert(std::strcmp(error,"Empty HTML response")==0);
 assert(!decode("g\r\nX\r\n0\r\n\r\n",b,n,error));
 // Real `.qeapp` downloader writes at most 512 bytes per call, including
 // chunk lengths above the small (32 KiB) HTML-renderer limit.
 std::string payload(2048,'Q');
 std::string frame="800\r\n"+payload+"\r\n0\r\n\r\n";
 unsigned idx=0;uint32_t decoded=0,writes=0;
 auto next=[&]()->int {return idx<frame.size()?(unsigned char)frame[idx++]:-1;};
 auto sink=[&](const uint8_t *data,size_t len)->bool {
   assert(len<=512);
   for(size_t j=0;j<len;++j)assert(data[j]=='Q');
   writes++;return true;
 };
 assert(HttpChunkedDecoder::decodeTo(next,sink,4*1024*1024,decoded,error));
 assert(decoded==2048&&writes==4);
 frame="400001\r\n";idx=0;
 assert(!HttpChunkedDecoder::decodeTo(next,sink,4*1024*1024,decoded,error));
 assert(std::strcmp(error,"Downloaded content exceeds size limit")==0);
 for(int i=0;i<1000;i++) {
   std::string garbage;
   unsigned x=uint32_t(i+1)*2654435761U;
   for(int j=0;j<i%90;j++){x^=x<<13;x^=x>>17;x^=x<<5;garbage.push_back(char(x&255));}
   std::memset(&b,0x7a,sizeof(b));
   decode(garbage,b,n,error);
   for(char ch:b.before)assert((unsigned char)ch==0x7a);
   for(char ch:b.after)assert((unsigned char)ch==0x7a);
 }
 puts("PASS 1000 malformed chunk framing/bounds + valid/trailer/size/empty");
}
