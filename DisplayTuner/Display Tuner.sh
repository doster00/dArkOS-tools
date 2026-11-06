#!/bin/bash
# Display Tuner 1.1.0: adjust the screen's gamma, brightness, contrast, saturation and warmth with a reference picture.
# Put this file and the "displaytuner" folder next to it in your Tools folder (/roms/tools), then open Tools > Display Tuner.
# "Display Tuner.sh --update" only brings an existing install up to date (no window), for use over SSH.

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)/displaytuner"
cd "$DIR" || { echo "Display Tuner: the 'displaytuner' folder must sit next to this script."; sleep 5; exit 1; }
chmod +x ./displaytuner 2>/dev/null
rm -f install.txt

# The binary needs glibc 2.38 or newer. On an older system, build it from src/ (needs gcc and libsdl2-dev).
if ldd ./displaytuner 2>&1 | grep -q "not found"; then
  if command -v gcc >/dev/null 2>&1 && command -v sdl2-config >/dev/null 2>&1; then
    printf "\033c" > /dev/tty1 2>/dev/null
    echo "Display Tuner: building for this system, please wait..." > /dev/tty1 2>/dev/null
    make displaytuner >/dev/null 2>&1
  fi
fi

CFG="/home/ark/.config/displaytuner.cfg"
OLDCFG="/home/ark/.config/colorstudio.cfg"

# Put the helper (the program that re-applies the saved adjustment) and its boot service in place.
install_helper() {
  sudo install -m 755 ./displaytuner /usr/local/bin/displaytuner
  sudo tee /etc/systemd/system/displaytuner.service > /dev/null <<'UNIT'
[Unit]
Description=Keep the saved display adjustment applied (Display Tuner)
After=emulationstation.service panel-lut.service

[Service]
Type=simple
Environment=DISPLAYTUNER_CFG=/home/ark/.config/displaytuner.cfg
Environment=APPLY_RETRIES=20
ExecStart=/usr/local/bin/displaytuner --keep
Restart=on-failure

[Install]
WantedBy=multi-user.target
UNIT
  sudo systemctl daemon-reload
  sudo systemctl enable displaytuner.service > /dev/null 2>&1
  sudo systemctl restart displaytuner.service > /dev/null 2>&1
}

# The older tools (Gamma / Display adjustment, and the first "Color Studio") run a service of their own that fights this one over the
# same table. The old Color Studio one is also the reason Vulkan emulators (3DS, PSP...) failed to open: it never let go of the screen.
retire_old_services() {
  if systemctl is-enabled gamma-lut.service > /dev/null 2>&1; then sudo systemctl disable --now gamma-lut.service > /dev/null 2>&1; fi
  if systemctl is-enabled colorstudio.service > /dev/null 2>&1 || systemctl is-active colorstudio.service > /dev/null 2>&1; then
    sudo systemctl disable --now colorstudio.service > /dev/null 2>&1
  fi
}

# Update an install made by an older version (nothing to do on a clean system: nothing is installed before the first Save).
# Version 1.1.0: the helper no longer keeps the screen's DRM master, and the old Color Studio service is retired. The look you had
# in Color Studio is carried over so it does not change.
legacy=0
if systemctl is-enabled colorstudio.service > /dev/null 2>&1 || systemctl is-active colorstudio.service > /dev/null 2>&1; then legacy=1; fi
stale=0
if [ -f /usr/local/bin/displaytuner ] && ! cmp -s ./displaytuner /usr/local/bin/displaytuner; then stale=1; fi
if [ "$legacy" = 1 ] || [ "$stale" = 1 ]; then
  if [ ! -f "$CFG" ] && [ -f "$OLDCFG" ]; then mkdir -p "$(dirname "$CFG")"; cp "$OLDCFG" "$CFG"; fi
  install_helper
  retire_old_services
  echo "Display Tuner: helper updated ($(./displaytuner --version 2>/dev/null))" >> "$DIR/log.txt"
fi
if [ "${1:-}" = "--update" ]; then
  echo "Display Tuner: $(/usr/local/bin/displaytuner --version 2>/dev/null) installed; legacy colorstudio service: $(systemctl is-active colorstudio.service 2>&1)"
  exit 0
fi

export SDL_GAMECONTROLLERCONFIG_FILE="/opt/inttools/gamecontrollerdb.txt"
export SDL_GAMECONTROLLER_USE_BUTTON_LABELS=0
export DISPLAYTUNER_DIR="$DIR"
export DISPLAYTUNER_CFG="$CFG"

./displaytuner 2> "$DIR/log.txt"
printf "\033c" > /dev/tty1 2>/dev/null

# After a Save the app leaves install.txt: put the helper and the boot service in place, so the adjustment is back after every
# boot and after a game resets the screen (the service re-applies it every few seconds). Nothing is installed before the first Save.
if [ -f install.txt ]; then
  rm -f install.txt
  install_helper
  retire_old_services
fi
