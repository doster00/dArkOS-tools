# Theme Store

A small, light theme browser and installer for **EmulationStation on dArkOS** handhelds (RK3562 and R36S-class, 64-bit). Browse ~55 themes, see a preview, check which screen
resolutions each one was tested on, install, remove, and choose the one in use, all with the pad.

## Install
Copy **both** to your Tools folder (`/roms/tools/`), side by side:

```
/roms/tools/Theme Store.sh      <- the launcher
/roms/tools/themestore/         <- the app (this folder)
```

Then open **Tools > Theme Store** in EmulationStation (reload the list if it does not show up: Start > Quit > Restart EmulationStation, or Update Games Lists).
It needs Wi-Fi to download the theme list, previews and themes; the list is also bundled, so it opens offline.

## Controls
| Button | Action |
|---|---|
| D-pad / L, R | move / jump 9 |
| A | install, or use the theme if it is installed |
| X | remove an installed theme (not the one in use) |
| Y | change the screen resolution that is checked |
| Select | filter: all / works on this screen / works or untested / installed |
| Start | update the theme list now |
| B | exit (cancels a download in progress). If you chose a theme with **A**, it is applied when you exit and ES restarts |

The list shows **IN USE**, **INSTALLED** and **NEXT** (chosen, applied on exit). Before downloading it asks GitHub for the repository size and tells you how long it may take.

## Where things come from
The theme list is the README table of [Jetup13/Emulationstation-OGA-Theme-Gallery](https://github.com/Jetup13/Emulationstation-OGA-Theme-Gallery) (previews and compatibility per resolution).
A theme is downloaded from its author's GitHub repository (the zip of the default branch) and unpacked into `/roms/themes/<repository name>`. Themes belong to their authors and
have their own licenses; this app does not change them. Themes made for ES-DE do **not** work with this EmulationStation and are not in the list.

## Requirements
- dArkOS (EmulationStation-fcamod) with `curl` and `unzip` (both are in the image), `/opt/inttools/gamecontrollerdb.txt` for the pad.
- The included binary is aarch64 and needs glibc 2.38 or newer (the current dArkOS). On an older system the launcher rebuilds it from `src/` if `gcc` and `libsdl2-dev` are installed
  (`make themestore` in this folder).

## Files
`themestore` (the app), `src/` (source), `Makefile`, `data/catalog.md` (bundled copy of the theme list), `cache/` and `log.txt` (created on first run).

## License
MIT (see `LICENSE`). `stb_image.h` and `stb_truetype.h` are public domain (Sean Barrett). Fonts are not bundled (it uses the system DejaVu fonts).
Made by doster00 together with Claude (Anthropic's AI).

---

## Português
Loja de temas para o EmulationStation do dArkOS. Copie **`Theme Store.sh`** e a pasta **`themestore`** para `/roms/tools/` (lado a lado) e abra **Tools > Theme Store**.
Mostra ~55 temas com prévia e a compatibilidade por resolução; instala, remove e escolhe o tema em uso. **A** instala/usa, **X** remove, **Y** troca a resolução, **Select** muda o filtro,
**Start** atualiza a lista, **B** sai (e aplica o tema escolhido, reiniciando o ES). Precisa de Wi-Fi para baixar. Os temas são dos seus autores; temas do ES-DE não funcionam neste ES.
