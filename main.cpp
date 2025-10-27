#include "Board.hpp"
#include "AIPlayer.hpp"
#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <cctype>
#include <vector>
#include <fstream>

int main() {
    Board board;
    AIPlayer aiWhite('W', 5);
    AIPlayer aiBlack('B', 5);

    std::cout << "Select mode:\n";
    std::cout << "1. Player vs Player\n";
    std::cout << "2. Player (White) vs AI (Black)\n";
    std::cout << "3. AI vs AI\n";
    std::cout << "Enter choice: ";

    int mode;
    std::cin >> mode;

    int moveDelayMs = 800; // default AI delay
    if (mode == 3) {
        std::cout << "Enter delay between AI moves (ms): ";
        std::cin >> moveDelayMs;
    }

    int moveCount = 0;
    const int maxMoves = 300; // Hard limit
    std::vector<std::string> moveHistory;

    std::cout << "Press 'q' then Enter at any time to quit.\n";

    while (moveCount < maxMoves) {
        char current = board.getCurrentPlayer();
        std::string move;

        // Human quit detection
        if (std::cin.rdbuf()->in_avail() > 0) {
            char c;
            std::cin >> c;
            if (c == 'q' || c == 'Q') {
                std::cout << "Quitting game...\n";
                break;
            }
        }

        // Display move counter and current player **above board**
        std::cout << "\n--- Move " << (moveCount + 1) << " ---\n";
        std::cout << "Current player: " << (current == 'W' ? "White" : "Black") << "\n";

        auto aiStart = std::chrono::high_resolution_clock::now(); // Timer for AI

        // Determine move based on mode
        if (mode == 1) { // PvP
            std::cout << ((current == 'W') ? "White" : "Black") << " move: ";
            std::cin >> move;
            if (move == "q" || move == "Q") break;
        }
        else if (mode == 2) { // PvAI
            if (current == 'W') {
                std::cout << "Enter move (e.g. e2e4) or 'q' to quit: ";
                std::cin >> move;
                if (move == "q" || move == "Q") break;
            } else {
                move = aiBlack.findBestMove(board);
                if (move.empty()) {
                    std::cout << "AI has no legal moves.\n";
                    break;
                }
            }
        }
        else if (mode == 3) { // AIvAI
            move = (current == 'W') ? aiWhite.findBestMove(board)
                                    : aiBlack.findBestMove(board);
            if (move.empty()) {
                std::cout << "AI (" << current << ") has no legal moves.\n";
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(moveDelayMs));
        }

        auto aiEnd = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = aiEnd - aiStart;

        // Make move
        if (!board.makeMove(move)) {
            std::cout << "Invalid move, try again.\n";
            continue; // Don't increment moveCount
        }
        moveHistory.push_back(move);
        moveCount++;

        // Clear terminal **before showing board**
        std::cout << "\033[2J\033[1;1H";

        // Display board
        board.display();

        // Show AI info under board
        if ((mode == 2 && current == 'B') || mode == 3) {
            std::cout << "\n--- AI INFO ---\n";
            std::cout << "AI (" << (current == 'W' ? "White" : "Black") << ") played: " << move << "\n";
            std::cout << "AI thinking time: " << elapsed.count() * 1000 << " ms\n";
        }

        // Check endgame
        char nextPlayer = board.getCurrentPlayer();
        if (board.isCheckmate(nextPlayer)) {
            std::cout << "Checkmate! "
                      << (nextPlayer == 'W' ? "Black" : "White")
                      << " wins!\n";
            break;
        }
        if (board.isStalemate(nextPlayer)) {
            std::cout << "Stalemate! It's a draw.\n";
            break;
        }
        if (board.isInCheck(nextPlayer)) {
            std::cout << (nextPlayer == 'W' ? "White" : "Black")
                      << " is in check!\n";
        }
    }

    if (moveCount >= maxMoves) {
        std::cout << "\nReached hard move limit of " << maxMoves << " — draw declared.\n";
    }

    // Write move history to file
    std::ofstream outFile("moves.txt");
    for (const auto &m : moveHistory) outFile << m << "\n";
    outFile.close();

    std::cout << "\nGame over after " << moveCount << " moves.\n";
    std::cout << "Move history saved to moves.txt\n";
    return 0;
}
