/* Renders every screen to a PNG, with the real themes, no window. */
#include "../src/studio.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
typedef uint32_t u32x;
static u32 crc_table[256];
static u32 crc(u32 c, const unsigned char *p, size_t n) { for (size_t i = 0; i < n; i++) c = crc_table[(c ^ p[i]) & 255] ^ (c >> 8); return c; }
static void chunk(FILE *f, const char *type, const unsigned char *d, uint32_t n)
{
    unsigned char hdr[8] = { n >> 24, n >> 16, n >> 8, n, type[0], type[1], type[2], type[3] };
    fwrite(hdr, 1, 8, f);
    if (n) fwrite(d, 1, n, f);
    u32 c = crc(crc(0xffffffffu, (const unsigned char *)type, 4), d, n) ^ 0xffffffffu;
    unsigned char t[4] = { c >> 24, c >> 16, c >> 8, c };
    fwrite(t, 1, 4, f);
}
static void write_png(const char *path, const u32 *img, int w, int h)
{
    for (u32 n = 0; n < 256; n++) { u32 c = n; for (int k = 0; k < 8; k++) c = c & 1 ? 0xedb88320u ^ (c >> 1) : c >> 1; crc_table[n] = c; }
    size_t raw = (size_t)(w * 4 + 1) * h;
    unsigned char *r = malloc(raw);
    for (int y = 0; y < h; y++) { r[y * (w * 4 + 1)] = 0; memcpy(r + y * (w * 4 + 1) + 1, img + y * w, (size_t)w * 4); }
    size_t nblk = (raw + 65534) / 65535, zlen = 2 + raw + nblk * 5 + 4;
    unsigned char *z = malloc(zlen), *p = z;
    *p++ = 0x78; *p++ = 0x01;
    uint32_t a = 1, b = 0;
    for (size_t i = 0; i < raw; i++) { a = (a + r[i]) % 65521; b = (b + a) % 65521; }
    for (size_t off = 0; off < raw; off += 65535) {
        size_t n = raw - off < 65535 ? raw - off : 65535;
        *p++ = (off + n >= raw); *p++ = n; *p++ = n >> 8; *p++ = ~n; *p++ = (~n) >> 8;
        memcpy(p, r + off, n); p += n;
    }
    uint32_t ad = (b << 16) | a;
    *p++ = ad >> 24; *p++ = ad >> 16; *p++ = ad >> 8; *p++ = ad;
    FILE *f = fopen(path, "wb");
    if (!f) { fprintf(stderr, "cannot write %s\n", path); exit(1); }
    fwrite("\x89PNG\r\n\x1a\n", 1, 8, f);
    unsigned char ihdr[13] = { w >> 24, w >> 16, w >> 8, w, h >> 24, h >> 16, h >> 8, h, 8, 6, 0, 0, 0 };
    chunk(f, "IHDR", ihdr, 13); chunk(f, "IDAT", z, (uint32_t)(p - z)); chunk(f, "IEND", NULL, 0);
    fclose(f); free(r); free(z);
}

#define SW 1280
#define SH 720
static u32 fb[SW * SH], shot_fb[SW * SH];
static Studio st;
static Canvas cv;

static void emulate_screen(const Adjust *a)               /* what the real display tables would do to the whole picture, the interface included */
{
    unsigned char tab[3][256];
    for (int i = 0; i < 256; i++) { float g[3] = { i / 255.0f, i / 255.0f, i / 255.0f }; Adjust t = *a; t.sat = 100; adj_apply_rgb(&t, g); for (int k = 0; k < 3; k++) tab[k][i] = (unsigned char)(g[k] * 255.0f + 0.5f); }
    float sat = a->sat / 100.0f;
    for (int i = 0; i < SW * SH; i++) {
        u32 p = fb[i];
        float c[3] = { (p & 255) / 255.0f, ((p >> 8) & 255) / 255.0f, ((p >> 16) & 255) / 255.0f };
        float l = 0.2126f * c[0] + 0.7152f * c[1] + 0.0722f * c[2];
        for (int k = 0; k < 3; k++) { c[k] = l + (c[k] - l) * sat; if (c[k] < 0) c[k] = 0; if (c[k] > 1) c[k] = 1; }
        shot_fb[i] = RGB(tab[0][(int)(c[0] * 255.0f + 0.5f)], tab[1][(int)(c[1] * 255.0f + 0.5f)], tab[2][(int)(c[2] * 255.0f + 0.5f)]);
    }
}

static void shot(const char *dir, const char *name)
{
    for (int i = 0; i < 40; i++) { Input none = { 0 }; none.xheld = st.compare; studio_update(&st, &none); }       /* let the knobs settle */
    studio_draw(&st, &cv);
    Adjust a = studio_screen_state(&st);
    emulate_screen(&a);
    char p[400];
    snprintf(p, sizeof p, "%s/%s.png", dir, name);
    memcpy(fb, shot_fb, sizeof fb);
    write_png(p, fb, SW, SH);
    printf("  [png] %s\n", name);
}

static void press(Input in) { studio_update(&st, &in); }

int main(int argc, char **argv)
{
    const char *app = argc > 1 ? argv[1] : ".", *out = argc > 2 ? argv[2] : "out/shots";
    ui_font("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf");
    ui_font_bold("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf");
    ui_init(&cv, fb, SW, SH);
    Adjust neutral;
    adj_default(&neutral);
    studio_init(&st, app, &neutral, HW_GAMMA | HW_CUBIC);
    shot(out, "1_natural");
    Input r = { 0 }; r.r = 1; press(r);                                          /* preset: Vivid */
    shot(out, "2_vivid");
    press(r);                                                                    /* Warm */
    shot(out, "3_warm");
    Input d = { 0 }; d.down = 1; press(d); press(d);
    Input rr = { 0 }; rr.right = 1; for (int i = 0; i < 12; i++) press(rr);      /* contrast up */
    shot(out, "4_custom_contrast");
    Input y = { 0 }; y.y = 1; press(y);
    shot(out, "5_chart");
    Input x = { 0 }; x.xheld = 1; press(x);
    shot(out, "6_compare");
    st.hw = 0;                                                                   /* the preview-only look */
    st.compare = 0;
    Input none = { 0 }; press(none);
    shot(out, "7_preview_only");
    studio_free(&st);
    return 0;
}
