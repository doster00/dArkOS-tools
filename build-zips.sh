#!/bin/bash
# Makes the release zips. Build the binaries first on a dArkOS console (make themestore / make displaytuner) and copy them next to the sources:
#   ThemeStore/themestore/themestore      DisplayTuner/displaytuner/displaytuner
# usage: ./build-zips.sh [output-dir]
set -e
here="$(cd "$(dirname "$0")" && pwd)"
out="${1:-$here/release}"
mkdir -p "$out"
rm -f "$out"/ThemeStore-tools.zip "$out"/DisplayTuner-tools.zip
( cd "$here/ThemeStore" && zip -qr -9 "$out/ThemeStore-tools.zip" "Theme Store.sh" themestore -x "themestore/cache/*" "themestore/log.txt" "themestore/apply.txt" )
( cd "$here/DisplayTuner" && zip -qr -9 "$out/DisplayTuner-tools.zip" "Display Tuner.sh" displaytuner -x "displaytuner/log.txt" "displaytuner/install.txt" )
( cd "$out" && sha256sum ThemeStore-tools.zip DisplayTuner-tools.zip > SHA256SUMS.txt && cat SHA256SUMS.txt )
