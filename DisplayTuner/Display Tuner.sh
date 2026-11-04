#!/bin/bash
# Display Tuner: adjust the screen's gamma, brightness, contrast, saturation and warmth with a reference picture.
# Put this file and the "displaytuner" folder next to it in your Tools folder (/roms/tools), then open Tools > Display Tuner.

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

export SDL_GAMECONTROLLERCONFIG_FILE="/opt/inttools/gamecontrollerdb.txt"
export SDL_GAMECONTROLLER_USE_BUTTON_LABELS=0
export DISPLAYTUNER_DIR="$DIR"
export DISPLAYTUNER_CFG="/home/ark/.config/displaytuner.cfg"

./displaytuner 2> "$DIR/log.txt"
printf "\033c" > /dev/tty1 2>/dev/null

# After a Save the app leaves install.txt: put the helper and the boot service in place, so the adjustment is back after every
# boot and after a game resets the screen (the service re-applies it every few seconds). Nothing is installed before the first Save.
if [ -f install.txt ]; then
  rm -f install.txt
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
  # The older Gamma / Display adjustment tools run a service of their own that would fight this one over the same table.
  if systemctl is-enabled gamma-lut.service > /dev/null 2>&1; then sudo systemctl disable --now gamma-lut.service > /dev/null 2>&1; fi
fi
