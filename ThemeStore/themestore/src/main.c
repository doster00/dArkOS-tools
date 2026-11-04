/* Theme Store: the SDL2 front end for the handheld. Run with ES stopped (the launcher does that), or in a window on a PC.
 *   THEMESTORE_THEMES=/roms/themes   where the themes go        THEMESTORE_DIR=.   where cache/ and data/ are
 *   THEMESTORE_WINDOWED=1            a window instead of the full screen
 *   THEMESTORE_SHOT=file.bmp         draw the first screen to a file and quit (for tests)                      */
#include "store.h"
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *FONTS[] = { "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", "/usr/share/fonts/dejavu/DejaVuSans.ttf", NULL };
static const char *BOLDS[] = { "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", "/usr/share/fonts/dejavu/DejaVuSans-Bold.ttf", NULL };

int main(void)
{
    const char *root = getenv("THEMESTORE_THEMES") ? getenv("THEMESTORE_THEMES") : "/roms/themes";
    const char *dir = getenv("THEMESTORE_DIR") ? getenv("THEMESTORE_DIR") : ".";
    int windowed = getenv("THEMESTORE_WINDOWED") != NULL;
    const char *shot = getenv("THEMESTORE_SHOT");
    int ok = 0;
    for (int i = 0; FONTS[i] && !ok; i++) ok = ui_font(FONTS[i]);
    if (!ok) { fprintf(stderr, "themestore: no font found\n"); return 1; }
    for (int i = 0; BOLDS[i]; i++) if (ui_font_bold(BOLDS[i])) break;
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK) < 0) { fprintf(stderr, "themestore: %s\n", SDL_GetError()); return 1; }
    SDL_Window *win = SDL_CreateWindow("Theme Store", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, windowed ? 960 : 0, windowed ? 720 : 0,
                                       windowed ? SDL_WINDOW_RESIZABLE : SDL_WINDOW_FULLSCREEN_DESKTOP);
    if (!win) { fprintf(stderr, "themestore: %s\n", SDL_GetError()); return 1; }
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

    static Store st;
    store_init(&st, root, dir, w, h);
    int hold[4] = { 0 };                                   /* up, down, left, right: key repeat counters */
    int quit = 0;
    for (int frame = 0; !quit; frame++) {
        Input in = { 0 };
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) quit = 1;
            if (e.type == SDL_CONTROLLERBUTTONDOWN) {
                switch (e.cbutton.button) {                /* by position: bottom = A, right = B, left = X, top = Y */
                case SDL_CONTROLLER_BUTTON_A: in.a = 1; break;
                case SDL_CONTROLLER_BUTTON_B: in.b = 1; break;
                case SDL_CONTROLLER_BUTTON_X: in.x = 1; break;
                case SDL_CONTROLLER_BUTTON_Y: in.y = 1; break;
                case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: in.l = 1; break;
                case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: in.r = 1; break;
                case SDL_CONTROLLER_BUTTON_START: in.start = 1; break;
                case SDL_CONTROLLER_BUTTON_BACK: in.back = 1; break;
                case SDL_CONTROLLER_BUTTON_DPAD_UP: in.up = 1; hold[0] = 1; break;
                case SDL_CONTROLLER_BUTTON_DPAD_DOWN: in.down = 1; hold[1] = 1; break;
                case SDL_CONTROLLER_BUTTON_DPAD_LEFT: in.left = 1; hold[2] = 1; break;
                case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: in.right = 1; hold[3] = 1; break;
                }
            } else if (e.type == SDL_CONTROLLERBUTTONUP) {
                switch (e.cbutton.button) {
                case SDL_CONTROLLER_BUTTON_DPAD_UP: hold[0] = 0; break;
                case SDL_CONTROLLER_BUTTON_DPAD_DOWN: hold[1] = 0; break;
                case SDL_CONTROLLER_BUTTON_DPAD_LEFT: hold[2] = 0; break;
                case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: hold[3] = 0; break;
                }
            } else if (e.type == SDL_KEYDOWN && !e.key.repeat) {
                switch (e.key.keysym.sym) {
                case SDLK_RETURN: case SDLK_z: in.a = 1; break;
                case SDLK_ESCAPE: case SDLK_x: in.b = 1; break;
                case SDLK_a: in.x = 1; break;
                case SDLK_s: in.y = 1; break;
                case SDLK_q: in.l = 1; break;
                case SDLK_w: in.r = 1; break;
                case SDLK_TAB: in.back = 1; break;
                case SDLK_F5: in.start = 1; break;
                case SDLK_UP: in.up = 1; hold[0] = 1; break;
                case SDLK_DOWN: in.down = 1; hold[1] = 1; break;
                case SDLK_LEFT: in.left = 1; hold[2] = 1; break;
                case SDLK_RIGHT: in.right = 1; hold[3] = 1; break;
                }
            } else if (e.type == SDL_KEYUP) {
                switch (e.key.keysym.sym) {
                case SDLK_UP: hold[0] = 0; break;
                case SDLK_DOWN: hold[1] = 0; break;
                case SDLK_LEFT: hold[2] = 0; break;
                case SDLK_RIGHT: hold[3] = 0; break;
                }
            }
        }
        for (int i = 0; i < 4; i++) {                      /* holding a direction repeats it, faster and faster */
            if (!hold[i]) continue;
            hold[i]++;
            if (hold[i] > 22 && hold[i] % (hold[i] > 70 ? 2 : 5) == 0) { if (i == 0) in.up = 1; if (i == 1) in.down = 1; if (i == 2) in.left = 1; if (i == 3) in.right = 1; }
        }
        store_update(&st, &in);
        if (st.action == ACT_QUIT) break;
        if (st.action == ACT_APPLY) {                      /* the launcher reads this file and restarts ES with the new ThemeSet */
            const char *file = getenv("THEMESTORE_APPLY") ? getenv("THEMESTORE_APPLY") : "apply.txt";
            FILE *f = fopen(file, "w");
            if (f) { fprintf(f, "%s\n", st.apply_name); fclose(f); }
            break;
        }
        if (st.redraw || frame < 2 || shot) {              /* only paint when something changed: easy on the battery */
            st.redraw = 0;
            store_draw(&st, &cv);
            SDL_UpdateTexture(tex, NULL, fb, w * 4);
            SDL_RenderClear(ren);
            SDL_RenderCopy(ren, tex, NULL, NULL);
            SDL_RenderPresent(ren);
        } else SDL_Delay(30);
        if (shot && frame == 80) { SDL_Surface *s = SDL_CreateRGBSurfaceFrom(fb, w, h, 32, w * 4, 255, 255u << 8, 255u << 16, 0); SDL_SaveBMP(s, shot); SDL_FreeSurface(s); break; }
        if (windowed || shot) SDL_Delay(16);
    }
    int applied = st.action == ACT_APPLY;
    store_free(&st);
    free(fb);
    SDL_DestroyTexture(tex); SDL_DestroyRenderer(ren); SDL_DestroyWindow(win); SDL_Quit();
    return applied ? 10 : 0;
}
