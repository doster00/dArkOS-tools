#include "store.h"
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#include "stb_image.h"
#include <ctype.h>
#include <strings.h>
#include <dirent.h>
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define CATALOG_URL "https://raw.githubusercontent.com/Jetup13/Emulationstation-OGA-Theme-Gallery/main/README.md"
#define BG1 RGB(13, 15, 27)
#define BG2 RGB(30, 24, 54)
#define CARD RGB(31, 35, 58)
#define CARD2 RGB(41, 46, 74)
#define TXT RGB(236, 240, 255)
#define SUB RGB(140, 150, 188)
#define ACC RGB(124, 164, 255)
#define ACC2 RGB(168, 128, 255)
#define OKC RGB(94, 214, 150)
#define WARN RGB(244, 190, 96)
#define BAD RGB(240, 106, 120)

const char *RES_NAME[NRES] = { "480x320", "640x480", "854x480", "960x544", "720x720" };
static const float RES_AR[NRES] = { 480 / 320.f, 640 / 480.f, 854 / 480.f, 960 / 544.f, 1.f };
static const char *FILTER_NAME[4] = { "all themes", "works on this screen", "works or untested", "installed only" };

/* --- small helpers -------------------------------------------------------------------------- */

static int safe_url(const char *s)                      /* only plain URL characters ever reach a shell */
{
    if (strncmp(s, "https://", 8)) return 0;
    for (; *s; s++) if (!(isalnum((unsigned char)*s) || strchr("._-/:~+%", *s))) return 0;
    return 1;
}

static int safe_dir(const char *s)                      /* a theme folder name */
{
    if (!*s || s[0] == '.') return 0;
    for (; *s; s++) if (!(isalnum((unsigned char)*s) || strchr("._-", *s))) return 0;
    return 1;
}

static void trim(char *s)
{
    char *p = s;
    while (*p && isspace((unsigned char)*p)) p++;
    memmove(s, p, strlen(p) + 1);
    for (int n = (int)strlen(s); n > 0 && isspace((unsigned char)s[n - 1]); ) s[--n] = 0;
}

static char *slurp(const char *path, size_t max)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    char *b = malloc(max + 1);
    size_t n = fread(b, 1, max, f);
    fclose(f);
    b[n] = 0;
    return b;
}

static int exists(const char *p) { struct stat st; return stat(p, &st) == 0; }
static int is_active_name(const Store *s, const char *repo) { return s->active[0] && !strcasecmp(repo, s->active); }
static void pv_free(Store *s);
static void ask(Store *s, int what, int idx);
static void fmt_size(char *out, size_t n, long kb);

static const char *author_of(const Entry *e, char *out, size_t n)      /* github.com/<author>/<repo> */
{
    const char *p = strstr(e->url, "github.com/");
    out[0] = 0;
    if (p) { p += 11; size_t k = strcspn(p, "/"); if (k >= n) k = n - 1; memcpy(out, p, k); out[k] = 0; }
    return out;
}

/* --- the catalog: the gallery README is a Markdown table, one theme per line ---------------- */

static int parse_catalog(Entry *e, const char *text)
{
    int n = 0;
    for (const char *line = text; *line && n < MAX_E; ) {
        const char *end = strchr(line, '\n');
        size_t len = end ? (size_t)(end - line) : strlen(line);
        char buf[1024];
        if (len >= sizeof buf) len = sizeof buf - 1;
        memcpy(buf, line, len); buf[len] = 0;
        line = end ? end + 1 : line + len;
        if (strncmp(buf, "| [", 3)) continue;
        char *p = buf + 3, *q = strstr(p, "](");
        if (!q) continue;
        Entry t;
        memset(&t, 0, sizeof t);
        *q = 0;
        snprintf(t.name, sizeof t.name, "%s", p);
        p = q + 2;
        q = strchr(p, ')');
        if (!q) continue;
        *q = 0;
        snprintf(t.url, sizeof t.url, "%s", p);
        p = q + 1;
        q = strstr(p, "![](");
        if (!q) continue;
        p = q + 4;
        q = strchr(p, ')');
        if (!q) continue;
        *q = 0;
        snprintf(t.prev, sizeof t.prev, "%s", p);
        p = q + 1;
        q = strchr(p, '|');
        if (!q) continue;
        p = q + 1;
        for (int i = 0; i < NRES; i++) {
            char cell[40];
            q = strchr(p, '|');
            size_t k = q ? (size_t)(q - p) : strlen(p);
            if (k >= sizeof cell) k = sizeof cell - 1;
            memcpy(cell, p, k); cell[k] = 0;
            trim(cell);
            t.res[i] = tolower((unsigned char)cell[0]) == 'y' ? 'Y' : tolower((unsigned char)cell[0]) == 'n' ? 'N' : 'U';
            p = q ? q + 1 : p + k;
        }
        size_t ul = strlen(t.url);                      /* the folder name is the repository name */
        while (ul && (t.url[ul - 1] == '/')) t.url[--ul] = 0;
        if (ul > 4 && !strcmp(t.url + ul - 4, ".git")) t.url[ul -= 4] = 0;
        const char *slash = strrchr(t.url, '/');
        snprintf(t.repo, sizeof t.repo, "%s", slash ? slash + 1 : t.url);
        if (!safe_dir(t.repo) || !safe_url(t.url) || !safe_url(t.prev)) continue;
        e[n++] = t;
    }
    return n;
}

static void refresh_installed(Store *s)
{
    for (int i = 0; i < s->n; i++) {
        char p[400];
        snprintf(p, sizeof p, "%s/%s", s->root, s->e[i].repo);
        s->e[i].installed = exists(p);
    }
    s->active[0] = 0;
    char *c = slurp(s->cfg, 60000);
    if (c) {
        char *k = strstr(c, "name=\"ThemeSet\" value=\"");
        if (k) { k += 23; size_t m = strcspn(k, "\""); if (m >= sizeof s->active) m = sizeof s->active - 1; memcpy(s->active, k, m); s->active[m] = 0; }
        free(c);
    }
}

static void rebuild(Store *s, int keep_entry)           /* apply the filter; keep the highlighted theme when it is still there */
{
    s->nvis = 0;
    for (int i = 0; i < s->n; i++) {
        char r = s->e[i].res[s->target];
        if (s->filter == 3) { if (s->e[i].installed) s->vis[s->nvis++] = i; continue; }
        if (s->filter == 1 && r != 'Y') continue;
        if (s->filter == 2 && r == 'N') continue;
        s->vis[s->nvis++] = i;
    }
    s->sel = 0;
    for (int i = 0; i < s->nvis; i++) if (s->vis[i] == keep_entry) s->sel = i;
    s->top = s->sel > 3 ? s->sel - 3 : 0;
}

static void load_catalog(Store *s)
{
    char p[400];
    snprintf(p, sizeof p, "%s/cache/catalog.md", s->dir);
    char *t = slurp(p, 400000);
    if (!t) { snprintf(p, sizeof p, "%s/data/catalog.md", s->dir); t = slurp(p, 400000); }
    int cur = s->nvis ? s->vis[s->sel] : 0;
    char keep[96] = "";
    if (s->n && cur < s->n) snprintf(keep, sizeof keep, "%s", s->e[cur].repo);
    s->n = t ? parse_catalog(s->e, t) : 0;
    free(t);
    memset(s->pvfail, 0, sizeof s->pvfail);
    pv_free(s);
    DIR *dh = opendir(s->root);                         /* themes on the console that the gallery does not know */
    for (struct dirent *de; dh && (de = readdir(dh)) && s->n < MAX_E; ) {
        if (!safe_dir(de->d_name)) continue;
        int known = 0;
        for (int i = 0; i < s->n; i++) if (!strcasecmp(s->e[i].repo, de->d_name)) known = 1;
        if (known) continue;
        char full[400];
        snprintf(full, sizeof full, "%s/%s", s->root, de->d_name);
        struct stat st;
        if (stat(full, &st) != 0 || !S_ISDIR(st.st_mode)) continue;
        Entry *e = &s->e[s->n];
        memset(e, 0, sizeof *e);
        snprintf(e->name, sizeof e->name, "%s", de->d_name);
        snprintf(e->repo, sizeof e->repo, "%s", de->d_name);
        memset(e->res, 'U', NRES);
        e->local = 1;
        s->pvfail[s->n++] = 1;                          /* no preview for these */
    }
    if (dh) closedir(dh);
    refresh_installed(s);
    int k = 0;
    for (int i = 0; i < s->n; i++) if (!strcmp(s->e[i].repo, keep)) k = i;
    rebuild(s, k);
}

/* --- background jobs: a child process (curl, unzip, rm); the screen keeps drawing ----------- */

static int job_start(Store *s, int kind, int idx, const char *cmd)
{
    if (s->pid && s->job == J_PREVIEW && kind != J_PREVIEW) {          /* a background preview never blocks something the user asked for */
        kill(-s->pid, SIGTERM);
        waitpid(s->pid, NULL, 0);
        s->pid = 0; s->job = J_NONE;
    }
    if (s->pid) return 0;
    pid_t p = fork();
    if (p < 0) return 0;
    if (p == 0) {
        int nul = open("/dev/null", O_RDWR);
        setsid();                                      /* own process group: a cancel stops curl and unzip too */
        if (nul >= 0) { dup2(nul, 0); dup2(nul, 1); dup2(nul, 2); }
        execl("/bin/sh", "sh", "-c", cmd, (char *)NULL);
        _exit(127);
    }
    s->pid = p; s->job = kind; s->job_idx = idx; s->job_bytes = 0; s->job_t0 = time(NULL);
    return 1;
}

static int job_poll(Store *s, int *ok)
{
    int st;
    pid_t r = waitpid(s->pid, &st, WNOHANG);
    if (r == 0) return 0;
    s->pid = 0;
    *ok = r > 0 && WIFEXITED(st) && WEXITSTATUS(st) == 0;
    return 1;
}

static void job_cancel(Store *s)                        /* give up on a download: stop it, throw the leftovers away */
{
    char cmd[400];
    kill(-s->pid, SIGTERM);
    waitpid(s->pid, NULL, 0);
    s->pid = 0;
    if (s->job == J_INSTALL) { snprintf(cmd, sizeof cmd, "rm -rf '%s/.tstore_tmp'", s->root); if (system(cmd)) {} }
    if (s->job == J_CATALOG) { snprintf(cmd, sizeof cmd, "rm -f '%s/cache/catalog.part'", s->dir); if (system(cmd)) {} }
    int quiet = s->job == J_SIZE;                       /* checking the size is not worth a message when dropped */
    s->job = J_NONE;
    if (quiet) { s->mode = M_LIST; } else { s->mode = M_MSG; snprintf(s->msg, sizeof s->msg, "Canceled."); }
    s->redraw = 1;
}
static void prev_path(const Store *s, int idx, char *out, size_t n) { snprintf(out, n, "%s/cache/prev_%s.img", s->dir, s->e[idx].repo); }

static void start_catalog(Store *s)
{
    char cmd[900];
    snprintf(cmd, sizeof cmd, "mkdir -p '%s/cache' && curl -fsSL --max-time 30 -o '%s/cache/catalog.part' '%s' && mv '%s/cache/catalog.part' '%s/cache/catalog.md'",
             s->dir, s->dir, CATALOG_URL, s->dir, s->dir);
    job_start(s, J_CATALOG, 0, cmd);
}

static void start_size(Store *s, int idx)               /* ask GitHub how big the repository is, before the user commits to a long download */
{
    const Entry *e = &s->e[idx];
    char au[64], cmd[900];
    author_of(e, au, sizeof au);
    if (!safe_dir(au) || s->no_net) { s->e[idx].size_kb = 0; s->mode = M_CONFIRM; s->confirm = C_INSTALL; snprintf(s->apply_name, sizeof s->apply_name, "%s", e->repo); return; }
    snprintf(cmd, sizeof cmd, "curl -fsSL --max-time 15 'https://api.github.com/repos/%s/%s' | grep -m1 '\"size\"' | tr -dc '0-9' > '%s/cache/size_%s.txt'", au, e->repo, s->dir, e->repo);
    if (job_start(s, J_SIZE, idx, cmd)) { s->mode = M_BUSY; snprintf(s->msg, sizeof s->msg, "Checking the size of %s", e->name); }
}

static void start_install(Store *s, int idx)
{
    const Entry *e = &s->e[idx];
    char cmd[2000], tmp[320];
    snprintf(tmp, sizeof tmp, "%s/.tstore_tmp", s->root);
    snprintf(s->job_file, sizeof s->job_file, "%s/t.zip", tmp);
    snprintf(cmd, sizeof cmd,
        "set -e; T='%s'; rm -rf \"$T\"; mkdir -p \"$T\"; "
        "curl -fsSL --retry 2 --max-time 900 -o \"$T/t.zip\" '%s/archive/HEAD.zip'; "
        "unzip -q \"$T/t.zip\" -d \"$T/x\"; rm -f \"$T/t.zip\"; "
        "f=$(find \"$T/x\" -maxdepth 3 -name theme.xml | awk '{print length($0) \" \" $0}' | sort -n | head -n1 | cut -d' ' -f2-); "
        "[ -n \"$f\" ]; d=$(dirname \"$f\"); rm -rf '%s/%s'; mv \"$d\" '%s/%s'; rm -rf \"$T\"",
        tmp, e->url, s->root, e->repo, s->root, e->repo);
    if (job_start(s, J_INSTALL, idx, cmd)) { s->mode = M_BUSY; snprintf(s->msg, sizeof s->msg, "Installing %s", e->name); }
}

static void start_remove(Store *s, int idx)
{
    char cmd[600];
    snprintf(cmd, sizeof cmd, "rm -rf '%s/%s'", s->root, s->e[idx].repo);
    if (job_start(s, J_REMOVE, idx, cmd)) { s->mode = M_BUSY; s->job_file[0] = 0; snprintf(s->msg, sizeof s->msg, "Removing %s", s->e[idx].name); }
}

static void pv_free(Store *s) { free(s->pv); s->pv = NULL; s->pv_idx = -1; }

static void pv_load(Store *s, int idx)                  /* decode a cached preview and shrink it once, so drawing stays cheap */
{
    char p[400];
    prev_path(s, idx, p, sizeof p);
    int w, h, n;
    unsigned char *d = stbi_load(p, &w, &h, &n, 4);
    pv_free(s);
    if (!d || w < 2 || h < 2) { if (d) stbi_image_free(d); s->pvfail[idx] = 1; remove(p); return; }
    int tw = w < 440 ? w : 440, th = (int)((long)h * tw / w);
    if (th < 1) th = 1;
    u32 *o = malloc((size_t)tw * th * 4);
    for (int y = 0; y < th; y++) {
        int y0 = (int)((long)y * h / th), y1 = (int)((long)(y + 1) * h / th);
        if (y1 <= y0) y1 = y0 + 1;
        for (int x = 0; x < tw; x++) {
            int x0 = (int)((long)x * w / tw), x1 = (int)((long)(x + 1) * w / tw);
            if (x1 <= x0) x1 = x0 + 1;
            unsigned r = 0, g = 0, b = 0, cnt = 0;
            for (int yy = y0; yy < y1 && yy < h; yy++)
                for (int xx = x0; xx < x1 && xx < w; xx++) { const unsigned char *q = d + ((size_t)yy * w + xx) * 4; r += q[0]; g += q[1]; b += q[2]; cnt++; }
            if (!cnt) cnt = 1;
            o[y * tw + x] = RGB(r / cnt, g / cnt, b / cnt);
        }
    }
    stbi_image_free(d);
    s->pv = o; s->pw = tw; s->ph = th; s->pv_idx = idx;
}

/* --- init ------------------------------------------------------------------------------------ */

static int nearest_res(int w, int h)                    /* which gallery column matches this screen best */
{
    float ar = w > h ? (float)w / h : (float)h / w;     /* a panel mounted upright still shows a landscape ES */
    int best = 1;
    float bd = 99;
    for (int i = 0; i < NRES; i++) {
        float d = fabsf(RES_AR[i] - ar);
        if (d < bd - 0.001f) { bd = d; best = i; }
    }
    return best;
}

void store_init(Store *s, const char *themes_root, const char *app_dir, int sw, int sh)
{
    memset(s, 0, sizeof *s);
    snprintf(s->root, sizeof s->root, "%s", themes_root);
    snprintf(s->dir, sizeof s->dir, "%s", app_dir);
    const char *cfg = getenv("THEMESTORE_CFG");
    snprintf(s->cfg, sizeof s->cfg, "%s", cfg ? cfg : "/home/ark/.emulationstation/es_settings.cfg");
    s->screen_w = sw; s->screen_h = sh;
    s->target = nearest_res(sw, sh);
    s->filter = 2;
    s->pv_idx = -1; s->want_idx = -1;
    char c[400];
    snprintf(c, sizeof c, "%s/cache", s->dir);
    mkdir(c, 0755);
    snprintf(c, sizeof c, "cd '%s/cache' && ls -t prev_* 2>/dev/null | tail -n +81 | xargs -r rm -f", s->dir);       /* keep the newest 80 previews */
    if (system(c)) {}
    load_catalog(s);
    s->redraw = 1;
}

void store_free(Store *s)
{
    if (s->pid) { kill(s->pid, SIGTERM); waitpid(s->pid, NULL, 0); }
    pv_free(s);
}

/* --- one frame of logic ---------------------------------------------------------------------- */

static void finish_job(Store *s, int ok)
{
    int idx = s->job_idx;
    s->redraw = 1;
    switch (s->job) {
    case J_CATALOG:
        if (ok) { load_catalog(s); s->cat_online = 1; if (s->mode == M_BUSY) { s->mode = M_MSG; snprintf(s->msg, sizeof s->msg, "Theme list updated: %d themes.", s->n); } }
        else if (s->mode == M_BUSY) { s->mode = M_MSG; snprintf(s->msg, sizeof s->msg, "Could not download the list. Check the Wi-Fi."); }
        break;
    case J_PREVIEW:
        if (ok && idx == s->want_idx) pv_load(s, idx); else if (!ok) { s->pvfail[idx] = 1; char p[400]; prev_path(s, idx, p, sizeof p); strcat(p, ".part"); remove(p); }
        break;
    case J_SIZE: {
        char p[400];
        snprintf(p, sizeof p, "%s/cache/size_%s.txt", s->dir, s->e[idx].repo);
        char *t = ok ? slurp(p, 40) : NULL;
        s->e[idx].size_kb = t ? atol(t) : 0;
        free(t);
        ask(s, C_INSTALL, idx);
        break;
    }
    case J_INSTALL:
        if (ok && time(NULL) - s->job_t0 > 3 && s->job_bytes > 500000) s->speed = s->job_bytes / (time(NULL) - s->job_t0);
        refresh_installed(s);
        if (ok && s->e[idx].installed) {
            s->mode = M_CONFIRM; s->confirm = C_APPLY;
            snprintf(s->apply_name, sizeof s->apply_name, "%s", s->e[idx].repo);
            snprintf(s->msg, sizeof s->msg, "%s is installed. Use it as your theme?\nIt is applied when you close the Theme Store.", s->e[idx].name);
        } else { s->mode = M_MSG; snprintf(s->msg, sizeof s->msg, "Install failed. Check the Wi-Fi and the free space."); }
        break;
    case J_REMOVE:
        refresh_installed(s);
        s->mode = M_MSG; snprintf(s->msg, sizeof s->msg, "%s was removed.", s->e[idx].name);
        break;
    }
    s->job = J_NONE;
}

static void move_sel(Store *s, int d)
{
    if (!s->nvis) return;
    s->sel += d;
    if (s->sel < 0) s->sel = 0;
    if (s->sel >= s->nvis) s->sel = s->nvis - 1;
    if (s->sel < s->top) s->top = s->sel;
    if (s->sel > s->top + 8) s->top = s->sel - 8;
    s->redraw = 1;
}

static void fmt_size(char *out, size_t n, long kb) { if (kb >= 1024) snprintf(out, n, "%.0f MB", kb / 1024.0); else snprintf(out, n, "%ld KB", kb); }

static void ask(Store *s, int what, int idx)
{
    const Entry *e = &s->e[idx];
    s->mode = M_CONFIRM; s->confirm = what;
    snprintf(s->apply_name, sizeof s->apply_name, "%s", e->repo);
    if (what == C_INSTALL) {
        if (e->size_kb > 0) {
            char sz[32], eta[64] = "";
            fmt_size(sz, sizeof sz, e->size_kb);
            long bps = s->speed > 20000 ? s->speed : 0;
            if (bps) { long sec = (long)(e->size_kb * 1024L / bps); snprintf(eta, sizeof eta, ", about %ld min at your speed", sec < 60 ? 1L : sec / 60); }
            snprintf(s->msg, sizeof s->msg, "Download and install %s?\nUp to %s%s. Press B to cancel any time.", e->name, sz, eta);
        } else snprintf(s->msg, sizeof s->msg, "Download and install %s?", e->name);
    }
    if (what == C_REMOVE) snprintf(s->msg, sizeof s->msg, "Remove %s from this console?", e->name);
    if (what == C_APPLY) snprintf(s->msg, sizeof s->msg, "Use %s now?\nEmulationStation restarts to load it.", e->name);
}

void store_update(Store *s, const Input *in)
{
    s->frame++;
    if (s->pid) {
        if (s->job == J_INSTALL && s->job_file[0]) { struct stat st; long b = stat(s->job_file, &st) == 0 ? (long)st.st_size : s->job_bytes; if (b != s->job_bytes) { s->job_bytes = b; } }
        int ok;
        if (job_poll(s, &ok)) finish_job(s, ok);
        if (s->mode == M_BUSY || s->job == J_CATALOG) s->redraw = 1;
    }
    if (s->mode == M_BUSY) {
        if (in->b && s->pid && (s->job == J_INSTALL || s->job == J_CATALOG || s->job == J_SIZE)) job_cancel(s);
        s->redraw = 1;
        return;
    }
    int idx = s->nvis ? s->vis[s->sel] : -1;
    if (s->mode == M_MSG) { if (in->a || in->b) { s->mode = M_LIST; s->redraw = 1; } return; }
    if (s->mode == M_CONFIRM) {
        if (in->b) { s->mode = M_LIST; s->redraw = 1; }
        if (in->a) {
            s->redraw = 1;
            int k = -1;
            for (int i = 0; i < s->n; i++) if (!strcmp(s->e[i].repo, s->apply_name)) k = i;
            if (k < 0) { s->mode = M_LIST; return; }
            if (s->confirm == C_INSTALL) start_install(s, k);
            else if (s->confirm == C_REMOVE) start_remove(s, k);
            else {                                      /* "use": remember it, apply when the store is closed */
                if (is_active_name(s, s->e[k].repo)) { s->pending[0] = 0; snprintf(s->msg, sizeof s->msg, "%s is already the theme in use.", s->e[k].name); }
                else { snprintf(s->pending, sizeof s->pending, "%s", s->e[k].repo); snprintf(s->msg, sizeof s->msg, "%s will be applied when you close the Theme Store.", s->e[k].name); }
                s->mode = M_MSG;
            }
        }
        return;
    }
    if (in->up) move_sel(s, -1);
    if (in->down) move_sel(s, 1);
    if (in->l || in->left) move_sel(s, -9);
    if (in->r || in->right) move_sel(s, 9);
    if (in->y) { s->target = (s->target + 1) % NRES; rebuild(s, idx); s->redraw = 1; }
    if (in->back) { s->filter = (s->filter + 1) % 4; rebuild(s, idx); s->redraw = 1; }
    if (in->start && !s->pid && !s->no_net) { start_catalog(s); s->mode = M_BUSY; snprintf(s->msg, sizeof s->msg, "Updating the theme list"); s->redraw = 1; }
    if (in->b) {
        s->action = ACT_QUIT;
        if (s->pending[0]) { s->action = ACT_APPLY; snprintf(s->apply_name, sizeof s->apply_name, "%s", s->pending); }
    }
    if (idx >= 0) {
        const Entry *e = &s->e[idx];
        if (in->a) {
            if (e->installed) ask(s, C_APPLY, idx);
            else if (s->no_net) { s->mode = M_MSG; snprintf(s->msg, sizeof s->msg, "No network."); }
            else if (e->local) { s->mode = M_MSG; snprintf(s->msg, sizeof s->msg, "This theme is not in the gallery."); }
            else start_size(s, idx);
            s->redraw = 1;
        }
        if (in->x && e->installed) {
            if (!strcmp(e->repo, s->active) || !strcmp(e->repo, s->pending)) { s->mode = M_MSG; snprintf(s->msg, sizeof s->msg, "%s is the theme in use or chosen next. Pick another one first.", e->name); }
            else ask(s, C_REMOVE, idx);
            s->redraw = 1;
        }
    }
    /* the preview of the highlighted theme: wait until the cursor rests, then load it from the cache or fetch it */
    if (idx != s->want_idx) { s->want_idx = idx; s->settle = s->frame; if (s->pv_idx != idx) { pv_free(s); s->redraw = 1; } }
    if (idx >= 0 && s->pv_idx != idx && s->frame - s->settle > 5 && !s->pvfail[idx]) {
        char p[400];
        prev_path(s, idx, p, sizeof p);
        if (exists(p)) { pv_load(s, idx); s->redraw = 1; }
        else if (!s->pid && !s->no_net) {
            char cmd[900];
            snprintf(cmd, sizeof cmd, "curl -fsSL --max-time 30 -o '%s.part' '%s' && mv '%s.part' '%s'", p, s->e[idx].prev, p, p);
            job_start(s, J_PREVIEW, idx, cmd);
            s->redraw = 1;
        }
    }
    if (idx >= 0 && s->pv_idx == idx && !s->pid && !s->no_net && s->frame - s->settle > 12) {      /* idle: fetch the previews around the cursor */
        static const int around[6] = { 1, 2, -1, 3, 4, -2 };
        for (int k = 0; k < 6; k++) {
            int p = s->sel + around[k];
            if (p < 0 || p >= s->nvis) continue;
            int j = s->vis[p];
            char pp[400];
            prev_path(s, j, pp, sizeof pp);
            if (s->pvfail[j] || exists(pp)) continue;
            char cmd[900];
            snprintf(cmd, sizeof cmd, "curl -fsSL --max-time 30 -o '%s.part' '%s' && mv '%s.part' '%s'", pp, s->e[j].prev, pp, pp);
            job_start(s, J_PREVIEW, j, cmd);
            break;
        }
    }
    if (!s->pid && !s->no_net && s->frame == 30) {      /* first run: fetch the fresh list quietly */
        char p[400];
        snprintf(p, sizeof p, "%s/cache/catalog.md", s->dir);
        struct stat st;
        if (stat(p, &st) != 0 || time(NULL) - st.st_mtime > 3 * 86400) start_catalog(s);
    }
}

/* --- drawing --------------------------------------------------------------------------------- */

static void fit_text(Canvas *c, float x, float y, float size, int bold, u32 col, float maxw, const char *s)
{
    char b[96];
    snprintf(b, sizeof b, "%s", s);
    int n = (int)strlen(b);
    while (n > 1 && text_w(size, bold, b) > maxw) { b[--n] = 0; if (n > 2) { b[n - 1] = '.'; b[n - 2] = '.'; } }
    text(c, x, y, size, bold, col, AL_LEFT, b);
}

static u32 res_col(char r) { return r == 'Y' ? OKC : r == 'N' ? BAD : WARN; }

static void chip(Canvas *c, float x, float y, float w, const char *label, char r, int mine)
{
    rrect(c, x, y, w, 24, 12, mine ? col_alpha(res_col(r), 0.30f) : CARD2);
    if (mine) rrect_line(c, x, y, w, 24, 12, 1.6f, res_col(r));
    disc(c, x + 12, y + 12, 3.5f, res_col(r));
    text(c, x + 20, y + 4.5f, 11, 0, mine ? TXT : SUB, AL_LEFT, label);
}

static void spinner(Canvas *c, float cx, float cy, int frame)
{
    for (int i = 0; i < 10; i++) {
        float a = (frame * 0.18f) - i * 0.5f;
        disc(c, cx + cosf(a) * 15, cy + sinf(a) * 15, 3.6f - i * 0.28f, col_alpha(ACC, 1.0f - i * 0.09f));
    }
}

typedef struct { float W, lx, lw, cx, cw; } Layout;      /* the screen is split in a list and a card; wide screens give the list the extra width */

static Layout layout_of(const Canvas *c)
{
    Layout l;
    l.W = c->dw;
    l.cw = l.W > 700 ? 360 : 328;
    l.lx = 24;
    l.lw = l.W - 24 - 14 - 24 - l.cw;
    l.cx = l.lx + l.lw + 14;
    return l;
}

/* --- button hints: A B X Y as round buttons, Select and Start as small pills ----------------- */

typedef struct { const char *key, *label; } Hint;

static float hint_gw(const char *key) { return key[1] ? text_w(9, 1, key) + 16 : 20; }
static float hint_w(const Hint *h) { return hint_gw(h->key) + 6 + text_w(12, 0, h->label); }

static u32 key_col(const char *k)
{
    return k[0] == 'A' && !k[1] ? OKC : k[0] == 'B' && !k[1] ? BAD : k[0] == 'X' && !k[1] ? ACC : k[0] == 'Y' && !k[1] ? WARN : SUB;
}

static void hint_draw(Canvas *c, float x, float y, const Hint *h)       /* y is the top of a 20 unit row */
{
    float gw = hint_gw(h->key);
    if (!h->key[1]) {
        u32 col = key_col(h->key);
        shadow(c, x + 1, y + 2, 18, 18, 9, 3, RGBA(0, 0, 0, 120));
        disc(c, x + 10, y + 10, 10, col_mix(col, RGB(0, 0, 0), 0.25f));
        disc(c, x + 10, y + 9.2f, 9.2f, col);
        ring(c, x + 10, y + 9.2f, 9.0f, 1.0f, RGBA(255, 255, 255, 70));
        text(c, x + 10, y + 3.2f, 12, 1, RGB(14, 16, 32), AL_CENTER, h->key);
    } else {
        shadow(c, x, y + 3, gw, 16, 8, 3, RGBA(0, 0, 0, 120));
        rrect_grad(c, x, y + 1, gw, 18, 9, CARD2, RGB(26, 30, 52));
        rrect_line(c, x, y + 1, gw, 18, 9, 1.1f, col_alpha(SUB, 0.8f));
        text(c, x + gw / 2, y + 4.2f, 9, 1, TXT, AL_CENTER, h->key);
    }
    text(c, x + gw + 6, y + 3.2f, 12, 0, TXT, AL_LEFT, h->label);
}

static void hints_row(Canvas *c, float cx, float y, const Hint *h, int n)
{
    float total = 0;
    for (int i = 0; i < n; i++) total += hint_w(&h[i]) + (i ? 16 : 0);
    float x = cx - total / 2;
    for (int i = 0; i < n; i++) { hint_draw(c, x, y, &h[i]); x += hint_w(&h[i]) + 16; }
}

static void modal(Canvas *c, const char *msg, const char *detail, const Hint *hints, int nh, int busy, int frame)
{
    float x0 = (c->dw - 460) / 2, cx = c->dw / 2;
    char l1[100], l2[100] = "";
    snprintf(l1, sizeof l1, "%s", msg);
    char *nl = strchr(l1, '\n');
    if (nl) { *nl = 0; snprintf(l2, sizeof l2, "%s", nl + 1); }
    if (detail && detail[0]) snprintf(l2, sizeof l2, "%s", detail);
    rrect(c, 0, 0, c->dw, DH, 0, RGBA(5, 6, 14, 190));
    shadow(c, x0, 170, 460, 150, 18, 24, RGBA(0, 0, 0, 160));
    rrect_grad(c, x0, 170, 460, 150, 18, CARD2, CARD);
    rrect_line(c, x0, 170, 460, 150, 18, 1.2f, col_alpha(ACC, 0.5f));
    if (busy) spinner(c, x0 + 40, 235, frame);
    float tx = x0 + (busy ? 70 : 24), tw = busy ? 370 : 412;
    fit_text(c, tx, 205, 17, 1, TXT, tw, l1);
    if (l2[0]) fit_text(c, tx, 234, 13, 0, SUB, tw, l2);
    else if (busy) text(c, tx, 234, 13, 0, SUB, AL_LEFT, "Please wait...");
    if (nh) hints_row(c, cx, 280, hints, nh);
}

static void pill(Canvas *c, float right, float y, const char *t, u32 fill, u32 ink)
{
    float tw = text_w(10, 1, t) + 16;
    rrect(c, right - tw, y, tw, 18, 9, fill);
    text(c, right - tw / 2, y + 3.2f, 10, 1, ink, AL_CENTER, t);
}

static int is_active(const Store *s, const Entry *e) { return s->active[0] && !strcasecmp(e->repo, s->active); }
static int is_next(const Store *s, const Entry *e) { return s->pending[0] && !strcasecmp(e->repo, s->pending); }

static u32 *layer;                                       /* the parts that never change, painted once */
static int layer_w, layer_h;

static void draw_static(Canvas *c)
{
    Layout L = layout_of(c);
    rrect_grad(c, -c->ox / c->s, -c->oy / c->s, c->w / c->s, c->h / c->s, 0, BG2, BG1);
    text(c, 24, 14, 22, 1, TXT, AL_LEFT, "Theme Store");
    rrect(c, 24, 46, L.W - 48, 24, 12, CARD);
    rrect(c, L.lx, 80, L.lw, 340, 16, CARD);
    rrect(c, L.cx, 80, L.cw, 340, 16, CARD);
    rrect(c, 24, 430, L.W - 48, 34, 17, CARD);
}

void store_draw(const Store *s, Canvas *c)
{
    if (!layer || layer_w != c->w || layer_h != c->h) {
        free(layer);
        layer = malloc((size_t)c->w * c->h * 4);
        layer_w = c->w; layer_h = c->h;
        draw_static(c);
        memcpy(layer, c->px, (size_t)c->w * c->h * 4);
    } else memcpy(c->px, layer, (size_t)c->w * c->h * 4);
    Layout L = layout_of(c);
    char buf[160];
    int ninst = 0;
    for (int i = 0; i < s->n; i++) ninst += s->e[i].installed;
    snprintf(buf, sizeof buf, "%d of %d themes  -  %d installed", s->nvis, s->n, ninst);
    text(c, L.W - 24, 20, 13, 0, SUB, AL_RIGHT, buf);
    if (s->job == J_CATALOG || (s->pid && s->job == J_PREVIEW && s->job_idx == s->want_idx))
        text(c, 190, 22, 11, 0, SUB, AL_LEFT, s->job == J_CATALOG ? "updating list..." : "loading preview...");

    /* screen, filter and the theme in use */
    if (s->filter == 3) snprintf(buf, sizeof buf, "Showing %s", FILTER_NAME[s->filter]);
    else snprintf(buf, sizeof buf, "Screen %s  -  %s", RES_NAME[s->target], FILTER_NAME[s->filter]);
    text(c, 38, 50.5f, 12, 0, ACC, AL_LEFT, buf);
    const char *inuse = s->active;
    for (int i = 0; i < s->n; i++) if (is_active(s, &s->e[i])) inuse = s->e[i].name;
    snprintf(buf, sizeof buf, "In use: %s", inuse[0] ? inuse : "unknown");
    for (int i = 0; i < s->n; i++) if (is_next(s, &s->e[i])) snprintf(buf + strlen(buf), sizeof buf - strlen(buf), "   -   Next: %s", s->e[i].name);
    text(c, L.W - 38, 50.5f, 12, 1, OKC, AL_RIGHT, buf);

    /* list */
    float rw = L.lw - 16;                               /* row width */
    if (!s->nvis) text(c, L.lx + L.lw / 2, 230, 13, 0, SUB, AL_CENTER, s->filter == 3 ? "No installed themes" : "No themes for this filter");
    for (int row = 0; row < 9; row++) {
        int pos = s->top + row;
        if (pos >= s->nvis) break;
        const Entry *e = &s->e[s->vis[pos]];
        int on = pos == s->sel, act = is_active(s, e), nxt = is_next(s, e);
        float y = 88 + row * 37.5f;
        if (on) {
            shadow(c, L.lx + 8, y, rw, 33, 12, 8, RGBA(0, 0, 0, 90));
            rrect_grad(c, L.lx + 8, y, rw, 33, 12, ACC, ACC2);
        }
        u32 ink = RGB(12, 14, 30), tc = on ? ink : TXT;
        float right = L.lx + L.lw - 34, pw = 0;
        if (nxt) { pill(c, right, y + 7.5f, "NEXT", on ? RGBA(12, 14, 30, 200) : col_alpha(WARN, 0.25f), WARN); pw = text_w(10, 1, "NEXT") + 24; }
        else if (act) { pill(c, right, y + 7.5f, "IN USE", on ? RGBA(12, 14, 30, 200) : col_alpha(OKC, 0.25f), OKC); pw = text_w(10, 1, "IN USE") + 24; }
        else if (e->installed) { pill(c, right, y + 7.5f, "INSTALLED", on ? RGBA(12, 14, 30, 60) : col_alpha(SUB, 0.22f), on ? ink : SUB); pw = text_w(10, 1, "INSTALLED") + 24; }
        fit_text(c, L.lx + 20, y + 8, 14, on, tc, rw - 48 - pw, e->name);
        if (!e->local) disc(c, L.lx + L.lw - 22, y + 16.5f, 4.2f, res_col(e->res[s->target]));
    }
    if (s->nvis > 9) {                                  /* scroll bar */
        float h = 320.f * 9 / s->nvis, y = 90 + (320.f - h) * s->top / (s->nvis - 9 > 0 ? s->nvis - 9 : 1);
        rrect(c, L.lx + L.lw - 6, y, 3, h, 1.5f, col_alpha(ACC, 0.55f));
    }

    /* the card of the highlighted theme */
    int cur = s->nvis ? s->vis[s->sel] : -1;
    if (cur >= 0) {
        const Entry *e = &s->e[cur];
        float bx = L.cx + 12, by = 92, bw = L.cw - 24, bh = 190;
        rrect(c, bx, by, bw, bh, 12, RGB(10, 12, 22));
        if (s->pv && s->pv_idx == cur) {
            float k = bw / s->pw, h = s->ph * k;
            if (h > bh) { k = bh / s->ph; h = bh; }
            float w = s->pw * k;
            image(c, s->pv, s->pw, s->ph, bx + (bw - w) / 2, by + (bh - h) / 2, w, h, 10, 1.0f);
        } else {
            text(c, bx + bw / 2, by + bh / 2 - 8, 13, 0, SUB, AL_CENTER, s->pvfail[cur] ? "No preview available" : "Loading preview...");
        }
        fit_text(c, bx, 290, 18, 1, TXT, bw - 96, e->name);
        char au[64];
        author_of(e, au, sizeof au);
        if (e->local) snprintf(buf, sizeof buf, "Installed here, not in the gallery");
        else snprintf(buf, sizeof buf, "by %s", au);
        text(c, bx, 314, 12, 0, SUB, AL_LEFT, buf);
        if (is_next(s, e)) pill(c, bx + bw, 292, "NEXT", col_alpha(WARN, 0.25f), WARN);
        else if (is_active(s, e)) pill(c, bx + bw, 292, "IN USE", col_alpha(OKC, 0.25f), OKC);
        else if (e->installed) pill(c, bx + bw, 292, "INSTALLED", col_alpha(SUB, 0.22f), SUB);
        if (!e->local) {
            float x = bx;
            for (int i = 0; i < NRES; i++) {
                float w = 20 + text_w(11, 0, RES_NAME[i]) + 8;
                chip(c, x, i < 3 ? 340 : 370, w, RES_NAME[i], e->res[i], i == s->target);
                x += w + 6;
                if (i == 2) x = bx;
            }
            text(c, bx, 398, 11, 0, SUB, AL_LEFT, "green works   yellow untested   red does not");
        }
    }

    /* the buttons that do something right now */
    Hint hs[8];
    int nh = 0;
    if (cur >= 0) {
        const Entry *e = &s->e[cur];
        hs[nh++] = (Hint){ "A", e->installed ? "Use" : "Install" };
        if (e->installed && !is_active(s, e)) hs[nh++] = (Hint){ "X", "Remove" };
    }
    hs[nh++] = (Hint){ "Y", "Screen" };
    hs[nh++] = (Hint){ "SELECT", "Filter" };
    hs[nh++] = (Hint){ "START", "Update list" };
    hs[nh++] = (Hint){ "B", s->pending[0] ? "Exit and apply" : "Exit" };
    hints_row(c, L.W / 2, 437, hs, nh);

    if (s->mode == M_BUSY) {
        char detail[100] = "";
        if (s->job == J_INSTALL) {
            long el = (long)(time(NULL) - s->job_t0);
            long bps = el > 2 ? s->job_bytes / el : 0;
            const Entry *e = &s->e[s->job_idx];
            char sz[48] = "";
            if (e->size_kb > 0) { char t[32]; fmt_size(t, sizeof t, e->size_kb); snprintf(sz, sizeof sz, " of up to %s", t); }
            if (bps > 0) snprintf(detail, sizeof detail, "%.1f MB%s  -  %ld KB/s", s->job_bytes / 1048576.0, sz, bps / 1024);
            else snprintf(detail, sizeof detail, "%.1f MB%s", s->job_bytes / 1048576.0, sz);
        }
        Hint cancel[1] = { { "B", "Cancel" } };
        modal(c, s->msg, detail, cancel, s->job == J_REMOVE ? 0 : 1, 1, s->frame);
    }
    if (s->mode == M_CONFIRM) { Hint yn[2] = { { "A", "Yes" }, { "B", "No" } }; modal(c, s->msg, NULL, yn, 2, 0, s->frame); }
    if (s->mode == M_MSG) { Hint ok[1] = { { "A", "OK" } }; modal(c, s->msg, NULL, ok, 1, 0, s->frame); }
}
