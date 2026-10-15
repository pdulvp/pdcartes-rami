#pragma once

#include "Cards.h"
#include "Melds.h"

#include <QObject>
#include <QString>
#include <QVector>

// La partie : les règles, et rien d'autre.
//
// Nothing in this file knows that there is a screen or that some of the
// players are not people. It holds the pack, whose turn it is, what is on the
// tapis and what everyone is owed at the end -- and it refuses a move that
// the rami does not allow, with a sentence saying why, because a rule that is
// enforced silently is a rule nobody learns.
//
// The turn, in full:
//
//   1. piocher     one card, off the top of the pioche or off the défausse.
//   2. poser       as many combinations as you like -- but the first time,
//                  they have to come to fifty-one points before the turn is
//                  over, or they go back in your hand.
//   3. compléter   once you have opened: a card onto any combination on the
//                  tapis, yours or anybody's.
//   4. défausser   one card, face up. If that was your last, the manche is
//                  over and everybody else counts what they are still holding.

enum class Phase { Idle, Draw, Build, HandOver, MatchOver };

struct SeatSpec {
    QString rivalId;     // empty for you
    QString name;
    int hue = 0;
};

struct Seat {
    QString rivalId;
    QString name;
    int hue = 0;

    QVector<Card> hand;
    // A pose begun and not yet worth fifty-one. It sits in front of the
    // player rather than on the tapis: it is not laid until it counts, and if
    // the turn ends with it short it goes back in the hand.
    QVector<Meld> waiting;
    bool opened = false;

    int score = 0;        // the match total, which is a penalty: low is good
    int lastHand = 0;     // what the manche just finished cost
    bool wentOut = false;

    bool human() const { return rivalId.isEmpty(); }
};

class Rami : public QObject {
    Q_OBJECT

public:
    explicit Rami(QObject *parent = nullptr);

    void startMatch(const QVector<SeatSpec> &seats, int targetPoints);
    void startHand();

    int seatCount() const { return m_seats.size(); }
    const Seat &seat(int at) const;
    const QVector<Seat> &seats() const { return m_seats; }
    int turn() const { return m_turn; }
    Phase phase() const { return m_phase; }
    int handNumber() const { return m_handNumber; }
    int dealer() const { return m_dealer; }
    int targetPoints() const { return m_target; }

    const QVector<Meld> &table() const { return m_table; }
    const Meld *meldById(int id) const;
    const QVector<Card> &pile() const { return m_pile; }
    Card pileTop() const;
    int stockLeft() const { return m_stock.size(); }
    // How many times the défausse has been turned over and dealt again. Two
    // is as many as a manche gets: past that, nobody is going to go out and
    // the manche is called where it stands.
    int reshuffles() const { return m_reshuffles; }

    int outSeat() const { return m_outSeat; }        // who went out, or -1
    int matchWinner() const { return m_matchWinner; } // or -1

    // What the pose in front of `at` comes to, and what it still needs.
    int poseValue(int at) const;

    // A joker bought back out of a combination has to be put to work before
    // the turn ends. True while one is still in hand.
    bool owesJoker(int at) const;

    // What was drawn this turn, so the table can make it glow, and where from.
    Card lastDrawn() const { return m_lastDrawn; }
    bool drewFromPile() const { return m_drewFromPile; }

    // The moves. Each says whether it was allowed, and fills `why` with a
    // sentence when it was not.
    bool drawStock(QString *why = nullptr);
    bool drawPile(QString *why = nullptr);
    bool lay(const QVector<int> &cardIds, QString *why = nullptr);
    bool layOff(int cardId, int meldId, QString *why = nullptr);
    bool buyJoker(int cardId, int meldId, QString *why = nullptr);
    bool takeBackPose(QString *why = nullptr);
    bool discard(int cardId, QString *why = nullptr);

    // Whether the turn could be ended now, without ending it.
    bool canDiscard(QString *why = nullptr) const;

    // Ranger une main. The order a hand is held in belongs to whoever is
    // holding it, so it lives here with the hand -- by colour for finding
    // suites, by rank for finding séries.
    void sortSeatHand(int at, bool byRank);

Q_SIGNALS:
    void changed();
    // One line of what just happened, for the strip along the bottom of the
    // table: the only running account of a game played by five other people.
    void acted(int seat, const QString &tale);
    void turnBegan(int seat);
    void handFinished();
    void matchFinished();

private:
    void dealHand();
    void nextTurn();
    void finishHand();
    bool refillStock();
    Seat &here();
    int nextMeldId() { return ++m_meldIds; }

    QVector<Seat> m_seats;
    QVector<Card> m_stock;
    QVector<Card> m_pile;
    QVector<Meld> m_table;

    Phase m_phase = Phase::Idle;
    int m_turn = 0;
    int m_dealer = 0;
    int m_handNumber = 0;
    int m_target = 0;
    int m_meldIds = 0;
    int m_outSeat = -1;
    int m_matchWinner = -1;
    int m_reshuffles = 0;

    Card m_lastDrawn;
    bool m_drewFromPile = false;
    // The jokers bought back this turn and not yet laid, by card identity.
    QVector<int> m_jokerDebt;
};
