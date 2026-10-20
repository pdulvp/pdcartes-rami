#pragma once

#include "RivalSkills.h"

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
// Ce qui les distingue est de deux sortes, et c'est RivalSkills.h qui le dit:
// quatre compétences innées, qui sont ce qu'ils sont et qui ne bougent pas,
// et quatre compétences acquises, chacune avec son expérience, qui montent en
// jouant. Ce fichier-ci ne fait que les ranger par profil et les relire --
// les six sont les mêmes pour tout le monde, ce qu'ils ont vécu appartient au
// profil sous lequel ils l'ont vécu.
//
// Chacun peut être remis à ce qu'il était, un à un, depuis sa fiche: son
// expérience seule, ou -- et cela lui coûte son expérience -- ce qu'il est.

struct Rival {
    QString id;
    QString name;
    QString blurb;       // what they are like at the table, in a line
    int hue;             // their own colour, worn on their plaque and their melds
    int startXp;         // the experience they sit down with, and are restored to
    int emblem;          // which mark is drawn on their medallion, 0-5
    Innates innate;      // ce qu'il est en arrivant, et ce que la phrase dit
};

const QVector<Rival> &rivals();
const Rival &rivalById(const QString &id);

// What one of them has to show for the games played under this profile.
struct Learning {
    Pools pools;         // les quatre expériences, une par compétence acquise
    int xp = 0;          // leur somme, qui est le niveau affiché
    int handsPlayed = 0;
    int handsWon = 0;
    int matchesPlayed = 0;
    int matchesWon = 0;
};

Learning learningFor(const QString &rivalId);
Learning learningFor(const QString &rivalId, int slot);

// Ce qu'il est, sur ce profil-là, et la phrase qui va avec.
Innates rivalInnate(const QString &rivalId);
Innates rivalInnate(const QString &rivalId, int slot);

// Le changer lui fait tout réapprendre.
//
// Les innées règlent la vitesse à laquelle les acquises montent: l'expérience
// gagnée par un joueur n'est donc pas l'expérience qu'un autre aurait. Changer
// ce qu'il est remet ses quatre expériences et son palmarès à zéro, et c'est
// cette fonction qui s'en charge -- pour que les deux ne puissent pas se
// désolidariser, où qu'on l'appelle. Ne fait rien quand rien ne change.
void setRivalInnate(const QString &rivalId, const Innates &innate);

// Et le ramener à ce qu'il était en arrivant, ce qui coûte pareil.
void resetRivalInnate(const QString &rivalId);

// La phrase de sa carte: la sienne tant qu'il est celui qui est arrivé, et
// celle que ses innées décrivent dès qu'on l'a changé -- une plaque ne doit
// pas annoncer un joueur qui n'est plus là.
QString rivalBlurb(const QString &rivalId);

// Ce que son cerveau joue, ici et maintenant.
Skill skillFor(const QString &rivalId);
Skill skillFor(const QString &rivalId, int slot);

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
void noteRivalHand(const QString &rivalId, int place, int seats, bool wentOut,
                   const Deeds &deeds);
void noteRivalMatch(const QString &rivalId, bool won);

// Back to the level this one sits down with, this profile only. The fiche
// offers it per rival rather than all at once: a player who has made Oriane
// unbeatable may still want Lulu to stay a beginner. Ce qu'il est lui reste.
void resetRivalExperience(const QString &rivalId);
