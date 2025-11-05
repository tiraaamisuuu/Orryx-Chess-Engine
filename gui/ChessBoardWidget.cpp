#include "ChessBoardWidget.hpp"

#include <QFont>
#include <QVBoxLayout>
#include <QFrame>
#include <QSvgRenderer>
#include <QPainter>
#include <QFile>
#include <QSizePolicy>
#include <algorithm>

/*
  ChessBoardWidget
  -----------------
  - Each square is a QLabel inside a QGridLayout.
  - If an SVG for a piece exists in the Qt resource system, we render it into a QPixmap
    sized to the current square and set that on the label.
  - Empty squares are left blank (no '.' text).
  - We keep the board square by re-calculating sizes on resizeEvent and re-rendering pixmaps.
*/

static QPixmap renderSvgToPixmap(const QString &resourcePath, const QSize &size) {
    QSvgRenderer renderer(resourcePath);
    if (!renderer.isValid()) return QPixmap();

    // Create a pixmap with transparency and render the SVG into it
    QPixmap pixmap(size);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    QRectF targetRect(0, 0, size.width(), size.height());
    renderer.render(&painter, targetRect);
    painter.end();
    return pixmap;
}

static QString svgPathFor(char piece) {
    bool white = std::isupper(static_cast<unsigned char>(piece));
    char lower = static_cast<char>(std::tolower(static_cast<unsigned char>(piece)));
    QString colour = white ? "white" : "black";
    QString name;

    switch (lower) {
        case 'p': name = "pawn";   break;
        case 'r': name = "rook";   break;
        case 'n': name = "knight"; break;
        case 'b': name = "bishop"; break;
        case 'q': name = "queen";  break;
        case 'k': name = "king";   break;
        default:  return QString();
    }

    return QString(":/pieces/%1_%2.svg").arg(colour, name);
}

ChessBoardWidget::ChessBoardWidget(QWidget *parent)
    : QWidget(parent), selectedX_(-1), selectedY_(-1), eval_(0.0)
{
    grid_ = new QGridLayout();
    grid_->setSpacing(0);
    grid_->setContentsMargins(0, 0, 0, 0);

    QFont font;
    font.setPointSize(28);
    font.setBold(true);

    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            QLabel *lbl = new QLabel();
            lbl->setFont(font);
            lbl->setAlignment(Qt::AlignCenter);
            lbl->setFrameStyle(QFrame::Box | QFrame::Plain);
            lbl->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

            bool light = ((x + y) % 2 == 0);
            lbl->setStyleSheet(light ? "background:#EEEEEE;" : "background:#77A0CC;");
            lbl->setText(QString());

            grid_->addWidget(lbl, y, x);
            labels_[y][x] = lbl;
        }
    }

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(6, 6, 6, 6);
    outer->addLayout(grid_);
    setLayout(outer);

    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMinimumSize(minimumSizeHint());
}

QSize ChessBoardWidget::sizeHint() const {
    return QSize(640, 640);
}

QSize ChessBoardWidget::minimumSizeHint() const {
    return QSize(480, 480);
}

void ChessBoardWidget::setSquareChar(int x, int y, char piece) {
    if (x < 0 || x > 7 || y < 0 || y > 7) return;
    updateLabel(x, y, piece);
}

void ChessBoardWidget::updateLabel(int x, int y, char piece) {
    QLabel *lbl = labels_[y][x];
    if (!lbl) return;

    if (piece == '.' || piece == '\0') {
        lbl->setPixmap(QPixmap());
        lbl->setText(QString());
        return;
    }

    QString path = svgPathFor(piece);
    if (!path.isEmpty() && QFile::exists(path)) {
        int squareSize = 800 / 8; // matches board size
        int pad = squareSize / 10;
        QSize renderSize(squareSize - pad, squareSize - pad);

        QPixmap pix = renderSvgToPixmap(path, renderSize);        if (!pix.isNull()) {
            lbl->setPixmap(pix);
            lbl->setScaledContents(false);
            lbl->setText(QString());
            lbl->setAlignment(Qt::AlignCenter);
            return;
        }
    }

    switch (piece) {
        case 'P': lbl->setText(QString::fromUtf8("♙")); break;
        case 'p': lbl->setText(QString::fromUtf8("♟")); break;
        case 'R': lbl->setText(QString::fromUtf8("♖")); break;
        case 'r': lbl->setText(QString::fromUtf8("♜")); break;
        case 'N': lbl->setText(QString::fromUtf8("♘")); break;
        case 'n': lbl->setText(QString::fromUtf8("♞")); break;
        case 'B': lbl->setText(QString::fromUtf8("♗")); break;
        case 'b': lbl->setText(QString::fromUtf8("♝")); break;
        case 'Q': lbl->setText(QString::fromUtf8("♕")); break;
        case 'q': lbl->setText(QString::fromUtf8("♛")); break;
        case 'K': lbl->setText(QString::fromUtf8("♔")); break;
        case 'k': lbl->setText(QString::fromUtf8("♚")); break;
        default:  lbl->setText(QString()); break;
    }
    lbl->setAlignment(Qt::AlignCenter);
}

void ChessBoardWidget::refreshLastMoveHighlight() {
    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            bool light = ((x + y) % 2 == 0);
            labels_[y][x]->setStyleSheet(light ? "background:#EEEEEE;" : "background:#77A0CC;");
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

    if (selectedX_ >= 0 && selectedY_ >= 0 &&
        selectedX_ < 8 && selectedY_ < 8) {
        labels_[selectedY_][selectedX_]->setStyleSheet("background: #fff176;");
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
        refreshLastMoveHighlight();
    } else {
        QString mv = QString("%1%2%3%4")
                     .arg(QChar('a' + selectedX_))
                     .arg(QChar('8' - selectedY_))
                     .arg(QChar('a' + selX))
                     .arg(QChar('8' - selY));

        selectedX_ = selectedY_ = -1;
        refreshLastMoveHighlight();

        emit userMoveRequested(mv);
    }
}

