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

    // Lequel, au juste.
    //
    // Il en faut le compte exact, et pas seulement l'existence: une main qui
    // tient deux jokers en place un, et si ce n'est pas celui qui est dû le
    // tour ne peut toujours pas finir -- ni poser, ni jeter. Savoir lequel
    // est la seule façon d'en sortir, et c'est aussi ce qu'il faut pour
    // conseiller le joueur.
    QVector<int> jokerDebt(int at) const;

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
    bool discard(int cardId, QString *why = nullptr);

    // Défaire une pose qui n'est pas encore partie : en entier, par
    // combinaison, ou carte à carte.
    //
    // Une pose en attente n'est pas sur le tapis -- elle est devant son
    // joueur en attendant les cinquante-et-un -- et tant que le tour n'est
    // pas fini elle se défait. Une seule carte ne revient que si la
    // combinaison tient encore sans elle ; sinon c'est la combinaison
    // entière, et c'est à l'appelant de la demander.
    bool takeBackPose(QString *why = nullptr);
    bool takeBackMeld(int meldId, QString *why = nullptr);
    bool takeBackCard(int cardId, QString *why = nullptr);

    // Combien de tours cette manche a déjà pris.
    //
    // La seule horloge d'une manche qui avance quoi qu'il arrive. La pioche
    // n'en est pas une: deux joueurs qui se servent tous les deux de la
    // défausse ne l'entament jamais, et une manche peut tourner ainsi sans
    // que rien ne s'épuise. Un cerveau qui attend son heure a besoin de
    // savoir qu'elle est passée.
    int turnsPlayed() const { return m_turnsPlayed; }

    // Une pose a-t-elle été reprise en main ce tour-ci ?
    //
    // Un joueur qui vient de ramasser sa pose ne la réétale pas dans le même
    // tour: il la garde et il jette. Le fait est ici parce qu'il appartient au
    // tour, et parce que sans lui un cerveau qui n'a pas appris à compter
    // avant d'ouvrir repose indéfiniment ce qu'il vient de reprendre.
    bool poseTakenBack() const { return m_tookBack; }

    // La combinaison de la pose en cours où se trouve cette carte, ou -1.
    int poseMeldOf(int cardId) const;

    // Cette carte peut-elle revenir seule ? Faux quand la combinaison ne
    // tiendrait plus sans elle, et faux quand ce n'est pas le moment.
    bool canTakeBackCard(int cardId) const;

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
    bool m_tookBack = false;
    int m_turnsPlayed = 0;
    // The jokers bought back this turn and not yet laid, by card identity.
    QVector<int> m_jokerDebt;
};
