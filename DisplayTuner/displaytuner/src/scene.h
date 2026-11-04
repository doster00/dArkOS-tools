/* Display Tuner: reference pictures drawn in code. Original artwork, nothing taken from any game. */
#ifndef SCENE_GUARD_H
#define SCENE_GUARD_H
#include "ui.h"
#define SCENE_W 256
#define SCENE_H 144
u32 *scene_platformer(void);     /* a colourful 16-bit style level: sky, hills, blocks, pipe, water, a hero, plus test strips */
u32 *scene_chart(void);          /* a test chart: skin tones, gray ramp, colour ramps, saturated and muted colours, shadow/highlight detail */
#endif
