#pragma once

#include "Cards.h"

#include <QHash>
#include <QPointF>
#include <QRectF>
#include <QVector>
#include <QWidget>

class QTimer;
class Rami;

// Affichage de la table.
//
// Un unique widget couvre l'ensemble du tapis : les plaques des adversaires
// en haut, les combinaisons posées au centre, la pioche et la défausse sur la
// droite, et la main du joueur en bas. Le regroupement en un seul widget est
// imposé par les animations : une carte doit pouvoir se déplacer de la plaque
// d'un joueur jusqu'à la défausse, ce qu'une disposition en panneaux
// distincts interdirait.
//
// Ce widget assure le rendu et signale les clics. Il ne connaît pas les
// règles : la détermination des cartes activables et le traitement des clics
// relèvent de la page qui le contient.
class TableView : public QWidget {
    Q_OBJECT

public:
    explicit TableView(QWidget *parent = nullptr);

    // Partie affichée. Elle n'appartient pas à cette classe, qui la lit sans
    // jamais la modifier.
    void watch(const Rami *game);

    // Cartes sélectionnées dans la main. Cette information relève de
    // l'affichage et non des règles : c'est ce qui sera transmis au bouton
    // Poser.
    const QVector<int> &chosen() const { return m_chosen; }
    void setChosen(const QVector<int> &cards);
    void clearChosen();

    // Ligne d'état affichée sur le tapis, au-dessus de la main du joueur.
    void say(const QString &line);
    void sayTrouble(const QString &line);

    // Coup qui vient d'être joué, afin de pouvoir l'animer : déplacement
    // d'une carte vers la défausse, et anneau qui s'estompe sur la carte
    // jouée.
    void noteDiscard(int seat, const Card &card);
    void noteDrawn(int seat, bool fromPile, const Card &card);
    // `by` désigne le joueur qui pose les cartes lorsqu'il diffère du
    // propriétaire de la combinaison : compléter la combinaison d'un
    // adversaire fait partir la carte de la main du joueur et non de la
    // plaque de l'adversaire. À -1, le propriétaire de la combinaison est
    // considéré comme l'auteur du coup.
    void noteLaid(const QVector<int> &cardIds, int by = -1);
    // Pose reprise en main : les cartes retournent du rail vers la main.
    void noteTakenBack(const QVector<int> &cardIds);
    void stopAnimations();

    // Applique au tapis la palette courante, à la suite d'un changement de
    // mode sombre.
    void refresh();

    // Taille des cartes, en pourcentage.
    //
    // Un jeu installé fournit des images d'une définition donnée — les
    // fichiers CARDS.DLL de Windows mesurent 71 sur 96 pixels — et une figure
    // réduite au quart de cette taille devient illisible. Aucun filtrage ne
    // restitue des pixels absents ; la seule solution est de pouvoir les
    // afficher plus grandes. Le zoom agit simultanément sur toutes les tailles
    // de carte — la main, le tapis, la pioche et la pose — et chaque zone de
    // la table est redimensionnée en conséquence.
    void setZoom(int percent);
    int zoom() const { return m_zoom; }

    // Position à l'écran de chaque carte posée, et son identité. Ces données
    // sont requises par l'animation de victoire, afin que les cartes semblent
    // se détacher du tapis plutôt que d'apparaître.
    void cardsOnScreen(QVector<Card> *cards, QVector<QPointF> *at) const;
    qreal meldCardHeight() const { return m_meldHeight; }

Q_SIGNALS:
    void stockPressed();
    void pilePressed();
    void cardPressed(int cardId);
    void cardDoubleClicked(int cardId);
    void meldPressed(int meldId);
    // Clic droit sur une combinaison : rachat de son joker.
    void meldRightPressed(int meldId);
    // Clic sur une carte de la pose en cours, sur le rail : reprise de la
    // carte.
    void poseCardPressed(int cardId);
    void chosenChanged();
    // Modifié à la molette ; la fenêtre principale enregistre la valeur.
    void zoomChanged(int percent);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    struct Spot {
        int id = -1;
        QRectF box;
    };

    struct Flight {
        Card card;
        bool faceUp = true;
        // Retournement : la carte part face cachée, passe de profil à
        // mi-parcours et arrive face visible. Réservé à la pioche du joueur,
        // seul moment où une carte se retourne à son intention.
        bool flips = false;
        QPointF from;
        QPointF to;
        QSizeF size;
        // Taille d'arrivée, lorsqu'elle diffère de la taille de départ : une
        // carte quittant la pioche pour la main est dessinée réduite au début
        // du déplacement et à taille réelle à la fin.
        QSizeF grow;
        qreal along = 0.0;
        qreal speed = 0.055;
    };

    void relayout();
    void layoutSeats();
    void layoutMelds();
    void layoutHand();

    void paintSeats(QPainter *painter);
    void paintStacks(QPainter *painter);
    void paintMelds(QPainter *painter);
    void paintRail(QPainter *painter);
    void paintHand(QPainter *painter);
    void paintFlights(QPainter *painter);
    void paintSeatPlaque(QPainter *painter, const QRectF &box, int at);

    int cardAt(const QPointF &where) const;
    int meldAt(const QPointF &where) const;
    // Plaque d'adversaire située sous ce point, ou -1. La plaque du joueur
    // n'est pas concernée : elle se trouve sous la défausse et ne donne accès
    // à aucun aperçu.
    int seatAt(const QPointF &where) const;
    // Carte de la pose en cours située sous le pointeur, ou -1. Les cartes
    // d'une combinaison se chevauchant, c'est celle du dessus qui est
    // retenue.
    int poseCardAt(const QPointF &where) const;

    void tick();
    void wake();
    QPointF seatAnchor(int at) const;

    // Les deux flèches indiquant où piocher : leur position, la condition de
    // leur affichage, et le rectangle minimal à redessiner lorsqu'elles seules
    // changent.
    QRectF arrowBox(const QRectF &card) const;

    // Indique si une carte est en cours de déplacement. Les règles placent la
    // carte à sa destination avant le démarrage de l'animation ; sans ce test,
    // elle serait dessinée deux fois, à sa destination et en cours de
    // déplacement.
    bool inFlight(int cardId) const;

    // Position de la i-ème carte d'une main en comptant `count`.
    //
    // Cette fonction ne dépend que de ces deux paramètres, ce qui permet de
    // reconstituer la position des cartes avant la pioche sans l'avoir
    // enregistrée : la main précédente est la main courante moins une carte.
    QRectF handSlot(int index, int count) const;

    // Position courante d'une carte de la main : sa position de repos, ou sa
    // position intermédiaire si la main est en cours d'écartement. Le rendu et
    // la détection des clics emploient tous deux cette fonction, ce qui
    // garantit que le clic porte sur la carte affichée.
    QRectF handBoxNow(int id, const QRectF &box) const;

    // Position de la k-ième carte d'une combinaison posée sur le tapis.
    QRectF meldCardBox(const QRectF &spot, int index) const;

    // Position de la k-ième carte d'une pose en cours, sur le rail.
    QRectF poseCardBox(const QRectF &spot, int index) const;

    // Position de rendu de chaque carte du rail : origine de l'animation de
    // reprise en main.
    QHash<int, QRectF> m_poseWas;

    // Écartement de la main pendant qu'une carte piochée rejoint sa
    // position : position de départ de chaque carte et avancement de
    // l'animation.
    QHash<int, QRectF> m_slideFrom;
    qreal m_slideAlong = 1.0;
    QRectF m_handRowWas;

    // Position de chaque carte de la main lors de la dernière disposition de
    // la table. La disposition étant recalculée pendant que la carte quitte la
    // main, sa position ne figure plus dans `m_handBoxes` au moment d'animer
    // son départ : elle est retrouvée ici.
    QHash<int, QRectF> m_handWas;
    bool beckoning() const;
    QRect beckonArea() const;

    const Rami *m_game = nullptr;

    QRectF m_seatsRow;
    QRectF m_tapis;
    QRectF m_stacks;
    QRectF m_rail;
    QRectF m_handRow;

    QRectF m_stockBox;
    QRectF m_pileBox;
    QVector<Spot> m_seatBoxes;
    QVector<Spot> m_meldBoxes;
    QVector<Spot> m_handBoxes;
    QVector<Spot> m_poseBoxes;
    // Position de chaque carte du rail prise isolément : `m_poseBoxes` ne
    // fournit que la position d'une combinaison entière, alors que la reprise
    // porte sur une carte.
    QVector<Spot> m_poseCardBoxes;
    qreal m_meldHeight = 54.0;
    int m_zoom = 100;
    qreal m_beckon = 0.0;       // la phase du va-et-vient des flèches

    QVector<int> m_chosen;
    int m_hovered = -1;
    int m_hoveredMeld = -1;
    int m_hoveredPose = -1;
    // Plaque survolée, et minuterie au terme de laquelle son aperçu
    // s'affiche : les huit compétences de l'adversaire, consultables sans
    // interrompre la manche.
    int m_hoveredSeat = -1;
    QTimer *m_glanceBeat = nullptr;
    bool m_hoverStock = false;
    bool m_hoverPile = false;

    QString m_line;
    bool m_trouble = false;

    QTimer *m_ticker = nullptr;
    QHash<int, qreal> m_glow;
    QVector<Flight> m_flights;
};
