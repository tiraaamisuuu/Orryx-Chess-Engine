#include "AIPlayer.hpp"
#include "Board.hpp"

#include <vector>
#include <cstdlib>
#include <ctime>
#include <cctype>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <chrono>
#include <limits>

AIPlayer::AIPlayer(char color, int maxDepth_)
  : playerColor(color), maxDepth(maxDepth_), lastMoveFrom("") {
    std::srand(static_cast<unsigned int>(std::time(nullptr)));
}

// piece values (const method)
double AIPlayer::pieceValue(char piece) const {
    switch (std::toupper(static_cast<unsigned char>(piece))) {
        case 'P': return 1.0;
        case 'N': return 3.0;
        case 'B': return 3.0;
        case 'R': return 5.0;
        case 'Q': return 9.0;
        case 'K': return 1000.0;
    }
    return 0.0;
}

// Public evaluator (const so GUI/Board can call without changing AI)
double AIPlayer::evaluateBoard(const Board &board) const {
    double score = 0.0;

    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            char p = board.getSquare(x, y);
            if (p == '.') continue;

            double val = pieceValue(p);
            // is piece same colour as this AI?
            bool isMine = (playerColor == 'W') ? std::isupper(static_cast<unsigned char>(p))
                                               : std::islower(static_cast<unsigned char>(p));

            // material contribution
            score += isMine ? val : -val;

            // small central control bonus
            if ((x == 3 || x == 4) && (y == 3 || y == 4)) score += isMine ? 0.15 : -0.15;

            // threat bonus: if piece can legally capture an enemy piece, give small bonus
            for (int ty = 0; ty < 8; ++ty) {
                for (int tx = 0; tx < 8; ++tx) {
                    char target = board.getSquare(tx, ty);
                    if (target == '.') continue;
                    bool targetIsEnemy = (playerColor == 'W')
                                            ? std::islower(static_cast<unsigned char>(target))
                                            : std::isupper(static_cast<unsigned char>(target));
                    if (isMine && targetIsEnemy) {
                        std::string attempt = std::string() + char('a' + x) + char('8' - y)
                                              + char('a' + tx) + char('8' - ty);
                        if (board.isMoveValid(attempt)) {
                            score += pieceValue(target) * 0.35;
                        }
                    } else if (!isMine && !targetIsEnemy) {
                        std::string attempt = std::string() + char('a' + x) + char('8' - y)
                                              + char('a' + tx) + char('8' - ty);
                        if (board.isMoveValid(attempt)) {
                            score -= pieceValue(target) * 0.35;
                        }
                    }
                }
            }
        }
    }

    // convert to AI's perspective: positive = good for AI
    return (playerColor == 'W') ? score : -score;
}

// A tiny zobrist-free board key: ASCII squares + side-to-move
std::string AIPlayer::boardKey(const Board &board) const {
    std::string k; k.reserve(65);
    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 8; ++x)
            k.push_back(board.getSquare(x, y));
    k.push_back(board.getCurrentPlayer());
    return k;
}

// Generate legal moves by brute force using board.isMoveValid
std::vector<std::string> AIPlayer::generateAllLegalMoves(Board &board, char color) const {
    std::vector<std::string> moves;
    for (int fromY = 0; fromY < 8; ++fromY) {
        for (int fromX = 0; fromX < 8; ++fromX) {
            char piece = board.getSquare(fromX, fromY);
            if (piece == '.') continue;
            if ((color == 'W' && !std::isupper(static_cast<unsigned char>(piece))) ||
                (color == 'B' && !std::islower(static_cast<unsigned char>(piece)))) continue;

            for (int toY = 0; toY < 8; ++toY) {
                for (int toX = 0; toX < 8; ++toX) {
                    std::string mv = std::string() + char('a' + fromX) + char('8' - fromY)
                                     + char('a' + toX) + char('8' - toY);
                    if (board.isMoveValid(mv)) moves.push_back(mv);
                }
            }
        }
    }
    return moves;
}

// alpha-beta with transposition table (TT)
// board is modified by copy/makeMove in callers; this function treats its Board& as mutable
double AIPlayer::alphaBeta(Board &board, int depth, double alpha, double beta, bool maximizing) {
    if (depth == 0) {
        return evaluateBoard(board);
    }

    // TT lookup
    std::string key = boardKey(board);
    auto it = tt.find(key);
    if (it != tt.end() && it->second.depth >= depth) {
        return it->second.value;
    }

    char color = maximizing ? playerColor : (playerColor == 'W' ? 'B' : 'W');
    std::vector<std::string> moves = generateAllLegalMoves(board, color);
    if (moves.empty()) {
        // no legal moves -> evaluate for checkmate / stalemate situations handled elsewhere
        return evaluateBoard(board);
    }

    // Move ordering: prefer captures (value of captured piece descending)
    std::sort(moves.begin(), moves.end(), [&](const std::string &a, const std::string &b) {
        char at = board.getSquare(a[2] - 'a', '8' - a[3]);
        char bt = board.getSquare(b[2] - 'a', '8' - b[3]);
        double av = (at == '.') ? 0.0 : pieceValue(at);
        double bv = (bt == '.') ? 0.0 : pieceValue(bt);
        return av > bv;
    });

    double best = maximizing ? -std::numeric_limits<double>::infinity()
                             :  std::numeric_limits<double>::infinity();

    for (const std::string &mv : moves) {
        Board copy = board; // copy current position
        copy.makeMove(mv);
        double val = alphaBeta(copy, depth - 1, alpha, beta, !maximizing);

        // small tactical bump for captures
        char captured = board.getSquare(mv[2] - 'a', '8' - mv[3]);
        if (captured != '.') {
            val += (maximizing ? 1.0 : -1.0) * pieceValue(captured) * 0.25;
        }

        if (maximizing) {
            if (val > best) best = val;
            alpha = std::max(alpha, val);
        } else {
            if (val < best) best = val;
            beta = std::min(beta, val);
        }

        if (beta <= alpha) break; // prune
    }

    // store in TT
    tt[key] = { best, depth };
    return best;
}


// Iterative deepening driver (non-const)
std::string AIPlayer::findBestMove(Board board) {
    using clock = std::chrono::high_resolution_clock;
    auto t0 = clock::now();

    std::vector<std::string> legalMoves = generateAllLegalMoves(board, playerColor);
    if (legalMoves.empty()) return "";

    double baseScore = evaluateBoard(board);
    std::string bestOverall = legalMoves.front();
    double bestOverallScore = -std::numeric_limits<double>::infinity();

    // Iterative deepening from 1..maxDepth
    for (int depth = 1; depth <= maxDepth; ++depth) {
        std::string bestAtDepth = "";
        double bestScoreAtDepth = -std::numeric_limits<double>::infinity();

        // order top-level moves by capture value
        std::sort(legalMoves.begin(), legalMoves.end(), [&](const std::string &a, const std::string &b) {
            char at = board.getSquare(a[2] - 'a', '8' - a[3]);
            char bt = board.getSquare(b[2] - 'a', '8' - b[3]);
            double av = (at == '.') ? 0.0 : pieceValue(at);
            double bv = (bt == '.') ? 0.0 : pieceValue(bt);
            return av > bv;
        });

        for (const std::string &mv : legalMoves) {
            Board copy = board;
            copy.makeMove(mv);

            double val = alphaBeta(copy, depth - 1,
                                   -std::numeric_limits<double>::infinity(),
                                    std::numeric_limits<double>::infinity(),
                                   false);

            // promotion/capture top-level bonus
            char captured = board.getSquare(mv[2] - 'a', '8' - mv[3]);
            if (captured != '.') val += pieceValue(captured) * 0.4;

            // slight randomness to diversify play
            char movingPiece = board.getSquare(mv[0] - 'a', '8' - mv[1]);
            double bias = 0.0;
            switch (std::toupper(static_cast<unsigned char>(movingPiece))) {
                case 'P': bias = ((std::rand() % 100) < 18) ? 0.12 : 0.0; break;
                case 'N': bias = ((std::rand() % 100) < 12) ? 0.16 : 0.0; break;
                case 'B': bias = ((std::rand() % 100) < 8)  ? 0.16 : 0.0; break;
                case 'R': bias = ((std::rand() % 100) < 5)  ? 0.20 : 0.0; break;
                case 'Q': bias = ((std::rand() % 100) < 3)  ? 0.25 : 0.0; break;
                case 'K': bias = -0.9; break;
            }
            val += bias;

            if (val > bestScoreAtDepth) {
                bestScoreAtDepth = val;
                bestAtDepth = mv;
            }
        }

        if (!bestAtDepth.empty()) {
            bestOverall = bestAtDepth;
            bestOverallScore = bestScoreAtDepth;
        }

        std::cout << "[ID] depth=" << depth << " best=" << bestAtDepth
                  << " score=" << std::fixed << std::setprecision(2) << bestScoreAtDepth << "\n";
    }

    auto t1 = clock::now();
    std::chrono::duration<double> elapsed = t1 - t0;
    totalThinkingTime += elapsed.count();
    movesCount++;

    if (!bestOverall.empty()) lastMoveFrom = bestOverall.substr(0, 2);

    std::cout << "\n========== AI DEBUG INFO ==========\n";
    std::cout << "AI Colour: " << (playerColor == 'W' ? "White" : "Black") << "\n";
    std::cout << "Base Eval: " << std::fixed << std::setprecision(2) << baseScore << "\n";
    std::cout << "Chosen Move: " << bestOverall << "   (depth " << maxDepth << ")\n";
    std::cout << "Eval (post-search): " << std::fixed << std::setprecision(2) << bestOverallScore << "\n";
    std::cout << "Thinking Time: " << (elapsed.count() * 1000.0) << " ms\n";
    std::cout << "Average per Move: " << ((movesCount > 0) ? (totalThinkingTime / movesCount * 1000.0) : 0.0) << " ms\n";
    std::cout << "===================================\n\n";

    return bestOverall;
}
