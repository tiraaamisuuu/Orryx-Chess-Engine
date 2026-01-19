#!/usr/bin/env bash
set -euo pipefail

mkdir -p assets/pieces_png

for f in assets/pieces/*.svg; do
  base="$(basename "$f" .svg)"
  rsvg-convert -w 512 -h 512 "$f" -o "assets/pieces_png/${base}.png"
done

echo "Done: assets/pieces_png generated."
