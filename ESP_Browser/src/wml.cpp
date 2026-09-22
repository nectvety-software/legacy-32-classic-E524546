// wml.cpp — Parser WML 1.1 subset + HTML subset -> danh sach dong da boc (wrap).
// Port co che DOM/Parser/Render* cua DMAX_QQ_Browser (323 module C++ goc) thanh
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
// style: 0 body, 1 field, 2 h1/h2, 3 folder, 4 h3-h6, 5 small/meta, 6 bold
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

// tach ten the + thuoc tinh href/title. Tra ve 1=the mo, -1=the dong/comment, 0=khong phai the
static int tag_scan(const char *s, size_t n, size_t *i, char *name, int ncap,
                    char *href, int hcap, char *title, int tcap) {
  name[0] = href[0] = title[0] = 0;
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
    while (p < n && (s[p] == ' ' || s[p] == '\t' || s[p] == '\r' || s[p] == '\n')) p++;
    size_t ks = p;
    while (p < n && s[p] != '=' && s[p] != '>' && s[p] != ' ' && s[p] != '/') p++;
    size_t ke = p;
    if (p < n && s[p] == '=') {
      p++;
      char q = (p < n && (s[p] == '"' || s[p] == '\'')) ? s[p] : 0;
      if (q) p++;
      size_t vs = p;
      while (p < n && s[p] != (q ? q : '>')) p++;
      size_t ve = p;
      if (p < n && q) p++;
      char key[16]; size_t kl = ke - ks; if (kl >= sizeof key) kl = sizeof key - 1;
      for (size_t k = 0; k < kl; k++) key[k] = tolower(s[ks + k]);
      key[kl] = 0;
      if (!strcmp(key, "href") && href && ve > vs) {
        size_t l = ve - vs; if (l >= (size_t)hcap) l = hcap - 1;
        memcpy(href, s + vs, l); href[l] = 0;
      } else if (!strcmp(key, "title") && title && ve > vs) {
        size_t l = ve - vs; if (l >= (size_t)tcap) l = tcap - 1;
        memcpy(title, s + vs, l); title[l] = 0;
      }
    } else {
      if (p < n && s[p] == '/') p++;
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

static void link_close_to(int line) {
  if (open_link >= 0) D->links[open_link].line1 = line;
}

static void flush_line() {
  if (D->nlines >= MAX_LINES) { cur_off = D->len; cur_chars = 0; cur_px = 0; return; }
  if (D->len + 1 >= D->cap) return;
  D->buf[D->len] = 0;                       // ket thuc dong bang NUL -> dung dc voi API null-term
  if (cur_chars == 0) {
    // dong rong: chi ghi neu khong phai dong dau tien lien tiep
    if (D->nlines > 0 && D->lines[D->nlines - 1].n == 0) { cur_off = D->len + 1; return; }
    D->lines[D->nlines].off = cur_off;
    D->lines[D->nlines].n = 0;
    D->lines[D->nlines].style = cur_style;
    D->lines[D->nlines].link = -1;
    if (open_link >= 0) D->links[open_link].line1 = D->nlines;
    D->nlines++;
    D->len++;                               // an NUL
    cur_off = D->len; cur_chars = 0; cur_px = 0;
    return;
  }
  D->lines[D->nlines].off = cur_off;
  D->lines[D->nlines].n = cur_chars;
  D->lines[D->nlines].style = cur_style;
  D->lines[D->nlines].link = (open_link >= 0) ? open_link : -1;
  if (open_link >= 0)
    D->links[open_link].line1 = D->nlines;   // mo rong den dong hien tai
  D->nlines++;
  D->len++;                                 // an NUL
  cur_off = D->len; cur_chars = 0; cur_px = 0;
}

static void put_char(char c) {
  if (D->len + 2 >= D->cap) return;
  char t[2] = { c, 0 };
  int w = gfx_textWidth(t, cur_style);
  if (cur_px + w > SCR_W - 6 && cur_chars > 0) {
    flush_line();
    if (c == ' ') return;          // khong bat dau dong bang khoang trang
  }
  D->buf[D->len++] = c;
  cur_chars++; cur_px += w;
}

static void put_word(const char *nul_term, int n) {
  if (D->len + n >= D->cap) return;
  int w = gfx_textWidth(nul_term, cur_style);
  if (cur_px + w > SCR_W - 6 && cur_chars > 0) flush_line();
  memcpy(D->buf + D->len, nul_term, n);
  D->len += n;
  cur_chars += n;
  cur_px += w;
}

bool doc_parse(Doc *d, const char *src, size_t n) {
  D = d;
  d->len = 0; d->nlines = 0; d->nlinks = 0;
  d->title[0] = 0; d->is_wml = false;
  cur_off = 0; cur_chars = 0; cur_px = 0; cur_style = 0; open_link = -1;

  size_t i = 0;
  char name[24], href[192], title[64];
  char word[256]; int wi = 0;

  while (i < n) {
    char c = src[i];
    if (c == '<') {
      if (wi) { word[wi]=0; put_word(word, wi); wi = 0; }
      size_t j = i;
      int r = tag_scan(src, n, &j, name, sizeof name, href, sizeof href, title, sizeof title);
      if (r == 0) { i++; continue; }
      i = j;
      if (r == -2) {                       // the dong: dong link neu la a/anchor
        if (!strcmp(name, "a") || !strcmp(name, "anchor") || !strcmp(name, "option")) {
          link_close_to(D->nlines);        // dong dang xay = D->nlines (chua flush)
          open_link = -1;
        }
        continue;
      }
      if (r == -1) continue;               // comment/doctype
      if (!strcmp(name, "wml") || !strcmp(name, "card")) {
        if (title[0] && !d->title[0]) strncpy(d->title, title, MAX_TITLE - 1);
        if (!strcmp(name, "wml")) d->is_wml = true;
      }
      bool is_link = !strcmp(name, "a") || !strcmp(name, "anchor") ||
                     (!strcmp(name, "option") && href[0]) ||
                     !strcmp(name, "go") || !strcmp(name, "prev");
      if (is_link && href[0] && d->nlinks < MAX_LINKS) {
        int li = d->nlinks++;
        strncpy(d->links[li].url, href, sizeof d->links[li].url - 1);
        d->links[li].url[sizeof d->links[li].url - 1] = 0;
        d->links[li].line0 = d->nlines;   // dong dang xay (chua flush)
        d->links[li].line1 = d->nlines;
        open_link = li;
      }
      if (isblock(name)) {
        flush_line();
        cur_style = heading_style(name);
      }
      continue;
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
      // numeric entity
      if (i + 2 < n && src[i + 1] == '#') {
        int base = (i + 2 < n && (src[i + 2] == 'x' || src[i + 2] == 'X')) ? 16 : 10;
        char tmp[8]; int tl = 0;
        size_t k = i + (base == 16 ? 3 : 2);
        while (k < n && tl < 6 && src[k] != ';') tmp[tl++] = src[k++];
        tmp[tl] = 0;
        long v = strtol(tmp, 0, base);
        if (tl > 0 && v > 31 && v < 127) {
          if (wi) { word[wi]=0; put_word(word, wi); wi = 0; }
          put_char((char)v);
          i = k; if (i < n && src[i] == ';') i++;
          continue;
        }
      }
      // & thuong -> giu nguyen
    }
    if (isspace((unsigned char)c)) {
      if (wi) { word[wi]=0; put_word(word, wi); wi = 0; }
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
