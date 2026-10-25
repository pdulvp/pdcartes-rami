#pragma once

#include "Cards.h"

#include <QVector>

// Définition des combinaisons, et décomposition d'une main en combinaisons.
//
// Le rami n'en connaît que deux :
//
//   une suite  — trois cartes ou plus de la même couleur qui se suivent :
//                7-8-9 de cœur. L'as se place sous le deux ou au-dessus du
//                roi, jamais les deux à la fois et sans continuité entre le
//                roi et le deux.
//   une série  — trois ou quatre cartes du même rang, de couleurs
//                différentes. Les deux jeux fournissant deux dames de cœur,
//                celles-ci comptent pour une même couleur et ne forment pas
//                une série.
//
// Un joker remplace une carte quelconque, à raison d'un seul par combinaison.
// La carte qu'il représente est enregistrée et non laissée indéterminée :
// lors d'un rachat, la réponse doit être celle qui prévalait au moment où la
// combinaison a été posée.

enum class MeldKind { None, Suite, Serie };

struct Meld {
    int id = -1;
    int owner = -1;
    MeldKind kind = MeldKind::None;

    // Une suite est conservée dans l'ordre croissant, le joker occupant la
    // position de la carte qu'il remplace. Une série est conservée par
    // couleur, le joker en dernier.
    QVector<Card> cards;

    // Suites uniquement : position de la première carte sur l'échelle de un à
    // quatorze propre aux suites, quatorze désignant l'as placé au-dessus du
    // roi.
    int low = 0;

    bool valid() const { return kind != MeldKind::None && cards.size() >= 3; }
    bool hasJoker() const;
    int jokerAt() const;             // position du joker, ou -1
    int runRankAt(int index) const { return low + index; }
    int rank() const;                // séries uniquement : le rang commun
    Suit suit() const;               // suites uniquement : la couleur commune
    int points() const;
    QString spell() const;           // « 7-8-9 de cœur », pour une phrase
};

// Indique si ces cartes forment une combinaison. Le cas échéant, `out` reçoit
// la combinaison ordonnée.
bool makeMeld(const QVector<Card> &cards, Meld *out);

// Indique si `card` peut être ajoutée à `meld`. Le cas échéant, `out` reçoit
// la combinaison complétée.
bool canExtend(const Meld &meld, const Card &card, Meld *out);

// Retire une carte d'une combinaison, lorsque celle-ci reste valide sans
// elle. Opération inverse de `canExtend` ; retourne faux s'il resterait moins
// de trois cartes, ou si le reste ne forme plus une combinaison.
//
// `out` conserve autant que possible la carte représentée par le joker, là où
// `makeMeld` réinterpréterait les cartes restantes et lui attribuerait parfois
// une autre valeur : retirer le 3 de 3-★-5-6, où le joker vaut le quatre,
// produirait 5-6-★ avec le joker en sept, c'est-à-dire une pose n'ayant rien
// perdu des trois points qu'on vient de lui retirer.
bool shorten(const Meld &meld, int cardId, Meld *out);

// Cartes permettant de racheter le joker de `meld`. Une suite n'en admet
// qu'une ; une série en admet une par couleur absente.
QVector<Card> jokerWants(const Meld &meld);

// Valeur des combinaisons d'une main, et cartes non combinées.
struct Plan {
    QVector<Meld> melds;
    QVector<Card> rest;
    int points = 0;      // total des combinaisons
    int deadwood = 0;    // pénalité des cartes restantes si la manche s'arrêtait
};

// Modes de lecture d'une main.
//
//   Balanced   — la décomposition laissant le moins de points en main ;
//                celle qu'un joueur recherche au moment de choisir sa
//                défausse.
//   MostPoints — la décomposition de plus forte valeur ; celle qu'un joueur
//                recherche pour déterminer s'il peut atteindre cinquante et
//                un.
//   Greedy     — la plus grande combinaison d'abord, puis ce qui s'y ajoute ;
//                c'est ainsi qu'un débutant lit sa main, et la raison pour
//                laquelle il laisse passer des ouvertures possibles.
enum class Reading { Balanced, MostPoints, Greedy };

Plan readHand(const QVector<Card> &hand, Reading how);

// Toutes les combinaisons que contient la main. Ces combinaisons constituent
// l'ensemble parmi lequel les décompositions sont choisies et, pour un
// adversaire évaluant sa défausse, l'inventaire du rôle de chaque carte.
QVector<Meld> allMelds(const QVector<Card> &hand);

// Total requis pour ouvrir : cinquante et un points en un seul tour.
inline constexpr int kOpeningPoints = 51;
