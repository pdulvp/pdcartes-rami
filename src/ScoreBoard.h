#pragma once

#include <QVector>
#include <QWidget>

class QPushButton;
class Rami;

// La feuille de marque, posée sur le tapis à la fin d'une manche.
//
// It covers the cloth rather than replacing it: the combinations everybody
// laid are still there underneath, which is what you want to look at while
// you are reading what they cost you.
//
// Two shapes, the same card: at the end of a manche it is what this donne
// came to and what everybody is on; at the end of the game it says who won
// it. The score is a penalty -- the fewest points wins -- so the column is
// read from the bottom up, and the sheet says so.
class ScoreBoard : public QWidget {
    Q_OBJECT

public:
    explicit ScoreBoard(QWidget *parent = nullptr);

    void showHand(const Rami *game);
    void showMatch(const Rami *game);
    void hideBoard();

    void refresh();

Q_SIGNALS:
    // La manche suivante, ou une nouvelle partie: whichever the sheet is
    // showing.
    void goOn();
    void leave();

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    struct Line {
        QString name;
        int hue = 0;
        int handPoints = 0;
        int total = 0;
        bool wentOut = false;
        bool human = false;
        bool winner = false;
    };

    QSizeF measure() const;
    void placeButtons();
    void dressButtons();

    QVector<Line> m_lines;
    QString m_headline;
    QString m_aside;
    bool m_matchOver = false;
    bool m_youWon = false;

    QPushButton *m_onwards = nullptr;
    QPushButton *m_away = nullptr;
};
