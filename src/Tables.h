#pragma once

#include <QString>
#include <QVector>

// The tables there are to sit at, and how long a game lasts.
//
// Five tables, as five sizes of board: what changes from one to the next is
// not the rules but how much of the pack is in somebody's hand. At two the
// pioche is deep and a card you want comes round again; at six, eighty-four
// of the hundred and eight cards are dealt out before the first turn, and
// what is thrown away is almost the only place to get anything.
//
// Elles sont toutes du même drap: la couleur du tapis est un goût, elle se
// choisit dans les réglages et appartient au profil -- voir TableLook.h. Une
// table de six n'est pas d'une autre couleur qu'une table de deux, elle est
// seulement plus pleine.
struct TableKind {
    QString id;
    QString name;
    int seats;      // you included
};

const QVector<TableKind> &tableKinds();
const TableKind &tableKindById(const QString &id);
QString defaultTableId();

// Combien de temps la partie dure.
//
// The scores are penalties -- what was left in your hand when somebody else
// went out -- so the winner is the one with the fewest, and the game ends as
// soon as anybody goes past the target.
//
// The target is held as a number of manches rather than as a number of
// points, because the number of points that makes a game of eight manches is
// not the same at every table: at a tête-à-tête the loser of a manche is one
// card behind and pays seven or eight points for it, while at six the cards
// nobody could place add up to forty a time. A fixed three hundred would be
// four manches at one table and thirty at the other. So the manches are what
// is chosen and the points are worked out from the table -- and both are
// shown, because the points are what the scoreboard counts.
struct Target {
    QString id;
    QString name;
    QString blurb;
    int manches;    // one for a single donne, and then the game is that donne
};

const QVector<Target> &targets();
const Target &targetById(const QString &id);
QString defaultTargetId();

// What the target comes to in points at a table of `seats`. Nought for the
// game of one manche, which ends when that manche does.
int targetPointsFor(const Target &target, int seats);
