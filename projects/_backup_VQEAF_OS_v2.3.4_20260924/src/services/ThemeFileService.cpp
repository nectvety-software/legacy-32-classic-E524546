#include "ThemeFileService.h"
#include <string.h>
#include <ctype.h>

constexpr int ThemeFileService::MAX_THEMES;
constexpr size_t ThemeFileService::MAX_FILE_BYTES;

static bool isVqeafPath(const String &path) {
  if (path.length() < 8 || path.length() >= 120 || path[0] != '/' ||
      path.indexOf("..") >= 0 || path.indexOf('\\') >= 0) return false;
  String lower = path;
  lower.toLowerCase();
  return lower.endsWith(".vqeaf");
}

static bool nextThemeLine(File &file, char *line, size_t cap, size_t &readTotal, bool &tooLong) {
  size_t len = 0;
  bool got = false;
  tooLong = false;
  while (file.available()) {
    int n = file.read();
    if (n < 0) break;
    got = true;
    if (++readTotal > ThemeFileService::MAX_FILE_BYTES) { tooLong = true; return false; }
    if (n == '\n') break;
    if (n == '\r') continue;
    if (len + 1 >= cap) { tooLong = true; return false; }
    line[len++] = (char)n;
  }
  line[len] = 0;
  return got;
}

static char *trimLine(char *s) {
  while (*s && isspace((unsigned char)*s)) ++s;
  size_t n = strlen(s);
  while (n && isspace((unsigned char)s[n - 1])) s[--n] = 0;
  return s;
}

static bool readQuoted(const char *s, char *out, size_t cap) {
  if (!s || !out || cap < 2) return false;
  const char *open = strchr(s, '"');
  if (!open) return false;
  const char *end = strchr(open + 1, '"');
  if (!end || end == open + 1 || (size_t)(end - (open + 1)) >= cap) return false;
  size_t n = (size_t)(end - open - 1);
  memcpy(out, open + 1, n);
  out[n] = 0;
  return true;
}

// VQEAF 1.0 supports #RGB, #ARGB, #RRGGBB and #AARRGGBB.
// Embedded flat-color renderer consumes RGB only; alpha is accepted but
// translucent compositor effects belong to Theme Studio, not this LCD.
static bool hexColor(const char *s, uint16_t &rgb) {
  // VQEAF Studio 1.0 defines #RGB, #ARGB, #RRGGBB, #AARRGGBB.
  // Alpha is discarded for the hardware RGB565 display. In particular an
  // all-zero glow ("#00000000") is legal and must not reject the theme.
  if(!s || s[0]!='#')return false;
  const size_t n=strlen(s+1);
  if(n!=3 && n!=4 && n!=6 && n!=8)return false;
  auto nibble=[](char ch)->int {
    if(ch>='0'&&ch<='9')return ch-'0';
    if(ch>='a'&&ch<='f')return ch-'a'+10;
    if(ch>='A'&&ch<='F')return ch-'A'+10;
    return -1;
  };
  const size_t shift=(n==4||n==8)?(n==4?1:2):0;
  const size_t step=n<=4?1:2;
  uint8_t channels[3]={0};
  for(int k=0;k<3;++k){
    int hi=nibble(s[1+shift+k*step]);if(hi<0)return false;
    if(step==1)channels[k]=uint8_t(hi*17);
    else {
      int lo=nibble(s[1+shift+k*step+1]);if(lo<0)return false;
      channels[k]=uint8_t((hi<<4)|lo);
    }
  }
  uint8_t r=channels[0],g=channels[1],b=channels[2];
  rgb=uint16_t(((r&0xF8)<<8)|((g&0xFC)<<3)|(b>>3));
  return true;
}

// RGB565 field override for the launcher extension; this extension is optional
// and intentionally resides immediately after palette, BEFORE large data-URI resources.
static int launcherKeyIndex(const char *key) {
  static const char *const keys[] = {"background","foreground","headerBg","headerFg","tabAccent",
      "listBg","listFg","selectedBg","selectedFg","previewBg","previewFg",
      "scrollbar","footerBg","footerFg","border"};
  for (int i=0; i<15; ++i) if (!strcmp(key,keys[i])) return i;
  return -1;
}
static void setLauncherColor(LauncherStyle &s, int i, uint16_t c) {
  switch (i) {
    case 0:s.background=c;break; case 1:s.foreground=c;break;
    case 2:s.headerBg=c;break; case 3:s.headerFg=c;break;
    case 4:s.tabAccent=c;break; case 5:s.listBg=c;break;
    case 6:s.listFg=c;break; case 7:s.selectedBg=c;break;
    case 8:s.selectedFg=c;break; case 9:s.previewBg=c;break;
    case 10:s.previewFg=c;break; case 11:s.scrollbar=c;break;
    case 12:s.footerBg=c;break; case 13:s.footerFg=c;break;
    case 14:s.border=c;break;
  }
}

bool ThemeFileService::parse(File &file, ThemeColors &out, String &name, String &error,
                             LauncherStyle *skin) {
  // The official Theme Studio places palette near the front. Other elements
  // (image base64, vectors, animations) are NOT executed or interpreted here.
  // Avoid buffering long base64 lines by parsing only the initial 16 KiB.
  ThemeColors c = themeFor(ThemeId::Classic);
  LauncherStyle overrides = LauncherStyle::fromPalette(c);
  uint16_t overrideMask = 0;
  char line[320];
  bool magic=false, paletteFound=false, paletteOpen=false, palettePending=false;
  bool launcherOpen=false, launcherPending=false;
  bool screen=false, accent=false, text=false, hasDim=false, hasPopup=false;
  size_t bytes=0;
  const size_t prefixCap = 16 * 1024;
  // Require complete <theme> envelope; seek only the trailing bytes of
  // potentially large files so scanning never allocates the whole theme.
  const size_t length = file.size();
  const size_t tailSize = length < 192 ? length : 192;
  if (length == 0 || length > MAX_FILE_BYTES || !file.seek(length-tailSize)) {
    error="Invalid or oversized theme";return false;
  }
  char tail[193];
  size_t tailRead=file.read(reinterpret_cast<uint8_t*>(tail),tailSize);
  tail[tailRead]=0;
  if (!strstr(tail,"</theme>")) {error="Theme closing tag missing";return false;}
  if (!file.seek(0)) {error="Theme seek failed";return false;}
  for (int lines=0; lines<400 && file.available() && bytes<prefixCap; ++lines) {
    bool tooLong=false;
    if (!nextThemeLine(file,line,sizeof line,bytes,tooLong)) {
      // After palette and optional launcher we deliberately skip everything
      // including data-URI source: a base64 line may exceed 320 characters.
      if (paletteFound && !paletteOpen && !launcherOpen && tooLong) break;
      error=tooLong?"Theme header line too long":"Theme read failed";return false;
    }
    char *s=trimLine(line);
    if (!magic && (unsigned char)s[0]==0xEF && (unsigned char)s[1]==0xBB && (unsigned char)s[2]==0xBF)s+=3;
    if (!*s || s[0]=='#' || (s[0]=='/'&&s[1]=='/'))continue;
    if (!magic) {
      if (strncmp(s,"@vqeaf 1.",9)) {error="Expected @vqeaf 1.x";return false;}
      magic=true;continue;
    }
    if (strncmp(s,"<theme ",7)==0) {
      const char *val=strstr(s,"name=");char label[40];
      if(val && readQuoted(val+5,label,sizeof label))name=label;
      continue;
    }
    if (palettePending && *s=='{') {paletteOpen=true;paletteFound=true;palettePending=false;continue;}
    if (launcherPending && *s=='{') {launcherOpen=true;launcherPending=false;continue;}
    if (!paletteOpen && !launcherOpen) {
      if (strstr(s,"</theme>")) break;
      if (strncmp(s,"palette",7)==0 && (s[7]=='{' || s[7]==0 || isspace((unsigned char)s[7]))) {
        if (strchr(s,'{')){paletteOpen=true;paletteFound=true;}
        else palettePending=true;
        continue;
      }
      if (strncmp(s,"launcher",8)==0 && (s[8]=='{' || s[8]==0 || isspace((unsigned char)s[8]))) {
        if (strchr(s,'{'))launcherOpen=true;
        else launcherPending=true;
        continue;
      }
      if (paletteFound && (strncmp(s,"<component",10)==0 || strncmp(s,"<resource",9)==0 ||
                           strncmp(s,"<vector",7)==0 || strncmp(s,"<animation",10)==0)) break;
      continue;
    }
    if (*s=='}') {paletteOpen=false;launcherOpen=false;continue;}
    char *colon=strchr(s,':');if(!colon)continue;*colon++=0;
    const char *key=trimLine(s);
    if (launcherOpen) {
      const int i=launcherKeyIndex(key);
      if (i<0)continue;
      char value[24];uint16_t pixel;
      if(!readQuoted(colon,value,sizeof value)||!hexColor(value,pixel)) {
        error="Invalid launcher color";return false;
      }
      setLauncherColor(overrides,i,pixel);overrideMask |= uint16_t(1U<<i);
      continue;
    }
    const char *known[]={"screen","key","panel","keyPressed","selected","keyBorder",
      "border","keyText","subText","shellTop","titlebar","chromeText","shellBottom","shellBorder","accent","glow"};
    bool recognized=false;for (const char *k:known)if(!strcmp(key,k)){recognized=true;break;}
    if(!recognized)continue;
    char value[24];uint16_t color;
    if(!readQuoted(colon,value,sizeof value)||!hexColor(value,color)) {
      error="Invalid RGB hex in palette";return false;
    }
    if(!strcmp(key,"screen")){c.bg=color;screen=true;}
    else if(!strcmp(key,"key")||!strcmp(key,"panel"))c.panel=color;
    else if(!strcmp(key,"keyPressed")||!strcmp(key,"selected")){c.selected=color;c.popupSelected=color;}
    else if(!strcmp(key,"keyBorder")||!strcmp(key,"border"))c.border=color;
    else if(!strcmp(key,"keyText")){c.text=color;c.popupText=color;c.chromeText=color;text=true;}
    else if(!strcmp(key,"subText")){c.dim=color;hasDim=true;}
    else if(!strcmp(key,"shellTop")||!strcmp(key,"titlebar"))c.chrome=color;
    else if(!strcmp(key,"chromeText"))c.chromeText=color;
    else if(!strcmp(key,"shellBottom")){c.popup=color;hasPopup=true;}
    else if(!strcmp(key,"accent")){c.accent=color;accent=true;}
    else if(!strcmp(key,"glow"))c.danger=color;
  }
  if (!magic || !paletteFound || paletteOpen || launcherOpen || palettePending || launcherPending ||
      !screen || !accent || !text) {error="Theme needs complete palette screen/keyText/accent";return false;}
  if (!hasDim)c.dim=c.text;
  if (!hasPopup)c.popup=c.panel;
  if(skin) {
    LauncherStyle computed=LauncherStyle::fromPalette(c);
    for(int i=0;i<15;++i)if(overrideMask & (1U<<i)){
      // copy by field through a bounded fixed-size slot mapping
      const uint16_t *vals=&overrides.background;
      setLauncherColor(computed,i,vals[i]);
    }
    *skin=computed;
  }
  out=c;error="";return true;
}

int ThemeFileService::scan(StorageService &storage) {
  used = 0;
  cardPresent = storage.mounted();
  if (!cardPresent) return 0;
  // This is intentionally capped to avoid RAM spikes on cards with thousands of files.
  const char *ext[] = {".vqeaf"};
  FsEntry found[MAX_THEMES];
  // Prioritize the standard /Themes folder. A bounded secondary root scan
  // finds themes elsewhere on the card without duplicate entries.
  const char *roots[] = {StoragePaths::THEMES, "/Themes", "/"};
  for (int folder = 0; folder < 3 && used < MAX_THEMES; ++folder) {
    const int n = storage.scanMedia(roots[folder], ext, 1, found, MAX_THEMES, 2);
    for (int i = 0; i < n && used < MAX_THEMES; ++i) {
      if (!isVqeafPath(found[i].path) || found[i].size > MAX_FILE_BYTES ||
          find(found[i].path) >= 0) continue;
      // Bad extension or random binary .vqeaf must not use a catalog slot.
      { File probe=storage.fs().open(found[i].path,FILE_READ);
        uint8_t magic[16]={0};size_t n=probe?probe.read(magic,15):0;
        if(probe)probe.close();
        size_t b=(n>=3 && magic[0]==0xEF && magic[1]==0xBB && magic[2]==0xBF)?3:0;
        if(n-b<9 || memcmp(magic+b,"@vqeaf 1.",9)!=0)continue;
      }
      Entry &e = entries[used];
      snprintf(e.path, sizeof e.path, "%s", found[i].path.c_str());
      snprintf(e.label, sizeof e.label, "%s", found[i].name.c_str());
      e.bytes = (uint32_t)found[i].size;
      File file = storage.fs().open(found[i].path, FILE_READ);
      if (file) {
        char b[320]; size_t got = file.read((uint8_t *)b, sizeof(b) - 1);
        b[got] = 0;
        const char *p = strstr(b, "<theme ");
        if (p) {
          const char *n = strstr(p, "name=");
          if (n) {
            char label[40];
            if (readQuoted(n + 5, label, sizeof label))
              snprintf(e.label, sizeof e.label, "%s", label);
          }
        }
        file.close();
      }
      ++used;
    }
  }
  return used;
}

int ThemeFileService::find(const String &path) const {
  for (int i = 0; i < used; ++i) if (path == entries[i].path) return i;
  return -1;
}

bool ThemeFileService::load(StorageService &storage, const String &path, ThemeColors &out,
                            String &name, String &error, LauncherStyle *skin) const {
  if (!storage.mounted()) { error = "microSD is not mounted"; return false; }
  if (!isVqeafPath(path)) { error = "Invalid theme path"; return false; }
  File file = storage.fs().open(path, FILE_READ);
  if (!file || file.isDirectory()) { if (file) file.close(); error = "Theme not found"; return false; }
  if (file.size() > MAX_FILE_BYTES) { file.close(); error = "Theme file is too large"; return false; }
  if (!name.length()) name = path.substring(path.lastIndexOf('/') + 1);
  const bool ok = parse(file, out, name, error, skin);
  file.close();
  return ok;
}
