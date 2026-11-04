/* Theme Store: browse the themes of the OGA theme gallery, see a preview, install, remove, use. No SDL in here:
 * store_update takes one frame of input, store_draw paints a frame. Downloads run as child processes (curl). */
#ifndef STORE_H
#define STORE_H
#include "ui.h"
#include <sys/types.h>
#include <time.h>

#define MAX_E 200
#define NRES 5

typedef struct { int up, down, left, right, a, b, x, y, l, r, start, back; } Input;      /* 1 on the frame a button goes down (or repeats) */

typedef struct {
    char name[56], repo[96], url[200], prev[240];
    char res[NRES];                                     /* per resolution: 'Y' works, 'U' untested, 'N' does not */
    int installed, local;                               /* local: a folder in the themes directory that is not in the gallery */
    long size_kb;                                       /* repository size from the GitHub API, 0 = unknown */
} Entry;

enum { M_LIST, M_BUSY, M_CONFIRM, M_MSG };
enum { C_INSTALL, C_REMOVE, C_APPLY };
enum { J_NONE, J_CATALOG, J_PREVIEW, J_INSTALL, J_REMOVE, J_SIZE };
enum { ACT_NONE, ACT_QUIT, ACT_APPLY };

typedef struct {
    Entry e[MAX_E];
    int n;
    int vis[MAX_E], nvis;                               /* entries that pass the filter */
    int sel, top;                                       /* positions in vis */
    int target, filter;                                 /* the screen column we care about; filter: 0 all, 1 works, 2 works or untested, 3 installed */
    int screen_w, screen_h;
    char root[260], dir[260], cfg[260], active[100];
    int mode, confirm, action, frame, redraw, no_net;
    pid_t pid;
    int job, job_idx, job_cat;
    long job_bytes;
    char job_file[320];
    u32 *pv; int pw, ph, pv_idx, want_idx, settle;
    char pvfail[MAX_E];
    char msg[200];
    char apply_name[100];
    char pending[100];                                  /* the theme chosen to be applied when the store is closed */
    int cat_online;                                     /* the list came from a fresh download */
    long speed;                                         /* bytes per second measured on the last big download, 0 = unknown */
    time_t job_t0;
} Store;

void store_init(Store *s, const char *themes_root, const char *app_dir, int screen_w, int screen_h);
void store_update(Store *s, const Input *in);
void store_draw(const Store *s, Canvas *c);
void store_free(Store *s);
extern const char *RES_NAME[NRES];
#endif
