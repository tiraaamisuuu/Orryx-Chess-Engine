#pragma once

#include <QMainWindow>
#include <QTextEdit>
#include <QPushButton>
#include <QComboBox>
#include <QSpinBox>
#include <QLabel>
#include <QTimer>
#include <vector>
#include "ChessBoardWidget.hpp"
#include "../include/Board.hpp"
#include "../include/AIPlayer.hpp"

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onStartClicked();
    void onResetClicked();
    void onMoveClicked();
    void makeAIMove();

private:
    enum Mode { HumanVsAI = 1, PvP = 2, AIVsAI = 3 };

    ChessBoardWidget *boardWidget_;
    QTextEdit *debugPanel_;
    QPushButton *startButton_;
    QPushButton *resetButton_;
    QPushButton *moveButton_;
    QLineEdit *moveInput_;
    QComboBox *modeBox_;
    QLabel *statusLabel_;
    QSpinBox *aiPlySpin_;

    QTimer *thinkingTimer_;
    int thinkingDots_ = 0;
    bool aiThinking_ = false;

    Board engineBoard_;
    AIPlayer *aiWhite_;
    AIPlayer *aiBlack_;
    char currentPlayer_;
    int aiPlyWhite_;
    int aiPlyBlack_;
    Mode mode_;

    std::vector<QString> moveHistory_;

    // Helpers
    void updateBoardDisplay();
    void appendDebug(const QString &text);
    void startAIVsAIGame();
    bool isLegalMove(const std::string &move, char player);
    void startThinking();
    void stopThinking();
};
