#pragma once

#include <QVector>
#include <QWidget>

class QPushButton;
class Rami;

// Feuille de marque, affichée sur le tapis à la fin d'une manche.
//
// Elle recouvre le tapis sans s'y substituer : les combinaisons posées restent
// visibles en dessous, ce qui permet de les consulter en lisant le décompte.
//
// Elle prend deux formes : à la fin d'une manche, elle indique le résultat de
// la donne et le total de chaque joueur ; à la fin de la partie, elle désigne
// le vainqueur. Le score étant une pénalité, le total le plus faible
// l'emporte : la colonne se lit donc de bas en haut, ce que la feuille
// signale.
class ScoreBoard : public QWidget {
    Q_OBJECT

public:
    explicit ScoreBoard(QWidget *parent = nullptr);

    void showHand(const Rami *game);
    void showMatch(const Rami *game);
    void hideBoard();

    void refresh();

Q_SIGNALS:
    // Passage à la manche suivante ou à une nouvelle partie, selon ce
    // qu'affiche la feuille.
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
