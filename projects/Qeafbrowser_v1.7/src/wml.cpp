// wml.cpp — Parser WML 1.1 subset + HTML subset -> danh sach dong da boc (wrap).
// Port co che DOM/Parser/Render* cua legacy keypad browser (323 module C++ goc) thanh
// 1 vong lap quet tag + pool tinh theo J2ME_to_MRE_Porting_Guide §1.
// The ho tro: wml card/p/a/anchor/br/go/prev/refresh, html h1..h6/p/br/div/
// a/tr/td/table/li/title. The khong biet ten -> bo qua noi dung cua no (khong in the name).
#include "browser.h"

extern "C" {
int gfx_textWidth(const char *nul_term, int style);   // main.cpp cung cap
}

static inline bool isblock(const char *t) {
  static const char *B[] = {"p","br","div","h1","h2","h3","h4","h5","h6",
                            "tr","td","li","card","wml","html","head","body",
                            "table","ul","ol","form","do","template","fieldset",
                            "input","field","folder","dir","b","strong","em","i",
                            "small","cite","span","article","header","nav",
                            "section","footer","blockquote","pre",0};
  for (int i = 0; B[i]; i++) if (!strcmp(t, B[i])) return true;
  return false;
}
// style: 0 body, 1 field, 2 h1/h2, 3 folder, 4 h3-h6, 5 small/meta, 6 bold, 7 image
static inline int heading_style(const char *t) {
  if (!strncmp(t, "h1", 2) || !strncmp(t, "h2", 2)) return 2;
  if (!strncmp(t, "h3", 2) || !strncmp(t, "h4", 2) ||
      !strncmp(t, "h5", 2) || !strncmp(t, "h6", 2)) return 4;
  if (!strcmp(t, "input") || !strcmp(t, "field")) return 1;
  if (!strcmp(t, "folder") || !strcmp(t, "dir")) return 3;
  if (!strcmp(t, "small") || !strcmp(t, "cite")) return 5;
  if (!strcmp(t, "b") || !strcmp(t, "strong")) return 6;
  return 0;
}


static inline bool starts_focus_block(const char *t) {
  static const char *B[] = {"p","div","h1","h2","h3","h4","h5","h6",
                            "tr","td","li","form","fieldset","input","field",
                            "folder","dir","article","header","nav","section",
                            "footer","blockquote","pre","small",0};
  for (int i = 0; B[i]; i++) if (!strcmp(t, B[i])) return true;
  return false;
}

static void attr_decode(char *s) {
  if (!s) return;
  char *w = s;
  const char *r = s;
  while (*r) {
    if (!strncmp(r, "&amp;", 5)) { *w++ = '&'; r += 5; continue; }
    if (!strncmp(r, "&quot;", 6)) { *w++ = '"'; r += 6; continue; }
    if (!strncmp(r, "&#39;", 5) || !strncmp(r, "&apos;", 6)) { *w++ = '\''; r += (*r == '&' && r[2] == '#') ? 5 : 6; continue; }
    *w++ = *r++;
  }
  *w = 0;
}

static bool prefix_ci(const char *s, const char *prefix) {
  while (*prefix) {
    if (!*s || tolower((unsigned char)*s) != tolower((unsigned char)*prefix)) return false;
    s++; prefix++;
  }
  return true;
}

static bool suffix_ci(const char *s, size_t n, const char *suffix) {
  size_t z = strlen(suffix);
  return n >= z && !strncasecmp(s + n - z, suffix, z);
}

static bool image_url_candidate(const char *u) {
  if (!u || !u[0] || isspace((unsigned char)u[0]) ||
      prefix_ci(u, "data:") || prefix_ci(u, "blob:") ||
      prefix_ci(u, "javascript:") || prefix_ci(u, "about:")) return false;
  size_t n = strlen(u);
  for (size_t i = 0; i < n; i++) {
    if (u[i] == '?' || u[i] == '#') { n = i; break; }
  }
  if (suffix_ci(u, n, ".gif") || suffix_ci(u, n, ".webp") ||
      suffix_ci(u, n, ".svg") || suffix_ci(u, n, ".avif")) return false;
  return true;
}

static void image_srcset_pick(const char *set, char *out, int cap) {
  out[0] = 0;
  if (!set || !set[0] || prefix_ci(set, "data:")) return;
  const char *p = set;
  while (*p) {
    while (*p == ',' || isspace((unsigned char)*p)) p++;
    const char *start = p;
    while (*p && *p != ',') p++;
    const char *end = p;
    while (end > start && isspace((unsigned char)end[-1])) end--;
    const char *ue = start;
    while (ue < end && !isspace((unsigned char)*ue)) ue++;
    size_t n = (size_t)(ue - start);
    if (n > 0 && n < (size_t)cap) {
      char candidate[192];
      size_t copy = n < sizeof candidate - 1 ? n : sizeof candidate - 1;
      memcpy(candidate, start, copy);
      candidate[copy] = 0;
      if (image_url_candidate(candidate)) {
        strncpy(out, candidate, (size_t)cap - 1);
        out[cap - 1] = 0;
        return;
      }
    }
    if (*p == ',') p++;
  }
}

static void image_source_pick(const char *lazy, const char *set, const char *src,
                             char *out, int cap) {
  out[0] = 0;
  if (image_url_candidate(lazy)) {
    strncpy(out, lazy, (size_t)cap - 1);
    out[cap - 1] = 0;
  } else {
    image_srcset_pick(set, out, cap);
    if (!out[0] && image_url_candidate(src)) {
      strncpy(out, src, (size_t)cap - 1);
      out[cap - 1] = 0;
    }
  }
  attr_decode(out);
}

// tach ten the + thuoc tinh href/title. Tra ve 1=the mo, -1=the dong/comment, 0=khong phai the
static int tag_scan(const char *s, size_t n, size_t *i, char *name, int ncap,
                    char *href, int hcap, char *title, int tcap,
                    char *alt, int acap, char *src, int scap,
                    char *lazy, int lcap, char *srcset, int setcap) {
  name[0] = href[0] = title[0] = alt[0] = src[0] = lazy[0] = srcset[0] = 0;
  if (*i >= n || s[*i] != '<') return 0;
  size_t p = *i + 1;
  bool closing = false;
  if (p < n && s[p] == '/') { closing = true; p++; }
  if (p < n && (s[p] == '!' || s[p] == '?')) {   // comment / doctype / xml decl
    size_t e = p; while (e < n && s[e] != '>') e++;
    *i = e + 1; return -1;
  }
  size_t ns = p;
  while (p < n && (isalnum((unsigned char)s[p]) || s[p] == '-' || s[p] == ':')) p++;
  size_t tl = p - ns;
  if (tl == 0 || tl >= (size_t)ncap) { *i = p; return -1; }
  for (size_t k = 0; k < tl; k++) name[k] = tolower(s[ns + k]);
  name[tl] = 0;
  if (closing) { while (p < n && s[p] != '>') p++; if (p < n) p++; *i = p; return -2; }
  while (p < n && s[p] != '>') {
    while (p < n && (isspace((unsigned char)s[p]) || s[p] == '/')) p++;
    if (p >= n || s[p] == '>') break;

    size_t ks = p;
    while (p < n && !isspace((unsigned char)s[p]) && s[p] != '=' && s[p] != '>' && s[p] != '/') p++;
    size_t ke = p;
    while (p < n && isspace((unsigned char)s[p])) p++;

    char key[20]; size_t kl = ke - ks; if (kl >= sizeof key) kl = sizeof key - 1;
    for (size_t k = 0; k < kl; k++) key[k] = (char)tolower((unsigned char)s[ks + k]);
    key[kl] = 0;

    if (p < n && s[p] == '=') {
      p++;
      while (p < n && isspace((unsigned char)s[p])) p++;
      char q = (p < n && (s[p] == '"' || s[p] == '\'')) ? s[p] : 0;
      if (q) p++;
      size_t vs = p;
      if (q) while (p < n && s[p] != q) p++;
      else   while (p < n && !isspace((unsigned char)s[p]) && s[p] != '>') p++;
      size_t ve = p;
      if (q && p < n && s[p] == q) p++;

      if (!strcmp(key, "href") && href && ve > vs) {
        size_t l = ve - vs; if (l >= (size_t)hcap) l = hcap - 1;
        memcpy(href, s + vs, l); href[l] = 0;
      } else if (!strcmp(key, "title") && title && ve > vs) {
        size_t l = ve - vs; if (l >= (size_t)tcap) l = tcap - 1;
        memcpy(title, s + vs, l); title[l] = 0;
      } else if ((!strcmp(key, "alt") || !strcmp(key, "aria-label")) && alt && ve > vs) {
        size_t l = ve - vs; if (l >= (size_t)acap) l = acap - 1;
        memcpy(alt, s + vs, l); alt[l] = 0;
      } else if (!strcmp(key, "src") && src && ve > vs) {
        size_t l = ve - vs; if (l >= (size_t)scap) l = scap - 1;
        memcpy(src, s + vs, l); src[l] = 0;
      } else if ((!strcmp(key, "data-src") || !strcmp(key, "data-original") ||
                  !strcmp(key, "data-lazy-src")) && lazy && ve > vs) {
        size_t l = ve - vs; if (l >= (size_t)lcap) l = lcap - 1;
        memcpy(lazy, s + vs, l); lazy[l] = 0;
      } else if ((!strcmp(key, "srcset") || !strcmp(key, "data-srcset")) &&
                 srcset && ve > vs) {
        size_t l = ve - vs; if (l >= (size_t)setcap) l = setcap - 1;
        memcpy(srcset, s + vs, l); srcset[l] = 0;
      }
    }
  }
  if (p < n && s[p] == '>') p++;
  *i = p;
  return 1;
}

// ---------------- trang thai parse ----------------
static Doc *D;
static int  cur_off;      // D->len luc mo dong
static int  cur_chars;    // so ky tu tren dong hien tai
static int  cur_px;       // chieu dai pixel dong hien tai
static int  cur_style;
static int  open_link;    // link dang mo (-1 = khong)
static int  current_image; // image block dang mo (-1 = khong)
static uint16_t cur_block, next_block;
static bool pending_space;
static bool in_title;
static bool title_space;
static char skip_name[16];
static int  skip_depth;

// ---- Feed (RSS 2.0 / Atom) detection + state ----
static bool feed_mode;
static bool feed_in_item;          // inside <item> or <entry>
static bool feed_in_title;         // capturing <title> text
static bool feed_in_link_text;     // capturing <link>URL</link> (RSS)
static bool feed_in_desc;          // capturing description/summary (plain text)
static bool feed_have_title, feed_have_link;
static char feed_title[160];
static char feed_link[192];
static char feed_desc[720];       // BBC: description truoc link — defer den sau emit
static int  feed_tlen, feed_llen, feed_dlen;

// ASCII whitespace only — UTF-8 continuation bytes (0x80-0xBF) must never match.
static inline bool ascii_space(char c) {
  return (unsigned char)c < 128 && isspace((unsigned char)c);
}

// Decode &#NNN; / &#xHH; at src[i] into UTF-8 out[4].
// Returns bytes written (0 = not a valid entity). *end = pos after ';'.
static int entity_utf8(const char *src, size_t n, size_t i, char out[4], size_t *end) {
  if (i + 2 >= n || src[i] != '&' || src[i + 1] != '#') return 0;
  size_t k = i + 2;
  int base = 10;
  if (k < n && (src[k] == 'x' || src[k] == 'X')) { base = 16; k++; }
  char tmp[8]; int tl = 0;
  while (k < n && tl < 7 && src[k] != ';') tmp[tl++] = src[k++];
  if (tl == 0 || k >= n || src[k] != ';') return 0;
  tmp[tl] = 0;
  char *ep = nullptr;
  long v = strtol(tmp, &ep, base);
  if (ep == tmp || v < 1 || v > 0x10FFFF) return 0;
  if (v >= 0xD800 && v <= 0xDFFF) return 0;
  *end = k + 1;
  if (v < 0x80) { out[0] = (char)v; return 1; }
  if (v < 0x800) {
    out[0] = (char)(0xC0 | (v >> 6));
    out[1] = (char)(0x80 | (v & 0x3F));
    return 2;
  }
  if (v < 0x10000) {
    out[0] = (char)(0xE0 | (v >> 12));
    out[1] = (char)(0x80 | ((v >> 6) & 0x3F));
    out[2] = (char)(0x80 | (v & 0x3F));
    return 3;
  }
  out[0] = (char)(0xF0 | (v >> 18));
  out[1] = (char)(0x80 | ((v >> 12) & 0x3F));
  out[2] = (char)(0x80 | ((v >> 6) & 0x3F));
  out[3] = (char)(0x80 | (v & 0x3F));
  return 4;
}

static bool looks_like_feed(const char *s, size_t n) {
  size_t lim = n < 4096 ? n : 4096;
  for (size_t i = 0; i + 5 < lim; i++) {
    if (s[i] != '<') continue;
    if ((s[i+1]=='r'||s[i+1]=='R') && (s[i+2]=='s'||s[i+2]=='S') &&
        (s[i+3]=='s'||s[i+3]=='S') && (s[i+4]=='>'||isspace((unsigned char)s[i+4])))
      return true;
    if ((s[i+1]=='f'||s[i+1]=='F') && (s[i+2]=='e'||s[i+2]=='E') &&
        (s[i+3]=='e'||s[i+3]=='E') && (s[i+4]=='d'||s[i+4]=='D'))
      return true;
    if ((s[i+1]=='a'||s[i+1]=='A') && (s[i+2]=='t'||s[i+2]=='T') &&
        (s[i+3]=='o'||s[i+3]=='O') && (s[i+4]=='m'||s[i+4]=='M'))
      return true;
    if ((s[i+1]=='r'||s[i+1]=='R') && (s[i+2]=='d'||s[i+2]=='D') &&
        (s[i+3]=='f'||s[i+3]=='F') && (s[i+4]==':'))
      return true;
  }
  return false;
}

static void feed_reset_item() {
  feed_have_title = feed_have_link = false;
  feed_tlen = feed_llen = feed_dlen = 0;
  feed_title[0] = 0; feed_link[0] = 0; feed_desc[0] = 0;
}

static void title_char(char c) {
  size_t n = strlen(D->title);
  if (n + 1 >= MAX_TITLE) return;
  if (ascii_space(c)) { title_space = n > 0; return; }
  if (title_space && n && n + 1 < MAX_TITLE) D->title[n++] = ' ';
  D->title[n++] = c; D->title[n] = 0; title_space = false;
}

// Feature-phone compatibility: normalize a few common UTF-8 punctuation characters
// that the tiny built-in font often cannot draw. Vietnamese/other UTF-8 text remains intact.
static bool utf8_compat_punct(const char *s, size_t n, size_t *i, char *out) {
  size_t p = *i;
  if (p + 2 < n && (unsigned char)s[p] == 0xE2 && (unsigned char)s[p + 1] == 0x80) {
    unsigned char c = (unsigned char)s[p + 2];
    if (c == 0x93 || c == 0x94) *out = '-';          // en/em dash
    else if (c == 0x98 || c == 0x99) *out = '\''; // curly single quote
    else if (c == 0x9C || c == 0x9D) *out = '"';   // curly double quote
    else if (c == 0xA2) *out = '*';                 // bullet
    else return false;
    *i = p + 3;
    return true;
  }
  if (p + 1 < n && (unsigned char)s[p] == 0xC2 && (unsigned char)s[p + 1] == 0xA0) {
    *out = ' '; *i = p + 2; return true;             // UTF-8 NBSP
  }
  return false;
}

static void link_close_to(int line) {
  if (open_link >= 0) D->links[open_link].line1 = line;
}

static void flush_line() {
  if (D->nlines >= MAX_LINES) { cur_off = D->len; cur_chars = 0; cur_px = 0; return; }
  if (D->len + 1 >= D->cap) return;
  D->buf[D->len] = 0;                       // ket thuc dong bang NUL -> dung dc voi API null-term
  if (cur_chars == 0) {
    // dong rong: chi ghi neu khong phai dong dau tien lien tiep
    if (D->nlines > 0 && D->lines[D->nlines - 1].n == 0) {
      // Khong tao them NUL khi bo qua empty-line trung lap; cur_off phai tro vao
      // byte se duoc ghi tiep. +1 o day se lam mat ky tu dau cua block ke tiep.
      cur_off = D->len;
      return;
    }
    D->lines[D->nlines].off = cur_off;
    D->lines[D->nlines].n = 0;
    D->lines[D->nlines].style = cur_style;
    D->lines[D->nlines].link = -1;
    D->lines[D->nlines].image = current_image;
    D->lines[D->nlines].block = cur_block;
    if (open_link >= 0) D->links[open_link].line1 = D->nlines;
    D->nlines++;
    D->len++;                               // an NUL
    cur_off = D->len; cur_chars = 0; cur_px = 0; pending_space = false;
    return;
  }
  D->lines[D->nlines].off = cur_off;
  D->lines[D->nlines].n = cur_chars;
  D->lines[D->nlines].style = cur_style;
  D->lines[D->nlines].link = (open_link >= 0) ? open_link : -1;
  D->lines[D->nlines].image = current_image;
  D->lines[D->nlines].block = cur_block;
  if (open_link >= 0)
    D->links[open_link].line1 = D->nlines;   // mo rong den dong hien tai
  D->nlines++;
  D->len++;                                 // an NUL
  cur_off = D->len; cur_chars = 0; cur_px = 0; pending_space = false;
}

static void put_char(char c) {
  if (D->len + 2 >= D->cap) return;
  // put_char chi dung cho entity/ky tu le (ASCII). UTF-8 nhieu byte di qua put_word.
  char t[2] = { c, 0 };
  int w = gfx_textWidth(t, cur_style);
  if (cur_px + w > SCR_W - 6 && cur_chars > 0) {
    flush_line();
    if (c == ' ') return;          // khong bat dau dong bang khoang trang
  }
  D->buf[D->len++] = c;
  cur_chars++; cur_px += w; pending_space = false;
}

static void put_word(const char *nul_term, int n) {
  if (D->len + n >= D->cap) return;
  if (pending_space && cur_chars > 0) put_char(' ');
  int w = gfx_textWidth(nul_term, cur_style);
  if (cur_px + w > SCR_W - 6 && cur_chars > 0) flush_line();
  memcpy(D->buf + D->len, nul_term, n);
  D->len += n;
  cur_chars += n;
  cur_px += w;
  pending_space = false;
}

// Feed: Đóng 1 mục — title thành link clickable nếu có URL, không thì text thường.
static void feed_emit_item() {
  if (feed_have_title || feed_have_link) {
    if (cur_chars > 0) flush_line();
    if (feed_have_link && D->nlinks < MAX_LINKS) {
      int li = D->nlinks++;
      strncpy(D->links[li].url, feed_link, sizeof D->links[li].url - 1);
      D->links[li].url[sizeof D->links[li].url - 1] = 0;
      D->links[li].line0 = D->nlines;
      D->links[li].line1 = D->nlines;
      open_link = li;
      const char *lab = feed_have_title ? feed_title : feed_link;
      put_word(lab, (int)strlen(lab));
      if (cur_chars > 0) flush_line();
      link_close_to(D->nlines > 0 ? D->nlines - 1 : 0);
      open_link = -1;
    } else if (feed_have_title) {
      put_word(feed_title, feed_tlen);
      flush_line();
    }
    pending_space = false;
    // Description defer (BBC radio: title, description, link)
    if (feed_dlen > 0) {
      flush_line();
      int save_style = cur_style;
      cur_style = 5;
      cur_block = next_block++;
      put_word(feed_desc, feed_dlen);
      flush_line();
      cur_style = save_style;
      pending_space = false;
    }
  }
  feed_reset_item();
}

static void feed_put_char(char c) {
  if (feed_in_title) {
    if (feed_tlen + 1 < (int)sizeof feed_title) {
      if (ascii_space(c)) {
        if (feed_tlen > 0 && feed_title[feed_tlen-1] != ' ') feed_title[feed_tlen++] = ' ';
      } else feed_title[feed_tlen++] = c;
      feed_title[feed_tlen] = 0;
      feed_have_title = feed_tlen > 0;
    }
  } else if (feed_in_link_text) {
    if (feed_llen + 1 < (int)sizeof feed_link) {
      feed_link[feed_llen++] = c;
      feed_link[feed_llen] = 0;
      feed_have_link = feed_llen > 0;
    }
  }
}

bool doc_line_is_separator(Doc *d, int i) {
  // Dong text ngan giua 2 link cung <p> (vd "|" tren Speed Dial) la dau phan cach,
  // khong phai khoi focus — D-Pad phai buoc qua.
  if (!d || i < 0 || i >= d->nlines) return false;
  if (d->lines[i].n == 0 || d->lines[i].link >= 0) return false;
  int a = i - 1;
  while (a >= 0 && d->lines[a].n == 0) a--;
  int b = i + 1;
  while (b < d->nlines && d->lines[b].n == 0) b++;
  if (a < 0 || b >= d->nlines) return false;
  return d->lines[a].link >= 0 && d->lines[b].link >= 0 &&
         d->lines[a].block == d->lines[i].block &&
         d->lines[b].block == d->lines[i].block;
}

bool doc_parse(Doc *d, const char *src, size_t n) {
  D = d;
  d->len = 0; d->nlines = 0; d->nlinks = 0; d->nimages = 0;
  d->title[0] = 0; d->is_wml = false;
  cur_off = 0; cur_chars = 0; cur_px = 0; cur_style = 0; open_link = -1; current_image = -1;
  cur_block = 0; next_block = 1;
  pending_space = false; in_title = false; title_space = false;
  skip_name[0] = 0; skip_depth = 0;
  feed_mode = looks_like_feed(src, n);
  feed_in_item = feed_in_title = feed_in_link_text = feed_in_desc = false;
  feed_reset_item();
  if (feed_mode) Serial.println("[feed] RSS/Atom detected");

  size_t i = 0;
  static char name[24], href[192], title[64], alt[96], src_url[192];
  static char lazy_url[192], srcset_url[512], image_url[192];
  static char word[256]; int wi = 0;

  while (i < n) {
    char c = src[i];
    // <![CDATA[ ... ]]> — RSS/Atom gap 2.0 (BBC, CNN…). tag_scan se huy no nhu comment.
    if (c == '<' && i + 9 <= n && !memcmp(src + i, "<![CDATA[", 9)) {
      if (wi) { word[wi]=0; put_word(word, wi); wi = 0; }
      size_t cs = i + 9;
      size_t ce = cs;
      while (ce + 2 < n && !(src[ce]==']' && src[ce+1]==']' && src[ce+2]=='>')) ce++;
      size_t clen = (ce > cs) ? ce - cs : 0;
      if (feed_mode && (feed_in_title || feed_in_link_text)) {
        for (size_t k = 0; k < clen; k++) feed_put_char(src[cs + k]);
      } else if (in_title) {
        for (size_t k = 0; k < clen; k++) title_char(src[cs + k]);
      } else       if (feed_mode && feed_in_desc) {
        for (size_t k = 0; k < clen; k++) {
          char ch = src[cs + k];
          if (feed_dlen + 1 < (int)sizeof feed_desc) {
            if (ascii_space(ch)) {
              if (feed_dlen > 0 && feed_desc[feed_dlen-1] != ' ') feed_desc[feed_dlen++] = ' ';
            } else feed_desc[feed_dlen++] = ch;
            feed_desc[feed_dlen] = 0;
          }
        }
      } else if (!feed_mode) {
        for (size_t k = 0; k < clen; k++) {
          char ch = src[cs + k];
          if (ascii_space(ch)) {
            if (wi) { word[wi]=0; put_word(word, wi); wi = 0; }
            if (cur_chars > 0) pending_space = true;
          } else if (wi < (int)sizeof word - 1) word[wi++] = ch;
        }
      }
      // feed nhung text KHONG nam trong capture -> bo qua (guid/pubDate/channel meta)
      i = (ce + 2 < n) ? ce + 3 : n;
      continue;
    }
    if (c == '<') {
      if (wi) { word[wi]=0; put_word(word, wi); wi = 0; }
      size_t j = i;
      int r = tag_scan(src, n, &j, name, sizeof name, href, sizeof href, title, sizeof title,
                       alt, sizeof alt, src_url, sizeof src_url,
                       lazy_url, sizeof lazy_url, srcset_url, sizeof srcset_url);
      if (r == 0) { i++; continue; }
      i = j;
      if (skip_depth > 0) {
        if (r == 1 && !strcmp(name, skip_name)) skip_depth++;
        else if (r == -2 && !strcmp(name, skip_name) && --skip_depth == 0) skip_name[0] = 0;
        continue;
      }
      // ---- Feed mode branch ----
      if (feed_mode) {
        bool is_item = !strcmp(name, "item") || !strcmp(name, "entry");
        bool is_title = !strcmp(name, "title");
        bool is_link = !strcmp(name, "link") || !strcmp(name, "atom:link");
        bool is_desc = !strcmp(name, "description") || !strcmp(name, "summary") ||
                       !strcmp(name, "content") || !strcmp(name, "encoded") ||
                       !strcmp(name, "media:description");
        bool is_skip = !strcmp(name, "guid") || !strcmp(name, "pubdate") ||
                       !strcmp(name, "published") || !strcmp(name, "updated") ||
                       !strcmp(name, "author") || !strcmp(name, "creator") ||
                       !strcmp(name, "category") || !strcmp(name, "comment") ||
                       !strcmp(name, "enclosure") || !strcmp(name, "thumbnail") ||
                       !strcmp(name, "media:content") || !strcmp(name, "media:thumbnail");

        if (r == -2) { // closing
          if (is_title) {
            if (feed_in_title) {
              feed_in_title = false;
            } else {
              in_title = false; title_space = false; // channel/page title done
            }
            continue;
          }
          if (is_link && feed_in_link_text) {
            feed_in_link_text = false;
            // emit se xay ra o </item> / </entry> de description luon nam sau title
            continue;
          }
          if (is_desc && feed_in_desc) {
            feed_in_desc = false;
            continue;
          }
          if (is_item && feed_in_item) {
            feed_in_item = false;
            feed_emit_item();
            cur_block = next_block++;
            continue;
          }
          continue;
        }
        if (r == -1) continue;
        if (is_skip) {
          // Leaf meta (guid/pubDate/…): bo qua tag — text rac se bi bo o feed text path.
          // KHONG dung skip_depth: media:thumbnail self-closing se lam skip_depth treo.
          continue;
        }
        if (is_item) {
          if (feed_in_item) feed_emit_item();
          feed_in_item = true;
          feed_reset_item();
          flush_line();
          cur_block = next_block++;
          cur_style = 0;
          continue;
        }
        if (is_title) {
          if (feed_in_item) {
            feed_in_title = true;
            feed_tlen = 0; feed_title[0] = 0; feed_have_title = false;
          } else if (!d->title[0]) {
            in_title = true; title_space = false; // channel/page title only once
          }
          continue;
        }
        if (is_link) {
          if (href[0]) { // Atom: <link href="..."/>
            if (feed_in_item) {
              strncpy(feed_link, href, sizeof feed_link - 1);
              feed_link[sizeof feed_link - 1] = 0;
              feed_have_link = feed_link[0] != 0;
              feed_llen = (int)strlen(feed_link);
              // emit o </entry> — giu description co thu tu dung
            }
            continue;
          }
          if (feed_in_item) {
            feed_in_link_text = true;
            feed_llen = 0; feed_link[0] = 0; feed_have_link = false;
          }
          continue;
        }
        if (is_desc) {
          // defer: khong in ngay — BBC co description TRUOC link
          if (feed_in_item) {
            feed_in_desc = true;
            feed_dlen = 0; feed_desc[0] = 0;
          }
          continue;
        }
        // Inside item, title/link known: if RSS order title then link, emit after link text closes.
        // Fall through for other tags (channel, rss, etc.)
        if (isblock(name)) {
          if (feed_in_item || strcmp(name, "channel") == 0 || strcmp(name, "rss") == 0 ||
              strcmp(name, "feed") == 0 || strcmp(name, "rdf") == 0) {
            // don't break lines aggressively inside item except at emit
            if (!feed_in_item) {
              flush_line();
              if (starts_focus_block(name)) cur_block = next_block++;
              cur_style = heading_style(name);
            }
          }
          continue;
        }
        // unknown/open tags in feed: ignore name, keep going
        continue;
      }
      // ---- End feed mode branch ----
      if (r == -2) {                       // the dong: dong link neu la a/anchor
        if (!strcmp(name, "title")) { in_title = false; title_space = false; continue; }
        if (!strcmp(name, "a") || !strcmp(name, "anchor") || !strcmp(name, "option")) {
          // HTML keypad browser: tach link thanh focus block rieng.
          // Flush KHI open_link con hieu luc de dong cuoi cua link khong bi mat link-id.
          if (open_link >= 0 && cur_chars > 0) flush_line();
          if (open_link >= 0) link_close_to(D->nlines > 0 ? D->nlines - 1 : 0);
          open_link = -1;
          pending_space = false;
        }
        continue;
      }
      if (r == -1) continue;               // comment/doctype
      attr_decode(href);
      attr_decode(title);
      attr_decode(alt);
      attr_decode(src_url);
      attr_decode(lazy_url);
      attr_decode(srcset_url);
      image_source_pick(lazy_url, srcset_url, src_url, image_url, sizeof image_url);
      if (!strcmp(name, "script") || !strcmp(name, "style") || !strcmp(name, "noscript") ||
          !strcmp(name, "svg") || !strcmp(name, "canvas") || !strcmp(name, "template") ||
          !strcmp(name, "iframe") || !strcmp(name, "object")) {
        strncpy(skip_name, name, sizeof skip_name - 1); skip_name[sizeof skip_name - 1] = 0;
        skip_depth = 1; continue;
      }
      if (!strcmp(name, "title")) { in_title = true; title_space = false; continue; }
      if (!strcmp(name, "wml") || !strcmp(name, "card")) {
        if (title[0] && !d->title[0]) strncpy(d->title, title, MAX_TITLE - 1);
        if (!strcmp(name, "wml")) d->is_wml = true;
      }
      bool is_link = !strcmp(name, "a") || !strcmp(name, "anchor") ||
                     (!strcmp(name, "option") && href[0]) ||
                     !strcmp(name, "go") || !strcmp(name, "prev");
      if (is_link && href[0] && d->nlinks < MAX_LINKS) {
        // Neu link nam giua paragraph, ket thuc text truoc link de focus rectangle
        // khong nuot ca phan van ban khong clickable.
        if (open_link < 0 && cur_chars > 0) flush_line();
        int li = d->nlinks++;
        strncpy(d->links[li].url, href, sizeof d->links[li].url - 1);
        d->links[li].url[sizeof d->links[li].url - 1] = 0;
        d->links[li].line0 = d->nlines;   // dong dang xay (chua flush)
        d->links[li].line1 = d->nlines;
        open_link = li;
      }
      if (!strcmp(name, "img")) {
        if (image_url[0]) {
          if (wi) { word[wi]=0; put_word(word, wi); wi = 0; }
          flush_line();
          cur_style = 7;
          cur_block = next_block++;
          current_image = -1;
          if (d->nimages < MAX_IMAGES) {
            int ii = d->nimages++;
            strncpy(d->images[ii].url, image_url, sizeof d->images[ii].url - 1);
            d->images[ii].url[sizeof d->images[ii].url - 1] = 0;
            const char *label = alt[0] ? alt : image_url;
            strncpy(d->images[ii].alt, label, sizeof d->images[ii].alt - 1);
            d->images[ii].alt[sizeof d->images[ii].alt - 1] = 0;
            d->images[ii].line0 = d->nlines;
            d->images[ii].line1 = d->nlines;
            current_image = ii;
          }
          const char *label = alt[0] ? alt : image_url;
          put_word(label, (int)strlen(label));
          flush_line();
          if (current_image >= 0) d->images[current_image].line1 = d->nlines - 1;
          cur_style = 0;
          current_image = -1;
          pending_space = false;
        } else if (alt[0]) {
          if (wi) { word[wi]=0; put_word(word, wi); wi = 0; }
          flush_line();
          cur_block = next_block++;
          put_word(alt, (int)strlen(alt));
          flush_line();
          cur_style = 0;
          pending_space = false;
        }
      }
      if (!strcmp(name, "hr")) {
        flush_line(); cur_style = 5;
        put_word("------------------------", 24); flush_line(); cur_style = 0;
      }
      if (isblock(name)) {
        flush_line();
        if (starts_focus_block(name)) cur_block = next_block++;
        cur_style = heading_style(name);
      }
      continue;
    }
    if (skip_depth > 0) { i++; continue; }
    // Feed text capture (title / link text / description body)
    if (feed_mode && (feed_in_title || feed_in_link_text)) {
      char compat = 0;
      size_t ni = i;
      if (utf8_compat_punct(src, n, &ni, &compat)) { feed_put_char(compat); i = ni; continue; }
      if (c == '&') {
        static const struct { const char *k; char v; } TE[] = {
          {"&amp;",'&'},{"&lt;",'<'},{"&gt;",'>'},{"&nbsp;",' '},{"&quot;",'"'},{"&apos;",'\''},{0,0}};
        bool done = false;
        for (int k = 0; TE[k].k; k++) {
          size_t el = strlen(TE[k].k);
          if (i + el <= n && !memcmp(src + i, TE[k].k, el)) {
            feed_put_char(TE[k].v); i += el; done = true; break;
          }
        }
        if (done) continue;
        char u[4]; size_t uend;
        int ul = entity_utf8(src, n, i, u, &uend);
        if (ul) {
          for (int b = 0; b < ul; b++) feed_put_char(u[b]);
          i = uend; continue;
        }
      }
      if (!ascii_space(c) || feed_in_title) feed_put_char(c);
      else if (feed_in_link_text && !ascii_space(c)) feed_put_char(c);
      i++; continue;
    }
    if (feed_mode && feed_in_desc) {
      // description -> buffer defer (in sau title link)
      if (c == '&') {
        char u[4]; size_t uend;
        int ul = entity_utf8(src, n, i, u, &uend);
        if (ul) {
          for (int b = 0; b < ul; b++) {
            char ch = u[b];
            if (feed_dlen + 1 < (int)sizeof feed_desc) {
              if (ascii_space(ch)) {
                if (feed_dlen > 0 && feed_desc[feed_dlen-1] != ' ') feed_desc[feed_dlen++] = ' ';
              } else feed_desc[feed_dlen++] = ch;
              feed_desc[feed_dlen] = 0;
            }
          }
          i = uend; continue;
        }
      }
      if (ascii_space(c)) {
        if (feed_dlen > 0 && feed_desc[feed_dlen-1] != ' ' && feed_dlen + 1 < (int)sizeof feed_desc)
          feed_desc[feed_dlen++] = ' ';
        i++; continue;
      }
      if (feed_dlen + 1 < (int)sizeof feed_desc) {
        feed_desc[feed_dlen++] = c;
        feed_desc[feed_dlen] = 0;
      }
      i++; continue;
    }
    // Feed text rac ngoai title/link/desc (guid, pubDate, channel link…) -> bo qua
    if (feed_mode && !feed_in_title && !feed_in_link_text && !feed_in_desc && !in_title) {
      if (c == '&' || isspace((unsigned char)c)) { i++; continue; }
      i++; continue;
    }
    if (in_title) {
      char compat = 0;
      size_t ni = i;
      if (utf8_compat_punct(src, n, &ni, &compat)) { title_char(compat); i = ni; continue; }
      if (c == '&') {
        static const struct { const char *k; char v; } TE[] = {
          {"&amp;",'&'},{"&lt;",'<'},{"&gt;",'>'},{"&nbsp;",' '},{"&quot;",'"'},{"&apos;",'\''},{0,0}};
        bool done = false;
        for (int k = 0; TE[k].k; k++) {
          size_t el = strlen(TE[k].k);
          if (i + el <= n && !memcmp(src + i, TE[k].k, el)) {
            title_char(TE[k].v); i += el; done = true; break;
          }
        }
        if (done) continue;
        char u[4]; size_t uend;
        int ul = entity_utf8(src, n, i, u, &uend);
        if (ul) {
          for (int b = 0; b < ul; b++) title_char(u[b]);
          i = uend; continue;
        }
      }
      title_char(c); i++; continue;
    }
    {
      char compat = 0;
      size_t ni = i;
      if (utf8_compat_punct(src, n, &ni, &compat)) {
        if (wi) { word[wi]=0; put_word(word, wi); wi = 0; }
        put_char(compat); i = ni; continue;
      }
    }
    if (c == '&') {                        // entity: giai ma tung loai
      static const struct { const char *k; char v; } E[] = {
        {"&amp;",'&'},{"&lt;",'<'},
        {"&gt;",'>'},{"&nbsp;",' '},
        {"&quot;",'"'},{"&apos;",'\''},{0,0}};
      bool done = false;
      for (int k = 0; E[k].k; k++) {
        size_t el = strlen(E[k].k);
        if (i + el <= n && !memcmp(src + i, E[k].k, el)) {
          if (wi) { word[wi]=0; put_word(word, wi); wi = 0; }
          put_char(E[k].v);
          i += el; done = true; break;
        }
      }
      if (done) continue;
      // numeric entity -> UTF-8 (Vietnamese: &#7871; ế, &#225; á, ...)
      char u[4]; size_t uend;
      int ul = entity_utf8(src, n, i, u, &uend);
      if (ul) {
        if (ul == 1 && ascii_space(u[0])) {
          if (wi) { word[wi]=0; put_word(word, wi); wi = 0; }
          if (cur_chars > 0) pending_space = true;
        } else {
          for (int b = 0; b < ul; b++)
            if (wi < (int)sizeof word - 1) word[wi++] = u[b];
        }
        i = uend;
        continue;
      }
      // & thuong -> giu nguyen
    }
    if (ascii_space(c)) {
      if (wi) { word[wi]=0; put_word(word, wi); wi = 0; }
      if (cur_chars > 0) pending_space = true;
      i++;
      continue;
    }
    if (wi < (int)sizeof word - 1) word[wi++] = c;
    i++;
  }
  if (wi) { word[wi]=0; put_word(word, wi); }
  flush_line();
  if (d->nlines && d->lines[d->nlines - 1].n == 0) d->nlines--;   // bo dong rong cuoi
  // gan chi so link cho cac dong trong vung cua no
  for (int l = 0; l < d->nlinks; l++)
    for (int y = d->links[l].line0; y >= 0 && y <= d->links[l].line1 && y < d->nlines; y++)
      d->lines[y].link = l;
  // bao dam moi link co it nhat 1 dong danh dau (truong hop link rong)
  for (int l = 0; l < d->nlinks; l++)
    if (d->links[l].line0 >= 0 && d->links[l].line0 < d->nlines)
      d->lines[d->links[l].line0].link = l;
  return true;
}

void doc_free(Doc *d) { (void)d; }   // buf PSRAM tai su dung, khong free
