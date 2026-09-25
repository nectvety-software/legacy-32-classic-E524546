#include "QeappFormat.h"
#include <string.h>

namespace Qeapp {
Meta::Meta() : id{0}, name{0}, version{0}, type{0}, entry{0}, hasIcon(false) {}
static inline uint32_t rotr(uint32_t a,unsigned n){return (a>>n)|(a<<(32-n));}
static const uint32_t K[64]={
0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
Sha256::Sha256() : state{0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19},block{0},used(0),processed(0) {}
void Sha256::transform(){
 uint32_t w[64];for(int i=0;i<16;i++)w[i]=(uint32_t(block[4*i])<<24)|(uint32_t(block[4*i+1])<<16)|(uint32_t(block[4*i+2])<<8)|block[4*i+3];
 for(int i=16;i<64;i++){uint32_t s0=rotr(w[i-15],7)^rotr(w[i-15],18)^(w[i-15]>>3);uint32_t s1=rotr(w[i-2],17)^rotr(w[i-2],19)^(w[i-2]>>10);w[i]=w[i-16]+s0+w[i-7]+s1;}
 uint32_t a=state[0],b=state[1],c=state[2],d=state[3],e=state[4],f=state[5],g=state[6],h=state[7];
 for(int i=0;i<64;i++){uint32_t s1=rotr(e,6)^rotr(e,11)^rotr(e,25);uint32_t ch=(e&f)^((~e)&g);uint32_t t1=h+s1+ch+K[i]+w[i];uint32_t s0=rotr(a,2)^rotr(a,13)^rotr(a,22);uint32_t maj=(a&b)^(a&c)^(b&c);uint32_t t2=s0+maj;h=g;g=f;f=e;e=d+t1;d=c;c=b;b=a;a=t1+t2;}
 state[0]+=a;state[1]+=b;state[2]+=c;state[3]+=d;state[4]+=e;state[5]+=f;state[6]+=g;state[7]+=h;
}
void Sha256::update(const uint8_t *data,size_t n){if(!data)return;processed+=n;for(size_t i=0;i<n;i++){block[used++]=data[i];if(used==64){transform();used=0;}}}
void Sha256::finish(uint8_t out[32]){uint64_t bits=processed*8;block[used++]=0x80;if(used>56){while(used<64)block[used++]=0;transform();used=0;}while(used<56)block[used++]=0;for(int i=7;i>=0;i--)block[used++]=uint8_t(bits>>(8*i));transform();for(int i=0;i<8;i++)for(int b=3;b>=0;b--)out[4*i+(3-b)]=uint8_t(state[i]>>(8*b));}
bool equalHash(const uint8_t a[32],const uint8_t b[32]){uint8_t diff=0;for(int i=0;i<32;i++)diff|=a[i]^b[i];return diff==0;}
static uint32_t le32(const uint8_t *p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
bool parseHeader(const uint8_t bytes[HEADER_BYTES],uint64_t fileSize,Header &out,const char *&error){
 error="";static const uint8_t magic[8]={'Q','E','A','P','P','2','\r','\n'};
 if(memcmp(bytes,magic,8)){error=memcmp(bytes,"QEAPP1\r\n",8)==0 ? "Unsigned QEAPP/1 rejected" : "Not a signed QEAPP/2 package";return false;}
 out.manifestLen=le32(bytes+8);out.iconLen=le32(bytes+12);out.payloadLen=le32(bytes+16);
 if(!out.manifestLen||out.manifestLen>MAX_MANIFEST||
    (out.iconLen!=0&&out.iconLen!=ICON_BYTES)||out.payloadLen>MAX_PAYLOAD){error="Package section too large";return false;}
 uint64_t expected=uint64_t(HEADER_BYTES)+out.manifestLen+out.iconLen+out.payloadLen+76; // signature trailer
 if(fileSize!=expected){error="Invalid or truncated package length";return false;}
 memcpy(out.manifestHash,bytes+20,32);memcpy(out.iconHash,bytes+52,32);memcpy(out.payloadHash,bytes+84,32);return true;
}
static bool copyVal(char *dst,size_t cap,const char *src,size_t n){if(!n||n>=cap)return false;for(size_t i=0;i<n;i++)if((unsigned char)src[i]<32||(unsigned char)src[i]>126)return false;memcpy(dst,src,n);dst[n]=0;return true;}
bool parseManifest(const char *src,size_t n,Meta &out,const char *&error){
 error="";if(!src||!n||n>MAX_MANIFEST){error="Invalid manifest length";return false;}
 out=Meta();uint8_t fields=0;
 for(size_t pos=0;pos<n;){size_t end=pos;while(end<n&&src[end]!='\n')end++;size_t len=end-pos;if(len&&src[pos+len-1]=='\r')len--;
  if(len){if(len>256){error="Manifest line too long";return false;}const char *line=src+pos;size_t eq=0;while(eq<len&&line[eq]!='=')eq++;if(eq==len){error="Expected key=value";return false;}
   const char *v=line+eq+1;size_t vn=len-eq-1;
   if(eq==2&&!memcmp(line,"id",2)){if(fields&1||!copyVal(out.id,sizeof out.id,v,vn)){error="Invalid app id";return false;}fields|=1;}
   else if(eq==4&&!memcmp(line,"name",4)){if(fields&2||!copyVal(out.name,sizeof out.name,v,vn)){error="Invalid app name";return false;}fields|=2;}
   else if(eq==7&&!memcmp(line,"version",7)){if(fields&4||!copyVal(out.version,sizeof out.version,v,vn)){error="Invalid version";return false;}fields|=4;}
   else if(eq==4&&!memcmp(line,"type",4)){if(fields&8||!copyVal(out.type,sizeof out.type,v,vn)){error="Invalid app type";return false;}fields|=8;}
   else if(eq==5&&!memcmp(line,"entry",5)){if(fields&16||!copyVal(out.entry,sizeof out.entry,v,vn)){error="Invalid entry";return false;}fields|=16;}
   else {error="Unknown manifest key";return false;}}
  pos=end<n?end+1:n;
 }
 if((fields&15)!=15){error="Missing id/name/version/type";return false;}
 for(const char *p=out.id;*p;p++)if(!((*p>='a'&&*p<='z')||(*p>='0'&&*p<='9')||*p=='_'||*p=='-')){error="Unsafe app id";return false;}
 bool versionDigit=false;
 if(out.version[0]=='.'){error="Invalid version syntax";return false;}
 for(const char *p=out.version;*p;p++){
   if(*p>='0'&&*p<='9')versionDigit=true;
   else if(*p!='.'||p[1]==0||p[1]=='.'){error="Invalid version syntax";return false;}
 }
 if(!versionDigit){error="Invalid version syntax";return false;}
 if(!strcmp(out.type,"web")){if(!(fields&16)||strncmp(out.entry,"https://",8)){error="Web apps require HTTPS entry";return false;} // reject control characters and shell-style pseudo URLs
   const char *host=out.entry+8;if(!*host||*host=='/'||strchr(out.entry,' ')){error="Invalid HTTPS URL";return false;}}
 else if(strcmp(out.type,"text")!=0||(fields&16)){error="Unsupported app type or text entry";return false;}
 return true;
}
} // namespace Qeapp
