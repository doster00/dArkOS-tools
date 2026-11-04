/* Display Tuner: the display adjustment engine. Gamma, brightness, contrast and warmth go in the CRTC's 1D GAMMA_LUT;
 * saturation goes in the 3D CUBIC_LUT, composed with the panel's own white-point LUT when the system has one.
 * The DRM calls go through dlopen, so nothing here needs libdrm headers. */
#ifndef LUT_H
#define LUT_H

typedef struct { float gamma; int bright, contrast, sat, warm; } Adjust;     /* 1.0 / 0 / 100 / 100 / 0 = neutral */

void adj_default(Adjust *a);
int adj_is_neutral(const Adjust *a);
void adj_apply_rgb(const Adjust *a, float rgb[3]);          /* the same transform in software, for previews (0..1) */
int adj_load(Adjust *a, const char *path);                   /* 0 on success; a missing file leaves the defaults */
int adj_save(const Adjust *a, const char *path);

enum { HW_GAMMA = 1, HW_CUBIC = 2 };
int hw_init(void);                                           /* opens the display, returns a mask of HW_* (0: cannot adjust) */
int hw_apply(const Adjust *a, int touch_cubic);              /* 0 on success */
void hw_close(void);
#endif
