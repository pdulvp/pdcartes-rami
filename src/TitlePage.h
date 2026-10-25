#pragma once

#include "Cards.h"

#include <QPixmap>
#include <QPointF>
#include <QRectF>
#include <QVector>
#include <QWidget>

class QTimer;

// Écran titre : le tapis, des cartes tracées au trait qui s'y déplacent, le
// logo en grand format, et la mention « Cliquer pour commencer » animée en
// dessous.
//
// Cet écran adopte les couleurs du profil, comme le reste de l'application, et
// n'attend aucune saisie : un clic quelconque ou une touche quelconque fait
// apparaître le menu, le logo se déplaçant vers sa position dans le menu.
//
// Les cartes sont tracées au trait doré et non en couleurs : des figures
// peintes seraient trop visibles derrière un titre. Elles ne proviennent pas
// du jeu installé pour la même raison : seuls le contour, le rang et le
// symbole de couleur sont représentés.
class TitlePage : public QWidget {
    Q_OBJECT

public:
    explicit TitlePage(QWidget *parent = nullptr);

    // Signale un changement de couleurs, qu'il s'agisse de la teinte du profil
    // ou du mode sombre. Les deux couches en cache doivent être régénérées.
    void refresh();

    // Position du logo dans les coordonnées de la page : point de départ de
    // l'animation vers le menu.
    QRectF markRect() const;
    // Pendant l'animation, le logo est rendu par le calque dédié ; la page le
    // masque afin qu'il ne soit pas dessiné deux fois.
    void setMarkHidden(bool hidden);

Q_SIGNALS:
    // Le joueur demande à commencer.
    void start();

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    // L'animation est suspendue lorsque la page n'est pas affichée : une
    // minuterie active derrière la table d'une partie en cours serait
    // inutile.
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    // Carte traversant l'écran. Sa position et sa taille sont exprimées en
    // proportions de la fenêtre et non en pixels, de sorte qu'un
    // redimensionnement ne réinitialise pas leur disposition.
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
