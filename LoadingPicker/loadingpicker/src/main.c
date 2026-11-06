/* Loading Picker: choose the picture shown while a game opens (/roms/launchimages/loading.jpg).
 * Tabs: "Minhas" (the gallery folder) and "Online" (Picsum Photos, free, no key). The choice is cropped to the screen shape.
 *   LOADINGPICKER_DIR=.   app folder (gallery/ and cache/ live here)     LOADINGPICKER_OUT=/roms/launchimages/loading.jpg
 *   LOADINGPICKER_WINDOWED=1   LOADINGPICKER_SHOT=file.bmp (draw one frame and quit)                              */
#include "ui.h"
#include <SDL2/SDL.h>
#include <dirent.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#include "stb_image.h"

#define MAXI 120
#define COLS 3
#define ROWS 2
typedef struct { char name[72], path[320]; long id; u32 *th; int tw, thh, state; } Item;    /* state: 0 todo, 1 loading, 2 ok, 3 failed */
typedef struct { int up, down, left, right, a, b, x, y, l, r, start; } Input;
enum { J_NONE, J_LIST, J_THUMB, J_APPLY, J_RESTORE };

static Item loc[MAXI], onl[MAXI];
static int nloc, nonl, tab, sel[2], top[2], page = 1, job, job_idx, no_net, redraw = 1, confirm_del;
static pid_t pid;
static char dir[260], out[320], gal[300], cache[300], msg[240];
static int sw, sh, has_orig;
static const char *FONTS[] = { "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", "/usr/share/fonts/dejavu/DejaVuSans.ttf", NULL };
static const char *BOLDS[] = { "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", "/usr/share/fonts/dejavu/DejaVuSans-Bold.ttf", NULL };

static int exists(const char *p) { struct stat st; return stat(p, &st) == 0; }
static long fsize(const char *p) { struct stat st; return stat(p, &st) == 0 ? (long)st.st_size : -1; }

static void run(int kind, int idx, const char *cmd)
{
    pid = fork();
    if (pid == 0) {
        setsid();
        if (!freopen("/dev/null", "w", stdout)) _exit(126);
        if (!freopen("/dev/null", "w", stderr)) _exit(126);
        execl("/bin/sh", "sh", "-c", cmd, (char *)NULL);
        _exit(127);
    }
    job = kind; job_idx = idx; redraw = 1;
}

static void shrink(Item *it, const unsigned char *src, int w, int h)
{
    int tw = 320, th = 180;
    float ar = (float)w / h, tr = (float)tw / th, u0 = 0, u1 = 1, v0 = 0, v1 = 1;
    if (ar > tr) { float k = tr / ar; u0 = (1 - k) / 2; u1 = u0 + k; } else { float k = ar / tr; v0 = (1 - k) / 2; v1 = v0 + k; }
    it->th = malloc((size_t)tw * th * 4);
    for (int y = 0; y < th; y++) for (int x = 0; x < tw; x++) {                /* crop to fill */
        int sx = (int)((u0 + (u1 - u0) * (x + 0.5f) / tw) * w), sy = (int)((v0 + (v1 - v0) * (y + 0.5f) / th) * h);
        if (sx >= w) sx = w - 1;
        if (sy >= h) sy = h - 1;
        it->th[y * tw + x] = ((const u32 *)src)[sy * w + sx];
    }
    it->tw = tw; it->thh = th;
}

static void load_thumb(Item *it, const char *file)
{
    int w, h, n;
    if (fsize(file) > 20L * 1024 * 1024) { it->state = 3; return; }
    unsigned char *px = stbi_load(file, &w, &h, &n, 4);
    if (!px) { it->state = 3; return; }
    shrink(it, px, w, h);
    stbi_image_free(px);
    it->state = 2;
}

static void scan_local(void)
{
    for (int i = 0; i < nloc; i++) free(loc[i].th);
    nloc = 0;
    DIR *d = opendir(gal);
    struct dirent *e;
    while (d && (e = readdir(d)) && nloc < MAXI) {
        const char *x = strrchr(e->d_name, '.');
        if (!x || !(!strcasecmp(x, ".jpg") || !strcasecmp(x, ".jpeg") || !strcasecmp(x, ".png"))) continue;
        if (strchr(e->d_name, '\'') || strlen(e->d_name) > 70) continue;
        Item *it = &loc[nloc++];
        memset(it, 0, sizeof *it);
        snprintf(it->name, sizeof it->name, "%s", e->d_name);
        snprintf(it->path, sizeof it->path, "%s/%s", gal, e->d_name);
    }
    if (d) closedir(d);
    for (int i = 0; i < nloc; i++) for (int j = i + 1; j < nloc; j++) if (strcmp(loc[i].name, loc[j].name) > 0) { Item t = loc[i]; loc[i] = loc[j]; loc[j] = t; }
    if (sel[0] >= nloc) sel[0] = nloc ? nloc - 1 : 0;
}

static void parse_list(void)                       /* cache/list<page>.json: [{"id":"10","author":"Paul Jarvis",...},...] */
{
    char f[340];
    snprintf(f, sizeof f, "%s/list%d.json", cache, page);
    FILE *fp = fopen(f, "r");
    if (!fp) return;
    static char buf[40000];
    size_t n = fread(buf, 1, sizeof buf - 1, fp);
    fclose(fp);
    buf[n] = 0;
    for (int i = 0; i < nonl; i++) free(onl[i].th);
    nonl = 0;
    char *p = buf;
    while ((p = strstr(p, "\"id\":\"")) && nonl < MAXI) {
        Item *it = &onl[nonl];
        memset(it, 0, sizeof *it);
        it->id = atol(p + 6);
        p += 6;
        char *next = strstr(p, "\"id\":\""), *a = strstr(p, "\"author\":\"");
        if (a && (!next || a < next)) { a += 10; int k = 0; while (*a && *a != '"' && k < 40) it->name[k++] = *a++; it->name[k] = 0; }
        if (!it->name[0]) snprintf(it->name, sizeof it->name, "foto %ld", it->id);
        nonl++;
    }
    sel[1] = top[1] = 0;
}

static void get_list(void)
{
    char f[340], cmd[1300];
    snprintf(f, sizeof f, "%s/list%d.json", cache, page);
    if (exists(f) && fsize(f) > 20) { parse_list(); return; }
    snprintf(cmd, sizeof cmd, "curl -fsL --max-time 25 -o '%s.part' 'https://picsum.photos/v2/list?page=%d&limit=30' && mv '%s.part' '%s'", f, page, f, f);
    run(J_LIST, 0, cmd);
}

static void finish_job(int rc)
{
    int k = job;
    job = J_NONE;
    if (k == J_LIST) {
        if (rc == 0) { parse_list(); no_net = 0; } else { no_net = 1; snprintf(msg, sizeof msg, "Sem internet (ou o servico esta fora do ar)."); }
    } else if (k == J_THUMB) {
        onl[job_idx].state = rc == 0 ? 0 : 3;
    } else if (k == J_APPLY || k == J_RESTORE) {
        if (rc == 0) snprintf(msg, sizeof msg, "%s", k == J_APPLY ? "Pronto! Essa imagem aparece ao abrir um jogo." : "Imagem original restaurada.");
        else snprintf(msg, sizeof msg, "Nao deu certo (codigo %d). Veja a internet e o espaco.", rc);
        char bak[340];
        snprintf(bak, sizeof bak, "%s.original", out);
        has_orig = exists(bak);
        if (rc == 0 && k == J_APPLY && !exists("/home/ark/.config/.GameLoadingIModePIC")) strncat(msg, " Aviso: o modo de carregamento nao esta em imagem (PIC).", sizeof msg - strlen(msg) - 1);
        scan_local();
    }
    redraw = 1;
}

static void apply(const char *srcfile, long picsum_id)
{
    char cmd[3200], bak[340], get[1300] = "", file[340];
    snprintf(bak, sizeof bak, "%s.original", out);
    snprintf(file, sizeof file, "%s", srcfile);
    if (picsum_id) {
        snprintf(file, sizeof file, "%s/picsum-%ld.jpg", gal, picsum_id);
        snprintf(get, sizeof get, "curl -fsL --max-time 60 -o '%s.part' 'https://picsum.photos/id/%ld/%d/%d.jpg' && mv '%s.part' '%s' && ", file, picsum_id, sw, sh, file, file);
    }
    snprintf(cmd, sizeof cmd,
             "%s{ [ -f '%s' ] || [ ! -f '%s' ] || cp '%s' '%s'; } && convert '%s[0]' -auto-orient -resize %dx%d^ -gravity center -extent %dx%d -quality 90 '%s.new.jpg' && mv '%s.new.jpg' '%s'",
             get, bak, out, out, bak, file, sw, sh, sw, sh, out, out, out);
    snprintf(msg, sizeof msg, "Aplicando...");
    run(J_APPLY, 0, cmd);
}

static void restore(void)
{
    char cmd[1300];
    snprintf(cmd, sizeof cmd, "[ -f '%s.original' ] && cp '%s.original' '%s'", out, out, out);
    snprintf(msg, sizeof msg, "Restaurando...");
    run(J_RESTORE, 0, cmd);
}

static int count(void) { return tab ? nonl : nloc; }
static Item *items(void) { return tab ? onl : loc; }

static void update(const Input *in)
{
    if (pid) {
        int st;
        if (waitpid(pid, &st, WNOHANG) == pid) { pid = 0; finish_job(WIFEXITED(st) ? WEXITSTATUS(st) : 1); }
        redraw = 1;
    }
    if (job == J_APPLY || job == J_RESTORE) return;
    int any = in->a || in->b || in->x || in->y || in->start || in->up || in->down || in->left || in->right || in->l || in->r;
    if (msg[0] && any) { msg[0] = 0; redraw = 1; return; }
    int n = count(), *s = &sel[tab];
    if (confirm_del) {
        if (in->a) { if (tab == 0 && n) { unlink(loc[*s].path); scan_local(); } confirm_del = 0; redraw = 1; }
        else if (in->b) { confirm_del = 0; redraw = 1; }
        return;
    }
    if (in->l || in->r) { tab = !tab; redraw = 1; if (tab && !nonl && !job) get_list(); return; }
    if (in->left && *s > 0) { (*s)--; redraw = 1; }
    if (in->right && *s < n - 1) { (*s)++; redraw = 1; }
    if (in->up && *s >= COLS) { *s -= COLS; redraw = 1; }
    if (in->down && *s + COLS < n) { *s += COLS; redraw = 1; }
    else if (in->down && *s / COLS < (n - 1) / COLS) { *s = n - 1; redraw = 1; }
    int row = *s / COLS;
    if (row < top[tab]) top[tab] = row;
    if (row >= top[tab] + ROWS) top[tab] = row - ROWS + 1;
    if (in->a && n) { Item *it = &items()[*s]; if (tab) apply("", it->id); else apply(it->path, 0); return; }
    if (in->y && has_orig) { restore(); return; }
    if (in->x && tab == 0 && n) { confirm_del = 1; redraw = 1; return; }
    if (in->start && tab == 1 && !job) { page = page % 30 + 1; nonl = 0; get_list(); }
    /* thumbnails: visible items only, one at a time */
    for (int r = top[tab]; r < top[tab] + ROWS; r++) for (int c = 0; c < COLS; c++) {
        int i = r * COLS + c;
        if (i >= n) break;
        Item *it = &items()[i];
        if (it->state != 0) continue;
        if (tab == 0) { load_thumb(it, it->path); redraw = 1; return; }
        char f[340];
        snprintf(f, sizeof f, "%s/t%ld.jpg", cache, it->id);
        if (exists(f) && fsize(f) > 100) { load_thumb(it, f); redraw = 1; return; }
        if (!job) {
            char cmd[1300];
            snprintf(cmd, sizeof cmd, "curl -fsL --max-time 25 -o '%s.part' 'https://picsum.photos/id/%ld/320/180.jpg' && mv '%s.part' '%s'", f, it->id, f, f);
            it->state = 1;
            run(J_THUMB, i, cmd);
        }
        return;
    }
}

static void draw(Canvas *c)
{
    clear(c, RGB(15, 20, 33));
    float W = c->dw, mx = 22, gap = 14;
    text(c, mx, 14, 22, 1, RGB(235, 240, 250), AL_LEFT, "Loading Picker");
    const char *tn[2] = { "Minhas", "Online" };
    for (int t = 0; t < 2; t++) {
        float x = W - mx - 2 * 92 + t * 92;
        rrect(c, x, 14, 86, 26, 13, t == tab ? RGB(36, 150, 170) : RGB(35, 44, 66));
        text(c, x + 43, 18, 14, t == tab, RGB(240, 248, 250), AL_CENTER, tn[t]);
    }
    text(c, mx, 46, 12, 0, RGB(140, 155, 185), AL_LEFT, tab ? "Fotos gratis do Picsum Photos (licenca Unsplash). Start = outras fotos." : "Imagens da pasta gallery. Coloque suas JPG/PNG la.");
    float cw = (W - 2 * mx - (COLS - 1) * gap) / COLS, ch = cw * 9 / 16, y0 = 72;
    int n = count();
    for (int r = 0; r < ROWS; r++) for (int col = 0; col < COLS; col++) {
        int i = (top[tab] + r) * COLS + col;
        if (i >= n) continue;
        Item *it = &items()[i];
        float x = mx + col * (cw + gap), y = y0 + r * (ch + 28);
        int on = i == sel[tab];
        shadow(c, x, y, cw, ch, 8, 8, RGBA(0, 0, 0, 120));
        rrect(c, x, y, cw, ch, 8, RGB(28, 36, 56));
        if (it->state == 2) image(c, it->th, it->tw, it->thh, x, y, cw, ch, 8, 1);
        else text(c, x + cw / 2, y + ch / 2 - 8, 12, 0, RGB(120, 135, 165), AL_CENTER, it->state == 3 ? "falhou" : "carregando...");
        if (on) rrect_line(c, x - 2, y - 2, cw + 4, ch + 4, 10, 3, RGB(60, 210, 230));
        text(c, x + 2, y + ch + 5, 11, on, on ? RGB(235, 240, 250) : RGB(150, 163, 190), AL_LEFT, it->name);
    }
    if (!n) text(c, W / 2, 190, 16, 0, RGB(150, 163, 190), AL_CENTER, tab ? (job == J_LIST ? "Buscando fotos..." : "Sem fotos. Veja a internet.") : "A pasta gallery esta vazia. Use a aba Online.");
    float fy = DH - 38;
    rrect(c, 0, fy - 4, W, 42, 0, RGB(22, 29, 46));
    char hint[220];
    snprintf(hint, sizeof hint, "A usar   %sB sair   L/R aba%s", tab ? "" : "X apagar   ", has_orig ? "   Y original" : "");
    text(c, mx, fy + 6, 13, 0, RGB(170, 183, 210), AL_LEFT, hint);
    char info[80];
    snprintf(info, sizeof info, "Tela %dx%d", sw, sh);
    text(c, W - mx, fy + 6, 12, 0, RGB(110, 124, 155), AL_RIGHT, info);
    if (msg[0] || confirm_del) {
        rrect(c, W * 0.12f, 170, W * 0.76f, 120, 14, RGBA(10, 14, 24, 245));
        rrect_line(c, W * 0.12f, 170, W * 0.76f, 120, 14, 2, RGB(60, 210, 230));
        text(c, W / 2, 205, 15, 1, RGB(240, 245, 250), AL_CENTER, confirm_del ? "Apagar essa imagem da pasta?" : msg);
        if (confirm_del) text(c, W / 2, 245, 13, 0, RGB(170, 183, 210), AL_CENTER, "A confirmar   B cancelar");
    }
}

int main(void)
{
    const char *d = getenv("LOADINGPICKER_DIR"), *o = getenv("LOADINGPICKER_OUT"), *shot = getenv("LOADINGPICKER_SHOT");
    int windowed = getenv("LOADINGPICKER_WINDOWED") != NULL;
    snprintf(dir, sizeof dir, "%s", d ? d : ".");
    snprintf(out, sizeof out, "%s", o ? o : "/roms/launchimages/loading.jpg");
    snprintf(gal, sizeof gal, "%s/gallery", dir);
    snprintf(cache, sizeof cache, "%s/cache", dir);
    mkdir(gal, 0755);
    mkdir(cache, 0755);
    int ok = 0;
    for (int i = 0; FONTS[i] && !ok; i++) ok = ui_font(FONTS[i]);
    if (!ok) { fprintf(stderr, "loadingpicker: no font\n"); return 1; }
    for (int i = 0; BOLDS[i]; i++) if (ui_font_bold(BOLDS[i])) break;
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK) < 0) { fprintf(stderr, "%s\n", SDL_GetError()); return 1; }
    SDL_Window *win = SDL_CreateWindow("Loading Picker", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, windowed ? 960 : 0, windowed ? 540 : 0,
                                       windowed ? SDL_WINDOW_RESIZABLE : SDL_WINDOW_FULLSCREEN_DESKTOP);
    if (!win) { fprintf(stderr, "%s\n", SDL_GetError()); return 1; }
    SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ren) ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
    int w, h;
    SDL_GetRendererOutputSize(ren, &w, &h);
    sw = w; sh = h;
    if (sh > sw) { int t = sw; sw = sh; sh = t; }          /* portrait panel: the picture is made landscape */
    SDL_Texture *tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, w, h);
    u32 *fb = malloc((size_t)w * h * 4);
    Canvas cv;
    ui_init(&cv, fb, w, h);
    SDL_ShowCursor(SDL_DISABLE);
    for (int i = 0; i < SDL_NumJoysticks(); i++) if (SDL_IsGameController(i)) SDL_GameControllerOpen(i); else SDL_JoystickOpen(i);
    {
        char bak[340];
        snprintf(bak, sizeof bak, "%s.original", out);
        has_orig = exists(bak);
    }
    scan_local();
    if (!nloc) { tab = 1; get_list(); }
    int hold[4] = { 0 }, quit = 0;
    for (int frame = 0; !quit; frame++) {
        Input in = { 0 };
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) quit = 1;
            if (e.type == SDL_CONTROLLERBUTTONDOWN) {
                switch (e.cbutton.button) {
                case SDL_CONTROLLER_BUTTON_A: in.a = 1; break;
                case SDL_CONTROLLER_BUTTON_B: in.b = 1; break;
                case SDL_CONTROLLER_BUTTON_X: in.x = 1; break;
                case SDL_CONTROLLER_BUTTON_Y: in.y = 1; break;
                case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: in.l = 1; break;
                case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: in.r = 1; break;
                case SDL_CONTROLLER_BUTTON_START: in.start = 1; break;
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
                case SDLK_d: in.x = 1; break;
                case SDLK_s: in.y = 1; break;
                case SDLK_q: in.l = 1; break;
                case SDLK_w: in.r = 1; break;
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
        for (int i = 0; i < 4; i++) {
            if (!hold[i]) continue;
            hold[i]++;
            if (hold[i] > 22 && hold[i] % 5 == 0) { if (i == 0) in.up = 1; if (i == 1) in.down = 1; if (i == 2) in.left = 1; if (i == 3) in.right = 1; }
        }
        if (in.b && !confirm_del && !msg[0] && job != J_APPLY && job != J_RESTORE) break;
        update(&in);
        if (redraw || frame < 2 || shot) {
            redraw = 0;
            draw(&cv);
            SDL_UpdateTexture(tex, NULL, fb, w * 4);
            SDL_RenderClear(ren);
            SDL_RenderCopy(ren, tex, NULL, NULL);
            SDL_RenderPresent(ren);
        } else SDL_Delay(30);
        if (shot && frame == 150) { SDL_Surface *s = SDL_CreateRGBSurfaceFrom(fb, w, h, 32, w * 4, 255, 255u << 8, 255u << 16, 0); SDL_SaveBMP(s, shot); SDL_FreeSurface(s); break; }
        if (windowed || shot) SDL_Delay(16);
    }
    if (pid) { kill(-pid, SIGTERM); waitpid(pid, NULL, 0); }
    SDL_DestroyTexture(tex); SDL_DestroyRenderer(ren); SDL_DestroyWindow(win); SDL_Quit();
    return 0;
}
