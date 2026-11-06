# Loading Picker

Pick the picture shown while a game opens on dArkOS (the file `/roms/launchimages/loading.jpg`).

**Use:** copy `Loading Picker.sh` and the `loadingpicker` folder into `/roms/tools` ("Extract here"), then open **Tools > Loading Picker**.

- **Minhas** tab: pictures in `loadingpicker/gallery` (put your own JPG/PNG there). `X` deletes one from the folder.
- **Online** tab: free photos from [Picsum Photos](https://picsum.photos) (Unsplash licence, no account or key). `Start` shows other photos. A chosen photo is also saved in `gallery`.
- `A` applies: the picture is cropped to the screen shape (the console's own size) and written to `loading.jpg`. The first time, the original is kept as `loading.jpg.original`; `Y` puts it back.
- `L`/`R` change tab, `B` quits.

Games that have their own image (`<system>/images/<game>-marquee.png` and the like) still show that one: this only changes the default. The loading screen must be in picture mode (the default on dArkOS).

Needs internet only for the Online tab. The binary needs glibc 2.38+ (the launcher rebuilds from `src/` with gcc and libsdl2-dev otherwise).

## Português
Escolhe a imagem que aparece ao abrir um jogo. Copie `Loading Picker.sh` e a pasta `loadingpicker` para `/roms/tools` e abra **Tools > Loading Picker**. Aba **Minhas** = suas imagens em `loadingpicker/gallery`; aba **Online** = fotos grátis do Picsum Photos (sem cadastro). `A` aplica (recorta para o formato da tela e guarda a original como `loading.jpg.original`; `Y` restaura), `X` apaga da pasta, `Start` troca as fotos online, `L`/`R` trocam de aba, `B` sai. Jogos com imagem própria continuam mostrando a deles.
