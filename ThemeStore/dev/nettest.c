/* Real network test, no screen: refresh the list, fetch a preview, install a theme, remove it again.
 * usage: nettest APP_DIR THEMES_ROOT REPO_NAME */
#include "../src/store.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static Store st;

static void run_until_idle(const char *what)
{
    Input none = { 0 };
    for (int i = 0; i < 4000; i++) {
        store_update(&st, &none);
        if (!st.pid && st.mode != M_BUSY && i > 40) break;
        usleep(20000);
    }
    printf("[%s] mode=%d msg=\"%s\"\n", what, st.mode, st.msg);
}

int main(int argc, char **argv)
{
    if (argc < 4) return 2;
    store_init(&st, argv[2], argv[1], 640, 480);
    printf("catalog: %d themes, screen column %s\n", st.n, RES_NAME[st.target]);
    Input start = { 0 }; start.start = 1;
    store_update(&st, &start);
    run_until_idle("catalog update");
    st.mode = M_LIST;
    int k = -1;
    for (int i = 0; i < st.n; i++) if (!strcmp(st.e[i].repo, argv[3])) k = i;
    if (k < 0) { printf("repo not in the list\n"); return 1; }
    for (int i = 0; i < st.nvis; i++) if (st.vis[i] == k) st.sel = i;
    run_until_idle("preview");
    printf("preview loaded: %s (%dx%d)\n", st.pv && st.pv_idx == k ? "yes" : "no", st.pw, st.ph);
    Input a = { 0 }; a.a = 1;
    store_update(&st, &a);                                 /* ask */
    store_update(&st, &a);                                 /* yes */
    run_until_idle("install");
    printf("installed flag: %d\n", st.e[k].installed);
    st.mode = M_LIST;
    Input x = { 0 }; x.x = 1;
    store_update(&st, &x);
    store_update(&st, &a);
    run_until_idle("remove");
    printf("installed flag after remove: %d\n", st.e[k].installed);
    if (argc > 4) {                                        /* cancel in the middle of a download */
        int c = -1;
        for (int i = 0; i < st.n; i++) if (!strcmp(st.e[i].repo, argv[4])) c = i;
        for (int i = 0; i < st.nvis; i++) if (st.vis[i] == c) st.sel = i;
        st.mode = M_LIST;
        store_update(&st, &a);
        store_update(&st, &a);
        Input none = { 0 };
        for (int i = 0; i < 60; i++) { store_update(&st, &none); usleep(20000); }
        printf("[cancel test] busy=%d bytes=%ld\n", st.mode == M_BUSY, st.job_bytes);
        Input b = { 0 }; b.b = 1;
        store_update(&st, &b);
        printf("[cancel test] mode=%d msg=\"%s\" pid=%d installed=%d\n", st.mode, st.msg, (int)st.pid, st.e[c].installed);
        char cmd[400];
        snprintf(cmd, sizeof cmd, "ls -a '%s' | tr '\n' ' '; echo; ps -eo args | grep -c '[a]rchive/HEAD.zip'", st.root);
        if (system(cmd)) {}
    }
    store_free(&st);
    return 0;
}
