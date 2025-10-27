#ifndef AIPLAYER_HPP
#define AIPLAYER_HPP

#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>

class Board; // forward decl

class AIPlayer {
public:
    // Construct AI for a colour ('W' or 'B') with a default search depth
    AIPlayer(char color, int maxDepth_ = 4);

    // Primary public entrypoint (safe to pass a Board copy or temporary).
    // The function may modify internal stats/TT so it is non-const.
    std::string findBestMove(Board board);

    // Public evaluator so Board::display() / GUI can call it without accessing private members.
    // This is const: it does not mutate the AI internal state.
    double evaluateBoard(const Board &board) const;

    // Set / change search depth (runtime adjustable)
    void setMaxDepth(int d) { maxDepth = d; }

    std::vector<std::string> generateAllLegalMoves(Board &board, char color) const;

private:
    char playerColor;
    int maxDepth;
    std::string lastMoveFrom;

    // Search stats
    double totalThinkingTime = 0.0;
    int movesCount = 0;

    // Transposition table entry
    struct TTEntry {
        double value;
        int depth;
    };
    std::unordered_map<std::string, TTEntry> tt;

    // Core search helpers
    double alphaBeta(Board &board, int depth, double alpha, double beta, bool maximizing);
    double pieceValue(char piece) const;
    std::string boardKey(const Board &board) const;
};

#endif // AIPLAYER_HPP
