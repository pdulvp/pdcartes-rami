#pragma once

#include "Cards.h"
#include "Rami.h"
#include "Rivals.h"

#include <QString>
#include <QVector>

// Comment un adversaire décide.
//
// Ce qu'il sait faire est dit ailleurs: les huit compétences, leurs niveaux
// et ce que chacun vaut sont dans RivalSkills.h, et ce qu'il en a sur ce
// profil-là est dans Rivals.h. Ici, on ne fait que jouer -- on reçoit un
// Skill, qui est la liste de ce que ce joueur-là fait ou ne fait pas, et on
// en tire le coup à jouer maintenant.
//
// La seule chose qui compte dans ce fichier: un niveau n'est pas un
// coefficient multiplié quelque part. C'est une liste de choses qu'un joueur
// fait ou ne fait pas, et chacune se voit arriver de l'autre côté de la
// table -- la main lue trop vite, la carte qu'on ne vous donne pas, le joker
// racheté, la bourde.

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
