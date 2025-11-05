#ifndef AIPLAYER_HPP
#define AIPLAYER_HPP

#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>

class Board; // forward declaration

class AIPlayer {
public:
    // Constructor: color ('W' or 'B') and optional max search depth
    AIPlayer(char color, int maxDepth_ = 4);

    // Main entry point: returns the best move as a string
    std::string findBestMove(Board board);

    // Evaluate board from AI's perspective (positive = good for AI)
    double evaluateBoard(const Board &board) const;

    // Adjust search depth at runtime
    void setMaxDepth(int d) { maxDepth = d; }

    // Generate all legal moves for a given board and color
    std::vector<std::string> generateAllLegalMoves(Board &board, char color) const;

    // Generate only capture moves (for quiescence search)
    std::vector<std::string> generateCaptures(Board &board, char color) const;

    // Quiescence search: static evaluation with capture extensions
    double quiescence(Board &board, double alpha, double beta, char color);

private:
    char playerColor;
    int maxDepth;
    std::string lastMoveFrom;

    // Statistics
    double totalThinkingTime = 0.0;
    int movesCount = 0;

    // Transposition table
    struct TTEntry {
        double value;
        int depth;
    };
    std::unordered_map<std::string, TTEntry> tt;

    // Core search: alpha-beta with pruning
    double alphaBeta(Board &board, int depth, double alpha, double beta, bool maximizing);

    // Evaluate piece value
    double pieceValue(char piece) const;

    // Lightweight board key for transposition table
    std::string boardKey(const Board &board) const;
};

#endif // AIPLAYER_HPP
