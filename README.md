# Chess Engine (SFML)

C++ chess engine + SFML GUI for A level Computer Science NEA.

## Features
- Full legal chess rules: check legality, castling, en passant, promotion
- GUI: drag-and-drop, legal move highlights, move list, modes (PvP / PvAI / AIvAI)
- Engine: iterative deepening + alpha-beta, quiescence, TT + Zobrist, move ordering

## Build (macOS, Homebrew SFML)
```bash
brew install sfml librsvg
./scripts/svg_to_png.sh
./scripts/build_mac.sh
./gui
