/* Theme Lab: a small software renderer for a pretty interface (anti-aliased shapes, shadows, gradients, real fonts).
 * Everything is drawn in a 640x480 design space and scaled to the real screen, so it stays sharp at any size. */
#ifndef UI_H
#define UI_H
#include <stdint.h>

typedef uint32_t u32;                         /* 0xAABBGGRR, little endian RGBA bytes */
#define RGBA(r, g, b, a) ((u32)(r) | ((u32)(g) << 8) | ((u32)(b) << 16) | ((u32)(a) << 24))
#define RGB(r, g, b) RGBA(r, g, b, 255)
#define DW 640                                /* design width and height */
#define DH 480

typedef struct { u32 *px; int w, h; float s, ox, oy, dw; } Canvas;      /* s: pixels per design unit; ox, oy: letterbox offset; dw: design width (wider than DW on wide screens) */

void ui_init(Canvas *c, u32 *px, int w, int h);
int ui_font(const char *path);                /* load the UI font (a .ttf); 0 on failure */
int ui_font_bold(const char *path);

u32 col_mix(u32 a, u32 b, float t);
u32 col_alpha(u32 c, float a);                /* same colour, alpha scaled */
void rgb_to_hsv(u32 c, float *h, float *s, float *v);       /* h 0..360 */
u32 hsv_to_rgb(float h, float s, float v);

void clear(Canvas *c, u32 col);
void rrect(Canvas *c, float x, float y, float w, float h, float r, u32 col);
void rrect_grad(Canvas *c, float x, float y, float w, float h, float r, u32 top, u32 bottom);
void rrect_line(Canvas *c, float x, float y, float w, float h, float r, float t, u32 col);
void shadow(Canvas *c, float x, float y, float w, float h, float r, float blur, u32 col);
void disc(Canvas *c, float cx, float cy, float r, u32 col);
void ring(Canvas *c, float cx, float cy, float r, float t, u32 col);
void hline(Canvas *c, float x, float y, float w, u32 col);
void image(Canvas *c, const u32 *src, int sw, int sh, float x, float y, float w, float h, float r, float alpha);   /* scaled, with rounded corners */

enum { AL_LEFT, AL_CENTER, AL_RIGHT };
float text_w(float size, int bold, const char *s);
void text(Canvas *c, float x, float y, float size, int bold, u32 col, int align, const char *s);   /* y is the top of the line */
#endif
