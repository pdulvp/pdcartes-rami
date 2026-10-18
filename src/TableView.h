#pragma once

#include "Cards.h"

#include <QHash>
#include <QPointF>
#include <QRectF>
#include <QVector>
#include <QWidget>

class QTimer;
class Rami;

// La table, à l'écran.
//
// One widget for the whole cloth: the plaques of the people you are playing
// against along the top, the combinations everybody has laid in the middle,
// the pioche and the défausse down the right, and your own fifteen cards
// along the bottom. It is one widget and not six because the cloth is one
// surface -- a card has to be able to fly from somebody's plaque to the
// défausse, and a layout of panels is exactly the thing that will not let it.
//
// It paints the game and it reports clicks. It does not know the rules: which
// of its cards may be pressed, and what happens when one is, belongs to the
// page above it.
class TableView : public QWidget {
    Q_OBJECT

public:
    explicit TableView(QWidget *parent = nullptr);

    // The game being shown. Not owned; the view reads it and never changes it.
    void watch(const Rami *game);

    // Les cartes choisies dans la main, which is a thing about the screen
    // rather than about the game: it is what the Poser button will be handed.
    const QVector<int> &chosen() const { return m_chosen; }
    void setChosen(const QVector<int> &cards);
    void clearChosen();

    // A line of what is going on, written on the cloth above your hand.
    void say(const QString &line);
    void sayTrouble(const QString &line);

    // Ce qui vient de se passer, so that it can be seen happening: a card
    // flying to the défausse, and a ring that fades on whatever was played.
    void noteDiscard(int seat, const Card &card);
    void noteDrawn(int seat, bool fromPile, const Card &card);
    // `by` est le joueur qui les pose, quand ce n'est pas celui à qui la
    // combinaison appartient: compléter la combinaison d'un adversaire fait
    // partir la carte de votre main, et non de sa plaque à lui. Laissé à -1,
    // c'est le propriétaire de la combinaison qui la joue.
    void noteLaid(const QVector<int> &cardIds, int by = -1);
    // La pose reprise en main: les cartes repartent du rail vers la main.
    void noteTakenBack(const QVector<int> &cardIds);
    void stopAnimations();

    // Puts the colours that are current now on the cloth, for the dark button.
    void refresh();

    // La taille des cartes, en pour cent.
    //
    // Un jeu installé apporte des images d'une finesse donnée -- les CARDS.DLL
    // de Windows font 71 sur 96 -- et une figure réduite au quart de cela sur
    // le tapis n'est plus qu'un bruit de couleurs. Aucun filtre ne rend des
    // pixels qui n'existent pas: ce qu'il faut, c'est pouvoir les dessiner plus
    // grandes. Le zoom tire sur toutes les tailles de carte à la fois, la main,
    // le tapis, la pioche et la pose, et chaque bande de la table grandit avec
    // elles plutôt que de les laisser déborder.
    void setZoom(int percent);
    int zoom() const { return m_zoom; }

    // Où chaque carte posée se trouve à l'écran, et laquelle c'est: ce qu'il
    // faut au feu d'artifice pour que les cartes aient l'air de se soulever du
    // tapis plutôt que d'arriver de nulle part.
    void cardsOnScreen(QVector<Card> *cards, QVector<QPointF> *at) const;
    qreal meldCardHeight() const { return m_meldHeight; }

Q_SIGNALS:
    void stockPressed();
    void pilePressed();
    void cardPressed(int cardId);
    void cardDoubleClicked(int cardId);
    void meldPressed(int meldId);
    // Le clic droit sur une combinaison : racheter son joker.
    void meldRightPressed(int meldId);
    // Le clic sur une carte de la pose en cours, sur le rail : la reprendre.
    void poseCardPressed(int cardId);
    void chosenChanged();
    // Changé à la molette, pour que la fenêtre le garde.
    void zoomChanged(int percent);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
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
        // Le demi-tour: la carte part de dos, passe de profil à mi-chemin et
        // arrive face en haut. Réservé à votre pioche -- c'est le seul moment
        // du jeu où une carte se retourne pour vous.
        bool flips = false;
        QPointF from;
        QPointF to;
        QSizeF size;
        // Where it is going, when that is not the size it started at: a card
        // leaving the pioche for your hand is drawn small at one end of the
        // flight and full size at the other.
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
    // La carte de la pose en cours sous le pointeur, ou -1. Les cartes d'une
    // combinaison se chevauchent : c'est celle du dessus qui répond.
    int poseCardAt(const QPointF &where) const;

    void tick();
    void wake();
    QPointF seatAnchor(int at) const;

    // Les deux flèches qui montrent où prendre: leur place, s'il y a lieu de
    // les montrer, et le petit rectangle qu'il suffit de redemander quand
    // elles seules bougent.
    QRectF arrowBox(const QRectF &card) const;

    // Une carte est-elle en train de voler ? Le jeu range la carte à sa
    // destination avant que l'animation parte, si bien que sans cette
    // question-là elle se trouve dessinée deux fois: une fois arrivée, une
    // fois en chemin.
    bool inFlight(int cardId) const;

    // La place qu'occupe la i-ème carte d'une main qui en compte `count`.
    //
    // Fonction pure de ces deux nombres: c'est ce qui permet de savoir où les
    // cartes étaient avant la pioche sans l'avoir noté -- la main d'avant est
    // la main d'après moins une carte.
    QRectF handSlot(int index, int count) const;

    // Où une carte de la main est *à cet instant*: sa place, ou le point du
    // trajet où elle en est si la main est en train de s'écarter. Le dessin
    // et le pointage s'en servent tous les deux -- c'est ce qui garantit
    // qu'on clique la carte qu'on voit.
    QRectF handBoxNow(int id, const QRectF &box) const;

    // La place de la k-ième carte d'une combinaison posée sur le tapis.
    QRectF meldCardBox(const QRectF &spot, int index) const;

    // La place de la k-ième carte d'une pose en cours, sur le rail.
    QRectF poseCardBox(const QRectF &spot, int index) const;

    // Où chaque carte posée sur le rail a été dessinée: d'où elle revient
    // quand on la reprend en main.
    QHash<int, QRectF> m_poseWas;

    // L'écartement de la main pendant qu'une carte piochée vole vers sa
    // place: d'où chaque carte part, et où en est le mouvement.
    QHash<int, QRectF> m_slideFrom;
    qreal m_slideAlong = 1.0;
    QRectF m_handRowWas;

    // Où chaque carte de la main a été posée la dernière fois qu'on a
    // disposé la table. La disposition est refaite pendant que la carte
    // quitte la main, si bien qu'au moment d'animer son départ sa place n'est
    // déjà plus dans m_handBoxes: c'est ici qu'on la retrouve.
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
    // Chaque carte du rail, une par une : m_poseBoxes donne la place d'une
    // combinaison, et c'est une carte qu'on reprend.
    QVector<Spot> m_poseCardBoxes;
    qreal m_meldHeight = 54.0;
    int m_zoom = 100;
    qreal m_beckon = 0.0;       // la phase du va-et-vient des flèches

    QVector<int> m_chosen;
    int m_hovered = -1;
    int m_hoveredMeld = -1;
    int m_hoveredPose = -1;
    bool m_hoverStock = false;
    bool m_hoverPile = false;

    QString m_line;
    bool m_trouble = false;

    QTimer *m_ticker = nullptr;
    QHash<int, qreal> m_glow;
    QVector<Flight> m_flights;
};
