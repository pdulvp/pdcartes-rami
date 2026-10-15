#pragma once

#include "Cards.h"

#include <QVector>

// What makes a combination, and how a hand is best read as a set of them.
//
// The rami knows two combinations and no others:
//
//   une suite  -- three cards or more of the same colour, following on from
//                 one another: 7-8-9 de cœur. L'as goes under the deux or
//                 above the roi, never both at once and never round the
//                 corner.
//   une série  -- three or four cards of the same rank in different colours:
//                 three dames, or four. Two dames de cœur out of the two
//                 packs are the same colour twice, and that is not a série.
//
// A joker stands in for any one card, and one joker at most to a combination.
// Which card it stands for is written down rather than left open, because the
// moment somebody wants to buy it back the answer has to be the same one the
// combination was laid with.

enum class MeldKind { None, Suite, Serie };

struct Meld {
    int id = -1;
    int owner = -1;
    MeldKind kind = MeldKind::None;

    // A suite is held in order, lowest first, with the joker sitting in the
    // place it stands for. A série is held by colour with the joker last.
    QVector<Card> cards;

    // A suite only: where the first card sits on the one-to-fourteen ladder
    // the suites are counted on, fourteen being l'as placed above the roi.
    int low = 0;

    bool valid() const { return kind != MeldKind::None && cards.size() >= 3; }
    bool hasJoker() const;
    int jokerAt() const;             // where the joker sits, or -1
    int runRankAt(int index) const { return low + index; }
    int rank() const;                // a série only: the rank they share
    Suit suit() const;               // a suite only: the colour they share
    int points() const;
    QString spell() const;           // "7-8-9 de cœur", for a sentence
};

// Do these cards make one combination? `out` gets it in order when they do.
bool makeMeld(const QVector<Card> &cards, Meld *out);

// Can `card` be added to `meld`? `out` gets the combination with it in place.
bool canExtend(const Meld &meld, const Card &card, Meld *out);

// Which cards would buy the joker back out of `meld`. A suite leaves exactly
// one answer; a série leaves one per colour it is missing.
QVector<Card> jokerWants(const Meld &meld);

// What the combinations in a hand are worth, and what is left over.
struct Plan {
    QVector<Meld> melds;
    QVector<Card> rest;
    int points = 0;      // what the combinations come to
    int deadwood = 0;    // what the rest would cost if the hand ended now
};

// How a hand is read.
//
//   Balanced  -- the reading that leaves the least in hand: what a player
//                wants when they are deciding what to throw.
//   MostPoints -- the reading worth the most: what a player wants when they
//                are counting whether they can reach fifty-one.
//   Greedy    -- the biggest combination first and then whatever still fits,
//                which is how a beginner reads a hand and why a beginner
//                misses an opening that was there.
enum class Reading { Balanced, MostPoints, Greedy };

Plan readHand(const QVector<Card> &hand, Reading how);

// Every combination the hand holds, which is the raw material the readings
// are chosen from -- and, for a rival weighing what to throw away, the list
// of what each card is doing there.
QVector<Meld> allMelds(const QVector<Card> &hand);

// What has to be laid to open: fifty-one points in one turn.
inline constexpr int kOpeningPoints = 51;
