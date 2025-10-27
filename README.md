# ♟️ Orryx

**Orryx** is a modern C++ chess engine with a built-in **Qt graphical interface** and a simple yet effective AI opponent.  
It’s designed for learning, experimentation, and testing AI strategies in chess — now featuring a fully interactive board.

---

## Features

- **Interactive Qt GUI** with clickable moves  
- **Full chessboard representation** with correct Unicode piece glyphs  
- **Move validation**, including castling and pawn promotion  
- **AI evaluation** using material balance and basic positional heuristics  
- Supports **Human vs AI**, **Player vs Player**, and **AI vs AI** modes  
- **Asynchronous AI computation** for smooth gameplay  
- Real-time **evaluation display** and move highlighting  

---

## Setup

### Dependencies

- **C++17** or later  
- **CMake 3.20+**  
- **Qt 6 (Widgets module)**  

---

### Build Instructions

1. **Clone the repository**
   ```bash
   git clone https://github.com/tiraaamisuuu/Orryx-Chess-Engine.git
   cd ChessAI
   ```
2. **Build the project with CMake**
   ```bash
   rm -rf build
   cmake -B build -S .
   cmake --build build -j
   ```
3. **Run either version**
   
   -	Terminal (CLI) version
   ```bash
   ./build/ChessAI
   ```
   
	-	Graphical (GUI) version
   ```bash
   ./build/ChessAI_GUI.app/Contents/MacOS/ChessAI_GUI   # macOS only
   ```
   or simply double-click the app in Finder.

## How to Play (GUI)

- Click on a **piece**, then click on its **destination square** to move.  
- You can also type moves manually (e.g. `e2e4`) in the move box.  
- The **mode selector** allows:
  - **Human vs AI**
  - **Player vs Player**
  - **AI vs AI** (watch two engines battle automatically)  
- The **status area** shows move history, AI depth, and evaluation info.  
- Use **Start / Reset** to begin or restart a game.  

---

## Development Notes

- GUI logic → `gui/MainWindow.cpp` and `gui/ChessBoardWidget.cpp`  
- AI logic → `src/AIPlayer.cpp` and `src/Board.cpp`  
- Build system automatically links both the CLI and GUI versions.  

---

## Contributing

Contributions are welcome!  

You can:
- Improve AI evaluation and move ordering  
- Enhance the GUI  
- Add new modes or features  

Open a pull request or issue on GitHub, let’s make **Orryx** smarter together.  

---

## Future Plans / To-Do List
 
- [ ] Undo / Redo move support  
- [ ] Move hints and visual arrows  
- [ ] Side-swap button (play as black or white)  
- [ ] Timer / clock system for timed games  
- [ ] Save & load game states (PGN or FEN)  
- [ ] Sound effects and move animations  
- [ ] Improved evaluation with positional heuristics  
- [ ] Cross-platform packaging (macOS `.app`, Windows `.exe`, Linux binary)
