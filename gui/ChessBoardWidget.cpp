#include "ChessBoardWidget.hpp"
#include <QFont>
#include <QVBoxLayout>
#include <QFrame>
#include <QPalette>
#include <algorithm>

ChessBoardWidget::ChessBoardWidget(QWidget *parent)
    : QWidget(parent), selectedX_(-1), selectedY_(-1), eval_(0.0)
{
    grid_ = new QGridLayout();
    grid_->setSpacing(2);
    QFont font;
    font.setPointSize(20);
    font.setBold(true);

    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            QLabel *lbl = new QLabel(".");
            lbl->setFont(font);
            lbl->setAlignment(Qt::AlignCenter);
            lbl->setFrameStyle(QFrame::Box | QFrame::Plain);
            lbl->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
            bool light = ((x + y) % 2 == 0);
            lbl->setStyleSheet(light ? "background:#EEE;" : "background:#77a;");
            grid_->addWidget(lbl, y, x);
            labels_[y][x] = lbl;
        }
    }

    auto *outer = new QVBoxLayout(this);
    outer->addLayout(grid_);
    setLayout(outer);
}

QSize ChessBoardWidget::sizeHint() const {
    return QSize(600, 600);
}

QSize ChessBoardWidget::minimumSizeHint() const {
    return QSize(400, 400);
}

void ChessBoardWidget::setSquareChar(int x, int y, char piece) {
    if (x < 0 || x > 7 || y < 0 || y > 7) return;
    updateLabel(x, y, piece);
}

void ChessBoardWidget::updateLabel(int x, int y, char piece) {
    QLabel *lbl = labels_[y][x];
    lbl->setText(glyphFor(piece));
}

QString ChessBoardWidget::glyphFor(char piece) {
    switch (piece) {
        case 'P': return QString::fromUtf8("♙");
        case 'p': return QString::fromUtf8("♟");
        case 'R': return QString::fromUtf8("♖");
        case 'r': return QString::fromUtf8("♜");
        case 'N': return QString::fromUtf8("♘");
        case 'n': return QString::fromUtf8("♞");
        case 'B': return QString::fromUtf8("♗");
        case 'b': return QString::fromUtf8("♝");
        case 'Q': return QString::fromUtf8("♕");
        case 'q': return QString::fromUtf8("♛");
        case 'K': return QString::fromUtf8("♔");
        case 'k': return QString::fromUtf8("♚");
        case '.': return ".";
        default:  return "?";
    }
}

void ChessBoardWidget::refreshLastMoveHighlight() {
    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            bool light = ((x + y) % 2 == 0);
            labels_[y][x]->setStyleSheet(light ? "background:#EEE;" : "background:#77a;");
        }
    }
    if (!lastMove_.isEmpty() && lastMove_.length() == 4) {
        int fx = lastMove_[0].toLatin1() - 'a';
        int fy = '8' - lastMove_[1].toLatin1();
        int tx = lastMove_[2].toLatin1() - 'a';
        int ty = '8' - lastMove_[3].toLatin1();
        if (fx >= 0 && fx < 8 && fy >= 0 && fy < 8)
            labels_[fy][fx]->setStyleSheet("background: #ffd27f;");
        if (tx >= 0 && tx < 8 && ty >= 0 && ty < 8)
            labels_[ty][tx]->setStyleSheet("background: #ffd27f;");
    }
}

void ChessBoardWidget::setLastMove(const QString &move) {
    lastMove_ = move;
    refreshLastMoveHighlight();
}

void ChessBoardWidget::setEval(double eval) {
    eval_ = eval;
}

void ChessBoardWidget::mousePressEvent(QMouseEvent *ev) {
    QWidget *w = childAt(ev->pos());
    if (!w) return;

    int selX = -1, selY = -1;
    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 8; ++x)
            if (labels_[y][x] == w) { selX = x; selY = y; }

    if (selX < 0) return;

    if (selectedX_ == -1) {
        selectedX_ = selX;
        selectedY_ = selY;
        labels_[selectedY_][selectedX_]->setStyleSheet("background: yellow;");
    } else {
        QString mv = QString("%1%2%3%4")
                     .arg(QChar('a' + selectedX_))
                     .arg(QChar('8' - selectedY_))
                     .arg(QChar('a' + selX))
                     .arg(QChar('8' - selY));

        bool lightPrev = ((selectedX_ + selectedY_) % 2 == 0);
        labels_[selectedY_][selectedX_]->setStyleSheet(lightPrev ? "background:#EEE;" : "background:#77a;");

        selectedX_ = selectedY_ = -1;
        emit userMoveRequested(mv);
    }
}

void ChessBoardWidget::resizeEvent(QResizeEvent *event) {
    int side = std::min(width(), height());
    resize(side, side);
    QWidget::resizeEvent(event);
}
