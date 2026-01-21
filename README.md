# README – Build Instructions

## Overview
This project is a C++ chess engine with an SFML 2.6.x GUI.
It will NOT compile against SFML 3.x.
Make sure you are using SFML 2.6.x on all platforms.

## BUILDING ON macOS (Apple Silicon / Intel)

### Requirements

macOS
clang++
SFML 2.6.x (built from source or installed locally)

#### Important
Homebrew installs SFML 3.x by default.
This codebase uses SFML 2.6.x APIs, so you must NOT link against SFML 3.x.
Recommended setup
Build SFML 2.6.2 locally into ~/.local/sfml-2.6.2
#### Compile command
```bash
clang++ -O2 -std=c++17 main.cpp -o gui -I"$HOME/.local/sfml-2.6.2/include" -L"$HOME/.local/sfml-2.6.2/lib" -lsfml-graphics -lsfml-window -lsfml-system -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo -pthread -Wl,-rpath,"$HOME/.local/sfml-2.6.2/lib"
```
Run
```bash
./gui
```

If you get missing dylib or freetype errors, it means the runtime linker cannot find SFML’s bundled frameworks. Re-check the rpath and that SFML was installed correctly.

## BUILDING ON Fedora / Linux

### Requirements
g++ or clang++
SFML 2.6.x development packages
pkg-config
#### Install dependencies (Fedora example)
```bash
sudo dnf install sfml-devel pkg-config
```

#### Compile command
```bash
g++ -O2 -std=c++17 main.cpp -o gui $(pkg-config --cflags --libs sfml-graphics sfml-window sfml-system) -pthread

```
Run
```bash
./gui
```
## NOTES

Default AI search depth is defined in main():
int aiMaxDepth = 8;

AI search runs on a worker thread to avoid UI freezes.

Board flipping is visual only and does not affect game logic.

Assets required:
assets/pieces_png/white_.png
assets/pieces_png/black_.png

Fonts are loaded dynamically from common Linux/macOS paths.
If no font loads, text will not render but the game will still run.

