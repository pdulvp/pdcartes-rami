#pragma once

#include <QString>
#include <QVector>

// Les six adversaires, et ce qu'ils ont appris.
//
// They are not six settings of one difficulty dial that the player turns.
// Each of them is a player with a history: two of them sit down knowing
// almost nothing, three of them know the game, and Oriane has been at it for
// years. And they keep playing after you have put the game away -- or rather,
// what they learnt from the games you did play is still there when you come
// back, so that a table you could beat in March is a table that beats you in
// June.
//
// What they know is a single figure, their expérience, which goes up a little
// at every manche they sit through and a little more at every one they win.
// Three names are put on the ranges of it, because "niveau 437" is not
// something anybody wants to read:
//
//     débutant        0 - 199     throws away what it needs, opens late,
//                                 misreads its own hand
//     intermédiaire 200 - 599     reads its hand properly, watches the
//                                 défausse, keeps what might serve
//     expert        600 - 1200    remembers every card thrown, will not feed
//                                 you, and buys its jokers back
//
// Each of the six is kept per profile, so switching profiles is switching
// tables -- and each can be put back to the level it came with, one at a
// time, from the réglages.

enum class Level { Debutant, Intermediaire, Expert };

inline constexpr int kMaxExperience = 1200;

struct Rival {
    QString id;
    QString name;
    QString blurb;       // what they are like at the table, in a line
    int hue;             // their own colour, worn on their plaque and their melds
    int startXp;         // the experience they sit down with, and are restored to
    int emblem;          // which mark is drawn on their medallion, 0-5
};

const QVector<Rival> &rivals();
const Rival &rivalById(const QString &id);

// What one of them has to show for the games played under this profile.
struct Learning {
    int xp = 0;
    int handsPlayed = 0;
    int handsWon = 0;
    int matchesPlayed = 0;
    int matchesWon = 0;
};

Learning learningFor(const QString &rivalId);
Learning learningFor(const QString &rivalId, int slot);

// Le nom sous lequel on joue contre lui, sur ce profil-là.
//
// Les six restent les six: même couleur, même emblème, même cerveau, même
// expérience. Seul le nom change, et il change par profil, comme tout ce qui
// appartient à un profil -- la table du dimanche peut avoir ses habitués
// sans que celle de la semaine les connaisse. Sans nom donné, c'est celui
// avec lequel il est arrivé.
QString rivalName(const QString &rivalId);
QString rivalName(const QString &rivalId, int slot);
void setRivalName(const QString &rivalId, const QString &name);
void clearRivalName(const QString &rivalId);

// Au plus long qu'un nom donné puisse être: il faut qu'il tienne sur la
// plaque, à côté de la pastille du niveau.
inline constexpr int kRivalNameLength = 18;

// Counted at the end of each manche. `place` is nought for whoever came out
// best; `wentOut` is true for the one who got rid of their last card, which
// is worth more than coming second by a little.
void noteRivalHand(const QString &rivalId, int place, int seats, bool wentOut);
void noteRivalMatch(const QString &rivalId, bool won);

// Back to the level this one sits down with, this profile only. The réglages
// offer it per rival rather than all at once: a player who has made Oriane
// unbeatable may still want Lulu to stay a beginner.
void resetRival(const QString &rivalId);

Level levelOf(int xp);
QString levelName(Level level);
int levelStars(Level level);           // one, two or three
int levelFloor(Level level);
int levelCeiling(Level level);

// Nought to one across the whole range: what the brain is handed, and what
// the gauge on their card is filled to.
qreal masteryOf(int xp);

// Nought to one inside the level they are at, for the gauge under the name.
qreal progressWithin(int xp);
