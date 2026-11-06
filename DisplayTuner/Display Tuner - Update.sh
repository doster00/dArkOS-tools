#!/bin/bash
# Display Tuner - Update: atualiza a instalacao do Display Tuner / Color Studio SEM abrir janela (sirva-se disto se um jogo
# de 3DS/PSP, ou ate a tela de Wi-Fi, nao abre depois de usar o Color Studio). Copie junto com "Display Tuner.sh" e a pasta
# "displaytuner" para /roms/tools, abra Tools > Display Tuner - Update, espere a mensagem "Pronto" e volte ao menu.
# Updates an existing Display Tuner / Color Studio install with no window; use it when a 3DS/PSP game, or even the Wi-Fi screen,
# will not open after using Color Studio.
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)"
printf "\033c" > /dev/tty1 2>/dev/null
{
  echo "Display Tuner: atualizando / updating..."
  echo
  bash "$DIR/Display Tuner.sh" --update 2>&1
  echo
  echo "Pronto / Done. Volte ao menu / go back to the menu."
} 2>&1 | tee /dev/tty1
sleep 8
printf "\033c" > /dev/tty1 2>/dev/null
