// AIPlayer.cpp — improved evaluation, quiescence, MVV-LVA ordering, iterative deepening
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
#include <unordered_map>

AIPlayer::AIPlayer(char color, int maxDepth_)
  : playerColor(color), maxDepth(maxDepth_), lastMoveFrom("") {
    std::srand(static_cast<unsigned int>(std::time(nullptr)));
    totalThinkingTime = 0.0;
    movesCount = 0;
}

// piece values (const method)
double AIPlayer::pieceValue(char piece) const {
    switch (std::toupper(static_cast<unsigned char>(piece))) {
        case 'P': return 1.0;
        case 'N': return 3.1;
        case 'B': return 3.3;
        case 'R': return 5.0;
        case 'Q': return 9.0;
        case 'K': return 200.0;
    }
    return 0.0;
}

// --- piece-square tables (white perspective). Flip vertically for black ---
static const int PST_P[8][8] = {
    {  0,  0,  0,  0,   0,  0,  0,  0},
    { 50, 50, 50, 50,  50, 50, 50, 50},
    { 10, 10, 20, 30,  30, 20, 10, 10},
    {  5,  5, 10, 25,  25, 10,  5,  5},
    {  0,  0,  0, 20,  20,  0,  0,  0},
    {  5, -5,-10,  0,   0,-10, -5,  5},
    {  5, 10, 10,-20, -20, 10, 10,  5},
    {  0,  0,  0,   0,   0,  0,  0,  0}
};
static const int PST_N[8][8] = {
    {-50,-40,-30,-30,-30,-30,-40,-50},
    {-40,-20,  0,  5,  5,  0,-20,-40},
    {-30,  5, 10, 15, 15, 10,  5,-30},
    {-30,  0, 15, 20, 20, 15,  0,-30},
    {-30,  5, 15, 20, 20, 15,  5,-30},
    {-30,  0, 10, 15, 15, 10,  0,-30},
    {-40,-20,  0,  0,  0,  0,-20,-40},
    {-50,-40,-30,-30,-30,-30,-40,-50}
};
static const int PST_B[8][8] = {
    {-20,-10,-10,-10,-10,-10,-10,-20},
    {-10,  0,  0,  0,  0,  0,  0,-10},
    {-10,  0,  5, 10, 10,  5,  0,-10},
    {-10,  5,  5, 10, 10,  5,  5,-10},
    {-10,  0, 10, 10, 10, 10,  0,-10},
    {-10, 10, 10, 10, 10, 10, 10,-10},
    {-10,  5,  0,  0,  0,  0,  5,-10},
    {-20,-10,-10,-10,-10,-10,-10,-20}
};
static const int PST_R[8][8] = {
    {  0,  0,  0,  0,  0,  0,  0,  0},
    {  5, 10, 10, 10, 10, 10, 10,  5},
    { -5,  0,  0,  0,  0,  0,  0, -5},
    { -5,  0,  0,  0,  0,  0,  0, -5},
    { -5,  0,  0,  0,  0,  0,  0, -5},
    { -5,  0,  0,  0,  0,  0,  0, -5},
    { -5,  0,  0,  0,  0,  0,  0, -5},
    {  0,  0,  0,  5,  5,  0,  0,  0}
};
static const int PST_Q[8][8] = {
    {-20,-10,-10, -5, -5,-10,-10,-20},
    {-10,  0,  0,  0,  0,  0,  0,-10},
    {-10,  0,  5,  5,  5,  5,  0,-10},
    { -5,  0,  5,  5,  5,  5,  0, -5},
    {  0,  0,  5,  5,  5,  5,  0, -5},
    {-10,  5,  5,  5,  5,  5,  0,-10},
    {-10,  0,  5,  0,  0,  0,  0,-10},
    {-20,-10,-10, -5, -5,-10,-10,-20}
};
static const int PST_K[8][8] = {
    {-30,-40,-40,-50,-50,-40,-40,-30},
    {-30,-40,-40,-50,-50,-40,-40,-30},
    {-30,-40,-40,-50,-50,-40,-40,-30},
    {-30,-40,-40,-50,-50,-40,-40,-30},
    {-20,-30,-30,-40,-40,-30,-30,-20},
    {-10,-20,-20,-20,-20,-20,-20,-10},
    { 20, 20,  0,  0,  0,  0, 20, 20},
    { 20, 30, 10,  0,  0, 10, 30, 20}
};

// Helper: read PST value for piece at (x,y) — PSTs are white perspective
static int pstValue(char piece, int x, int y) {
    int row = y; // y = 0..7 (top = 0)
    int col = x;
    bool white = std::isupper(static_cast<unsigned char>(piece));
    int v = 0;
    switch (std::toupper(static_cast<unsigned char>(piece))) {
        case 'P': v = PST_P[row][col]; break;
        case 'N': v = PST_N[row][col]; break;
        case 'B': v = PST_B[row][col]; break;
        case 'R': v = PST_R[row][col]; break;
        case 'Q': v = PST_Q[row][col]; break;
        case 'K': v = PST_K[row][col]; break;
        default: v = 0; break;
    }
    // If piece is black, flip vertically and invert sign (we'll apply sign later)
    if (!white) {
        int flippedRow = 7 - row;
        switch (std::toupper(static_cast<unsigned char>(piece))) {
            case 'P': v = PST_P[flippedRow][col]; break;
            case 'N': v = PST_N[flippedRow][col]; break;
            case 'B': v = PST_B[flippedRow][col]; break;
            case 'R': v = PST_R[flippedRow][col]; break;
            case 'Q': v = PST_Q[flippedRow][col]; break;
            case 'K': v = PST_K[flippedRow][col]; break;
            default: v = 0; break;
        }
    }
    return v;
}

// Public evaluator (const so GUI/Board can call without changing AI)
double AIPlayer::evaluateBoard(const Board &board) const {
    double score = 0.0;

    int mobilityMy = 0, mobilityOpp = 0;
    // Count material, PST and mobility
    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            char p = board.getSquare(x, y);
            if (p == '.') continue;

            double val = pieceValue(p);
            bool isWhitePiece = std::isupper(static_cast<unsigned char>(p));
            bool isMine = (playerColor == 'W') ? isWhitePiece : !isWhitePiece;

            // material
            score += isMine ? val : -val;

            // PST: small position bonus (scaled down)
            double pst = pstValue(p, x, y) * 0.01; // convert from int table to fraction
            score += isMine ? pst : -pst;
        }
    }

    // Mobility: number of legal moves for each side (cheap approximate)
    // We use board.validateMove / isMoveValid generator to count quickly
    mobilityMy = generateAllLegalMoves(const_cast<Board&>(board), playerColor).size();
    char opp = (playerColor == 'W') ? 'B' : 'W';
    mobilityOpp = generateAllLegalMoves(const_cast<Board&>(board), opp).size();
    score += 0.05 * (mobilityMy - mobilityOpp);

    // King safety: penalize if own king is in check
    bool myInCheck = board.isInCheck(playerColor);
    bool oppInCheck = board.isInCheck( (playerColor == 'W') ? 'B' : 'W' );
    if (myInCheck) score -= 2.0;
    if (oppInCheck) score += 0.5;

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

// Generate capture moves only (for quiescence)
std::vector<std::string> AIPlayer::generateCaptures(Board &board, char color) const {
    std::vector<std::string> captures;
    for (int fromY = 0; fromY < 8; ++fromY) {
        for (int fromX = 0; fromX < 8; ++fromX) {
            char piece = board.getSquare(fromX, fromY);
            if (piece == '.') continue;
            if ((color == 'W' && !std::isupper(static_cast<unsigned char>(piece))) ||
                (color == 'B' && !std::islower(static_cast<unsigned char>(piece)))) continue;

            for (int toY = 0; toY < 8; ++toY) {
                for (int toX = 0; toX < 8; ++toX) {
                    char dest = board.getSquare(toX, toY);
                    if (dest == '.') continue; // only captures
                    std::string mv = std::string() + char('a' + fromX) + char('8' - fromY)
                                     + char('a' + toX) + char('8' - toY);
                    if (board.isMoveValid(mv)) captures.push_back(mv);
                }
            }
        }
    }
    return captures;
}

// MVV-LVA comparator (Most Valuable Victim - Least Valuable Attacker)
static double mvv_lva_score(const std::string &mv, const Board &board) {
    char victim = board.getSquare(mv[2] - 'a', '8' - mv[3]);
    char attacker = board.getSquare(mv[0] - 'a', '8' - mv[1]);
    double vVal = 0.0, aVal = 0.0;
    switch (std::toupper(static_cast<unsigned char>(victim))) {
        case 'P': vVal = 1.0; break;
        case 'N': vVal = 3.1; break;
        case 'B': vVal = 3.3; break;
        case 'R': vVal = 5.0; break;
        case 'Q': vVal = 9.0; break;
        case 'K': vVal = 1000.0; break;
        default: vVal = 0.0; break;
    }
    switch (std::toupper(static_cast<unsigned char>(attacker))) {
        case 'P': aVal = 1.0; break;
        case 'N': aVal = 3.1; break;
        case 'B': aVal = 3.3; break;
        case 'R': aVal = 5.0; break;
        case 'Q': aVal = 9.0; break;
        case 'K': aVal = 1000.0; break;
        default: aVal = 0.0; break;
    }
    // Higher is better: victim value * 100 - attacker value
    return vVal * 100.0 - aVal;
}

// Quiescence search (captures-only) to avoid horizon effects
double AIPlayer::quiescence(Board &board, double alpha, double beta, char color) {
    double stand_pat = evaluateBoard(board);
    if (stand_pat >= beta) return beta;
    if (alpha < stand_pat) alpha = stand_pat;

    std::vector<std::string> captures = generateCaptures(board, color);
    // order captures by MVV-LVA descending
    std::sort(captures.begin(), captures.end(), [&](const std::string &a, const std::string &b){
        return mvv_lva_score(a, board) > mvv_lva_score(b, board);
    });

    for (const auto &mv : captures) {
        Board copy = board;
        copy.makeMove(mv);
        char nextColor = (color == 'W') ? 'B' : 'W';
        double score = -quiescence(copy, -beta, -alpha, nextColor);
        if (score >= beta) return beta;
        if (score > alpha) alpha = score;
    }
    return alpha;
}

// alpha-beta with transposition table (TT)
// board is modified by copy/makeMove in callers; this function treats its Board& as mutable
double AIPlayer::alphaBeta(Board &board, int depth, double alpha, double beta, bool maximizing) {
    // If depth == 0, do quiescence search instead of raw eval
    if (depth == 0) {
        char colorToMove = maximizing ? playerColor : (playerColor == 'W' ? 'B' : 'W');
        return quiescence(board, alpha, beta, colorToMove);
    }

    // TT lookup (basic)
    std::string key = boardKey(board);
    auto it = tt.find(key);
    if (it != tt.end() && it->second.depth >= depth) {
        return it->second.value;
    }

    char color = maximizing ? playerColor : (playerColor == 'W' ? 'B' : 'W');
    std::vector<std::string> moves = generateAllLegalMoves(board, color);

    // No legal moves: mate/stalemate handling
    if (moves.empty()) {
        if (board.isInCheck(color)) {
            // Checkmate for the side to move => very bad for them
            double mateScore = (maximizing ? -1.0 : 1.0) * 10000.0;
            return mateScore;
        } else {
            // Stalemate
            return 0.0;
        }
    }

    // Move ordering: captures first by MVV-LVA, then prefer moves that increase mobility
    std::sort(moves.begin(), moves.end(), [&](const std::string &a, const std::string &b) {
        char at = board.getSquare(a[2] - 'a', '8' - a[3]);
        char bt = board.getSquare(b[2] - 'a', '8' - b[3]);
        double av = (at == '.') ? 0.0 : pieceValue(at);
        double bv = (bt == '.') ? 0.0 : pieceValue(bt);
        if (av != bv) return av > bv; // captures of higher value victim first
        // tie-breaker: prefer moves that capture, then lexicographic stable order
        return a < b;
    });

    double best = maximizing ? -std::numeric_limits<double>::infinity()
                             :  std::numeric_limits<double>::infinity();

    for (const std::string &mv : moves) {
        Board copy = board; // copy current position
        copy.makeMove(mv);

        // Negamax-style sign flip: unify evaluation direction
        double val = alphaBeta(copy, depth - 1, alpha, beta, !maximizing);
        // Note: no additional random bias here — keep play deterministic & sensible

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

        // pre-order top-level moves: MVV-LVA
        std::sort(legalMoves.begin(), legalMoves.end(), [&](const std::string &a, const std::string &b) {
            char at = board.getSquare(a[2] - 'a', '8' - a[3]);
            char bt = board.getSquare(b[2] - 'a', '8' - b[3]);
            double av = (at == '.') ? 0.0 : pieceValue(at);
            double bv = (bt == '.') ? 0.0 : pieceValue(bt);
            if (av != bv) return av > bv;
            return a < b;
        });

        for (const std::string &mv : legalMoves) {
            Board copy = board;
            copy.makeMove(mv);

            double val = alphaBeta(copy, depth - 1,
                                   -std::numeric_limits<double>::infinity(),
                                    std::numeric_limits<double>::infinity(),
                                   false);

            // small capture bonus (top-level) to prefer winning material
            char captured = board.getSquare(mv[2] - 'a', '8' - mv[3]);
            if (captured != '.') val += pieceValue(captured) * 0.45;

            // minor preference to centralising moves (if you want)
            int fx = mv[0] - 'a', fy = '8' - mv[1];
            int tx = mv[2] - 'a', ty = '8' - mv[3];
            if ((tx == 3 || tx == 4) && (ty == 3 || ty == 4)) val += 0.02;

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
