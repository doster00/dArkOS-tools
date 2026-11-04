/* How long does one frame take? usage: bench APP_DIR THEMES_ROOT WIDTH HEIGHT */
#include "../src/store.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec * 1000.0 + t.tv_nsec / 1e6; }

int main(int argc, char **argv)
{
    if (argc < 5) return 2;
    int w = atoi(argv[3]), h = atoi(argv[4]);
    ui_font("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf");
    ui_font_bold("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf");
    u32 *fb = malloc((size_t)w * h * 4);
    static Store st;
    static Canvas cv;
    ui_init(&cv, fb, w, h);
    store_init(&st, argv[2], argv[1], w, h);
    st.no_net = 1;
    Input none = { 0 };
    for (int i = 0; i < 20; i++) store_update(&st, &none);
    double t0 = now();
    for (int i = 0; i < 20; i++) store_draw(&st, &cv);
    printf("%dx%d: draw %.1f ms/frame\n", w, h, (now() - t0) / 20);
    Input down = { 0 }; down.down = 1;
    t0 = now();
    for (int i = 0; i < 10; i++) { store_update(&st, &down); for (int k = 0; k < 8; k++) store_update(&st, &none); }
    printf("update+preview load (10 moves): %.1f ms per move\n", (now() - t0) / 10);
    return 0;
}
