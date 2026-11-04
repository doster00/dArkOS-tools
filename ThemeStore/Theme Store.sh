#!/bin/bash
# Theme Store: browse, preview, install and use EmulationStation themes.
# Put this file and the "themestore" folder next to it in your Tools folder (/roms/tools), then open Tools > Theme Store.

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)/themestore"
cd "$DIR" || { echo "Theme Store: the 'themestore' folder must sit next to this script."; sleep 5; exit 1; }
chmod +x ./themestore 2>/dev/null
rm -f apply.txt

# The binary needs glibc 2.38 or newer. On an older system, build it from src/ (needs gcc and libsdl2-dev).
if ldd ./themestore 2>&1 | grep -q "not found"; then
  if command -v gcc >/dev/null 2>&1 && command -v sdl2-config >/dev/null 2>&1; then
    printf "\033c" > /dev/tty1 2>/dev/null
    echo "Theme Store: building for this system, please wait..." > /dev/tty1 2>/dev/null
    make themestore >/dev/null 2>&1
  fi
fi

export SDL_GAMECONTROLLERCONFIG_FILE="/opt/inttools/gamecontrollerdb.txt"
export SDL_GAMECONTROLLER_USE_BUTTON_LABELS=0
export THEMESTORE_THEMES="/roms/themes"
export THEMESTORE_DIR="$DIR"

./themestore 2> "$DIR/log.txt"
printf "\033c" > /dev/tty1 2>/dev/null

# "Use" in the app only marks the theme; it is applied when the app is closed. This script runs inside EmulationStation's
# own service, so stopping ES here would kill it before it could start ES again: hand the restart to a transient unit.
if [ -s apply.txt ]; then
  name="$(head -n1 apply.txt | tr -cd 'A-Za-z0-9_.-')"
  cfg="/home/ark/.emulationstation/es_settings.cfg"
  [ -n "$name" ] && sudo systemd-run --no-block --collect --unit=themestore-apply sh -c "systemctl stop emulationstation; sed -i 's|\(name=\"ThemeSet\" value=\"\)[^\"]*|\1$name|' $cfg; systemctl start emulationstation"
fi
