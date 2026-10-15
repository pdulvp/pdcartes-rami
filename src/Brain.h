#pragma once

#include "Cards.h"
#include "Rami.h"
#include "Rivals.h"

#include <QString>
#include <QVector>

// Comment un adversaire décide.
//
// A level is not a number multiplied into a strength somewhere. It is a list
// of things a player does or does not do, and each of them is a thing you can
// watch happen across the table:
//
//   lire sa main      A beginner reads their hand greedily -- biggest
//                     combination first, then whatever still fits -- and so
//                     genuinely fails to see an opening that three small
//                     combinations would have made. From intermédiaire up the
//                     hand is read exactly, every way at once, and nothing is
//                     missed.
//   la défausse       A beginner takes the card on the défausse only when it
//                     finishes a combination there and then. The better ones
//                     take it for a combination that is one card away, and
//                     weigh it against what the pioche is likely to hold.
//   ce qu'on garde    A beginner throws whatever is worth the most, which is
//                     how a beginner throws away the card they needed. The
//                     better ones price a card by what it is doing in the
//                     hand as well as by what it costs.
//   ce qu'on donne    An expert will not hand you a card that goes straight
//                     onto something already on the tapis, and remembers
//                     which cards you have taken off the défausse -- so the
//                     eight you are plainly collecting is the one card that
//                     never comes round.
//   le joker          Only an expert bothers to buy a joker back out of a
//                     combination.
//   la bourde         And on top of all of it, a beginner simply plays the
//                     wrong card now and then. One move in three or so at the
//                     very bottom, and none at all at the very top.

struct Skill {
    qreal mastery = 0.0;
    qreal blunder = 0.0;         // how often the wrong move is made on purpose
    bool thorough = false;       // reads the hand exactly rather than greedily
    bool watches = false;        // remembers everything that has gone face up
    bool guardsFeed = false;     // will not throw what the tapis is waiting for
    bool buysJokers = false;
    qreal layoffSight = 1.0;     // how often a card that fits is spotted
    qreal pileThreshold = 2.0;   // how much better the défausse has to look
    qreal potentialWeight = 1.0; // how much a combination one card away is worth
};

Skill skillFor(int xp);
Skill bestSkill();               // what the hint button thinks with

// Ce qui est passé sous les yeux de tout le monde.
//
// Kept by the table and handed to each brain in turn: the cards that have
// been face up, and which of them each player picked up off the défausse. It
// is all public -- an expert is a player who remembers it, not one who is
// allowed to look at your hand.
struct Watch {
    QVector<Card> seen;
    QVector<QVector<Card>> taken;   // one list per seat

    void note(const Card &card);
    void noteTaken(int seat, const Card &card, int seats);
    void clear();
    // How many of the two copies of this card nobody has seen yet.
    int unseenCopies(int rank, Suit suit) const;
};

struct Move {
    enum Kind { None, DrawStock, DrawPile, Lay, LayOff, BuyJoker, TakeBack, Discard };
    Kind kind = None;
    QVector<int> cards;    // Lay: the combination. The rest: one card.
    int meldId = -1;
};

// The one move to make now, given where the game has got to. Called over and
// over by the table until the turn has been played out, so that each thing a
// rival does can be shown happening rather than arriving all at once.
Move chooseMove(const Rami &game, int at, const Skill &skill, const Watch &watch);

// What the best player at the table would do with your hand, said in French.
// This is the Conseil button, and it is the same brain -- there is no second,
// better one kept for the player.
QString advise(const Rami &game, int at, const Watch &watch);
