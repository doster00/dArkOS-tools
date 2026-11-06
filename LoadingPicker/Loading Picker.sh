#!/bin/bash
# Loading Picker: choose the picture shown while a game opens.
# Put this file and the "loadingpicker" folder next to it in your Tools folder (/roms/tools), then open Tools > Loading Picker.
# Your own pictures: drop JPG/PNG files in loadingpicker/gallery. The original picture is kept as loading.jpg.original.

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)/loadingpicker"
cd "$DIR" || { echo "Loading Picker: the 'loadingpicker' folder must sit next to this script."; sleep 5; exit 1; }
chmod +x ./loadingpicker 2>/dev/null

# The binary needs glibc 2.38 or newer. On an older system, build it from src/ (needs gcc and libsdl2-dev).
if ldd ./loadingpicker 2>&1 | grep -q "not found"; then
  if command -v gcc >/dev/null 2>&1 && command -v sdl2-config >/dev/null 2>&1; then
    printf "\033c" > /dev/tty1 2>/dev/null
    echo "Loading Picker: building for this system, please wait..." > /dev/tty1 2>/dev/null
    make loadingpicker >/dev/null 2>&1
  fi
fi

export SDL_GAMECONTROLLERCONFIG_FILE="/opt/inttools/gamecontrollerdb.txt"
export SDL_GAMECONTROLLER_USE_BUTTON_LABELS=0
export LOADINGPICKER_DIR="$DIR"
export LOADINGPICKER_OUT="/roms/launchimages/loading.jpg"

mkdir -p /roms/launchimages 2>/dev/null
./loadingpicker 2> "$DIR/log.txt"
printf "\033c" > /dev/tty1 2>/dev/null
