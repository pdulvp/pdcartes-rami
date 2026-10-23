#pragma once

#include "Cards.h"

#include <QPixmap>
#include <QPointF>
#include <QRectF>
#include <QVector>
#include <QWidget>

class QTimer;

// L'écran titre : le tapis, des cartes au trait qui dérivent dessus, le logo
// en grand, et « Cliquer pour commencer » qui respire en dessous.
//
// Le seuil de la séance. Il porte les couleurs du profil comme le reste du
// jeu — c'est le même tapis que celui sur lequel on va jouer — et il ne
// demande rien : un clic n'importe où, ou une touche quelconque, et le menu
// paraît en emportant le logo avec lui.
//
// Les cartes sont dessinées ici, au trait d'or et non en couleurs : derrière
// un titre, une figure peinte est une tache. Elles ne viennent pas du jeu
// installé pour la même raison — ce sont des formes de cartes, le rang et
// l'enseigne, rien de plus.
class TitlePage : public QWidget {
    Q_OBJECT

public:
    explicit TitlePage(QWidget *parent = nullptr);

    // Les couleurs ont changé — la teinte du profil, ou la moitié sombre du
    // jeu. Les deux couches en cache sont à refaire.
    void refresh();

    // Où le logo se tient, dans les coordonnées de la page : c'est d'ici que
    // part le vol vers le menu.
    QRectF markRect() const;
    // Le temps du vol, c'est le calque qui porte le logo : la page le cache
    // pour qu'il ne soit pas dessiné deux fois.
    void setMarkHidden(bool hidden);

Q_SIGNALS:
    // Le joueur veut commencer.
    void start();

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    // Rien ne bouge quand la page n'est pas à l'écran : un timer qui bat
    // derrière le tapis d'une partie serait un timer pour rien.
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    // Une carte qui traverse l'écran. Sa place et sa taille sont en parts de
    // la fenêtre et non en pixels : redimensionner déplace le tapis sous
    // elles sans rebattre les cartes.
    struct Drifter {
        Card card;
        bool back = false;   // de dos, un simple treillis au trait
        QPointF at;          // le centre, de 0 à 1 dans les deux sens
        QPointF speed;       // parts de fenêtre par image
        qreal height = 0.12; // part de la hauteur de la fenêtre
        qreal angle = 0.0;
        qreal spin = 0.0;    // degrés par image
        qreal ink = 1.0;     // ce qu'elle a d'or : les lointaines sont pâles
    };

    void dealDrifters();
    void driftOn();

    void rebuildFelt();
    void rebuildMark();

    QPixmap m_felt;
    QPixmap m_mark;
    QVector<Drifter> m_drifters;
    QTimer *m_beat = nullptr;
    int m_frame = 0;
    bool m_markHidden = false;
};
