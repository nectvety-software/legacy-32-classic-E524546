#include "ImageViewerService.h"
#include <TJpg_Decoder.h>
#include <PNGdec.h>
#include <esp_heap_caps.h>
#include <new>

namespace {
struct JpegCtx {
  TFT_eSPI *tft = nullptr;
  int x = 0, y = 0, right = 0, bottom = 0;
};
static JpegCtx jpgCtx;

static bool jpegOutput(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap) {
  if (!jpgCtx.tft || !bitmap) return false;
  const int dx = jpgCtx.x + x;
  const int dy = jpgCtx.y + y;
  if (dy >= jpgCtx.bottom || dx >= jpgCtx.right) return false;

  int cw = jpgCtx.right - dx;
  int ch = jpgCtx.bottom - dy;
  if (cw > (int)w) cw = w;
  if (ch > (int)h) ch = h;
  if (cw <= 0 || ch <= 0) return true;

  // TJpg_Decoder gives MCU tiles with a stride of the original tile width.
  // When a tile touches the right/bottom edge, draw row-by-row so clipping does
  // not reinterpret the source stride and corrupt the following rows.
  if (cw == (int)w && ch == (int)h) {
    jpgCtx.tft->pushImage(dx, dy, w, h, bitmap);
  } else {
    for (int yy = 0; yy < ch; ++yy)
      jpgCtx.tft->pushImage(dx, dy + yy, cw, 1, bitmap + yy * w);
  }
  return true;
}

struct PngCtx {
  TFT_eSPI *tft = nullptr;
  PNG *png = nullptr;
  int srcW = 0, srcH = 0, outW = 0, outH = 0, x = 0, y = 0;
};
static PngCtx pngCtx;
static PNG *pngObj = nullptr;
static uint16_t pngLine[1024];
static uint16_t scaledLine[240];

static PNG *getPngObject() {
  if (!pngObj) {
    void *p = heap_caps_malloc(sizeof(PNG), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!p) p = malloc(sizeof(PNG));
    if (p) pngObj = new (p) PNG;
  }
  return pngObj;
}

static int pngDraw(PNGDRAW *draw) {
  if (!pngCtx.tft || !pngCtx.png || !draw || pngCtx.srcW < 1 || pngCtx.srcW > 1024) return 1;
  pngCtx.png->getLineAsRGB565(draw, pngLine, PNG_RGB565_LITTLE_ENDIAN, 0xFFFFFFFF);
  int oy = (draw->y * pngCtx.outH) / pngCtx.srcH;
  int nextOy = ((draw->y + 1) * pngCtx.outH) / pngCtx.srcH;
  if (nextOy <= oy || oy < 0 || oy >= pngCtx.outH) return 1;
  for (int ox = 0; ox < pngCtx.outW && ox < 240; ++ox) {
    int sx = (ox * pngCtx.srcW) / pngCtx.outW;
    scaledLine[ox] = pngLine[sx];
  }
  for (int yy = oy; yy < min(nextOy, pngCtx.outH); ++yy)
    pngCtx.tft->pushImage(pngCtx.x, pngCtx.y + yy, pngCtx.outW, 1, scaledLine);
  return 1;
}

static uint16_t rd16(File &f) {
  uint8_t b[2]; if (f.read(b,2)!=2) return 0; return uint16_t(b[0]) | (uint16_t(b[1])<<8);
}
static uint32_t rd32(File &f) {
  uint8_t b[4]; if (f.read(b,4)!=4) return 0; return uint32_t(b[0]) | (uint32_t(b[1])<<8) | (uint32_t(b[2])<<16) | (uint32_t(b[3])<<24);
}
}

bool ImageViewerService::drawFile(TFT_eSPI &tft, fs::FS &fs, const String &path,
                                  int x, int y, int w, int h, String &error) {
  error = "";
  String lower = path; lower.toLowerCase();
  if (lower.endsWith(".bmp")) return drawBmp(tft, fs, path, x, y, w, h, error);
  if (lower.endsWith(".jpg") || lower.endsWith(".jpeg")) return drawJpeg(tft, fs, path, x, y, w, h, error);
  if (lower.endsWith(".png")) return drawPng(tft, fs, path, x, y, w, h, error);
  error = "Unsupported image format";
  return false;
}

bool ImageViewerService::drawJpeg(TFT_eSPI &tft, fs::FS &fs, const String &path,
                                  int x, int y, int w, int h, String &error) {
  File f = fs.open(path, FILE_READ);
  if (!f) { error = "Cannot open JPEG"; return false; }
  size_t len = f.size();
  if (!len || len > 3UL * 1024UL * 1024UL) { f.close(); error = "JPEG too large"; return false; }
  uint8_t *buf = (uint8_t*)heap_caps_malloc(len, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!buf) { f.close(); error = "Not enough PSRAM"; return false; }
  size_t got = f.read(buf, len); f.close();
  if (got != len) { free(buf); error = "JPEG read error"; return false; }

  uint16_t sw=0, sh=0;
  if (!TJpgDec.getJpgSize(&sw, &sh, buf, (uint32_t)len) || !sw || !sh) {
    free(buf); error = "Invalid JPEG"; return false;
  }
  uint8_t scale=1;
  while (scale < 8 && ((sw/scale) > w || (sh/scale) > h)) scale <<= 1;
  int ow=max(1,(int)sw/scale), oh=max(1,(int)sh/scale);
  jpgCtx.tft=&tft; jpgCtx.x=x+(w-ow)/2; jpgCtx.y=y+(h-oh)/2; jpgCtx.right=x+w; jpgCtx.bottom=y+h;
  TJpgDec.setCallback(jpegOutput);
  TJpgDec.setJpgScale(scale);
  tft.setSwapBytes(true);
  TJpgDec.drawJpg(0, 0, buf, (uint32_t)len);
  tft.setSwapBytes(false);
  jpgCtx.tft=nullptr;
  free(buf);
  return true;
}

bool ImageViewerService::drawPng(TFT_eSPI &tft, fs::FS &fs, const String &path,
                                 int x, int y, int w, int h, String &error) {
  File f = fs.open(path, FILE_READ);
  if (!f) { error = "Cannot open PNG"; return false; }
  size_t len=f.size();
  if (!len || len > 3UL*1024UL*1024UL) { f.close(); error="PNG too large"; return false; }
  uint8_t *buf=(uint8_t*)heap_caps_malloc(len, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!buf) { f.close(); error="Not enough PSRAM"; return false; }
  size_t got=f.read(buf,len); f.close();
  if (got!=len) { free(buf); error="PNG read error"; return false; }

  PNG *png=getPngObject();
  if (!png) { free(buf); error="PNG decoder unavailable"; return false; }
  PNG_DRAW_CALLBACK *cb=pngDraw;
  int rc=png->openRAM(buf,(int)len,cb);
  if (rc!=PNG_SUCCESS) { free(buf); error="Invalid PNG"; return false; }
  int sw=png->getWidth(), sh=png->getHeight();
  if (sw<1 || sh<1 || sw>1024) { png->close(); free(buf); error="PNG dimensions unsupported"; return false; }
  int ow=w, oh=(sh*w)/sw;
  if (oh>h) { oh=h; ow=(sw*h)/sh; }
  ow=constrain(ow,1,min(w,240)); oh=constrain(oh,1,h);
  pngCtx.tft=&tft; pngCtx.png=png; pngCtx.srcW=sw; pngCtx.srcH=sh; pngCtx.outW=ow; pngCtx.outH=oh;
  pngCtx.x=x+(w-ow)/2; pngCtx.y=y+(h-oh)/2;
  rc=png->decode(nullptr,0);
  png->close();
  pngCtx.tft=nullptr; pngCtx.png=nullptr;
  free(buf);
  if (rc!=PNG_SUCCESS) { error="PNG decode error"; return false; }
  return true;
}

bool ImageViewerService::drawBmp(TFT_eSPI &tft, fs::FS &fs, const String &path,
                                 int x, int y, int w, int h, String &error) {
  File f=fs.open(path,FILE_READ);
  if (!f) { error="Cannot open BMP"; return false; }
  if (rd16(f)!=0x4D42) { f.close(); error="Invalid BMP"; return false; }
  (void)rd32(f); (void)rd32(f); uint32_t dataOffset=rd32(f);
  uint32_t dib=rd32(f); if (dib<40) { f.close(); error="BMP header unsupported"; return false; }
  int32_t sw=(int32_t)rd32(f), sh=(int32_t)rd32(f);
  uint16_t planes=rd16(f), bpp=rd16(f); uint32_t compression=rd32(f);
  if (planes!=1 || bpp!=24 || compression!=0 || sw<=0 || sh==0 || sw>4096 || abs(sh)>4096) {
    f.close(); error="Use uncompressed 24-bit BMP"; return false;
  }
  bool bottomUp=sh>0; int srcH=abs(sh);
  int ow=w, oh=(srcH*w)/sw; if (oh>h) { oh=h; ow=(sw*h)/srcH; }
  ow=constrain(ow,1,w); oh=constrain(oh,1,h);
  int dx=x+(w-ow)/2, dy=y+(h-oh)/2;
  size_t rowBytes=((size_t)sw*3+3)&~3U;
  uint8_t *row=(uint8_t*)heap_caps_malloc(rowBytes,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
  uint16_t *out=(uint16_t*)heap_caps_malloc(ow*2,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
  if (!row || !out) { if(row)free(row); if(out)free(out); f.close(); error="Not enough memory"; return false; }
  for (int oy=0; oy<oh; ++oy) {
    int sy=(oy*srcH)/oh; if (bottomUp) sy=srcH-1-sy;
    f.seek(dataOffset+(uint32_t)sy*rowBytes);
    if (f.read(row,rowBytes)!=(int)rowBytes) { free(row); free(out); f.close(); error="BMP read error"; return false; }
    for (int ox=0; ox<ow; ++ox) {
      int sx=(ox*sw)/ow; uint8_t *p=row+sx*3;
      out[ox]=tft.color565(p[2],p[1],p[0]);
    }
    tft.pushImage(dx,dy+oy,ow,1,out);
  }
  free(row); free(out); f.close(); return true;
}
