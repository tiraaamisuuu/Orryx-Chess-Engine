#pragma once

#include <QWidget>
#include <QLabel>
#include <QGridLayout>
#include <QMouseEvent>
#include <QString>
#include <QResizeEvent>
#include <array>

class ChessBoardWidget : public QWidget {
    Q_OBJECT
public:
    explicit ChessBoardWidget(QWidget *parent = nullptr);

    // Update UI from Board::squares (call from main thread)
    void setSquareChar(int x, int y, char piece);
    void setLastMove(const QString &move);
    void setEval(double eval); // -inf..inf scaled inside widget

signals:
    // Emitted when user attempts a move (e.g. "e2e4")
    void userMoveRequested(const QString &move);

protected:
    void mousePressEvent(QMouseEvent *ev) override;

    // 🔹 Keep the board square and nicely scaled
    void resizeEvent(QResizeEvent *event) override;
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

private:
    QGridLayout *grid_;
    std::array<std::array<QLabel*, 8>, 8> labels_;
    int selectedX_, selectedY_;
    QString lastMove_;
    double eval_; // Store last eval for optional display

    void updateLabel(int x, int y, char piece);
    static QString glyphFor(char piece);
    void refreshLastMoveHighlight();
};
