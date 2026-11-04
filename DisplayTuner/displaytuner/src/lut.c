#include "lut.h"
#include <dlfcn.h>
#include <fcntl.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define PANEL_LUT "/etc/panel-lut/current.lut"

static float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }

void adj_default(Adjust *a) { a->gamma = 1.0f; a->bright = 0; a->contrast = 100; a->sat = 100; a->warm = 0; }

int adj_is_neutral(const Adjust *a)
{
    return fabsf(a->gamma - 1.0f) < 0.001f && a->bright == 0 && a->contrast == 100 && a->sat == 100 && a->warm == 0;
}

/* --- the transform ---------------------------------------------------------------------------- */

static float curve(const Adjust *a, int ch, float x)         /* what the 1D table does to one channel */
{
    float y = powf(x, 1.0f / a->gamma);                      /* above 1.0 brighter, below darker */
    y = (y - 0.5f) * (a->contrast / 100.0f) + 0.5f + a->bright / 100.0f;
    float w = a->warm / 50.0f;                               /* warm: more red, less blue */
    if (ch == 0) y *= 1.0f + 0.20f * w;
    else if (ch == 2) y *= 1.0f - 0.20f * w;
    return clampf(y, 0, 1);
}

static void sat_rgb(float s, float c[3])                     /* the 3D table: saturation around the luma */
{
    float l = 0.2126f * c[0] + 0.7152f * c[1] + 0.0722f * c[2];
    for (int k = 0; k < 3; k++) c[k] = clampf(l + (c[k] - l) * s, 0, 1);
}

void adj_apply_rgb(const Adjust *a, float rgb[3])
{
    sat_rgb(a->sat / 100.0f, rgb);
    for (int k = 0; k < 3; k++) rgb[k] = curve(a, k, rgb[k]);
}

/* --- the panel's own LUT (RG43H: a warmer white point), kept under the saturation --------------- */

static uint16_t panel[729 * 4];
static int panel_state = -1;                                 /* -1 not looked at, 0 none, 1 loaded */

static int panel_load(void)
{
    if (panel_state >= 0) return panel_state;
    panel_state = 0;
    FILE *f = fopen(PANEL_LUT, "rb");
    if (!f) return 0;
    if (fread(panel, 1, sizeof panel, f) == sizeof panel) panel_state = 1;
    fclose(f);
    return panel_state;
}

static void panel_sample(const float in[3], float out[3])    /* trilinear lookup in the 9x9x9 cube, result 0..1 */
{
    float f[3]; int i0[3]; float t[3];
    for (int k = 0; k < 3; k++) { f[k] = clampf(in[k], 0, 1) * 8; i0[k] = (int)f[k]; if (i0[k] > 7) i0[k] = 7; t[k] = f[k] - i0[k]; }
    out[0] = out[1] = out[2] = 0;
    for (int c = 0; c < 8; c++) {
        int dr = c & 1, dg = (c >> 1) & 1, db = (c >> 2) & 1;
        float w = (dr ? t[0] : 1 - t[0]) * (dg ? t[1] : 1 - t[1]) * (db ? t[2] : 1 - t[2]);
        int idx = (i0[0] + dr) + (i0[1] + dg) * 9 + (i0[2] + db) * 81;
        for (int k = 0; k < 3; k++) out[k] += w * panel[idx * 4 + k] / 4095.0f;
    }
}

/* --- settings file ----------------------------------------------------------------------------- */

int adj_load(Adjust *a, const char *path)
{
    adj_default(a);
    FILE *f = fopen(path, "r");
    if (!f) return 1;
    char line[96];
    while (fgets(line, sizeof line, f)) {
        float v;
        if (sscanf(line, "GAMMA=%f", &v) == 1) a->gamma = clampf(v, 0.6f, 1.8f);
        else if (sscanf(line, "BRIGHT=%f", &v) == 1) a->bright = (int)clampf(v, -30, 30);
        else if (sscanf(line, "CONTRAST=%f", &v) == 1) a->contrast = (int)clampf(v, 70, 140);
        else if (sscanf(line, "SAT=%f", &v) == 1) a->sat = (int)clampf(v, 0, 200);
        else if (sscanf(line, "WARM=%f", &v) == 1) a->warm = (int)clampf(v, -50, 50);
    }
    fclose(f);
    return 0;
}

int adj_save(const Adjust *a, const char *path)
{
    FILE *f = fopen(path, "w");
    if (!f) return 1;
    fprintf(f, "GAMMA=%.2f\nBRIGHT=%d\nCONTRAST=%d\nSAT=%d\nWARM=%d\n", a->gamma, a->bright, a->contrast, a->sat, a->warm);
    fclose(f);
    return 0;
}

/* --- the display (DRM, through dlopen) ---------------------------------------------------------- */

typedef struct { int count_fbs; uint32_t *fbs; int count_crtcs; uint32_t *crtcs; int count_connectors; uint32_t *connectors;
                 int count_encoders; uint32_t *encoders; uint32_t min_width, max_width, min_height, max_height; } DRes;
typedef struct { uint32_t clock; uint16_t hdisplay, hsync_start, hsync_end, htotal, hskew, vdisplay, vsync_start, vsync_end, vtotal, vscan;
                 uint32_t vrefresh, flags, type; char name[32]; } DMode;
typedef struct { uint32_t crtc_id, buffer_id, x, y, width, height; int mode_valid; DMode mode; int gamma_size; } DCrtc;
typedef struct { uint32_t count_props; uint32_t *props; uint64_t *prop_values; } DObjProps;
typedef struct { uint32_t prop_id, flags; char name[32]; int count_values; uint64_t *values; int count_enums; void *enums;
                 int count_blobs; uint32_t *blob_ids; } DProp;

static struct {
    void *h;
    int (*SetClientCap)(int, uint64_t, uint64_t);
    DRes *(*GetRes)(int); void (*FreeRes)(DRes *);
    DCrtc *(*GetCrtc)(int, uint32_t); void (*FreeCrtc)(DCrtc *);
    DObjProps *(*GetProps)(int, uint32_t, uint32_t); void (*FreeProps)(DObjProps *);
    DProp *(*GetProp)(int, uint32_t); void (*FreeProp)(DProp *);
    int (*CreateBlob)(int, const void *, size_t, uint32_t *); int (*DestroyBlob)(int, uint32_t);
    void *(*AtomicAlloc)(void); void (*AtomicFree)(void *);
    int (*AtomicAdd)(void *, uint32_t, uint32_t, uint64_t); int (*AtomicCommit)(int, void *, uint32_t, void *);
} D;

static struct { int fd; uint32_t crtc, gprop, cprop; int gn; int caps; } hw = { -1 };

static int load_drm(void)
{
    if (D.h) return 1;
    D.h = dlopen("libdrm.so.2", RTLD_NOW);
    if (!D.h) return 0;
#define SYM(field, name) *(void **)&D.field = dlsym(D.h, name); if (!D.field) { D.h = NULL; return 0; }
    SYM(SetClientCap, "drmSetClientCap") SYM(GetRes, "drmModeGetResources") SYM(FreeRes, "drmModeFreeResources")
    SYM(GetCrtc, "drmModeGetCrtc") SYM(FreeCrtc, "drmModeFreeCrtc") SYM(GetProps, "drmModeObjectGetProperties")
    SYM(FreeProps, "drmModeFreeObjectProperties") SYM(GetProp, "drmModeGetProperty") SYM(FreeProp, "drmModeFreeProperty")
    SYM(CreateBlob, "drmModeCreatePropertyBlob") SYM(DestroyBlob, "drmModeDestroyPropertyBlob")
    SYM(AtomicAlloc, "drmModeAtomicAlloc") SYM(AtomicFree, "drmModeAtomicFree") SYM(AtomicAdd, "drmModeAtomicAddProperty")
    SYM(AtomicCommit, "drmModeAtomicCommit")
#undef SYM
    return 1;
}

int hw_init(void)
{
    if (hw.fd >= 0) return hw.caps;
    if (!load_drm()) return 0;
    for (int n = 0; n < 4; n++) {
        char path[32];
        snprintf(path, sizeof path, "/dev/dri/card%d", n);
        int fd = open(path, O_RDWR | O_CLOEXEC);
        if (fd < 0) continue;
        if (D.SetClientCap(fd, 3, 1) != 0) { close(fd); continue; }          /* DRM_CLIENT_CAP_ATOMIC */
        DRes *res = D.GetRes(fd);
        int found = 0;
        for (int i = 0; res && i < res->count_crtcs && !found; i++) {
            uint32_t id = res->crtcs[i];
            DCrtc *c = D.GetCrtc(fd, id);
            if (!c) continue;
            int active = c->mode_valid;
            D.FreeCrtc(c);
            if (!active) continue;
            DObjProps *p = D.GetProps(fd, id, 0xcccccccc);               /* DRM_MODE_OBJECT_CRTC */
            if (!p) continue;
            uint32_t gp = 0, cp = 0; int gn = 0;
            for (uint32_t k = 0; k < p->count_props; k++) {
                DProp *pr = D.GetProp(fd, p->props[k]);
                if (!pr) continue;
                if (!strcmp(pr->name, "GAMMA_LUT")) gp = pr->prop_id;
                else if (!strcmp(pr->name, "GAMMA_LUT_SIZE")) gn = (int)p->prop_values[k];
                else if (!strcmp(pr->name, "CUBIC_LUT")) cp = pr->prop_id;
                D.FreeProp(pr);
            }
            D.FreeProps(p);
            if ((gp && gn > 1) || cp) { hw.fd = fd; hw.crtc = id; hw.gprop = (gp && gn > 1) ? gp : 0; hw.gn = gn; hw.cprop = cp; found = 1; }
        }
        if (res) D.FreeRes(res);
        if (found) { hw.caps = (hw.gprop ? HW_GAMMA : 0) | (hw.cprop ? HW_CUBIC : 0); return hw.caps; }
        close(fd);
    }
    return 0;
}

void hw_close(void) { if (hw.fd >= 0) close(hw.fd); hw.fd = -1; hw.caps = 0; }

int hw_apply(const Adjust *a, int touch_cubic)
{
    if (hw.fd < 0 && !hw_init()) return 1;
    uint32_t gblob = 0, cblob = 0;
    int rc = 0;
    if (hw.gprop) {
        uint16_t *t = malloc((size_t)hw.gn * 8);
        for (int i = 0; i < hw.gn; i++) {
            float x = i / (float)(hw.gn - 1);
            for (int k = 0; k < 3; k++) t[i * 4 + k] = (uint16_t)(curve(a, k, x) * 65535.0f + 0.5f);
            t[i * 4 + 3] = 0;
        }
        if (D.CreateBlob(hw.fd, t, (size_t)hw.gn * 8, &gblob) != 0) gblob = 0;
        free(t);
    }
    if (hw.cprop && touch_cubic) {
        static uint16_t t[729 * 4];
        int pl = panel_load();
        for (int b = 0; b < 9; b++) for (int g = 0; g < 9; g++) for (int r = 0; r < 9; r++) {
            float c[3] = { r / 8.0f, g / 8.0f, b / 8.0f };
            sat_rgb(a->sat / 100.0f, c);
            if (pl) { float o[3]; panel_sample(c, o); c[0] = o[0]; c[1] = o[1]; c[2] = o[2]; }
            int idx = r + g * 9 + b * 81;
            for (int k = 0; k < 3; k++) t[idx * 4 + k] = (uint16_t)(clampf(c[k], 0, 1) * 4095.0f + 0.5f);
            t[idx * 4 + 3] = 0;
        }
        if (D.CreateBlob(hw.fd, t, sizeof t, &cblob) != 0) cblob = 0;
    }
    void *req = D.AtomicAlloc();
    if (!req) rc = 1;
    else {
        int added = 0;
        if (gblob && D.AtomicAdd(req, hw.crtc, hw.gprop, gblob) >= 0) added++;
        if (cblob && D.AtomicAdd(req, hw.crtc, hw.cprop, cblob) >= 0) added++;
        rc = added ? D.AtomicCommit(hw.fd, req, 0, NULL) : 1;
        D.AtomicFree(req);
    }
    if (gblob) D.DestroyBlob(hw.fd, gblob);
    if (cblob) D.DestroyBlob(hw.fd, cblob);
    return rc != 0;
}
