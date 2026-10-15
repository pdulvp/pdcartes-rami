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
    void noteLaid(const QVector<int> &cardIds);
    void stopAnimations();

    // Puts the colours that are current now on the cloth, for the dark button.
    void refresh();

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
    void chosenChanged();

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    struct Spot {
        int id = -1;
        QRectF box;
    };

    struct Flight {
        Card card;
        bool faceUp = true;
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

    void tick();
    void wake();
    QPointF seatAnchor(int at) const;

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
    qreal m_meldHeight = 54.0;

    QVector<int> m_chosen;
    int m_hovered = -1;
    int m_hoveredMeld = -1;
    bool m_hoverStock = false;
    bool m_hoverPile = false;

    QString m_line;
    bool m_trouble = false;

    QTimer *m_ticker = nullptr;
    QHash<int, qreal> m_glow;
    QVector<Flight> m_flights;
};
