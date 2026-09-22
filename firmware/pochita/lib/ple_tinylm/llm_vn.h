// Byte-level GPT-style transformer for Vietnamese (no-diacritics) text.
// Not a PLE model: vocab = raw UTF-8 bytes (0..255), weights are raw FP32.
//
// File layout:
//   0..3    magic 0x4C494C4B ("KLIL")
//   4..15   u16 LE: vocab, dim, n_layers, n_heads, ffn, seq_len
//   16..31  zero padding
//   32..    FP32 weights in state_dict order:
//           tok_emb [V*D]
//           per layer: norm1 [D], qkv_proj [3D*D], out_proj [D*D],
//                      norm2 [D], ffn_gate [F*D], ffn_up [F*D], ffn_down [D*F]
//           out_norm [D]
//           head [V*D]  (tied to tok_emb; duplicated in the file)
//   remainder of the file may be zero padding.
#ifndef LLM_VN_H
#define LLM_VN_H
#include <stdint.h>
#include <math.h>
#include <string.h>

#define VN_MAGIC 0x4C494C4Bu
#define VN_HEADER_BYTES 32
#define VN_RMS_EPS 1e-6f
#define VN_MAX_D 512
#define VN_MAX_HALF (VN_MAX_D / 2 / 2)
#define VN_MAX_LAYERS 32
#define VN_MAX_FFN 1024

typedef struct {
  int vocab, dim, n_layers, n_heads, ffn, seq_len;
} VnCfg;

typedef struct {
  const float *norm1;
  const float *qkv;      // [3*D*D]
  const float *out_proj; // [D*D]
  const float *norm2;
  const float *gate;     // [F*D]
  const float *up;       // [F*D]
  const float *down;     // [D*F]
} VnLayer;

typedef struct {
  VnCfg c;
  const float *tok_emb;              // [V*D]
  VnLayer layer[VN_MAX_LAYERS];
  const float *out_norm;
} VnModel;

typedef struct {
  float *x, *h, *qkv, *att, *g1, *g2, *logits, *scores;
  float *kcache, *vcache;
} VnScratch;

static size_t vn_weight_floats(const VnCfg *c) {
  size_t D = c->dim, L = c->n_layers, F = c->ffn, V = c->vocab;
  size_t n = V * D;
  for (size_t l = 0; l < L; l++)
    n += D + 3 * D * D + D * D + D + F * D + F * D + D * F;
  n += D;      // out_norm
  n += V * D;  // head (tied duplicate)
  return n;
}

static size_t vn_weight_bytes(const VnCfg *c) {
  return vn_weight_floats(c) * sizeof(float);
}

// Parse the 32-byte header. Returns 0 on success, -1 if magic/size invalid.
static int vn_parse_header(const uint8_t *hdr, VnCfg *c) {
  uint32_t magic;
  memcpy(&magic, hdr, 4);
  if (magic != VN_MAGIC) return -1;
  uint16_t u;
  memcpy(&u, hdr + 4, 2);  c->vocab    = u;
  memcpy(&u, hdr + 6, 2);  c->dim      = u;
  memcpy(&u, hdr + 8, 2);  c->n_layers = u;
  memcpy(&u, hdr + 10, 2); c->n_heads  = u;
  memcpy(&u, hdr + 12, 2); c->ffn      = u;
  memcpy(&u, hdr + 14, 2); c->seq_len  = u;
  if (c->vocab <= 0 || c->dim <= 0 || c->n_layers <= 0 || c->n_heads <= 0 ||
      c->ffn <= 0 || c->seq_len <= 0 || c->dim > VN_MAX_D ||
      c->ffn > VN_MAX_FFN || c->n_layers > VN_MAX_LAYERS ||
      c->dim % c->n_heads != 0)
    return -1;
  return 0;
}

// Bind tensor pointers from the raw FP32 weights (starting at hdr+32).
static void vn_bind(const uint8_t *p, VnModel *m) {
  int D = m->c.dim, L = m->c.n_layers, F = m->c.ffn, V = m->c.vocab;
  m->tok_emb = (const float *)p; p += (size_t)V * D * 4;
  for (int l = 0; l < L; l++) {
    m->layer[l].norm1    = (const float *)p; p += (size_t)D * 4;
    m->layer[l].qkv      = (const float *)p; p += (size_t)3 * D * D * 4;
    m->layer[l].out_proj = (const float *)p; p += (size_t)D * D * 4;
    m->layer[l].norm2    = (const float *)p; p += (size_t)D * 4;
    m->layer[l].gate     = (const float *)p; p += (size_t)F * D * 4;
    m->layer[l].up       = (const float *)p; p += (size_t)F * D * 4;
    m->layer[l].down     = (const float *)p; p += (size_t)D * F * 4;
  }
  m->out_norm = (const float *)p;
}

// Parse header (must already be loaded at model_buf[0]) and bind weights.
static int vn_load(const uint8_t *base, VnModel *m) {
  if (vn_parse_header(base, &m->c)) return -1;
  vn_bind(base + VN_HEADER_BYTES, m);
  return 0;
}

static inline float vn_rmsnorm(const float *x, const float *w, int n, float *out) {
  float ss = 0;
  for (int i = 0; i < n; i++) ss += x[i] * x[i];
  float inv = 1.0f / sqrtf(ss / n + VN_RMS_EPS);
  for (int i = 0; i < n; i++) out[i] = w[i] * (x[i] * inv);
  return 0;
}

static inline float vn_silu(float v) { return v / (1.0f + expf(-v)); }

// y[r] = sum_j W[r,j] * x[j]
static void vn_matvec(const float *W, int rows, int cols, const float *x, float *y) {
  for (int r = 0; r < rows; r++) {
    const float *row = W + (size_t)r * cols;
    float acc = 0;
    for (int j = 0; j < cols; j++) acc += row[j] * x[j];
    y[r] = acc;
  }
}

static void vn_forward(VnModel *m, int token, int pos, VnScratch *s) {
  int D = m->c.dim, L = m->c.n_layers, H = m->c.n_heads, Dh = D / H;
  int F = m->c.ffn, S = m->c.seq_len, V = m->c.vocab;
  if (pos >= S) pos = S - 1;  // clamp to avoid KV-cache / scores overflow

  memcpy(s->x, m->tok_emb + (size_t)token * D, (size_t)D * sizeof(float));

  // RoPE frequencies for this absolute position (head_dim split-half)
  float rc[VN_MAX_HALF], rs[VN_MAX_HALF];
  for (int i = 0; i < Dh / 2; i++) {
    float freq = powf(10000.0f, -2.0f * i / (float)Dh);
    rc[i] = cosf(pos * freq);
    rs[i] = sinf(pos * freq);
  }

  for (int l = 0; l < L; l++) {
    const VnLayer *ly = &m->layer[l];

    // ---- attention ----
    vn_rmsnorm(s->x, ly->norm1, D, s->h);
    vn_matvec(ly->qkv, 3 * D, D, s->h, s->qkv);
    float *q = s->qkv, *k = s->qkv + D, *v = s->qkv + 2 * D;
    for (int hh = 0; hh < H; hh++) {
      float *qh = q + hh * Dh, *kh = k + hh * Dh;
      for (int i = 0; i < Dh / 2; i++) {
        float c = rc[i], sn = rs[i];
        float q1 = qh[i], q2 = qh[i + Dh / 2];
        qh[i] = q1 * c - q2 * sn; qh[i + Dh / 2] = q2 * c + q1 * sn;
        float k1 = kh[i], k2 = kh[i + Dh / 2];
        kh[i] = k1 * c - k2 * sn; kh[i + Dh / 2] = k2 * c + k1 * sn;
      }
    }
    float *kc = s->kcache + (size_t)l * S * D;
    float *vc = s->vcache + (size_t)l * S * D;
    memcpy(kc + (size_t)pos * D, k, (size_t)D * sizeof(float));
    memcpy(vc + (size_t)pos * D, v, (size_t)D * sizeof(float));

    float scale = 1.0f / sqrtf((float)Dh);
    for (int hh = 0; hh < H; hh++) {
      float *qh = q + hh * Dh;
      float *ao = s->att + hh * Dh;
      memset(ao, 0, (size_t)Dh * sizeof(float));
      float maxs = -1e30f;
      for (int t = 0; t <= pos; t++) {
        const float *kt = kc + (size_t)t * D + hh * Dh;
        float dot = 0;
        for (int i = 0; i < Dh; i++) dot += qh[i] * kt[i];
        dot *= scale;
        s->scores[t] = dot;
        if (dot > maxs) maxs = dot;
      }
      float denom = 0;
      for (int t = 0; t <= pos; t++) {
        float w = expf(s->scores[t] - maxs);
        denom += w;
        const float *vt = vc + (size_t)t * D + hh * Dh;
        for (int i = 0; i < Dh; i++) ao[i] += w * vt[i];
      }
      for (int i = 0; i < Dh; i++) ao[i] /= denom;
    }
    vn_matvec(ly->out_proj, D, D, s->att, s->h);
    for (int i = 0; i < D; i++) s->x[i] += s->h[i];

    // ---- SwiGLU FFN ----
    vn_rmsnorm(s->x, ly->norm2, D, s->h);
    vn_matvec(ly->gate, F, D, s->h, s->g1);
    vn_matvec(ly->up, F, D, s->h, s->g2);
    for (int i = 0; i < F; i++) s->g1[i] = vn_silu(s->g1[i]) * s->g2[i];
    vn_matvec(ly->down, D, F, s->g1, s->h);
    for (int i = 0; i < D; i++) s->x[i] += s->h[i];
  }

  vn_rmsnorm(s->x, m->out_norm, D, s->x);
  // tied head: logits = tok_emb * x
  vn_matvec(m->tok_emb, V, D, s->x, s->logits);
  (void)V;
}

#endif
