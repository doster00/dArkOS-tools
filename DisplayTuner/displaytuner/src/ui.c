#include "ui.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

static float clampf(float v, float a, float b) { return v < a ? a : v > b ? b : v; }

void ui_init(Canvas *c, u32 *px, int w, int h)
{
    c->px = px; c->w = w; c->h = h;
    float sx = w / (float)DW, sy = h / (float)DH;
    if (sy <= sx) { c->s = sy; c->dw = w / sy; c->ox = 0; c->oy = 0; }          /* 4:3 or wider: the height decides, the layout gets the extra width */
    else { c->s = sx; c->dw = DW; c->ox = 0; c->oy = (h - DH * c->s) / 2; }
}

/* --- colour --------------------------------------------------------------------------------- */

u32 col_mix(u32 a, u32 b, float t)
{
    u32 o = 0;
    for (int i = 0; i < 32; i += 8) o |= (u32)(((a >> i) & 255) * (1 - t) + ((b >> i) & 255) * t + 0.5f) << i;
    return o;
}

u32 col_alpha(u32 c, float a) { return (c & 0x00ffffffu) | ((u32)clampf((c >> 24) * a + 0.5f, 0, 255) << 24); }

void rgb_to_hsv(u32 c, float *h, float *s, float *v)
{
    float r = (c & 255) / 255.0f, g = ((c >> 8) & 255) / 255.0f, b = ((c >> 16) & 255) / 255.0f;
    float mx = fmaxf(r, fmaxf(g, b)), mn = fminf(r, fminf(g, b)), d = mx - mn;
    *v = mx; *s = mx > 0 ? d / mx : 0;
    if (d <= 0) { *h = 0; return; }
    float hh = mx == r ? fmodf((g - b) / d, 6) : mx == g ? (b - r) / d + 2 : (r - g) / d + 4;
    *h = hh * 60; if (*h < 0) *h += 360;
}

u32 hsv_to_rgb(float h, float s, float v)
{
    h = fmodf(h, 360); if (h < 0) h += 360;
    float c = v * s, x = c * (1 - fabsf(fmodf(h / 60, 2) - 1)), m = v - c, r, g, b;
    int k = (int)(h / 60);
    switch (k) { case 0: r = c; g = x; b = 0; break; case 1: r = x; g = c; b = 0; break; case 2: r = 0; g = c; b = x; break;
                 case 3: r = 0; g = x; b = c; break; case 4: r = x; g = 0; b = c; break; default: r = c; g = 0; b = x; }
    return RGB((int)((r + m) * 255 + 0.5f), (int)((g + m) * 255 + 0.5f), (int)((b + m) * 255 + 0.5f));
}

/* --- pixels --------------------------------------------------------------------------------- */

static inline void put(Canvas *c, int x, int y, u32 col, float cov)  /* col over the pixel, with extra coverage 0..1 */
{
    if ((unsigned)x >= (unsigned)c->w || (unsigned)y >= (unsigned)c->h) return;
    unsigned a = (unsigned)((col >> 24) * cov + 0.5f);
    if (a == 0) return;
    u32 *p = &c->px[y * c->w + x];
    if (a >= 255) { *p = col | 0xff000000u; return; }
    u32 d = *p, ia = 255 - a;
    u32 r = ((col & 255) * a + (d & 255) * ia + 127) / 255;
    u32 g = (((col >> 8) & 255) * a + ((d >> 8) & 255) * ia + 127) / 255;
    u32 b = (((col >> 16) & 255) * a + ((d >> 16) & 255) * ia + 127) / 255;
    *p = 0xff000000u | r | (g << 8) | (b << 16);
}

static inline void put_opaque(Canvas *c, int x, int y, u32 col) { c->px[y * c->w + x] = col | 0xff000000u; }

void clear(Canvas *c, u32 col) { for (int i = 0; i < c->w * c->h; i++) c->px[i] = col | 0xff000000u; }

static float sd_rbox(float px, float py, float cx, float cy, float hw, float hh, float r)
{
    float qx = fabsf(px - cx) - (hw - r), qy = fabsf(py - cy) - (hh - r);
    return sqrtf(fmaxf(qx, 0) * fmaxf(qx, 0) + fmaxf(qy, 0) * fmaxf(qy, 0)) + fminf(fmaxf(qx, qy), 0) - r;
}

typedef struct { float cx, cy, hw, hh, r; int x0, y0, x1, y1; } Box;

static Box box_of(Canvas *c, float x, float y, float w, float h, float r, float pad)
{
    Box b;
    float s = c->s, X = c->ox + x * s, Y = c->oy + y * s, Wd = w * s, Ht = h * s;
    b.cx = X + Wd / 2; b.cy = Y + Ht / 2; b.hw = Wd / 2; b.hh = Ht / 2;
    b.r = fminf(r * s, fminf(b.hw, b.hh));
    b.x0 = (int)floorf(X - pad - 1); b.y0 = (int)floorf(Y - pad - 1); b.x1 = (int)ceilf(X + Wd + pad + 1); b.y1 = (int)ceilf(Y + Ht + pad + 1);
    return b;
}

/* the part of row py that is certainly inside (coverage 1): [*lo, *hi), found without any square root */
static void solid_span(const Box *b, int py, int *lo, int *hi)
{
    float dy = fabsf(py + 0.5f - b->cy), inner_h = b->hh - 1.0f;
    *lo = *hi = 0;
    if (dy > inner_h) return;
    float over = dy - (b->hh - b->r);                           /* how far into the rounded corner band this row is */
    float cut = over > 0 ? sqrtf(fmaxf(b->r * b->r - over * over, 0)) - b->r : 0;     /* shrinks the span inside the band */
    float hx = b->hw - b->r + (over > 0 ? b->r + cut : b->r) - 1.5f;
    if (hx <= 0) return;
    *lo = (int)ceilf(b->cx - hx); *hi = (int)floorf(b->cx + hx);
}

void rrect(Canvas *c, float x, float y, float w, float h, float r, u32 col)
{
    Box b = box_of(c, x, y, w, h, r, 0);
    int opaque = (col >> 24) == 255;
    for (int py = b.y0; py < b.y1; py++) {
        if ((unsigned)py >= (unsigned)c->h) continue;
        int lo, hi;
        solid_span(&b, py, &lo, &hi);
        if (hi > lo) { if (lo < 0) lo = 0; if (hi > c->w) hi = c->w; }
        for (int px = b.x0; px < b.x1; px++) {
            if (px == lo && hi > lo) {
                if (opaque) for (; px < hi; px++) put_opaque(c, px, py, col);
                else for (; px < hi; px++) put(c, px, py, col, 1.0f);
                px--; continue;
            }
            put(c, px, py, col, clampf(0.5f - sd_rbox(px + 0.5f, py + 0.5f, b.cx, b.cy, b.hw, b.hh, b.r), 0, 1));
        }
    }
}

void rrect_grad(Canvas *c, float x, float y, float w, float h, float r, u32 top, u32 bottom)
{
    Box b = box_of(c, x, y, w, h, r, 0);
    for (int py = b.y0; py < b.y1; py++) {
        if ((unsigned)py >= (unsigned)c->h) continue;
        float t = clampf((py + 0.5f - (b.cy - b.hh)) / (2 * b.hh), 0, 1);
        u32 col = col_mix(top, bottom, t);
        int opaque = (col >> 24) == 255, lo, hi;
        solid_span(&b, py, &lo, &hi);
        if (hi > lo) { if (lo < 0) lo = 0; if (hi > c->w) hi = c->w; }
        for (int px = b.x0; px < b.x1; px++) {
            if (px == lo && hi > lo) {
                if (opaque) for (; px < hi; px++) put_opaque(c, px, py, col);
                else for (; px < hi; px++) put(c, px, py, col, 1.0f);
                px--; continue;
            }
            put(c, px, py, col, clampf(0.5f - sd_rbox(px + 0.5f, py + 0.5f, b.cx, b.cy, b.hw, b.hh, b.r), 0, 1));
        }
    }
}

void rrect_line(Canvas *c, float x, float y, float w, float h, float r, float t, u32 col)
{
    Box b = box_of(c, x, y, w, h, r, 0);
    float tt = t * c->s;
    for (int py = b.y0; py < b.y1; py++) {
        int lo, hi;
        solid_span(&b, py, &lo, &hi);
        int skip_lo = lo + (int)ceilf(tt) + 1, skip_hi = hi - (int)ceilf(tt) - 1;      /* deep inside: the ring is not here */
        float dy = fabsf(py + 0.5f - b.cy);
        int deep_row = dy < b.hh - b.r - tt - 1;
        for (int px = b.x0; px < b.x1; px++) {
            if (deep_row && hi > lo && px >= skip_lo && px < skip_hi) { px = skip_hi - 1; continue; }
            float d = sd_rbox(px + 0.5f, py + 0.5f, b.cx, b.cy, b.hw, b.hh, b.r);
            put(c, px, py, col, clampf(0.5f - fabsf(d + tt / 2) + tt / 2, 0, 1));
        }
    }
}

void shadow(Canvas *c, float x, float y, float w, float h, float r, float blur, u32 col)
{
    Box b = box_of(c, x, y, w, h, r, blur * c->s);
    float bl = blur * c->s;
    for (int py = b.y0; py < b.y1; py++)
        for (int px = b.x0; px < b.x1; px++) {
            float d = sd_rbox(px + 0.5f, py + 0.5f, b.cx, b.cy, b.hw, b.hh, b.r), t = clampf(0.5f - d / (bl * 2), 0, 1);
            put(c, px, py, col, t * t * (3 - 2 * t));
        }
}

void disc(Canvas *c, float cx, float cy, float r, u32 col)
{
    float s = c->s, X = c->ox + cx * s, Y = c->oy + cy * s, R = r * s;
    for (int py = (int)(Y - R - 1); py <= (int)(Y + R + 1); py++)
        for (int px = (int)(X - R - 1); px <= (int)(X + R + 1); px++)
            put(c, px, py, col, clampf(R + 0.5f - sqrtf((px + 0.5f - X) * (px + 0.5f - X) + (py + 0.5f - Y) * (py + 0.5f - Y)), 0, 1));
}

void ring(Canvas *c, float cx, float cy, float r, float t, u32 col)
{
    float s = c->s, X = c->ox + cx * s, Y = c->oy + cy * s, R = r * s, T = t * s;
    for (int py = (int)(Y - R - T - 1); py <= (int)(Y + R + T + 1); py++)
        for (int px = (int)(X - R - T - 1); px <= (int)(X + R + T + 1); px++) {
            float d = sqrtf((px + 0.5f - X) * (px + 0.5f - X) + (py + 0.5f - Y) * (py + 0.5f - Y));
            put(c, px, py, col, clampf(T / 2 + 0.5f - fabsf(d - R), 0, 1));
        }
}

void hline(Canvas *c, float x, float y, float w, u32 col) { rrect(c, x, y, w, 1, 0, col); }

void image(Canvas *c, const u32 *src, int sw, int sh, float x, float y, float w, float h, float r, float alpha)
{
    Box b = box_of(c, x, y, w, h, r, 0);
    float X = c->ox + x * c->s, Y = c->oy + y * c->s, Wd = w * c->s, Ht = h * c->s;
    for (int py = b.y0; py < b.y1; py++) {
        if ((unsigned)py >= (unsigned)c->h) continue;
        int sy = (int)((py + 0.5f - Y) / Ht * sh), lo, hi;
        sy = sy < 0 ? 0 : sy >= sh ? sh - 1 : sy;
        const u32 *row = src + sy * sw;
        solid_span(&b, py, &lo, &hi);
        if (hi > lo) { if (lo < 0) lo = 0; if (hi > c->w) hi = c->w; }
        float inv = sw / Wd;
        for (int px = b.x0; px < b.x1; px++) {
            if (px == lo && hi > lo && alpha >= 1.0f) {
                u32 *dst = &c->px[py * c->w];
                float fx = (px + 0.5f - X) * inv;
                for (; px < hi; px++, fx += inv) { int sx = (int)fx; sx = sx < 0 ? 0 : sx >= sw ? sw - 1 : sx; dst[px] = row[sx] | 0xff000000u; }
                px--; continue;
            }
            float cov = clampf(0.5f - sd_rbox(px + 0.5f, py + 0.5f, b.cx, b.cy, b.hw, b.hh, b.r), 0, 1);
            if (cov <= 0) continue;
            int sx = (int)((px + 0.5f - X) * inv);
            sx = sx < 0 ? 0 : sx >= sw ? sw - 1 : sx;
            put(c, px, py, row[sx], cov * alpha);
        }
    }
}

/* --- text ----------------------------------------------------------------------------------- */

static stbtt_fontinfo fonts[2];
static unsigned char *fdata[2];
static int fok[2];

static int load_font(int i, const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    free(fdata[i]); fdata[i] = malloc((size_t)n);
    if (fread(fdata[i], 1, (size_t)n, f) != (size_t)n) { fclose(f); return 0; }
    fclose(f);
    fok[i] = stbtt_InitFont(&fonts[i], fdata[i], stbtt_GetFontOffsetForIndex(fdata[i], 0));
    return fok[i];
}

int ui_font(const char *path) { return load_font(0, path); }
int ui_font_bold(const char *path) { return load_font(1, path); }

typedef struct { int key; unsigned char *bmp; int w, h, xo, yo; } Glyph;
static Glyph cache[4096];

static Glyph *glyph(int fi, float px, int cp)
{
    int key = (fi << 28) ^ ((int)(px * 4) << 14) ^ cp ^ 0x40000000;
    Glyph *g = &cache[((unsigned)key * 2654435761u >> 20) & 4095];
    if (g->key == key && g->bmp) return g;
    free(g->bmp);
    float sc = stbtt_ScaleForPixelHeight(&fonts[fi], px);
    g->bmp = stbtt_GetCodepointBitmap(&fonts[fi], sc, sc, cp, &g->w, &g->h, &g->xo, &g->yo);
    g->key = key;
    return g;
}

static int pick(int bold) { return bold && fok[1] ? 1 : fok[0] ? 0 : -1; }

float text_w(float size, int bold, const char *s)
{
    int fi = pick(bold);
    if (fi < 0) return 0;
    float sc = stbtt_ScaleForPixelHeight(&fonts[fi], size), w = 0;
    for (; *s; s++) { int adv, lsb; stbtt_GetCodepointHMetrics(&fonts[fi], (unsigned char)*s, &adv, &lsb); w += adv * sc; }
    return w;
}

void text(Canvas *c, float x, float y, float size, int bold, u32 col, int align, const char *s)
{
    int fi = pick(bold);
    if (fi < 0) return;
    float px = size * c->s, w = text_w(size, bold, s);
    float sc = stbtt_ScaleForPixelHeight(&fonts[fi], px);
    int asc, desc, gap;
    stbtt_GetFontVMetrics(&fonts[fi], &asc, &desc, &gap);
    float X = c->ox + (align == AL_CENTER ? x - w / 2 : align == AL_RIGHT ? x - w : x) * c->s, Y = c->oy + y * c->s + asc * sc;
    for (; *s; s++) {
        Glyph *g = glyph(fi, px, (unsigned char)*s);
        int adv, lsb;
        stbtt_GetCodepointHMetrics(&fonts[fi], (unsigned char)*s, &adv, &lsb);
        for (int gy = 0; gy < g->h; gy++)
            for (int gx = 0; gx < g->w; gx++) put(c, (int)(X + 0.5f) + g->xo + gx, (int)(Y + 0.5f) + g->yo + gy, col, g->bmp[gy * g->w + gx] / 255.0f);
        X += adv * sc;
    }
}
