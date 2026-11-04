/* Display Tuner: the SDL2 front end for the handheld, and the boot-time helper.
 *   displaytuner              the app (run with ES stopped: the launcher does that, or in a window on a PC)
 *   displaytuner --apply      set the saved adjustment on the screen once (what the boot service does), no window
 *   displaytuner --keep       the same, and again every few seconds: emulators reset the display tables when they start
 *   DISPLAYTUNER_DIR=.   where images/ is        DISPLAYTUNER_CFG=file   the settings file (default ~/.config/displaytuner.cfg)
 *   DISPLAYTUNER_WINDOWED=1  a window            DISPLAYTUNER_SHOT=file.bmp   draw one frame to a file and quit
 *   DISPLAYTUNER_NOHW=1      pretend the screen cannot be adjusted (preview only)                                           */
#include "studio.h"
#include <SDL2/SDL.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define LIVE "/tmp/displaytuner.live"

static const char *FONTS[] = { "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", "/usr/share/fonts/dejavu/DejaVuSans.ttf", NULL };
static const char *BOLDS[] = { "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", "/usr/share/fonts/dejavu/DejaVuSans-Bold.ttf", NULL };

static void cfg_path(char *out, size_t n)
{
    const char *e = getenv("DISPLAYTUNER_CFG"), *h = getenv("HOME");
    if (e) snprintf(out, n, "%s", e);
    else snprintf(out, n, "%s/.config/displaytuner.cfg", h ? h : "/home/ark");
}

static int live_app_running(void)                        /* the app is open: it owns the screen's tables, the keeper stays out */
{
    FILE *f = fopen(LIVE, "r");
    int pid = 0;
    if (f) { if (fscanf(f, "%d", &pid) != 1) pid = 0; fclose(f); }
    return pid > 0 && kill(pid, 0) == 0;
}

static int run_helper(int keep)
{
    char cfg[300];
    cfg_path(cfg, sizeof cfg);
    int retries = getenv("APPLY_RETRIES") ? atoi(getenv("APPLY_RETRIES")) : 1;
    static const int plan[] = { 2, 2, 2, 3, 3, 5, 5, 10, 10, 10 };
    int step = 0, was_neutral = 1, ok_once = 0;
    for (;;) {
        Adjust a;
        adj_load(&a, cfg);
        if (!live_app_running() && (!adj_is_neutral(&a) || !was_neutral)) {          /* a neutral file touches nothing, unless it just became neutral */
            int caps = 0;
            for (int i = 0; i < (retries > 0 ? retries : 1) && !(caps = hw_init()); i++) usleep(500000);
            if (caps && hw_apply(&a, a.sat != 100 || !was_neutral) == 0) { ok_once = 1; was_neutral = adj_is_neutral(&a); }
        }
        if (!keep) return ok_once || adj_is_neutral(&a) ? 0 : 2;
        sleep(step < (int)(sizeof plan / sizeof plan[0]) ? plan[step++] : 15);
    }
}

int main(int argc, char **argv)
{
    if (argc > 1 && !strcmp(argv[1], "--apply")) return run_helper(0);
    if (argc > 1 && !strcmp(argv[1], "--keep")) return run_helper(1);
    if (argc > 1 && !strcmp(argv[1], "--reset")) {         /* neutral gamma and an identity (or panel-only) 3D table */
        Adjust n;
        adj_default(&n);
        return hw_init() ? hw_apply(&n, 1) : 2;
    }

    const char *dir = getenv("DISPLAYTUNER_DIR") ? getenv("DISPLAYTUNER_DIR") : ".";
    int windowed = getenv("DISPLAYTUNER_WINDOWED") != NULL;
    const char *shot = getenv("DISPLAYTUNER_SHOT");
    char cfg[300];
    cfg_path(cfg, sizeof cfg);
    int ok = 0;
    for (int i = 0; FONTS[i] && !ok; i++) ok = ui_font(FONTS[i]);
    if (!ok) { fprintf(stderr, "displaytuner: no font found\n"); return 1; }
    for (int i = 0; BOLDS[i]; i++) if (ui_font_bold(BOLDS[i])) break;
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK) < 0) { fprintf(stderr, "displaytuner: %s\n", SDL_GetError()); return 1; }
    SDL_Window *win = SDL_CreateWindow("Display Tuner", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, windowed ? 960 : 0, windowed ? 720 : 0,
                                       windowed ? SDL_WINDOW_RESIZABLE : SDL_WINDOW_FULLSCREEN_DESKTOP);
    if (!win) { fprintf(stderr, "displaytuner: %s\n", SDL_GetError()); return 1; }
    SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ren) ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
    int w, h;
    SDL_GetRendererOutputSize(ren, &w, &h);
    SDL_Texture *tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, w, h);
    u32 *fb = malloc((size_t)w * h * 4);
    Canvas cv;
    ui_init(&cv, fb, w, h);
    SDL_ShowCursor(SDL_DISABLE);
    for (int i = 0; i < SDL_NumJoysticks(); i++) if (SDL_IsGameController(i)) SDL_GameControllerOpen(i); else SDL_JoystickOpen(i);
    SDL_SetHint(SDL_HINT_GAMECONTROLLER_USE_BUTTON_LABELS, "0");

    Adjust saved;
    adj_load(&saved, cfg);
    int hw = getenv("DISPLAYTUNER_NOHW") ? 0 : hw_init();
    static Studio st;
    studio_init(&st, dir, &saved, hw);
    FILE *lf = fopen(LIVE, "w");
    if (lf) { fprintf(lf, "%d\n", (int)getpid()); fclose(lf); }

    int hold[4] = { 0 }, xheld = 0, cubic_touched = 0, quit = 0;
    Uint32 last_apply = 0;
    for (int frame = 0; !quit; frame++) {
        Input in = { 0 };
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) quit = 1;
            if (e.type == SDL_CONTROLLERBUTTONDOWN) {
                switch (e.cbutton.button) {
                case SDL_CONTROLLER_BUTTON_A: in.a = 1; break;
                case SDL_CONTROLLER_BUTTON_B: in.b = 1; break;
                case SDL_CONTROLLER_BUTTON_X: xheld = 1; break;
                case SDL_CONTROLLER_BUTTON_Y: in.y = 1; break;
                case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: in.l = 1; break;
                case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: in.r = 1; break;
                case SDL_CONTROLLER_BUTTON_START: in.start = 1; break;
                case SDL_CONTROLLER_BUTTON_BACK: in.back = 1; break;
                case SDL_CONTROLLER_BUTTON_DPAD_UP: in.up = 1; break;
                case SDL_CONTROLLER_BUTTON_DPAD_DOWN: in.down = 1; break;
                case SDL_CONTROLLER_BUTTON_DPAD_LEFT: in.left = 1; hold[2] = 1; break;
                case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: in.right = 1; hold[3] = 1; break;
                }
            } else if (e.type == SDL_CONTROLLERBUTTONUP) {
                switch (e.cbutton.button) {
                case SDL_CONTROLLER_BUTTON_X: xheld = 0; break;
                case SDL_CONTROLLER_BUTTON_DPAD_LEFT: hold[2] = 0; break;
                case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: hold[3] = 0; break;
                }
            } else if (e.type == SDL_KEYDOWN && !e.key.repeat) {
                switch (e.key.keysym.sym) {
                case SDLK_RETURN: case SDLK_z: in.a = 1; break;
                case SDLK_ESCAPE: in.b = 1; break;
                case SDLK_a: xheld = 1; break;
                case SDLK_s: in.y = 1; break;
                case SDLK_q: in.l = 1; break;
                case SDLK_w: in.r = 1; break;
                case SDLK_TAB: in.back = 1; break;
                case SDLK_F5: in.start = 1; break;
                case SDLK_UP: in.up = 1; break;
                case SDLK_DOWN: in.down = 1; break;
                case SDLK_LEFT: in.left = 1; hold[2] = 1; break;
                case SDLK_RIGHT: in.right = 1; hold[3] = 1; break;
                }
            } else if (e.type == SDL_KEYUP) {
                switch (e.key.keysym.sym) {
                case SDLK_a: xheld = 0; break;
                case SDLK_LEFT: hold[2] = 0; break;
                case SDLK_RIGHT: hold[3] = 0; break;
                }
            }
        }
        for (int i = 2; i < 4; i++) {                      /* holding left/right keeps adjusting, faster and faster */
            if (!hold[i]) continue;
            hold[i]++;
            if (hold[i] > 16 && hold[i] % (hold[i] > 60 ? 1 : 3) == 0) { if (i == 2) in.left = 1; else in.right = 1; }
        }
        in.xheld = xheld;
        studio_update(&st, &in);
        if (st.action != ST_RUN) break;
        if (st.dirty_hw && st.hw && SDL_GetTicks() - last_apply > 30) {                 /* put the adjustment on the real screen */
            Adjust a = studio_screen_state(&st);
            int touch = a.sat != 100 || cubic_touched;
            if (hw_apply(&a, touch && (st.hw & HW_CUBIC)) == 0) { if (touch) cubic_touched = 1; st.dirty_hw = 0; }
            else { st.dirty_hw = 0; studio_toast(&st, "The screen refused the change"); }
            last_apply = SDL_GetTicks();
        }
        if (st.redraw || frame < 2 || shot) {              /* only paint when something changed */
            st.redraw = 0;
            studio_draw(&st, &cv);
            SDL_UpdateTexture(tex, NULL, fb, w * 4);
            SDL_RenderClear(ren);
            SDL_RenderCopy(ren, tex, NULL, NULL);
            SDL_RenderPresent(ren);
        } else SDL_Delay(16);
        if (shot && frame == 60) { SDL_Surface *sf = SDL_CreateRGBSurfaceFrom(fb, w, h, 32, w * 4, 255, 255u << 8, 255u << 16, 0); SDL_SaveBMP(sf, shot); SDL_FreeSurface(sf); if (getenv("DISPLAYTUNER_TEST_SAVE")) st.action = ST_SAVE; break; }
        if (windowed || shot) SDL_Delay(16);
    }
    int saved_now = st.action == ST_SAVE;
    if (saved_now) {
        adj_save(&st.cur, cfg);
        char flag[300]; snprintf(flag, sizeof flag, "%s/install.txt", dir);      /* the launcher installs the boot service when this exists */
        FILE *f = fopen(flag, "w"); if (f) { fputs("1\n", f); fclose(f); }
    }
    if (st.hw) { const Adjust *final = saved_now ? &st.cur : &saved; hw_apply(final, (final->sat != 100 || cubic_touched) && (st.hw & HW_CUBIC)); }
    unlink(LIVE);
    hw_close();
    studio_free(&st);
    free(fb);
    SDL_DestroyTexture(tex); SDL_DestroyRenderer(ren); SDL_DestroyWindow(win); SDL_Quit();
    return 0;
}
