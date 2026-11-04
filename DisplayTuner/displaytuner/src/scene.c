#include "scene.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static u32 *B;

static u32 C(int r, int g, int b) { return RGB(r, g, b); }
static void P(int x, int y, u32 c) { if (x >= 0 && y >= 0 && x < SCENE_W && y < SCENE_H) B[y * SCENE_W + x] = c; }
static void R(int x, int y, int w, int h, u32 c) { for (int j = 0; j < h; j++) for (int i = 0; i < w; i++) P(x + i, y + j, c); }
static void Dsc(int cx, int cy, int r, u32 c) { for (int y = -r; y <= r; y++) for (int x = -r; x <= r; x++) if (x * x + y * y <= r * r + r / 2) P(cx + x, cy + y, c); }
static void Ell(int cx, int cy, int rx, int ry, u32 c) { for (int y = -ry; y <= ry; y++) for (int x = -rx; x <= rx; x++) if (x * x * ry * ry + y * y * rx * rx <= rx * rx * ry * ry) P(cx + x, cy + y, c); }
static u32 mixc(u32 a, u32 b, float t) { return col_mix(a, b, t) | 0xff000000u; }

static void cloud(int x, int y, int s)
{
    u32 w = C(252, 252, 255), sh = C(204, 222, 246);
    Dsc(x, y, 6 * s, w); Dsc(x + 8 * s, y - 3 * s, 8 * s, w); Dsc(x + 17 * s, y, 6 * s, w); R(x - 4 * s, y, 26 * s, 5 * s, w);
    R(x - 4 * s, y + 5 * s, 26 * s, 1, sh);
}

static void block_q(int x, int y)
{
    R(x, y, 16, 16, C(196, 126, 14)); R(x + 1, y + 1, 14, 14, C(252, 196, 36)); R(x + 1, y + 1, 14, 2, C(255, 234, 128)); R(x + 1, y + 1, 2, 14, C(255, 222, 96));
    static const char *q[7] = { ".###.", "#...#", "....#", "..##.", "..#..", ".....", "..#.." };
    for (int j = 0; j < 7; j++) for (int i = 0; i < 5; i++) if (q[j][i] == '#') P(x + 5 + i, y + 4 + j, C(140, 72, 8));
}

static void block_b(int x, int y)
{
    R(x, y, 16, 16, C(244, 176, 128)); R(x, y, 16, 1, C(255, 205, 160));
    for (int r = 0; r < 4; r++) { int off = (r & 1) ? 4 : 0; R(x, y + r * 4 + 1, 16, 3, C(206, 78, 44));
        R(x + off, y + r * 4 + 1, 1, 3, C(244, 176, 128)); R(x + off + 8, y + r * 4 + 1, 1, 3, C(244, 176, 128)); }
    R(x, y + 15, 16, 1, C(138, 40, 24));
}

u32 *scene_platformer(void)
{
    B = malloc(SCENE_W * SCENE_H * 4);
    for (int y = 0; y < SCENE_H; y++) for (int x = 0; x < SCENE_W; x++) P(x, y, mixc(C(76, 132, 246), C(182, 228, 255), y < 104 ? y / 104.0f : 1.0f));
    cloud(30, 22, 1); cloud(150, 14, 2); cloud(216, 36, 1);
    Dsc(70, 118, 44, C(132, 206, 142)); Dsc(190, 122, 52, C(120, 198, 134)); Dsc(70, 118, 30, C(112, 192, 128));
    Dsc(116, 124, 34, C(84, 170, 98)); Dsc(246, 126, 34, C(84, 170, 98)); Dsc(20, 126, 30, C(84, 170, 98));
    for (int i = 0; i < 18; i++) P(100 + (i * 7) % 30, 100 + (i * 5) % 14, C(150, 224, 160));
    /* ground */
    R(0, 106, SCENE_W, 8, C(92, 206, 60));
    for (int x = 0; x < SCENE_W; x += 8) R(x, 109, 4, 4, C(66, 172, 46));
    R(0, 106, SCENE_W, 1, C(176, 244, 120));
    R(0, 114, SCENE_W, 18, C(196, 116, 52));
    for (int r = 0; r < 3; r++) for (int x = (r & 1) * 8; x < SCENE_W; x += 16) { R(x, 116 + r * 6, 15, 5, C(176, 98, 40)); R(x, 116 + r * 6, 15, 1, C(228, 152, 88)); }
    /* water */
    R(150, 106, 34, 26, C(36, 120, 228)); R(150, 106, 34, 3, C(120, 196, 255));
    for (int y = 112; y < 132; y += 5) for (int x = 152 + (y & 4); x < 182; x += 8) R(x, y, 4, 1, C(150, 208, 255));
    /* blocks and coins */
    int bx[5] = { 96, 112, 128, 144, 160 };
    for (int i = 0; i < 5; i++) { if (i & 1) block_b(bx[i], 62); else block_q(bx[i], 62); }
    Ell(104, 46, 3, 5, C(252, 214, 60)); Ell(104, 46, 1, 4, C(255, 240, 160));
    Ell(136, 42, 3, 5, C(252, 214, 60)); Ell(136, 42, 1, 4, C(255, 240, 160));
    Ell(168, 46, 3, 5, C(252, 214, 60)); Ell(168, 46, 1, 4, C(255, 240, 160));
    /* pipe */
    R(202, 90, 24, 17, C(28, 164, 72)); R(202, 90, 5, 17, C(110, 224, 140)); R(221, 90, 5, 17, C(14, 108, 48));
    R(199, 82, 30, 9, C(28, 164, 72)); R(199, 82, 6, 9, C(120, 232, 150)); R(223, 82, 6, 9, C(14, 108, 48)); R(199, 82, 30, 1, C(10, 70, 32)); R(199, 90, 30, 1, C(10, 70, 32));
    /* flag (red), gem (purple), flower (pink), pumpkin (orange) */
    R(18, 54, 2, 52, C(226, 226, 236)); P(18, 53, C(252, 214, 60));
    for (int j = 0; j < 10; j++) R(20, 56 + j, 14 - j, 1, C(232, 36, 48));
    Dsc(60, 98, 5, C(170, 70, 220)); Dsc(58, 96, 2, C(226, 170, 255));
    for (int a = 0; a < 5; a++) Dsc(80 + (int)(5 * cosf(a * 1.2566f)), 100 + (int)(5 * sinf(a * 1.2566f)), 3, C(250, 118, 170));
    Dsc(80, 100, 2, C(252, 220, 60)); R(80, 104, 1, 3, C(40, 150, 60));
    Dsc(122, 100, 6, C(252, 140, 24)); Ell(120, 99, 2, 5, C(255, 178, 80)); R(121, 93, 2, 3, C(44, 140, 52));
    /* hero: a small explorer (green hat, red scarf, blue tunic) */
    R(41, 80, 12, 4, C(40, 176, 84)); R(38, 84, 18, 2, C(24, 128, 60)); R(42, 86, 10, 7, C(252, 200, 152)); P(44, 88, C(40, 28, 24)); P(49, 88, C(40, 28, 24)); R(45, 91, 4, 1, C(176, 80, 64));
    R(40, 93, 14, 2, C(232, 40, 52)); R(41, 95, 12, 8, C(32, 120, 220)); R(41, 100, 12, 1, C(250, 220, 60));
    R(38, 95, 3, 6, C(252, 200, 152)); R(53, 95, 3, 6, C(252, 200, 152)); R(42, 103, 4, 4, C(120, 64, 28)); R(48, 103, 4, 4, C(120, 64, 28));
    /* test strips at the bottom: gray ramp and detail steps on the left, colour bars and highlight steps on the right */
    R(0, 132, SCENE_W, 12, C(0, 0, 0));
    for (int i = 0; i < 16; i++) R(i * 8, 132, 8, 6, C(i * 17, i * 17, i * 17));
    for (int i = 0; i < 8; i++) R(i * 16, 138, 14, 6, C(i * 4, i * 4, i * 4));
    static const int bars[8][3] = { { 255, 0, 0 }, { 0, 255, 0 }, { 0, 0, 255 }, { 0, 255, 255 }, { 255, 0, 255 }, { 255, 255, 0 }, { 255, 255, 255 }, { 128, 128, 128 } };
    for (int i = 0; i < 8; i++) R(128 + i * 16, 132, 16, 6, C(bars[i][0], bars[i][1], bars[i][2]));
    R(128, 138, 128, 6, C(255, 255, 255));
    for (int i = 0; i < 8; i++) R(128 + i * 16 + 1, 139, 14, 4, C(224 + i * 4, 224 + i * 4, 224 + i * 4));
    return B;
}

u32 *scene_chart(void)
{
    B = malloc(SCENE_W * SCENE_H * 4);
    R(0, 0, SCENE_W, SCENE_H, C(30, 30, 34));
    static const int skin[6][3] = { { 255, 224, 196 }, { 241, 194, 155 }, { 224, 172, 125 }, { 198, 134, 92 }, { 141, 85, 54 }, { 89, 52, 33 } };
    for (int i = 0; i < 6; i++) R(6 + i * 20, 6, 18, 26, C(skin[i][0], skin[i][1], skin[i][2]));
    for (int i = 0; i < 32; i++) { int v = i * 255 / 31; R(6 + i * 7, 38, 7, 22, C(v, v, v)); }       /* gray ramp, 32 steps */
    for (int x = 0; x < 230; x++) { float t = x / 229.0f; int v = (int)(t * 255); R(6 + x, 64, 1, 7, C(v, 0, 0)); R(6 + x, 71, 1, 7, C(0, v, 0)); R(6 + x, 78, 1, 7, C(0, 0, v)); R(6 + x, 85, 1, 7, C(v, v, v)); }
    static const int col[8][3] = { { 255, 0, 0 }, { 255, 128, 0 }, { 255, 255, 0 }, { 0, 200, 0 }, { 0, 220, 220 }, { 0, 0, 255 }, { 160, 0, 255 }, { 255, 0, 160 } };
    for (int i = 0; i < 8; i++) {
        R(6 + i * 29, 96, 27, 11, C(col[i][0], col[i][1], col[i][2]));
        R(6 + i * 29, 108, 27, 9, C(col[i][0] / 2 + 64, col[i][1] / 2 + 64, col[i][2] / 2 + 64));          /* muted */
        R(6 + i * 29, 118, 27, 8, C(col[i][0] / 4 + 96, col[i][1] / 4 + 96, col[i][2] / 4 + 96));          /* almost gray */
    }
    R(6, 128, 110, 12, C(0, 0, 0)); R(122, 128, 114, 12, C(255, 255, 255));
    for (int i = 0; i < 8; i++) { R(8 + i * 13, 130, 11, 8, C(i * 3, i * 3, i * 3)); R(124 + i * 14, 130, 12, 8, C(255 - (7 - i) * 4, 255 - (7 - i) * 4, 255 - (7 - i) * 4)); }
    return B;
}
