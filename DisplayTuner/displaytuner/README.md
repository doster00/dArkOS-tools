# Display Tuner

Adjust your handheld's screen with a picture to judge by, not a menu of numbers. For **dArkOS** on RK3562 handhelds (RG52 Mini, RG43H/V) and other consoles whose display controller
has gamma and 3D colour tables. It changes the **real screen** (the interface changes colour as you move a slider), saves your setting and brings it back after every boot and after a game.

## Install
Copy **both** to your Tools folder (`/roms/tools/`), side by side:

```
/roms/tools/Display Tuner.sh     <- the launcher
/roms/tools/displaytuner/        <- the app (this folder)
```

Open **Tools > Display Tuner** (reload the list if it does not show up).

## Use
| Button | Action |
|---|---|
| Up / Down | choose a slider |
| Left / Right | adjust it (hold to go faster) |
| A | reset the chosen slider |
| Select | reset everything |
| X (hold) | show the original, unadjusted screen to compare |
| Y | next reference picture |
| L / R | previous / next preset (Natural, Vivid, Warm, Cool, Soft, Cinema, Bright room) |
| Start | **save** and exit |
| B | cancel (the screen goes back to the saved setting) |

Sliders: **Gamma** (midtones lighter or darker), **Brightness**, **Contrast**, **Saturation**, **Warmth** (cooler to warmer).
Look at the picture: you should still tell the darkest gray steps apart, the brightest steps should not melt into white, and skin and grass should look natural.

## Reference pictures
Two are built in and drawn by the program (a colourful platformer scene and a test chart). To use your own, put **PNG or JPG** screenshots in `displaytuner/images/`, for example a frame of a
colourful game you know well, and press Y to cycle to them. Nothing from any game is bundled.

## Saving, and the boot service
The first time you press **Start**, the launcher installs `/usr/local/bin/displaytuner` and a small service (`displaytuner.service`) that applies your setting at boot and re-applies it every
15 seconds, because emulators reset the display tables when they start. A neutral setting touches nothing. The older Gamma / Display adjustment tools run a service of their own; the launcher turns
the old `gamma-lut.service` off so the two do not fight. Your setting is in `/home/ark/.config/displaytuner.cfg`.
**To go back to normal:** open Display Tuner, press Select, then Start. To remove it completely:
`sudo systemctl disable --now displaytuner.service; sudo rm /etc/systemd/system/displaytuner.service /usr/local/bin/displaytuner /home/ark/.config/displaytuner.cfg`.

## How it works
Gamma, brightness, contrast and warmth go in the display's 1D `GAMMA_LUT`; saturation goes in the 3D `CUBIC_LUT`, composed with the panel's own white-point table
(`/etc/panel-lut/current.lut`, present on the RG43H) so that calibration is kept. If the screen cannot be adjusted on your system the app says **PREVIEW ONLY** and changes only the picture.
`displaytuner --reset` puts the screen back to neutral; `--apply` and `--keep` are what the service uses.

## Requirements
dArkOS with `/opt/inttools/gamecontrollerdb.txt`, `sudo`, and `libdrm.so.2` (in the image). The included binary is aarch64 and needs glibc 2.38 or newer (the current dArkOS); on an older system the
launcher rebuilds it from `src/` if `gcc` and `libsdl2-dev` are installed (`make displaytuner`).

## License
MIT (see `LICENSE`). `stb_image.h` and `stb_truetype.h` are public domain (Sean Barrett). Fonts are not bundled (it uses the system DejaVu fonts).
Made by doster00 together with Claude (Anthropic's AI).

---

## Português
Ajuste a tela do console olhando uma imagem de referência, sem menu de números. Copie **`Display Tuner.sh`** e a pasta **`displaytuner`** para `/roms/tools/` (lado a lado) e abra **Tools > Display Tuner**.
Cima/baixo escolhe a barra, esquerda/direita ajusta, **X (segurando)** compara com a tela original, **Y** troca a imagem, **L/R** troca o preset, **Start** salva, **B** cancela, **A** zera a barra, **Select** zera tudo.
Gamma, brilho, contraste, saturação e calor. Ao salvar pela primeira vez, o lançador instala um serviço que reaplica o ajuste no boot e depois de cada jogo. Para usar uma imagem sua, ponha PNG/JPG em `displaytuner/images/`.
