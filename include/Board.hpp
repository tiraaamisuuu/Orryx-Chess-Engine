#ifndef BOARD_HPP
#define BOARD_HPP

#include <array>
#include <string>
#include <iostream>

class Board {
  friend class AIPlayer; // AIPlayer can now access private members
public:
    Board();                                // Constructor: sets up initial board, player, and last move
    void display() const;                   // Print board in terminal with current player and last move

    // Apply a move in format "e2e4". Returns true if move was legal and applied.
    bool makeMove(const std::string &move);

    // Accessor for current player (useful for main / checking game state)
    char getCurrentPlayer() const { return currentPlayer; }

    // Small accessor for lastMove for UI
    std::string getLastMove() const { return lastMove; }

    // Check utilities
    bool isInCheck(char color) const;       // true if 'W' or 'B' king is under attack
    bool isCheckmate(char color) const;     // true if that colour is checkmated
    bool isStalemate(char color) const;     // true if that colour has no legal moves but not in check

    // Return the piece at (x, y)
    char getSquare(int x, int y) const { return squares[y][x]; }
    // Check if a move is valid
    bool isMoveValid(const std::string &move) const { return validateMove(move).empty(); }

private:
    std::array<std::array<char, 8>, 8> squares; // 8x8 board; '.' is empty
    char currentPlayer;                     // 'W' for White (uppercase pieces), 'B' for Black (lowercase)
    std::string lastMove;                   // Stores the last move in format "e2e4"

    // Castling/move tracking
    bool whiteKingMoved = false;
    bool blackKingMoved = false;
    // rook moved flags: [0] = a-file rook (a1/a8), [1] = h-file rook (h1/h8)
    bool whiteRookMoved[2] = { false, false };
    bool blackRookMoved[2] = { false, false };

    // En-passant target square: if no target, enPassantX/Y == -1
    int enPassantX = -1;
    int enPassantY = -1;

    // Move validation with detailed error messages
    std::string validateMove(const std::string &move) const;

    // A lower-level checker that verifies piece movement & capture rules but ignores player-turn,
    // used to enumerate pseudo-legal moves when testing for checkmate/stalemate.
    bool isPseudoLegalMove(int fromX, int fromY, int toX, int toY) const;

    // Per-piece movement rules (these inspect the board and piece at from-square)
    bool isValidPawnMove(int fromX, int fromY, int toX, int toY) const;
    bool isValidRookMove(int fromX, int fromY, int toX, int toY) const;
    bool isValidBishopMove(int fromX, int fromY, int toX, int toY) const;
    bool isValidKnightMove(int fromX, int fromY, int toX, int toY) const;
    bool isValidQueenMove(int fromX, int fromY, int toX, int toY) const;
    bool isValidKingMove(int fromX, int fromY, int toX, int toY) const;

    // Helpers
    int fileToX(char file) const;           // 'a' -> 0, 'h' -> 7
    int rankToY(char rank) const;           // '1' -> 7, '8' -> 0  (board row mapping)
    bool isPathClear(int fromX, int fromY, int toX, int toY) const; // for sliding pieces
    bool isCorrectPlayerMove(char piece) const; // checks piece belongs to current player

    // Attack/check utilities:
    bool isSquareAttacked(int x, int y, bool byWhite) const;

    // Check simulation helper
    bool wouldLeaveKingInCheck(int fromX, int fromY, int toX, int toY) const;

    // When searching for legal moves we need to test moves by applying them and undoing them.
    bool hasAnyLegalMove(char color) const;
};

#endif
