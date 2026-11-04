/* Renders every screen to a PNG, with the real themes, no window. */
#include "../src/store.h"
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
static u32 fb[SW * SH];
static Store st;
static Canvas cv;

static void frames(int n) { Input none = { 0 }; for (int i = 0; i < n; i++) store_update(&st, &none); }
static void shot(const char *dir, const char *name)
{
    frames(12);
    store_draw(&st, &cv);
    char p[400];
    snprintf(p, sizeof p, "%s/%s.png", dir, name);
    write_png(p, fb, SW, SH);
    printf("  [png] %s\n", name);
}
static void press(Input in) { store_update(&st, &in); }

int main(int argc, char **argv)
{
    const char *app = argc > 1 ? argv[1] : ".", *themes = argc > 2 ? argv[2] : "out/themes", *out = argc > 3 ? argv[3] : "out/shots";
    ui_font("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf");
    ui_font_bold("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf");
    ui_init(&cv, fb, SW, SH);
    store_init(&st, themes, app, SW, SH);
    st.no_net = 1;                                         /* previews must already be in cache/ */
    shot(out, "1_list");
    Input d = { 0 }; d.down = 1;
    for (int i = 0; i < 5; i++) press(d);
    shot(out, "2_down");
    Input y = { 0 }; y.y = 1; press(y);
    shot(out, "3_screen");
    Input a = { 0 }; a.a = 1; press(a);
    shot(out, "4_confirm");
    st.mode = M_BUSY; snprintf(st.msg, sizeof st.msg, "Installing Replica"); st.job_bytes = 7340032;
    shot(out, "5_busy");
    st.mode = M_LIST;
    Input sel = { 0 }; sel.back = 1; press(sel);
    shot(out, "6_installed");
    st.filter = 2; st.mode = M_LIST;
    st.sel = 0; st.speed = 300000;
    st.e[st.vis[0]].size_kb = 160791;
    st.mode = M_CONFIRM; st.confirm = C_INSTALL; snprintf(st.apply_name, sizeof st.apply_name, "%s", st.e[st.vis[0]].repo);
    snprintf(st.msg, sizeof st.msg, "Download and install %s?\nUp to 157 MB, about 9 min at your speed. Press B to cancel any time.", st.e[st.vis[0]].name);
    shot(out, "7_confirm_size");
    /* choose a theme to apply later: the store must stay open, mark it NEXT, and apply it on exit */
    st.filter = 3; st.mode = M_LIST; st.action = ACT_NONE;
    { Input s3 = { 0 }; s3.back = 1; press(s3); press(s3); press(s3); }       /* filter back to 'all' then on: cycle to installed */
    st.filter = 3;
    for (int i = 0; i < st.n; i++) if (!strcmp(st.e[i].repo, "es-theme-freeplay")) { for (int k = 0; k < st.nvis; k++) if (st.vis[k] == i) st.sel = k; }
    { Input aa = { 0 }; aa.a = 1; press(aa); printf("  confirm mode=%d (2 = asking)\n", st.mode); press(aa); printf("  after yes: mode=%d (3 = message) pending=%s action=%d (0 = still open)\n", st.mode, st.pending, st.action); }
    shot(out, "8_pending_msg");
    st.mode = M_LIST;
    shot(out, "9_pending_list");
    { Input bb = { 0 }; bb.b = 1; press(bb); printf("  on close: action=%d (2 = apply) name=%s\n", st.action, st.apply_name); }
    /* choose a theme to apply later: the store must stay open, mark it NEXT, and apply it on exit */
    st.filter = 3; st.mode = M_LIST; st.action = ACT_NONE;
    { Input s3 = { 0 }; s3.back = 1; press(s3); press(s3); press(s3); }       /* filter back to 'all' then on: cycle to installed */
    st.filter = 3;
    for (int i = 0; i < st.n; i++) if (!strcmp(st.e[i].repo, "es-theme-freeplay")) { for (int k = 0; k < st.nvis; k++) if (st.vis[k] == i) st.sel = k; }
    { Input aa = { 0 }; aa.a = 1; press(aa); printf("  confirm mode=%d (2 = asking)\n", st.mode); press(aa); printf("  after yes: mode=%d (3 = message) pending=%s action=%d (0 = still open)\n", st.mode, st.pending, st.action); }
    shot(out, "8_pending_msg");
    st.mode = M_LIST;
    shot(out, "9_pending_list");
    { Input bb = { 0 }; bb.b = 1; press(bb); printf("  on close: action=%d (2 = apply) name=%s\n", st.action, st.apply_name); }
    /* choose a theme to apply later: the store must stay open, mark it NEXT, and apply it on exit */
    st.filter = 3; st.mode = M_LIST; st.action = ACT_NONE;
    { Input s3 = { 0 }; s3.back = 1; press(s3); press(s3); press(s3); }       /* filter back to 'all' then on: cycle to installed */
    st.filter = 3;
    for (int i = 0; i < st.n; i++) if (!strcmp(st.e[i].repo, "es-theme-freeplay")) { for (int k = 0; k < st.nvis; k++) if (st.vis[k] == i) st.sel = k; }
    { Input aa = { 0 }; aa.a = 1; press(aa); printf("  confirm mode=%d (2 = asking)\n", st.mode); press(aa); printf("  after yes: mode=%d (3 = message) pending=%s action=%d (0 = still open)\n", st.mode, st.pending, st.action); }
    shot(out, "8_pending_msg");
    st.mode = M_LIST;
    shot(out, "9_pending_list");
    { Input bb = { 0 }; bb.b = 1; press(bb); printf("  on close: action=%d (2 = apply) name=%s\n", st.action, st.apply_name); }
    store_free(&st);
    return 0;
}
