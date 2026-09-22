// On-device LLM chat app (ported from the PLE TinyLM reference firmware).
// Runs a model file (.bin) straight from the SD card inside PochitaOS:
//   - PLE  quantized TinyLM   -> lib/ple_tinylm/llm.h
//   - VN   byte-level Vi model -> lib/ple_tinylm/llm_vn.h
// The chat UI (terminal + on-screen keyboard) is drawn through the shared
// `tft` instance using the 8x16 bitmap fonts from lib/ple_tinylm, with input
// via the OS ButtonManager.
#include <Arduino.h>
#include <SD.h>
#include <dirent.h>
#include "component/ButtonManager.h"
#include "ai_chat.h"
#include "system_launch.h"
#include "llm.h"
#include "llm_vn.h"
#include "vocab.h"
#include "font8x16.h"
#include "viet8x16.h"

// ----------------------------------------------------------------------------
// Palette (same terminal scheme as the reference UI)
// ----------------------------------------------------------------------------
#define TFT_BLACK   0x0000
#define TFT_WHITE   0xFFFF
#define TFT_GREEN   0x07E0
#define TFT_RED     0xF800
#define TFT_YELLOW  0xFFE0
#define TFT_GRAY    0x18E3

#define SCR_W   240
#define SCR_H   320
#define FONT_H  16
#define FONT_W  8
#define CHAT_LEN (SCR_W / FONT_W)  // 30 chars/line

#define COL_BG       TFT_BLACK
#define COL_PROMPT   TFT_GREEN
#define COL_AI_TXT   TFT_WHITE
#define COL_STAT_BG  0x2104
#define COL_STAT_TXT TFT_WHITE
#define COL_KEY_BG   0x2945
#define COL_INP_TXT  TFT_GREEN

// ----------------------------------------------------------------------------
// UTF-8 + Vietnamese glyph drawing on the shared tft (8x16, 1bpp bitmaps)
// ----------------------------------------------------------------------------
static void aiDrawStr(const char *s, int32_t x, int32_t y, uint16_t color) {
  int32_t xo = x;
  for (; *s;) {
    unsigned char c = (unsigned char)*s;
    if (c == '\n') { x = xo; y += FONT_H; s++; continue; }
    if (c < 0x80) {
      tft.drawBitmap(x, y, FontLib8x16 + (size_t)c * 16, 8, FONT_H, color);
      x += FONT_W; s++; continue;
    }
    uint16_t cp = 0; int adv = 1;
    if (c >= 0xE0 && (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80) {
      cp = ((uint16_t)(c & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F);
      adv = 3;
    } else if (c >= 0xC0 && (s[1] & 0xC0) == 0x80) {
      cp = ((uint16_t)(c & 0x1F) << 6) | (s[1] & 0x3F);
      adv = 2;
    }
    bool drawn = false;
    for (int i = 0; i < VIET_N; i++) {
      if (VIET_CP[i] == cp) {
        tft.drawBitmap(x, y, VIET_GLYPH[i], 8, FONT_H, color);
        drawn = true;
        break;
      }
    }
    if (!drawn) tft.drawBitmap(x, y, FontLib8x16, 8, FONT_H, color);
    x += FONT_W;
    s += adv;
  }
}

// ----------------------------------------------------------------------------
// Chat buffer (terminal-style lines)
// ----------------------------------------------------------------------------
#define CHAT_MAX 300
static struct { char text[CHAT_LEN + 1]; bool is_user; } chat[CHAT_MAX];
static int chat_total = 0;
static int chat_top = 0;
static int kbd_vis_lines = 0;

static int mn(int a, int b) { return a < b ? a : b; }
static int mx(int a, int b) { return a > b ? a : b; }

static int utf8_step(const char *s) {
  unsigned char c = (unsigned char)*s;
  if (c >= 0xE0 && (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80) return 3;
  if (c >= 0xC0 && (s[1] & 0xC0) == 0x80) return 2;
  return 1;
}

static void sanitize_ascii(char *s) {
  for (; *s; s++) {
    unsigned char c = (unsigned char)*s;
    if (c < 0x20) *s = ' ';
  }
}

static void add_line(const char *text, bool is_user) {
  int i = chat_total % CHAT_MAX;
  strncpy(chat[i].text, text, CHAT_LEN); chat[i].text[CHAT_LEN] = 0;
  chat[i].is_user = is_user;
  chat_total++;
  chat_top = mx(0, chat_total - kbd_vis_lines);
}

static void add_text(const char *s, bool is_user) {
  while (*s) {
    int n = 0;
    while (s[n]) {
      if (s[n] == '\n') break;
      int cl = utf8_step(s + n);
      if (n + cl > CHAT_LEN) break;
      n += cl;
    }
    char buf[CHAT_LEN + 1];
    strncpy(buf, s, n); buf[n] = 0;
    sanitize_ascii(buf);
    add_line(buf, is_user);
    s += n;
    if (*s == '\n') s++;
  }
}

static void display_print(const char *s) { add_text(s, false); }
static void display_print_user(const char *s) { add_text(s, true); }

static void display_append(const char *s) {
  if (chat_total == 0) { display_print(s); return; }
  int last = (chat_total - 1) % CHAT_MAX;
  if (chat[last].is_user) { display_print(s); return; }
  int cur = strlen(chat[last].text);
  int slen = strlen(s);
  if (cur + slen <= CHAT_LEN) {
    memcpy(chat[last].text + cur, s, slen + 1);
    sanitize_ascii(chat[last].text + cur);
  } else {
    int space = CHAT_LEN - cur;
    int used = 0;
    while (used < space && s[used]) {
      int cl = utf8_step(s + used);
      if (used + cl > space) break;
      used += cl;
    }
    memcpy(chat[last].text + cur, s, used);
    chat[last].text[cur + used] = 0;
    sanitize_ascii(chat[last].text + cur);
    char rest[CHAT_LEN + 1];
    strncpy(rest, s + used, CHAT_LEN); rest[CHAT_LEN] = 0;
    sanitize_ascii(rest);
    add_line(rest, false);
  }
}

static void display_printf(const char *fmt, ...) {
  char buf[256]; va_list ap; va_start(ap, fmt); vsnprintf(buf, 256, fmt, ap); va_end(ap);
  display_print(buf);
}

#define TERM_LINES ((SCR_H - FONT_H) / FONT_H)  // 19

static void display_draw_chat() {
  if (chat_total > TERM_LINES)
    chat_top = mx(0, mn(chat_top, chat_total - TERM_LINES));
  else chat_top = 0;

  int start = chat_top;
  int end = mn(chat_total, start + TERM_LINES);
  for (int i = start; i < end; i++) {
    int ci = i % CHAT_MAX;
    int y = (i - start) * FONT_H;
    if (chat[ci].is_user) {
      char line[CHAT_LEN + 4];
      line[0] = '>'; line[1] = ' ';
      int sl = strlen(chat[ci].text);
      int cp = sl > CHAT_LEN - 2 ? CHAT_LEN - 2 : sl;
      memcpy(line + 2, chat[ci].text, cp);
      line[2 + cp] = 0;
      aiDrawStr(line, 2, y, COL_PROMPT);
    } else {
      aiDrawStr(chat[ci].text, 2, y, COL_AI_TXT);
    }
  }
}

static void display_draw_status(const char *status) {
  int sy = SCR_H - FONT_H;
  tft.fillRect(0, sy, SCR_W, FONT_H, COL_STAT_BG);
  if (status) aiDrawStr(status, 2, sy, COL_STAT_TXT);
  else if (chat_total > 0) {
    char buf[32];
    snprintf(buf, sizeof(buf), "A=type SEL=gen  %d msgs", chat_total);
    aiDrawStr(buf, 2, sy, COL_STAT_TXT);
  } else {
    aiDrawStr("A=type  SEL=gen  UP/DN=scroll", 2, sy, COL_STAT_TXT);
  }
}

// ---- terminal / command-bridge hooks ----------------------------------------
// When g_genSink is set, generated text is forwarded there instead of the chat
// buffer, and screen refresh is suppressed (the terminal owns the display).
static void (*g_genSink)(const char *, void *) = NULL;
static void *g_genCtx = NULL;
static bool g_genNoUi = false;
static bool g_launchTaken = false;

static void display_refresh(const char *status) {
  if (g_genNoUi) return;  // terminal generation owns the screen
  static int last_ct = -1, last_ctop = -1;
  if (last_ct != chat_total || last_ctop != chat_top) {
    tft.fillScreen(COL_BG);
    display_draw_chat();
    last_ct = chat_total; last_ctop = chat_top;
  }
  display_draw_status(status);
}

static void display_clear() {
  chat_total = 0; chat_top = 0;
  tft.fillScreen(COL_BG);
}

// ----------------------------------------------------------------------------
// On-screen keyboard
// ----------------------------------------------------------------------------
#define KC 10
static const char kbd_ch[5][KC] = {
  {'1','2','3','4','5','6','7','8','9','0'},
  {'A','B','C','D','E','F','G','H','I','J'},
  {'K','L','M','N','O','P','Q','R','S','T'},
  {'U','V','W','X','Y','Z','-','/','.',','},
  {0,0,0,0,0,0,0,0,0,0},
};
static const uint8_t kbd_typ[5][KC] = {
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0},
  {1,1,2,2,2,2,3,3,4,4},
};
struct KbdBtn { uint8_t typ, c0, span; const char *lbl; };
static const KbdBtn kbd_btns[4] = {
  {1, 0, 2, "abc"}, {2, 2, 4, "space"}, {3, 6, 2, "<"}, {4, 8, 2, "OK"},
};
static int kbd_btn_idx(int c) {
  for (int i = 0; i < 4; i++)
    if (c >= kbd_btns[i].c0 && c < kbd_btns[i].c0 + kbd_btns[i].span) return i;
  return 0;
}
#define KBD_KW    (SCR_W / KC)              // 24 px
#define KBD_KH    26
#define KBD_BTN_H 30
#define KBD_H     (4 * KBD_KH + KBD_BTN_H)
static int kbd_cx = 0, kbd_cy = 2;
static bool kbd_shift = false;

static void kbd_draw_key(int kbd_y, int r, int c, bool hl, bool lower) {
  if (r < 4) {
    int x = c * KBD_KW, y = kbd_y + r * KBD_KH;
    uint16_t bg = hl ? TFT_GREEN : COL_BG;
    tft.fillRect(x, y, KBD_KW, KBD_KH, bg);
    char ch = kbd_ch[r][c];
    if (!ch) return;
    if (lower && ch >= 'A' && ch <= 'Z') ch += 32;
    aiDrawStr(&ch, x + (KBD_KW - FONT_W) / 2, y + (KBD_KH - FONT_H) / 2,
              hl ? TFT_BLACK : TFT_WHITE);
  } else {
    const KbdBtn &b = kbd_btns[kbd_btn_idx(c)];
    int x = b.c0 * KBD_KW + 2, w = b.span * KBD_KW - 4;
    int y = kbd_y + 4 * KBD_KH + 2, h = KBD_BTN_H - 4;
    uint16_t bg = hl ? TFT_GREEN : COL_BG;
    tft.fillRoundRect(x, y, w, h, 8, bg);
    tft.drawRoundRect(x, y, w, h, 8, TFT_WHITE);
    const char *lb = (b.typ == 1) ? (lower ? "ABC" : "abc") : b.lbl;
    aiDrawStr(lb, x + (w - (int)strlen(lb) * FONT_W) / 2, y + (h - FONT_H) / 2,
              hl ? TFT_BLACK : TFT_WHITE);
  }
}

// Opens the on-screen keyboard; returns the typed prompt (possibly empty).
// Sets *quitApp when the user asked to leave the whole AI app.
static void display_input(char *buf, int maxlen, bool *quitApp) {
  buf[0] = 0; int pos = 0; kbd_cx = 0; kbd_cy = 2; kbd_shift = false;
  int blink = 0;
  int old_cx = kbd_cx, old_cy = kbd_cy;
  *quitApp = false;

  int inp_y = 2, inp_h = FONT_H + 10;
  int kbd_y = SCR_H - KBD_H;
  int chat_y = inp_y + inp_h + 4;
  kbd_vis_lines = (kbd_y - chat_y) / FONT_H;

  tft.fillScreen(COL_BG);

  for (int i = 0; i < kbd_vis_lines && i < chat_total; i++) {
    int ci = (chat_total - mn(kbd_vis_lines, chat_total) + i) % CHAT_MAX;
    int y = chat_y + i * FONT_H;
    if (chat[ci].is_user) {
      char line[CHAT_LEN + 4];
      line[0] = '>'; line[1] = ' ';
      int sl = strlen(chat[ci].text);
      int cp = sl > CHAT_LEN - 2 ? CHAT_LEN - 2 : sl;
      memcpy(line + 2, chat[ci].text, cp);
      line[2 + cp] = 0;
      aiDrawStr(line, 2, y, COL_PROMPT);
    } else {
      aiDrawStr(chat[ci].text, 2, y, COL_AI_TXT);
    }
  }

  tft.drawRoundRect(2, inp_y, SCR_W - 4, inp_h, 6, TFT_WHITE);

  for (int r = 0; r < 4; r++)
    for (int c = 0; c < KC; c++)
      kbd_draw_key(kbd_y, r, c, (r == kbd_cy && c == kbd_cx), kbd_shift);
  for (int i = 0; i < 4; i++)
    kbd_draw_key(kbd_y, 4, kbd_btns[i].c0, (kbd_cy == 4 && kbd_btn_idx(kbd_cx) == i), kbd_shift);

  bool shift_changed = false;

  while (true) {
    buttonManager.update();

    if (buttonManager.isJustPressed(KEY_UP) && kbd_cy > 0) { old_cy = kbd_cy; kbd_cy--; }
    if (buttonManager.isJustPressed(KEY_DOWN) && kbd_cy < 4) { old_cy = kbd_cy; kbd_cy++; }
    if (buttonManager.isJustPressed(KEY_LEFT) && kbd_cx > 0) {
      old_cx = kbd_cx;
      if (kbd_cy == 4) { int i = kbd_btn_idx(kbd_cx); kbd_cx = kbd_btns[i > 0 ? i - 1 : 0].c0; }
      else kbd_cx--;
    }
    if (buttonManager.isJustPressed(KEY_RIGHT) && kbd_cx < KC - 1) {
      old_cx = kbd_cx;
      if (kbd_cy == 4) { int i = kbd_btn_idx(kbd_cx); kbd_cx = kbd_btns[i < 3 ? i + 1 : 3].c0; }
      else kbd_cx++;
    }

    if (buttonManager.isJustPressed(KEY_B)) {
      if (pos > 0) { buf[--pos] = 0; }
      else break;
    }

    bool pressed = buttonManager.isJustPressed(KEY_START);
    if (pressed) {
      uint8_t t = kbd_typ[kbd_cy][kbd_cx];
      char ch = kbd_ch[kbd_cy][kbd_cx];
      if (t == 0 && ch) {
        if (pos < maxlen - 1) {
          buf[pos++] = kbd_shift && ch >= 'A' && ch <= 'Z' ? ch + 32 : ch;
          buf[pos] = 0;
        }
      } else if (t == 1) { kbd_shift = !kbd_shift; shift_changed = true; }
      else if (t == 2) { if (pos < maxlen - 1) { buf[pos++] = ' '; buf[pos] = 0; } }
      else if (t == 3) { if (pos > 0) buf[--pos] = 0; }
      else if (t == 4) break;
    }
    if (buttonManager.isJustPressed(KEY_OPTION)) { *quitApp = true; break; }
    if (buttonManager.isJustPressed(KEY_A)) { buf[0] = 0; break; }

    if (old_cx != kbd_cx || old_cy != kbd_cy) {
      bool same_btn = (old_cy == 4 && kbd_cy == 4 && kbd_btn_idx(old_cx) == kbd_btn_idx(kbd_cx));
      if (!same_btn) {
        kbd_draw_key(kbd_y, old_cy, old_cx, false, kbd_shift);
        kbd_draw_key(kbd_y, kbd_cy, kbd_cx, true, kbd_shift);
      }
      old_cx = kbd_cx; old_cy = kbd_cy;
    }

    if (shift_changed) {
      for (int r = 0; r < 4; r++)
        for (int c = 0; c < KC; c++)
          if (kbd_typ[r][c] == 0)
            kbd_draw_key(kbd_y, r, c, (r == kbd_cy && c == kbd_cx), kbd_shift);
      kbd_draw_key(kbd_y, 4, kbd_btns[0].c0, (kbd_cy == 4 && kbd_btn_idx(kbd_cx) == 0), kbd_shift);
      shift_changed = false;
    }

    tft.fillRect(4, inp_y + 2, SCR_W - 8, inp_h - 4, COL_BG);
    tft.drawRoundRect(2, inp_y, SCR_W - 4, inp_h, 6, TFT_WHITE);
    char disp[CHAT_LEN + 1];
    strncpy(disp, buf, CHAT_LEN - 2); disp[CHAT_LEN - 2] = 0;
    aiDrawStr(disp, 8, inp_y + (inp_h - FONT_H) / 2, TFT_WHITE);
    int cx = 8 + mn(pos, CHAT_LEN - 2) * FONT_W;
    tft.fillRect(cx, inp_y + 4, 2, inp_h - 8, blink < 8 ? TFT_WHITE : COL_BG);

    delay(30);
    blink = (blink + 1) % 16;
  }
  kbd_vis_lines = 0;
}

// ----------------------------------------------------------------------------
// Model engine state
// ----------------------------------------------------------------------------
enum { MK_PLE, MK_VN };
static int model_kind = MK_PLE;
static uint8_t *model_buf = NULL;
static Model model;
static Scratch scratch;
static VnModel lmodel;
static VnScratch lscratch;
static int gen_pos = 0;
static int gen_ids[512];
static int gen_count = 0;
static volatile int gen_running = 0;
static int temperature = 80;

static void gen_emit(const char *s) {
  if (g_genSink) {
    if (s && *s) g_genSink(s, g_genCtx);
  } else {
    display_append(s);
  }
}

static void free_scratch() {
  free(scratch.x); scratch.x = NULL;
  free(scratch.h); scratch.h = NULL;
  free(scratch.qkv); scratch.qkv = NULL;
  free(scratch.att); scratch.att = NULL;
  free(scratch.g1); scratch.g1 = NULL;
  free(scratch.g2); scratch.g2 = NULL;
  free(scratch.ple); scratch.ple = NULL;
  free(scratch.tmpP); scratch.tmpP = NULL;
  free(scratch.trow); scratch.trow = NULL;
  free(scratch.logits); scratch.logits = NULL;
  free(scratch.scores); scratch.scores = NULL;
  free(scratch.kcache); scratch.kcache = NULL;
  free(scratch.vcache); scratch.vcache = NULL;
  free(lscratch.x); lscratch.x = NULL;
  free(lscratch.h); lscratch.h = NULL;
  free(lscratch.qkv); lscratch.qkv = NULL;
  free(lscratch.att); lscratch.att = NULL;
  free(lscratch.g1); lscratch.g1 = NULL;
  free(lscratch.g2); lscratch.g2 = NULL;
  free(lscratch.logits); lscratch.logits = NULL;
  free(lscratch.scores); lscratch.scores = NULL;
  free(lscratch.kcache); lscratch.kcache = NULL;
  free(lscratch.vcache); lscratch.vcache = NULL;
}

static int alloc_scratch() {
  if (model_kind == MK_VN) {
    int D = lmodel.c.dim, L = lmodel.c.n_layers, F = lmodel.c.ffn;
    int V = lmodel.c.vocab, S = lmodel.c.seq_len;
    void *(*alloc)(size_t) = psramFound() ? ps_malloc : malloc;
    lscratch.x      = (float *)alloc(D * 4);
    lscratch.h      = (float *)alloc(D * 4);
    lscratch.qkv    = (float *)alloc(3 * D * 4);
    lscratch.att    = (float *)alloc(D * 4);
    lscratch.g1     = (float *)alloc(F * 4);
    lscratch.g2     = (float *)alloc(F * 4);
    lscratch.logits = (float *)alloc(V * 4);
    lscratch.scores = (float *)alloc(S * 4);
    lscratch.kcache = (float *)alloc((size_t)L * S * D * 4);
    lscratch.vcache = (float *)alloc((size_t)L * S * D * 4);
    return (lscratch.x && lscratch.h && lscratch.qkv && lscratch.att &&
            lscratch.g1 && lscratch.g2 && lscratch.logits && lscratch.scores &&
            lscratch.kcache && lscratch.vcache) ? 0 : -1;
  }

  int D = model.c.dim, L = model.c.n_layers, P = model.c.ple_dim;
  int F = model.c.ffn, V = model.c.vocab, S = model.c.seq_len;
  void *(*alloc)(size_t) = psramFound() ? ps_malloc : malloc;
  scratch.x       = (float *)alloc(D * 4);
  scratch.h       = (float *)alloc((F > D ? F : D) * 4);
  scratch.qkv     = (float *)alloc(3 * D * 4);
  scratch.att     = (float *)alloc(D * 4);
  scratch.g1      = (float *)alloc(F * 4);
  scratch.g2      = (float *)alloc((P > F ? P : F) * 4);
  scratch.ple     = (float *)alloc(L * P * 4);
  scratch.tmpP    = (float *)alloc(L * P * 4);
  scratch.trow    = (float *)alloc(L * P * 4);
  scratch.logits  = (float *)alloc(V * 4);
  scratch.scores  = (float *)alloc(S * 4);
  scratch.kcache  = (float *)alloc((size_t)L * S * D * 4);
  scratch.vcache  = (float *)alloc((size_t)L * S * D * 4);
  return (scratch.x && scratch.h && scratch.qkv && scratch.att &&
          scratch.g1 && scratch.g2 && scratch.ple && scratch.tmpP &&
          scratch.trow && scratch.logits && scratch.scores &&
          scratch.kcache && scratch.vcache) ? 0 : -1;
}

// Greedy BPE tokenize for PLE; raw UTF-8 bytes for VN.
static int tokenize(const char *text, int *ids, int max_ids) {
  int n = 0, len = strlen(text), pos = 0;
  if (model_kind == MK_VN) {
    while (pos < len && n < max_ids) ids[n++] = (uint8_t)text[pos++];
    return n;
  }
  while (pos < len && n < max_ids) {
    int best_id = -1, best_len = 0;
    for (int v = 0; v < VOCAB_N; v++) {
      int off = VOCAB_OFF[v];
      int tlen = VOCAB_OFF[v + 1] - off;
      if (tlen > best_len && tlen <= len - pos &&
          memcmp(VOCAB_BLOB + off, text + pos, tlen) == 0) {
        best_id = v; best_len = tlen;
      }
    }
    if (best_id < 0) { pos++; continue; }
    ids[n++] = best_id; pos += best_len;
  }
  return n;
}

static void model_forward(int token, int pos) {
  if (model_kind == MK_VN) vn_forward(&lmodel, token, pos, &lscratch);
  else llm_forward(&model, token, pos, &scratch);
}

static int decode_token(int id, char *out) {
  if (model_kind == MK_VN) {
    out[0] = (char)(uint8_t)id;
    out[1] = 0;
    return 1;
  }
  if (id >= VOCAB_N) { out[0] = 0; return 0; }
  int off = VOCAB_OFF[id];
  int len = VOCAB_OFF[id + 1] - off;
  if (len > 127) len = 127;
  memcpy(out, VOCAB_BLOB + off, len);
  out[len] = 0;
  return len;
}

static int generate_next() {
  model_forward(gen_ids[gen_count - 1], gen_pos++);
  int V = (model_kind == MK_VN) ? lmodel.c.vocab : model.c.vocab;
  if (V > VOCAB_N) V = VOCAB_N;
  float *logits = (model_kind == MK_VN) ? lscratch.logits : scratch.logits;
  if (temperature == 0) {
    int best = 0;
    for (int v = 1; v < V; v++)
      if (logits[v] > logits[best]) best = v;
    return best;
  }
  float t = temperature / 100.0f;
  int top_k = (model_kind == MK_VN) ? 15 : 40;
  if (top_k < V) {
    float thr = 1e30f;
    for (int k = 0; k < top_k; k++) {
      float nxt = -1e30f;
      for (int v = 0; v < V; v++)
        if (logits[v] < thr && logits[v] > nxt) nxt = logits[v];
      thr = nxt;
    }
    for (int v = 0; v < V; v++)
      if (logits[v] < thr) logits[v] = -1e30f;
  }
  float max_l = logits[0];
  for (int v = 1; v < V; v++)
    if (logits[v] > max_l) max_l = logits[v];
  float sum = 0;
  for (int v = 0; v < V; v++)
    sum += expf((logits[v] - max_l) / t);
  float r = (float)random(10000) / 10000.0f * sum;
  float cum = 0;
  for (int v = 0; v < V; v++) {
    cum += expf((logits[v] - max_l) / t);
    if (cum >= r) return v;
  }
  return V - 1;
}

static void gen_task(const char *prompt_text, int n_tokens) {
  gen_running = 1;
  gen_pos = 0;
  gen_count = 0;

  int n_prompt = tokenize(prompt_text, gen_ids, 500);
  if (n_prompt == 0) {
    if (model_kind == MK_VN) {
      static const int def[] = {'H', 'i', '\n', 'T', 'h', 'e', ' ', 'r'};
      n_prompt = 8;
      memcpy(gen_ids, def, sizeof(def));
    } else {
      static const int def[] = {1, 500, 1000, 200, 42, 777, 13, 99};
      n_prompt = 8;
      memcpy(gen_ids, def, sizeof(def));
    }
  }
  int ctx = (model_kind == MK_VN) ? lmodel.c.seq_len : model.c.seq_len;
  if (n_prompt > ctx - 1) {
    int drop = n_prompt - (ctx - 1);
    memmove(gen_ids, gen_ids + drop, (size_t)(n_prompt - drop) * sizeof(int));
    n_prompt = ctx - 1;
  }
  gen_count = n_prompt;

  for (int i = 0; i < n_prompt; i++)
    model_forward(gen_ids[i], gen_pos++);

  int max_gen = ctx - 1 - n_prompt;
  if (max_gen < 0) max_gen = 0;
  if (n_tokens < max_gen) max_gen = n_tokens;

  char genbuf[CHAT_LEN + 1] = "";
  unsigned long t0 = millis();
  for (int i = 0; i < max_gen && gen_running; i++) {
    int tok = generate_next();
    gen_ids[gen_count++ % 512] = tok;

    char tbuf[128];
    int len = decode_token(tok, tbuf);
    bool stop_nl = false;
    if (len > 0 && tbuf[len - 1] == '\n' && gen_count - n_prompt >= 10) {
      while (len > 0 && tbuf[len - 1] == '\n') len--;
      stop_nl = true;
    }

    int cur = strlen(genbuf);
    if (cur + len > CHAT_LEN) { gen_emit(genbuf); genbuf[0] = 0; cur = 0; }
    memcpy(genbuf + cur, tbuf, len);
    genbuf[cur + len] = 0;

    buttonManager.update();
    if (buttonManager.isJustPressed(KEY_B)) break;
    if (buttonManager.isJustPressed(KEY_SELECT)) break;
    if (buttonManager.isJustPressed(KEY_OPTION)) break;

    if (stop_nl) {
      if (genbuf[0]) gen_emit(genbuf);
      genbuf[0] = 0;
      break;
    }

    if (i % 5 == 0) {
      if (genbuf[0]) gen_emit(genbuf);
      genbuf[0] = 0;
      char status[64];
      int elapsed = (millis() - t0) / 1000;
      if (elapsed > 0)
        snprintf(status, sizeof(status), "%d tok/s  %d/%d",
                 (i * 1000) / (int)(millis() - t0), i, max_gen);
      else
        snprintf(status, sizeof(status), "tok %d/%d", i, max_gen);
      display_refresh(status);
    }
  }
  if (genbuf[0]) gen_emit(genbuf);
  gen_running = 0;
  display_refresh("DONE");
}

static const char *demo_prompts[] = {
  "Xin chào",
  "Bạn là ai",
  "Việt Nam ở đâu",
  "Phở là món ăn gì",
  "Kể chuyện cười đi",
};
static const int N_DEMOS = sizeof(demo_prompts) / sizeof(demo_prompts[0]);
static int demo_idx = 0;

// ----------------------------------------------------------------------------
// Model access API (shared with the terminal `ai` command)
// ----------------------------------------------------------------------------
void aiModelFree() {
  free_scratch();
  if (model_buf) {
    free(model_buf);
    model_buf = NULL;
  }
}

bool aiModelLoaded() { return model_buf != NULL; }

int aiModelLoad(const String &path, char *err, int errCap) {
  err[0] = 0;
  aiModelFree();

  if (!SD.exists(path)) {
    snprintf(err, errCap, "model not found: %s", path.c_str());
    return -1;
  }
  File f = SD.open(path, FILE_READ);
  if (!f) {
    snprintf(err, errCap, "SD open failed");
    return -1;
  }
  size_t file_size = f.size();
  uint8_t hdr[VN_HEADER_BYTES];
  if (f.read(hdr, sizeof(hdr)) != (int)sizeof(hdr)) {
    f.close();
    snprintf(err, errCap, "short header");
    return -1;
  }
  f.seek(0);

  uint32_t magic;
  memcpy(&magic, hdr, 4);
  size_t need = file_size;
  if (magic == LLM_MAGIC) {
    model_kind = MK_PLE;
  } else if (magic == VN_MAGIC) {
    VnCfg c;
    if (vn_parse_header(hdr, &c)) {
      f.close();
      snprintf(err, errCap, "bad VN header");
      return -1;
    }
    model_kind = MK_VN;
    need = VN_HEADER_BYTES + vn_weight_bytes(&c);
    if (need > file_size) need = file_size;
  } else {
    f.close();
    snprintf(err, errCap, "not a PLE/VN model");
    return -1;
  }

  if (!psramFound() || need > (size_t)ESP.getFreePsram()) {
    f.close();
    snprintf(err, errCap, "need %d MB PSRAM, free %d MB",
             (int)(need / 1048576), (int)(ESP.getFreePsram() / 1048576));
    return -1;
  }
  model_buf = (uint8_t *)ps_malloc(need);
  if (!model_buf) {
    f.close();
    snprintf(err, errCap, "PSRAM alloc failed");
    return -1;
  }
  size_t rd = f.read(model_buf, need);
  f.close();
  if (rd != need) {
    free(model_buf);
    model_buf = NULL;
    snprintf(err, errCap, "SD read error");
    return -1;
  }

  int lr = (model_kind == MK_VN) ? vn_load(model_buf, &lmodel)
                                 : llm_load(model_buf, &model);
  if (lr) {
    free(model_buf);
    model_buf = NULL;
    snprintf(err, errCap, "bad model file");
    return -1;
  }
  if (alloc_scratch() != 0) {
    aiModelFree();
    snprintf(err, errCap, "scratch OOM");
    return -1;
  }
  temperature = (model_kind == MK_VN) ? 0 : 80;

  aiModelInfo(err, errCap);
  return 0;
}

bool aiModelInfo(char *buf, int cap) {
  if (!model_buf) return false;
  if (model_kind == MK_VN)
    snprintf(buf, cap, "VN V=%d D=%d L=%d S=%d", lmodel.c.vocab, lmodel.c.dim,
             lmodel.c.n_layers, lmodel.c.seq_len);
  else
    snprintf(buf, cap, "PLE V=%d D=%d L=%d S=%d", model.c.vocab, model.c.dim,
             model.c.n_layers, model.c.seq_len);
  return true;
}

bool aiGenerate(const char *prompt, int maxTokens, ai_sink_t sink, void *ctx) {
  if (!model_buf) return false;
  g_genSink = sink;
  g_genCtx = ctx;
  g_genNoUi = true;
  char fmt[160];
  snprintf(fmt, sizeof(fmt), "User: %s\nAssistant:", prompt);
  gen_task(fmt, maxTokens);
  g_genNoUi = false;
  g_genSink = NULL;
  g_genCtx = NULL;
  return true;
}

bool aiTookLaunch() {
  bool r = g_launchTaken;
  g_launchTaken = false;
  return r;
}

// Recursive SD scan for AI model files (PLE/VN magic).
static int ai_model_found = 0;
static String aiScanOut[24];
static void aiScanVfs(const String &p, int depth) {
  if (depth > 4) return;
  DIR *d = opendir(p.c_str());
  if (!d) return;
  struct dirent *e;
  while ((e = readdir(d)) != NULL) {
    String n = e->d_name;
    if (n.isEmpty() || n == "." || n == ".." || n.startsWith(".")) continue;
    String full = p + "/" + n;
    if (e->d_type == DT_DIR) {
      if (n != "system" && n != "data" && n != "assets")
        aiScanVfs(full, depth + 1);
    } else {
      String lo = n;
      lo.toLowerCase();
      if (lo.endsWith(".bin")) {
        File mf = SD.open(full.substring(3), FILE_READ);
        uint8_t h[4];
        if (mf && mf.read(h, 4) == 4) {
          uint32_t m;
          memcpy(&m, h, 4);
          if (m == AI_MAGIC_PLE || m == AI_MAGIC_VN) {
            if (ai_model_found < 24)
              aiScanOut[ai_model_found] = full.substring(3);
            ai_model_found++;
          }
        }
        if (mf) mf.close();
      }
    }
  }
  closedir(d);
}
int aiScanModels(String *outPaths, int maxOut) {
  ai_model_found = 0;
  aiScanVfs("/sd", 0);
  int n = min(ai_model_found, maxOut);
  for (int i = 0; i < n; i++) outPaths[i] = aiScanOut[i];
  return ai_model_found;
}

// ----------------------------------------------------------------------------
// Slash commands in the chat prompt. Returns true when the chat should quit
// (a /run launch or /quit).
// ----------------------------------------------------------------------------
static bool ai_chat_cmd(const char *cmd) {
  String line(cmd);
  line.trim();
  String name = line, arg = "";
  int sp = line.indexOf(' ');
  if (sp != -1) {
    arg = line.substring(sp + 1);
    arg.trim();
    name = line.substring(0, sp);
  }
  name.toLowerCase();

  if (name == "/help") {
    display_print("/help /apps /status /sync /load /temp /run /quit");
    return false;
  }
  if (name == "/apps") {
    int n = systemAppCount();
    display_printf("Apps: %d", n);
    for (int i = 0; i < n && i < 8; i++) display_printf("  %s", systemAppName(i));
    if (n > 8) display_printf("  ... +%d more", n - 8);
    return false;
  }
  if (name == "/status") {
    char info[96];
    display_printf("Model: %s",
                   aiModelInfo(info, sizeof(info)) ? info : "none");
    display_printf("PSRAM free %d KB", (int)(ESP.getFreePsram() / 1024));
    display_printf("Apps: %d", systemAppCount());
    return false;
  }
  if (name == "/sync") {
    String tmp[24];
    int m = aiScanModels(tmp, 24);
    display_printf("Sync: %d models, %d apps on SD", m, systemAppCount());
    for (int i = 0; i < m && i < 4; i++) display_printf("  %s", tmp[i].c_str());
    return false;
  }
  if (name == "/load") {
    if (arg.length() == 0) {
      display_print("usage: /load <path>");
      return false;
    }
    char err[96];
    int rc = aiModelLoad(arg, err, sizeof(err));
    if (rc == 0) {
      display_clear();
      display_printf("Loaded: %s", err);
      display_printf("PSRAM free %d KB", (int)(ESP.getFreePsram() / 1024));
    } else {
      display_printf("Load failed: %s", err);
    }
    return false;
  }
  if (name == "/temp") {
    int t = arg.toInt();
    if (arg.length() > 0 && t >= 0 && t <= 200) {
      temperature = t;
      display_printf("Temp: %d.%02d", temperature / 100, temperature % 100);
    } else {
      display_print("usage: /temp <0-200>");
    }
    return false;
  }
  if (name == "/run") {
    if (arg.length() == 0) {
      display_print("usage: /run <app>");
      return false;
    }
    int id = findSystemAppByName(arg);
    if (id < 0) {
      display_printf("No app: %s (try /apps)", arg.c_str());
      return false;
    }
    if (launchSystemApp(id)) {
      g_launchTaken = true;
      display_refresh("Launching...");
      delay(400);
      return true;
    }
    display_print("Launch failed");
    return false;
  }
  if (name == "/quit") return true;

  display_printf("Unknown cmd: %s (/help)", cmd);
  return false;
}

// ----------------------------------------------------------------------------
// Public entry point
// ----------------------------------------------------------------------------
bool runAiChat(const String &modelPath) {
  uint8_t oldRot = tft.getRotation();
  tft.setRotation(0);
  tft.fillScreen(COL_BG);
  aiModelFree();  // drop anything the terminal `ai` command may have loaded
  display_clear();
  chat_total = 0; chat_top = 0; demo_idx = 0;

  // ---- sanity + model size check ----
  if (!SD.exists(modelPath)) {
    aiDrawStr("SD: model not found", 2, 0, TFT_RED);
    aiDrawStr(modelPath.c_str(), 2, 16, TFT_WHITE);
    aiDrawStr("MENU = back", 2, 32, TFT_YELLOW);
    while (true) {
      buttonManager.update();
      if (buttonManager.isJustPressed(KEY_A) ||
          buttonManager.isJustPressed(KEY_OPTION)) break;
      delay(20);
    }
    tft.setRotation(oldRot);
    return true;
  }

  File f = SD.open(modelPath, FILE_READ);
  if (!f) {
    aiDrawStr("SD: open failed", 2, 0, TFT_RED);
    aiDrawStr("MENU = back", 2, 32, TFT_YELLOW);
    while (true) {
      buttonManager.update();
      if (buttonManager.isJustPressed(KEY_A) ||
          buttonManager.isJustPressed(KEY_OPTION)) break;
      delay(20);
    }
    tft.setRotation(oldRot);
    return true;
  }
  size_t file_size = f.size();

  // ---- detect kind from magic ----
  uint8_t hdr[VN_HEADER_BYTES];
  if (f.read(hdr, sizeof(hdr)) != (int)sizeof(hdr)) {
    f.close();
    aiDrawStr("Short header", 2, 0, TFT_RED);
    aiDrawStr("MENU = back", 2, 32, TFT_YELLOW);
    while (true) {
      buttonManager.update();
      if (buttonManager.isJustPressed(KEY_A) ||
          buttonManager.isJustPressed(KEY_OPTION)) break;
      delay(20);
    }
    tft.setRotation(oldRot);
    return true;
  }
  f.seek(0);

  uint32_t magic;
  memcpy(&magic, hdr, 4);
  size_t need = file_size;
  if (magic == LLM_MAGIC) {
    model_kind = MK_PLE;
  } else if (magic == VN_MAGIC) {
    model_kind = MK_VN;
    VnCfg c;
    if (vn_parse_header(hdr, &c)) {
      f.close();
      aiDrawStr("Bad VN header", 2, 0, TFT_RED);
      aiDrawStr("MENU = back", 2, 32, TFT_YELLOW);
      while (true) {
        buttonManager.update();
        if (buttonManager.isJustPressed(KEY_A) ||
            buttonManager.isJustPressed(KEY_OPTION)) break;
        delay(20);
      }
      tft.setRotation(oldRot);
      return true;
    }
    need = VN_HEADER_BYTES + vn_weight_bytes(&c);
    if (need > file_size) need = file_size;
  } else {
    f.close();
    aiDrawStr("Not a PLE/VN model", 2, 0, TFT_RED);
    aiDrawStr("MENU = back", 2, 32, TFT_YELLOW);
    while (true) {
      buttonManager.update();
      if (buttonManager.isJustPressed(KEY_A) ||
          buttonManager.isJustPressed(KEY_OPTION)) break;
      delay(20);
    }
    tft.setRotation(oldRot);
    return true;
  }

  // ---- load weights into PSRAM ----
  aiDrawStr("Loading model...", 2, 0, TFT_YELLOW);
  char sz[48];
  snprintf(sz, sizeof(sz), "%d MB   PSRAM free %d MB", (int)(need / 1048576),
           (int)(ESP.getFreePsram() / 1048576));
  aiDrawStr(sz, 2, 16, TFT_WHITE);

  if (!psramFound() || need > (size_t)ESP.getFreePsram()) {
    f.close();
    aiDrawStr("Not enough PSRAM", 2, 48, TFT_RED);
    aiDrawStr("MENU = back", 2, 64, TFT_YELLOW);
    while (true) {
      buttonManager.update();
      if (buttonManager.isJustPressed(KEY_A) ||
          buttonManager.isJustPressed(KEY_OPTION)) break;
      delay(20);
    }
    tft.setRotation(oldRot);
    return true;
  }
  model_buf = (uint8_t *)ps_malloc(need);
  if (!model_buf) {
    f.close();
    aiDrawStr("PSRAM alloc failed", 2, 48, TFT_RED);
    aiDrawStr("MENU = back", 2, 64, TFT_YELLOW);
    while (true) {
      buttonManager.update();
      if (buttonManager.isJustPressed(KEY_A) ||
          buttonManager.isJustPressed(KEY_OPTION)) break;
      delay(20);
    }
    tft.setRotation(oldRot);
    return true;
  }
  size_t rd = f.read(model_buf, need);
  f.close();
  if (rd != need) {
    free(model_buf); model_buf = NULL;
    aiDrawStr("SD read error", 2, 48, TFT_RED);
    aiDrawStr("MENU = back", 2, 64, TFT_YELLOW);
    while (true) {
      buttonManager.update();
      if (buttonManager.isJustPressed(KEY_A) ||
          buttonManager.isJustPressed(KEY_OPTION)) break;
      delay(20);
    }
    tft.setRotation(oldRot);
    return true;
  }

  // ---- bind model + allocate scratch ----
  if (model_kind == MK_VN) {
    if (vn_load(model_buf, &lmodel)) {
      free(model_buf); model_buf = NULL;
      aiDrawStr("Bad VN model", 2, 48, TFT_RED);
      aiDrawStr("MENU = back", 2, 64, TFT_YELLOW);
      while (true) {
        buttonManager.update();
        if (buttonManager.isJustPressed(KEY_A) ||
            buttonManager.isJustPressed(KEY_OPTION)) break;
        delay(20);
      }
      tft.setRotation(oldRot);
      return true;
    }
  } else {
    if (llm_load(model_buf, &model)) {
      free(model_buf); model_buf = NULL;
      aiDrawStr("Bad PLE model", 2, 48, TFT_RED);
      aiDrawStr("MENU = back", 2, 64, TFT_YELLOW);
      while (true) {
        buttonManager.update();
        if (buttonManager.isJustPressed(KEY_A) ||
            buttonManager.isJustPressed(KEY_OPTION)) break;
        delay(20);
      }
      tft.setRotation(oldRot);
      return true;
    }
  }
  if (alloc_scratch() != 0) {
    free_scratch();
    free(model_buf); model_buf = NULL;
    aiDrawStr("Scratch OOM", 2, 48, TFT_RED);
    aiDrawStr("MENU = back", 2, 64, TFT_YELLOW);
    while (true) {
      buttonManager.update();
      if (buttonManager.isJustPressed(KEY_A) ||
          buttonManager.isJustPressed(KEY_OPTION)) break;
      delay(20);
    }
    tft.setRotation(oldRot);
    return true;
  }

  temperature = (model_kind == MK_VN) ? 0 : 80;

  // ---- ready ----
  display_clear();
  display_printf("PLE TinyLM ready");
  display_printf("Model: %s", modelPath.substring(modelPath.lastIndexOf('/') + 1).c_str());
  if (model_kind == MK_VN)
    display_printf("VN V=%d D=%d L=%d S=%d", lmodel.c.vocab, lmodel.c.dim,
                   lmodel.c.n_layers, lmodel.c.seq_len);
  else
    display_printf("PLE V=%d D=%d L=%d S=%d", model.c.vocab, model.c.dim,
                   model.c.n_layers, model.c.seq_len);
  display_printf("PSRAM free %d KB", (int)(ESP.getFreePsram() / 1024));
  display_print("A=type SEL=gen MENU=exit");
  display_refresh("Ready");

  // ---- main loop ----
  while (true) {
    buttonManager.update();

    if (buttonManager.isJustPressed(KEY_SELECT) ||
        buttonManager.isJustPressed(KEY_OPTION)) break;

    if (buttonManager.isJustPressed(KEY_UP)) {
      chat_top = mx(0, chat_top - 1);
      display_refresh(NULL);
    }
    if (buttonManager.isJustPressed(KEY_DOWN)) {
      chat_top = mn(chat_total > TERM_LINES ? chat_total - TERM_LINES : 0, chat_top + 1);
      display_refresh(NULL);
    }

    if (buttonManager.isJustPressed(KEY_LEFT)) {
      temperature = mx(temperature - 10, 10);
      char buf[40];
      snprintf(buf, sizeof(buf), "T: %d.%02d", temperature / 100, temperature % 100);
      display_print_user(buf);
      display_refresh(buf);
    }
    if (buttonManager.isJustPressed(KEY_RIGHT)) {
      temperature = mn(temperature + 10, 200);
      char buf[40];
      snprintf(buf, sizeof(buf), "T: %d.%02d", temperature / 100, temperature % 100);
      display_print_user(buf);
      display_refresh(buf);
    }

    if (buttonManager.isJustPressed(KEY_START)) {
      display_print_user(demo_prompts[demo_idx]);
      display_refresh("Generating...");
      char fmt_buf[128];
      snprintf(fmt_buf, sizeof(fmt_buf), "User: %s\nAssistant:", demo_prompts[demo_idx]);
      gen_task(fmt_buf, 200);
      display_refresh(NULL);
    }

    if (buttonManager.isJustPressed(KEY_START)) {
      char prompt_buf[64];
      bool quitApp = false;
      display_input(prompt_buf, 60, &quitApp);
      if (quitApp) break;
      size_t len = strlen(prompt_buf);
      if (len > 0) {
        while (len > 0 && prompt_buf[len - 1] == ' ') prompt_buf[--len] = 0;
        if (len > 0) {
          if (prompt_buf[0] == '/') {
            if (ai_chat_cmd(prompt_buf)) break;  // /run or /quit
          } else {
            display_print_user(prompt_buf);
            display_refresh("Generating...");
            char fmt_buf[128];
            snprintf(fmt_buf, sizeof(fmt_buf), "User: %s\nAssistant:", prompt_buf);
            gen_task(fmt_buf, 200);
          }
        }
      }
      display_refresh(NULL);
    }

    delay(40);
  }

  // ---- teardown ----
  free_scratch();
  free(model_buf); model_buf = NULL;
  tft.fillScreen(COL_BG);
  tft.setRotation(oldRot);
  return true;
}