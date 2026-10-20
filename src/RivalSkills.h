#pragma once

#include <QString>
#include <QVector>

// Les huit compétences d'un adversaire, et ce qu'elles font de lui.
//
// Nothing in this file reads or writes anything: it is the table of what the
// compétences are, and the one function that turns a given adversaire into
// the handful of numbers his brain plays on. Where those figures are kept per
// profile is Rivals.h, and what is done with them is Brain.cpp. Keeping the
// three apart is what lets the arène measure a brain without a profile and
// without a screen.
//
// Elles sont de deux familles, et la différence est la seule chose qu'il faut
// retenir:
//
//   les innées    Four of them, each on four levels, and they never move.
//                 They are what he is: how far he gambles, how tightly he
//                 holds his cards, how much of the table he watches, how well
//                 he keeps his head. They can be changed from his fiche --
//                 and changing them costs him everything he has learnt,
//                 because experience earned as one player is not experience
//                 the other one has.
//   les acquises  Four as well, each on four rungs, each with its own
//                 expérience filled by playing. They are what he knows:
//                 reading his hand, remembering the table, choosing what to
//                 give away, and not fumbling.
//
// Les innées règlent la vitesse des acquises -- un distrait apprend la
// mémoire lentement -- ce qui fait que deux adversaires qui ont joué autant
// de manches ne sont pas devenus le même joueur.
//
// L'équilibre, écrit ici une fois pour toutes: **les rangs portent la force,
// les innées portent la saveur**. Une innée décale un levier d'au plus ce
// qu'un rang vaut, et ne débloque jamais un rang. Aucun réglage ne fait donc
// jouer un débutant comme un expert, et la pastille de niveau sur la plaque
// continue de dire vrai.

// ---------------------------------------------------------------------------
// Ce qu'il est.

enum class Innate { Audace, Prudence, Vigilance, Calme };

// Ce qu'il a appris.
enum class Learned { Lecture, Memoire, Defausse, Maitrise };

inline constexpr int kInnateCount = 4;
inline constexpr int kLearnedCount = 4;

// Quatre niveaux pour une innée (0 à 3), quatre rangs pour une acquise.
inline constexpr int kInnateTop = 3;
inline constexpr int kRungCount = 4;

// Ce qu'une acquise peut accumuler. Les quatre ensemble font les 1200 de
// l'expérience totale, qui est ce que la carte affiche et ce dont le niveau
// -- débutant, intermédiaire, expert -- est tiré.
inline constexpr int kPoolCeiling = 300;

// Les quatre niveaux innés d'un adversaire, et les quatre expériences.
// Volontairement des structures plates et copiables: elles passent de la
// fiche au cerveau sans rien allouer.
struct Innates {
    int level[kInnateCount] = {1, 1, 1, 1};

    int at(Innate which) const { return level[int(which)]; }
    void set(Innate which, int value) { level[int(which)] = qBound(0, value, kInnateTop); }
    bool operator==(const Innates &other) const;
    bool operator!=(const Innates &other) const { return !(*this == other); }
};

struct Pools {
    int xp[kLearnedCount] = {0, 0, 0, 0};

    int at(Learned which) const { return xp[int(which)]; }
    void set(Learned which, int value) { xp[int(which)] = qBound(0, value, kPoolCeiling); }
    int total() const;
};

// Ce qu'une manche a vu un adversaire faire, compétence par compétence.
//
// La table les compte au fil de la manche -- une carte ramassée qui servait,
// une carte donnée qu'on lui a reprise, un joker racheté -- et c'est d'après
// eux que l'expérience de la manche se répartit. Ce qu'une manche rapporte en
// tout ne change pas pour autant: c'est la même somme qu'avant les
// compétences, posée là où il s'est passé quelque chose plutôt qu'en
// quatre parts égales. Un adversaire qui a passé la manche le nez sur la
// défausse apprend la défausse.
struct Deeds {
    int count[kLearnedCount] = {0, 0, 0, 0};

    int at(Learned which) const { return count[int(which)]; }
    void did(Learned which, int times = 1) { count[int(which)] += times; }
    void clear() {
        for (int i = 0; i < kLearnedCount; ++i) count[i] = 0;
    }
};

// ---------------------------------------------------------------------------
// Le niveau, qui est une lecture des quatre acquises ensemble.
//
// Trois noms sont posés sur les plages de l'expérience totale, parce que
// « niveau 437 » n'est pas quelque chose qu'on a envie de lire:
//
//     débutant        0 - 199     throws away what it needs, opens late,
//                                 misreads its own hand
//     intermédiaire 200 - 599     reads its hand properly, watches the
//                                 défausse, keeps what might serve
//     expert        600 - 1200    remembers every card thrown, will not feed
//                                 you, and buys its jokers back

enum class Level { Debutant, Intermediaire, Expert };

// Les quatre compétences pleines, et pas un point de plus.
inline constexpr int kMaxExperience = kLearnedCount * kPoolCeiling;

Level levelOf(int xp);
QString levelName(Level level);
int levelStars(Level level);           // one, two or three
int levelFloor(Level level);
int levelCeiling(Level level);

// Nought to one across the whole range: what the gauge on their card is
// filled to.
qreal masteryOf(int xp);

// Nought to one inside the level they are at, for the gauge under the name.
qreal progressWithin(int xp);

// ---------------------------------------------------------------------------
// Ce qu'on en dit à l'écran.

// Une innée: son nom, sa couleur, le mot de chacun de ses quatre niveaux, et
// ce que ce niveau change à la table -- une ligne au présent, sur ce qui se
// passe, jamais sur un coefficient.
struct InnateSpec {
    QString name;
    int hue;                      // la sienne, chaude: voir rivalColour()
    QString levels[kInnateTop + 1];
    QString says[kInnateTop + 1];
    QString blurbs[kInnateTop + 1];   // la phrase de la carte, quand c'est elle qui domine
};

// Une acquise: son nom, sa couleur, le nom de chacun de ses quatre rangs, et
// ce que le rang change.
struct LearnedSpec {
    QString name;
    int hue;                      // la sienne, froide
    QString rungs[kRungCount];
    QString says[kRungCount];
};

const InnateSpec &innateSpec(Innate which);
const LearnedSpec &learnedSpec(Learned which);

// ---------------------------------------------------------------------------
// Les rangs.

// Ce qu'il faut d'expérience, dans cette compétence-là, pour en être au rang
// qu'on est. Le premier rang est acquis d'emblée: un joueur qui n'a jamais
// joué sait déjà jouer mal.
int rungFloor(int rung);
int rungOf(int poolXp);
// Ce qui manque pour le rang suivant, et zéro au dernier.
int rungGap(int poolXp);
// Nought to one inside the rung one is at, for the little gauge on the row.
qreal rungProgress(int poolXp);

// Zéro à un sur les quatre rangs d'un coup: ce qu'une compétence vaut, prise
// de bout en bout, pour les leviers qui montent sans marche -- la bourde, le
// seuil de la défausse. Le rang est ce qui se voit, ceci est ce qui se sent.
qreal depthOf(int poolXp);

// ---------------------------------------------------------------------------
// Ce que le cerveau joue.
//
// A level is not a number multiplied into a strength somewhere. It is a list
// of things a player does or does not do, and each of them is a thing you can
// watch happen across the table -- which is why they are spelled out one by
// one rather than hidden behind a single difficulty.
struct Skill {
    qreal mastery = 0.0;         // zéro à un, sur l'échelle entière

    // Ce qu'il a appris, rang par rang. Chacun de ces champs est un rang
    // précis d'une des quatre acquises, et c'est là que la force est: un
    // adversaire devient meilleur en les gagnant, pas en étant réglé.
    bool thorough = false;       // Lecture II : reads the hand exactly
    bool countsOpening = false;  // Lecture III : no pose begun that falls short
    bool watches = false;        // Mémoire III : counts what cannot come back
    bool readsTakes = false;     // Mémoire IV : remembers what you picked up
    bool guardsFeed = false;     // Défausse III : will not feed the tapis
    bool readsEndgame = false;   // Défausse IV : plays the last cards as such
    bool buysJokers = false;     // Maîtrise IV
    qreal blunder = 0.0;         // Maîtrise : how often the wrong move is made
    qreal layoffSight = 1.0;     // Maîtrise : how often a card that fits is seen
    qreal pileThreshold = 2.0;   // Mémoire : how much better the défausse must look
    qreal potentialWeight = 1.0; // Lecture : what a combination one card away is worth

    // Ce qu'il est, qui ne le rend ni meilleur ni pire -- seulement
    // reconnaissable. A timid expert and a reckless expert are both experts.
    int openingSlack = 0;        // points past the fifty-one it waits for
    qreal jokerGrip = 0.0;       // reluctance to put a joker down
    qreal keepWeight = 1.0;      // how dearly what might serve is held
    qreal recall = 1.0;          // how reliably the memory actually works
    qreal eagerness = 1.0;       // how readily what can go down goes down
};

// Un adversaire, réduit à ce que son cerveau joue. La seule fonction qui
// compte ici, et la seule qui ait le droit de décider ce qu'une compétence
// vaut.
Skill composeSkill(const Innates &innates, const Pools &pools);

// Ce que le meilleur joueur que le jeu sache faire jouerait: le bouton
// Conseil, et l'étalon de l'arène. Tous les rangs au dernier, toutes les
// innées au bout utile, et pas une bourde.
Skill bestSkill();

// Une expérience unique répartie sur les quatre compétences, penchée du côté
// de ce que l'adversaire est. Sert deux fois: pour le niveau de départ des
// six, et pour reprendre l'expérience d'un profil d'avant les compétences.
Pools poolsFromXp(int xp, const Innates &innates);

// À quelle vitesse cette compétence-là s'apprend, chez lui. Un entre un et un
// et demi: les innées pressent l'apprentissage, elles ne le remplacent pas.
qreal learnRate(const Innates &innates, Learned which);

// La phrase qui le décrit, tirée de ses innées -- celle que porte sa carte
// quand on l'a changé, pour qu'elle ne mente pas sur ce qu'il est devenu.
QString blurbFrom(const Innates &innates);

// Celle de ses innées qui frappe le plus, sur laquelle la phrase est prise.
Innate dominantInnate(const Innates &innates);
