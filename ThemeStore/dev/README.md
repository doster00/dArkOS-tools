# Development tools (Theme Store)

Not needed to use the app. These build outside the console (the WSL / PC has no SDL2 headers; they use the software renderer only):

- `headless.c`: renders every screen to a PNG: `cc -I../themestore/src headless.c ../themestore/src/ui.c ../themestore/src/store.c -o headless -lm -ldl` then `./headless <app dir> <themes dir> <out dir>` (put a few `prev_<repo>.img` previews in `<app dir>/cache`).
- `nettest.c`: a real list / preview / install / cancel / remove cycle without a screen, on a console: `nettest APP_DIR THEMES_ROOT REPO_NAME [CANCEL_REPO]`.
- `bench.c`: how long one frame takes.
