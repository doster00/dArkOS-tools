/* Display Tuner: the screen. studio_update takes one frame of input, studio_draw paints a frame. No SDL in here. */
#ifndef STUDIO_H
#define STUDIO_H
#include "ui.h"
#include "lut.h"

typedef struct { int up, down, left, right, a, b, x, y, l, r, start, back, xheld; } Input;     /* 1 on the frame a button goes down (or repeats); xheld while X is down */
enum { ST_RUN, ST_SAVE, ST_QUIT };
#define MAX_REF 16

typedef struct { char name[64], path[300]; int builtin; } Ref;

typedef struct {
    Adjust cur, saved;
    int sel, preset, action, frame, redraw;
    int hw;                                             /* HW_* mask; 0 = the screen cannot be adjusted here (preview only) */
    int dirty_hw, compare;
    Ref refs[MAX_REF]; int nref, ref;
    u32 *scene[2], *user, *shown; int iw, ih, shown_w, shown_h;
    const u32 *img;                                     /* the picture being shown (a built-in or the decoded user image) */
    float knob[5];
    char toast[96]; int toast_t;
    char dir[260];
} Studio;

void studio_init(Studio *s, const char *app_dir, const Adjust *saved, int hw_mask);
void studio_update(Studio *s, const Input *in);
void studio_draw(const Studio *s, Canvas *c);
void studio_free(Studio *s);
Adjust studio_screen_state(const Studio *s);            /* what the real screen should show right now (neutral while comparing) */
void studio_toast(Studio *s, const char *msg);
#endif
