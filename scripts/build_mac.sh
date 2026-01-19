#!/usr/bin/env bash
set -euo pipefail

g++ -std=c++17 main.cpp -o gui \
  -I/opt/homebrew/include \
  -L/opt/homebrew/lib \
  -lsfml-graphics -lsfml-window -lsfml-system \
  -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo

echo "Built ./gui"
