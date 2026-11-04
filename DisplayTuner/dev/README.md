# Development tools (Display Tuner)

Not needed to use the app.

- `headless.c`: renders the screens to PNG, with a software emulation of what the display tables would do to the whole picture: `cc -I../displaytuner/src headless.c ../displaytuner/src/{ui,lut,scene,studio}.c -o headless -lm -ldl` then `./headless <app dir> <out dir>`.

Test hooks of the app itself: `DISPLAYTUNER_SHOT=file.bmp` draws one frame and quits, `DISPLAYTUNER_TEST_SAVE=1` makes that quit count as a Save (used to test the install of the boot service without a pad),
`DISPLAYTUNER_NOHW=1` pretends the screen cannot be adjusted, `DISPLAYTUNER_WINDOWED=1` runs in a window. `displaytuner --reset` puts the screen back to neutral.
