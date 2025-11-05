#include "MainWindow.hpp"
#include <QTimer>
#include <QLineEdit>
#include <future>
#include <algorithm>
#include <iostream>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      boardWidget_(new ChessBoardWidget()),
      debugPanel_(new QTextEdit()),
      startButton_(new QPushButton("Start / Reset")),
      resetButton_(new QPushButton("Reset")),
      moveButton_(new QPushButton("Play Move")),
      moveInput_(new QLineEdit()),
      modeBox_(new QComboBox()),
      statusLabel_(new QLabel()),
      aiPlySpin_(new QSpinBox()),
      aiWhite_(nullptr),
      aiBlack_(nullptr),
      currentPlayer_('W'),
      aiPlyWhite_(4),
      aiPlyBlack_(4),
      mode_(HumanVsAI)
{
    QWidget *central = new QWidget();
    setCentralWidget(central);
    // Lock the window size 
    // Force fixed size based on board + side panel
    int boardSize = 800; // or whatever size you want for the chessboard
    int sidePanelWidth = 200;
    setFixedSize(boardSize + sidePanelWidth, boardSize);

    debugPanel_->setReadOnly(true);

    // --- Mode selection ---
    modeBox_->addItem("Human vs AI");
    modeBox_->addItem("Player vs Player");
    modeBox_->addItem("AI vs AI");
    modeBox_->setCurrentIndex(0);

    aiPlySpin_->setRange(1, 6);
    aiPlySpin_->setValue(aiPlyBlack_);

    // --- Layout ---
    QVBoxLayout *right = new QVBoxLayout();
    right->addWidget(modeBox_);
    right->addWidget(new QLabel("AI search depth:"));
    right->addWidget(aiPlySpin_);
    right->addWidget(startButton_);
    right->addWidget(resetButton_);
    right->addWidget(moveInput_);
    right->addWidget(moveButton_);
    right->addWidget(statusLabel_);
    right->addWidget(debugPanel_);

    QHBoxLayout *mainLayout = new QHBoxLayout(central);
    mainLayout->addWidget(boardWidget_, 1);
    mainLayout->addLayout(right);

    // --- Connect signals ---
    connect(startButton_, &QPushButton::clicked, this, &MainWindow::onStartClicked);
    connect(resetButton_, &QPushButton::clicked, this, &MainWindow::onResetClicked);
    connect(moveButton_, &QPushButton::clicked, this, &MainWindow::onMoveClicked);

    // ✅ NEW: click-to-move connection
    connect(boardWidget_, &ChessBoardWidget::userMoveRequested, this, [this](const QString &mv) {
        moveInput_->setText(mv);
        onMoveClicked();
    });

    updateBoardDisplay();
    appendDebug("Ready. Mode: Human vs AI by default.");
}

MainWindow::~MainWindow() {
    delete aiWhite_;
    delete aiBlack_;
}

void MainWindow::onStartClicked() {
    int idx = modeBox_->currentIndex();
    mode_ = static_cast<Mode>(idx + 1);
    appendDebug(QString("Game started. Mode: %1").arg(idx + 1));

    engineBoard_ = Board(); // reset board
    currentPlayer_ = 'W';
    moveHistory_.clear();

    aiPlyWhite_ = aiPlySpin_->value();
    aiPlyBlack_ = aiPlySpin_->value();

    delete aiWhite_;
    delete aiBlack_;
    aiWhite_ = new AIPlayer('W', aiPlyWhite_);
    aiBlack_ = new AIPlayer('B', aiPlyBlack_);

    updateBoardDisplay();

    if (mode_ == AIVsAI)
        startAIVsAIGame();
}

void MainWindow::onResetClicked() {
    engineBoard_ = Board(); // reset board
    currentPlayer_ = 'W';
    moveHistory_.clear();
    updateBoardDisplay();
    statusLabel_->setText("Game reset.");

    if (mode_ == AIVsAI)
        QTimer::singleShot(1000, this, [this]() { startAIVsAIGame(); });
}

void MainWindow::onMoveClicked() {
    if (mode_ == AIVsAI) return;

    QString moveText = moveInput_->text().trimmed();
    if (moveText.isEmpty()) return;

    std::string move = moveText.toStdString();

    if (!isLegalMove(move, currentPlayer_)) {
        statusLabel_->setText("Invalid move!");
        return;
    }

    engineBoard_.makeMove(move);
    moveHistory_.push_back(moveText);
    updateBoardDisplay();
    currentPlayer_ = (currentPlayer_ == 'W' ? 'B' : 'W');
    statusLabel_->setText(QString::fromStdString("Player moved: " + move));

    // If AI's turn
    if (mode_ == HumanVsAI && currentPlayer_ == 'B') {
        QTimer::singleShot(500, this, &MainWindow::makeAIMove);
    }
}

bool MainWindow::isLegalMove(const std::string &move, char player) {
    AIPlayer evaluator(player);
    auto moves = evaluator.generateAllLegalMoves(engineBoard_, player);
    return std::find(moves.begin(), moves.end(), move) != moves.end();
}

void MainWindow::makeAIMove() {
    AIPlayer *ai = (currentPlayer_ == 'W') ? aiWhite_ : aiBlack_;
    if (!ai) {
        statusLabel_->setText("No AI for this side!");
        return;
    }

    appendDebug(QString("AI (%1) is thinking...").arg(currentPlayer_));

    Board snapshot = engineBoard_;
    std::future<std::string> aiFuture = std::async(std::launch::async, [ai, snapshot]() mutable {
        return ai->findBestMove(snapshot);
    });

    QTimer *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, [this, aiFuture = std::move(aiFuture), timer]() mutable {
        if (aiFuture.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
            std::string mv = aiFuture.get();
            timer->stop();
            timer->deleteLater();

            if (!mv.empty() && isLegalMove(mv, currentPlayer_)) {
                engineBoard_.makeMove(mv);
                moveHistory_.push_back(QString::fromStdString(mv));
                updateBoardDisplay();
                appendDebug(QString("AI (%1) played: %2").arg(currentPlayer_).arg(QString::fromStdString(mv)));
                statusLabel_->setText(QString::fromStdString(
                    std::string("AI (") + currentPlayer_ + ") played " + mv));
                currentPlayer_ = (currentPlayer_ == 'W' ? 'B' : 'W');

                if (mode_ == AIVsAI)
                    QTimer::singleShot(500, this, &MainWindow::makeAIMove);
            } else {
                appendDebug("AI has no legal moves (checkmate or stalemate).");
                statusLabel_->setText("AI has no legal moves.");
            }
        }
    });
    timer->start(100);
}

void MainWindow::updateBoardDisplay() {
    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 8; ++x)
            boardWidget_->setSquareChar(x, y, engineBoard_.getSquare(x, y));

    boardWidget_->setLastMove(QString::fromStdString(engineBoard_.getLastMove()));

    AIPlayer evaler('W');
    double score = evaler.evaluateBoard(engineBoard_);
    boardWidget_->setEval(score);
}

void MainWindow::startAIVsAIGame() {
    delete aiWhite_;
    delete aiBlack_;
    aiWhite_ = new AIPlayer('W', aiPlyWhite_);
    aiBlack_ = new AIPlayer('B', aiPlyBlack_);
    currentPlayer_ = 'W';
    statusLabel_->setText("AI vs AI started!");
    appendDebug("AI vs AI simulation in progress...");

    QTimer::singleShot(500, this, &MainWindow::makeAIMove);
}

void MainWindow::appendDebug(const QString &text) {
    debugPanel_->append(text);
}
