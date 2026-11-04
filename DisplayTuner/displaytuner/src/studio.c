#include "studio.h"
#include "scene.h"
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#include "stb_image.h"
#include <dirent.h>
#include <math.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BG1 RGB(11, 13, 24)
#define BG2 RGB(28, 22, 52)
#define CARD RGB(30, 34, 58)
#define CARD2 RGB(41, 46, 74)
#define TRACK RGB(20, 23, 42)
#define TXT RGB(236, 240, 255)
#define SUB RGB(140, 150, 188)
#define ACC RGB(124, 164, 255)
#define ACC2 RGB(176, 128, 255)
#define OKC RGB(94, 214, 150)
#define WARN RGB(244, 190, 96)

typedef struct { const char *name; const char *hint; float min, max, step, def; int bipolar; } Spec;
static const Spec SPEC[5] = {
    { "Gamma", "midtones lighter or darker", 0.60f, 1.80f, 0.05f, 1.00f, 0 },
    { "Brightness", "the whole picture", -30, 30, 1, 0, 1 },
    { "Contrast", "distance between dark and light", 70, 140, 1, 100, 0 },
    { "Saturation", "colour strength", 0, 200, 5, 100, 0 },
    { "Warmth", "cooler to warmer", -50, 50, 1, 0, 1 },
};

typedef struct { const char *name; Adjust a; } Preset;
static const Preset PRESETS[] = {
    { "Natural", { 1.00f, 0, 100, 100, 0 } },
    { "Vivid", { 1.08f, 0, 112, 130, 0 } },
    { "Warm", { 1.00f, 0, 100, 105, 25 } },
    { "Cool", { 1.00f, 0, 100, 100, -22 } },
    { "Soft", { 0.95f, 2, 90, 90, 6 } },
    { "Cinema", { 0.95f, -2, 112, 90, 8 } },
    { "Bright room", { 1.25f, 6, 100, 110, 0 } },
};
#define NPRESET ((int)(sizeof PRESETS / sizeof PRESETS[0]))

static float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }

static float get(const Adjust *a, int i) { return i == 0 ? a->gamma : i == 1 ? (float)a->bright : i == 2 ? (float)a->contrast : i == 3 ? (float)a->sat : (float)a->warm; }
static void put(Adjust *a, int i, float v)
{
    v = clampf(v, SPEC[i].min, SPEC[i].max);
    if (i == 0) a->gamma = roundf(v / 0.05f) * 0.05f; else if (i == 1) a->bright = (int)lroundf(v); else if (i == 2) a->contrast = (int)lroundf(v);
    else if (i == 3) a->sat = (int)lroundf(v); else a->warm = (int)lroundf(v);
}
static void fmt(char *out, size_t n, const Adjust *a, int i)
{
    if (i == 0) snprintf(out, n, "%.2f", a->gamma); else if (i == 1) snprintf(out, n, "%+d", a->bright); else if (i == 2) snprintf(out, n, "%d%%", a->contrast);
    else if (i == 3) snprintf(out, n, "%d%%", a->sat); else snprintf(out, n, "%+d", a->warm);
}
static float frac(int i, float v) { return (v - SPEC[i].min) / (SPEC[i].max - SPEC[i].min); }

/* --- pictures ---------------------------------------------------------------------------------- */

static void free_user(Studio *s) { free(s->user); s->user = NULL; }

static int load_user(Studio *s, const char *path)         /* decode, then shrink once with a box filter so drawing stays cheap */
{
    int w, h, n;
    unsigned char *d = stbi_load(path, &w, &h, &n, 4);
    if (!d || w < 2 || h < 2) { if (d) stbi_image_free(d); return 0; }
    float k = fminf(1.0f, fminf(480.0f / w, 270.0f / h));
    int tw = (int)(w * k), th = (int)(h * k);
    if (tw < 1) tw = 1;
    if (th < 1) th = 1;
    u32 *o = malloc((size_t)tw * th * 4);
    for (int y = 0; y < th; y++) {
        int y0 = (int)((long)y * h / th), y1 = (int)((long)(y + 1) * h / th); if (y1 <= y0) y1 = y0 + 1;
        for (int x = 0; x < tw; x++) {
            int x0 = (int)((long)x * w / tw), x1 = (int)((long)(x + 1) * w / tw); if (x1 <= x0) x1 = x0 + 1;
            unsigned r = 0, g = 0, b = 0, cnt = 0;
            for (int yy = y0; yy < y1 && yy < h; yy++) for (int xx = x0; xx < x1 && xx < w; xx++) { const unsigned char *q = d + ((size_t)yy * w + xx) * 4; r += q[0]; g += q[1]; b += q[2]; cnt++; }
            if (!cnt) cnt = 1;
            o[y * tw + x] = RGB(r / cnt, g / cnt, b / cnt);
        }
    }
    stbi_image_free(d);
    free_user(s);
    s->user = o; s->iw = tw; s->ih = th;
    return 1;
}

static void rebuild_shown(Studio *s)                      /* no hardware LUT: adjust a copy of the picture instead */
{
    if (s->hw) return;
    size_t n = (size_t)s->iw * s->ih;
    if (!s->shown || (size_t)s->shown_w * s->shown_h != n) { free(s->shown); s->shown = malloc(n * 4); }
    s->shown_w = s->iw; s->shown_h = s->ih;
    Adjust a = s->compare ? (Adjust){ 1.0f, 0, 100, 100, 0 } : s->cur;
    unsigned char tab[3][256];
    for (int i = 0; i < 256; i++) { float g[3] = { i / 255.0f, i / 255.0f, i / 255.0f }; adj_apply_rgb(&a, g); for (int k = 0; k < 3; k++) tab[k][i] = (unsigned char)(g[k] * 255.0f + 0.5f); }
    float sat = a.sat / 100.0f;
    for (size_t i = 0; i < n; i++) {
        u32 p = s->img[i];
        float c[3] = { (p & 255) / 255.0f, ((p >> 8) & 255) / 255.0f, ((p >> 16) & 255) / 255.0f };
        float l = 0.2126f * c[0] + 0.7152f * c[1] + 0.0722f * c[2];
        for (int k = 0; k < 3; k++) c[k] = clampf(l + (c[k] - l) * sat, 0, 1);
        s->shown[i] = RGB(tab[0][(int)(c[0] * 255.0f + 0.5f)], tab[1][(int)(c[1] * 255.0f + 0.5f)], tab[2][(int)(c[2] * 255.0f + 0.5f)]);
    }
}

static void set_ref(Studio *s, int i)
{
    s->ref = i;
    if (s->refs[i].builtin) { s->img = s->scene[s->refs[i].path[0] - '0']; s->iw = SCENE_W; s->ih = SCENE_H; free_user(s); }
    else if (load_user(s, s->refs[i].path)) { s->img = s->user; }
    else { s->img = s->scene[0]; s->iw = SCENE_W; s->ih = SCENE_H; snprintf(s->toast, sizeof s->toast, "Could not open that picture"); s->toast_t = 90; }
    rebuild_shown(s);
    s->redraw = 1;
}

static int cmp_ref(const void *a, const void *b) { return strcmp(((const Ref *)a)->name, ((const Ref *)b)->name); }

void studio_init(Studio *s, const char *app_dir, const Adjust *saved, int hw_mask)
{
    memset(s, 0, sizeof *s);
    snprintf(s->dir, sizeof s->dir, "%s", app_dir);
    s->saved = *saved; s->cur = *saved; s->hw = hw_mask; s->preset = -1; s->dirty_hw = 1; s->redraw = 1;
    for (int i = 0; i < NPRESET; i++) if (!memcmp(&s->cur, &PRESETS[i].a, sizeof(Adjust)) || adj_is_neutral(&s->cur)) { s->preset = adj_is_neutral(&s->cur) ? 0 : i; break; }
    s->scene[0] = scene_platformer(); s->scene[1] = scene_chart();
    snprintf(s->refs[0].name, 64, "Platformer scene"); snprintf(s->refs[0].path, 300, "0"); s->refs[0].builtin = 1;
    snprintf(s->refs[1].name, 64, "Test chart"); snprintf(s->refs[1].path, 300, "1"); s->refs[1].builtin = 1;
    s->nref = 2;
    char dir[300];
    snprintf(dir, sizeof dir, "%s/images", app_dir);
    DIR *d = opendir(dir);
    for (struct dirent *e; d && (e = readdir(d)) && s->nref < MAX_REF; ) {
        const char *x = strrchr(e->d_name, '.');
        if (!x || (strcasecmp(x, ".png") && strcasecmp(x, ".jpg") && strcasecmp(x, ".jpeg"))) continue;
        Ref *r = &s->refs[s->nref++];
        snprintf(r->name, sizeof r->name, "%.*s", (int)(x - e->d_name) < 63 ? (int)(x - e->d_name) : 63, e->d_name);
        snprintf(r->path, sizeof r->path, "%s/%s", dir, e->d_name);
    }
    if (d) closedir(d);
    if (s->nref > 2) qsort(s->refs + 2, s->nref - 2, sizeof(Ref), cmp_ref);
    for (int i = 0; i < 5; i++) s->knob[i] = frac(i, get(&s->cur, i));
    set_ref(s, 0);
}

void studio_free(Studio *s) { free(s->scene[0]); free(s->scene[1]); free(s->user); free(s->shown); }

void studio_toast(Studio *s, const char *msg) { snprintf(s->toast, sizeof s->toast, "%s", msg); s->toast_t = 100; s->redraw = 1; }

Adjust studio_screen_state(const Studio *s) { Adjust n; adj_default(&n); return s->compare ? n : s->cur; }

/* --- one frame of logic ------------------------------------------------------------------------ */

void studio_update(Studio *s, const Input *in)
{
    s->frame++;
    int changed = 0;
    if (in->up) { s->sel = (s->sel + 4) % 5; s->redraw = 1; }
    if (in->down) { s->sel = (s->sel + 1) % 5; s->redraw = 1; }
    int d = in->right - in->left;
    if (d) { const Spec *p = &SPEC[s->sel]; put(&s->cur, s->sel, get(&s->cur, s->sel) + d * p->step); changed = 1; }
    if (in->a) { put(&s->cur, s->sel, SPEC[s->sel].def); changed = 1; }
    if (in->back) { adj_default(&s->cur); changed = 1; }
    if (in->l || in->r) {
        int cur = s->preset < 0 ? (in->r ? -1 : 0) : s->preset;
        cur = (cur + (in->r ? 1 : -1) + NPRESET) % NPRESET;
        s->cur = PRESETS[cur].a; s->preset = cur; changed = 1; s->dirty_hw = 1;
        snprintf(s->toast, sizeof s->toast, "Preset: %s", PRESETS[cur].name); s->toast_t = 70;
    } else if (changed) s->preset = -1;
    if (in->y) { set_ref(s, (s->ref + 1) % s->nref); }
    if ((in->xheld != 0) != s->compare) { s->compare = in->xheld != 0; s->dirty_hw = 1; s->redraw = 1; rebuild_shown(s); }
    if (changed) { s->dirty_hw = 1; s->redraw = 1; rebuild_shown(s); }
    if (in->start) s->action = ST_SAVE;
    if (in->b) s->action = ST_QUIT;
    for (int i = 0; i < 5; i++) {                       /* the knobs glide */
        float t = frac(i, get(&s->cur, i)), dd = t - s->knob[i];
        if (fabsf(dd) > 0.002f) { s->knob[i] += dd * 0.4f; s->redraw = 1; } else s->knob[i] = t;
    }
    if (s->toast_t > 0 && --s->toast_t == 0) s->redraw = 1;
}

/* --- drawing ----------------------------------------------------------------------------------- */

typedef struct { const char *key, *label; } Hint;
static float hint_gw(const char *key) { return key[1] ? text_w(9, 1, key) + 16 : 20; }
static float hint_w(const Hint *h) { return hint_gw(h->key) + 6 + text_w(12, 0, h->label); }
static u32 key_col(const char *k) { return k[1] ? SUB : k[0] == 'A' ? OKC : k[0] == 'B' ? RGB(240, 106, 120) : k[0] == 'X' ? ACC : WARN; }

static void hint_draw(Canvas *c, float x, float y, const Hint *h)
{
    float gw = hint_gw(h->key);
    if (!h->key[1]) {
        u32 col = key_col(h->key);
        shadow(c, x + 1, y + 2, 18, 18, 9, 3, RGBA(0, 0, 0, 120));
        disc(c, x + 10, y + 10, 10, col_mix(col, RGB(0, 0, 0), 0.25f)); disc(c, x + 10, y + 9.2f, 9.2f, col);
        ring(c, x + 10, y + 9.2f, 9.0f, 1.0f, RGBA(255, 255, 255, 70));
        text(c, x + 10, y + 3.2f, 12, 1, RGB(14, 16, 32), AL_CENTER, h->key);
    } else {
        shadow(c, x, y + 3, gw, 16, 8, 3, RGBA(0, 0, 0, 120));
        rrect_grad(c, x, y + 1, gw, 18, 9, CARD2, RGB(26, 30, 52)); rrect_line(c, x, y + 1, gw, 18, 9, 1.1f, col_alpha(SUB, 0.8f));
        text(c, x + gw / 2, y + 4.2f, 9, 1, TXT, AL_CENTER, h->key);
    }
    text(c, x + gw + 6, y + 3.2f, 12, 0, TXT, AL_LEFT, h->label);
}

static void hints_row(Canvas *c, float cx, float y, const Hint *h, int n)
{
    float total = 0;
    for (int i = 0; i < n; i++) total += hint_w(&h[i]) + (i ? 16 : 0);
    float x = cx - total / 2;
    for (int i = 0; i < n; i++) { hint_draw(c, x, y, &h[i]); x += hint_w(&h[i]) + 16; }
}

static void pill(Canvas *c, float x, float y, const char *t, u32 fill, u32 ink, int size)
{
    float w = text_w((float)size, 1, t) + 18;
    rrect(c, x, y, w, size + 10, (size + 10) / 2.0f, fill);
    text(c, x + w / 2, y + 4.2f, (float)size, 1, ink, AL_CENTER, t);
}

static u32 *layer;
static int layer_w, layer_h;

static void draw_static(Canvas *c)
{
    rrect_grad(c, -c->ox / c->s, -c->oy / c->s, c->w / c->s, c->h / c->s, 0, BG2, BG1);
    text(c, 24, 12, 22, 1, TXT, AL_LEFT, "Display Tuner");
    float rw = c->dw > 700 ? 292 : 262, cw = c->dw - 48 - rw - 14;
    rrect(c, 24, 58, cw, 360, 16, CARD);
    rrect(c, 24 + cw + 14, 58, rw, 360, 16, CARD);
    rrect(c, 24, 430, c->dw - 48, 34, 17, CARD);
}

void studio_draw(const Studio *s, Canvas *c)
{
    if (!layer || layer_w != c->w || layer_h != c->h) {
        free(layer); layer = malloc((size_t)c->w * c->h * 4); layer_w = c->w; layer_h = c->h;
        draw_static(c); memcpy(layer, c->px, (size_t)c->w * c->h * 4);
    } else memcpy(c->px, layer, (size_t)c->w * c->h * 4);

    float W = c->dw, RW = W > 700 ? 292 : 262, CW = W - 48 - RW - 14, RX = 24 + CW + 14;
    char buf[96];

    /* header: the preset in use */
    const char *pn = s->preset >= 0 ? PRESETS[s->preset].name : "Custom";
    float pw = text_w(12, 1, pn) + 22;
    text(c, W - 24 - pw - 10, 22, 12, 0, SUB, AL_RIGHT, "Preset");
    rrect_grad(c, W - 24 - pw, 17, pw, 24, 12, s->preset >= 0 ? ACC : CARD2, s->preset >= 0 ? ACC2 : RGB(52, 58, 92));
    text(c, W - 24 - pw / 2, 21.5f, 12, 1, s->preset >= 0 ? RGB(12, 14, 30) : TXT, AL_CENTER, pn);

    /* the reference picture */
    float bx = 36, by = 70, bw = CW - 24, bh = 262;
    rrect(c, bx - 4, by - 4, bw + 8, bh + 8, 14, RGB(8, 10, 20));
    const u32 *img = s->hw ? s->img : (s->shown ? s->shown : s->img);
    if (img) {
        float k = fminf(bw / s->iw, bh / s->ih), w = s->iw * k, h = s->ih * k;
        shadow(c, bx + (bw - w) / 2, by + (bh - h) / 2, w, h, 10, 10, RGBA(0, 0, 0, 150));
        image(c, img, s->iw, s->ih, bx + (bw - w) / 2, by + (bh - h) / 2, w, h, 10, 1.0f);
    }
    if (s->compare) pill(c, bx + 12, by + 12, "ORIGINAL", WARN, RGB(30, 20, 4), 11);
    snprintf(buf, sizeof buf, "Picture: %s", s->refs[s->ref].name);
    text(c, bx, 340, 12, 1, TXT, AL_LEFT, buf);
    text(c, bx, 360, 11, 0, SUB, AL_LEFT, "Shadows: you should still tell the darkest gray steps apart.");
    text(c, bx, 376, 11, 0, SUB, AL_LEFT, "Highlights: the brightest steps should not melt into white.");
    text(c, bx, 392, 11, 0, SUB, AL_LEFT, "Skin and grass should look natural, not orange or neon.");
    if (!s->hw) pill(c, bx + bw - text_w(11, 1, "PREVIEW ONLY") - 18, 340, "PREVIEW ONLY", WARN, RGB(30, 20, 4), 11);

    /* the sliders */
    text(c, RX + 18, 70, 11, 1, SUB, AL_LEFT, "ADJUST");
    for (int i = 0; i < 5; i++) {
        float ry = 92 + i * 64, x0 = RX + 18, w = RW - 36;
        int on = i == s->sel;
        int off = (i == 3 && s->hw && !(s->hw & HW_CUBIC));
        if (on) {
            rrect(c, RX + 8, ry - 6, RW - 16, 58, 12, col_alpha(ACC, 0.13f));
            rrect_line(c, RX + 8, ry - 6, RW - 16, 58, 12, 1.2f, col_alpha(ACC, 0.6f));
        }
        text(c, x0, ry - 1, 14, on, off ? SUB : TXT, AL_LEFT, SPEC[i].name);
        fmt(buf, sizeof buf, &s->cur, i);
        text(c, x0 + w, ry - 1, 14, 1, on ? ACC : TXT, AL_RIGHT, off ? "n/a" : buf);
        float ty = ry + 24;
        rrect(c, x0, ty, w, 8, 4, TRACK);
        float kn = clampf(s->knob[i], 0, 1), kx = x0 + kn * w;
        float from = SPEC[i].bipolar ? x0 + w / 2 : x0, to = kx;
        float fl = from < to ? from : to, fw = fabsf(to - from);
        if (fw > 1) rrect_grad(c, fl, ty, fw < 8 ? 8 : fw, 8, 4, ACC, ACC2);
        float nx = x0 + frac(i, SPEC[i].def) * w;
        rrect(c, nx - 1, ty - 3, 2, 14, 1, col_alpha(TXT, 0.4f));
        if (on) disc(c, kx, ty + 4, 13, col_alpha(ACC, 0.22f));
        shadow(c, kx - 8, ty - 2, 16, 16, 8, 4, RGBA(0, 0, 0, 150));
        disc(c, kx, ty + 4, 9, RGB(236, 240, 255)); disc(c, kx, ty + 4, 5.2f, on ? ACC : SUB);
        if (on) text(c, x0, ry + 38, 10, 0, SUB, AL_LEFT, SPEC[i].hint);
    }

    /* buttons */
    Hint hs[8]; int nh = 0;
    if (W > 700) hs[nh++] = (Hint){ "D-PAD", "Adjust" };
    hs[nh++] = (Hint){ "A", "Reset" };
    hs[nh++] = (Hint){ "X", "Hold to compare" };
    hs[nh++] = (Hint){ "Y", "Picture" };
    hs[nh++] = (Hint){ "L/R", "Preset" };
    hs[nh++] = (Hint){ "START", "Save" };
    hs[nh++] = (Hint){ "B", "Cancel" };
    hints_row(c, W / 2, 437, hs, nh);

    if (s->toast_t > 0) {
        float tw = text_w(13, 1, s->toast) + 30;
        shadow(c, W / 2 - tw / 2, 16, tw, 28, 14, 10, RGBA(0, 0, 0, 160));
        rrect_grad(c, W / 2 - tw / 2, 16, tw, 28, 14, CARD2, CARD);
        rrect_line(c, W / 2 - tw / 2, 16, tw, 28, 14, 1.0f, col_alpha(ACC, 0.7f));
        text(c, W / 2, 22, 13, 1, TXT, AL_CENTER, s->toast);
    }
}
