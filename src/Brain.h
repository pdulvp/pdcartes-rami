#pragma once

#include "Cards.h"
#include "Rami.h"
#include "Rivals.h"

#include <QString>
#include <QVector>

// Logique de décision d'un adversaire.
//
// Ses capacités sont décrites ailleurs : les huit compétences, leurs niveaux
// et leur valeur figurent dans RivalSkills.h, et leur état pour un profil
// donné dans Rivals.h. Ce fichier se limite au choix du coup : il reçoit un
// `Skill`, énumération des comportements que ce joueur adopte ou non, et en
// déduit le coup à jouer.
//
// Principe directeur de ce fichier : un niveau n'est pas un coefficient
// appliqué à une force globale, mais un ensemble de comportements que le
// joueur adopte ou non, chacun observable en cours de partie — une main
// analysée trop vite, une carte qui n'est pas défaussée, un joker racheté, une
// erreur.

// Informations publiques accumulées depuis le début de la manche.
//
// Elles sont tenues à jour par la table et transmises à chaque adversaire à
// son tour : les cartes qui ont été face visible, et celles que chaque joueur
// a ramassées sur la défausse. Toutes ces informations sont publiques ; un
// expert est un joueur qui les mémorise, et non un joueur autorisé à consulter
// la main des autres.
struct Watch {
    QVector<Card> seen;
    QVector<QVector<Card>> taken;   // one list per seat

    void note(const Card &card);
    void noteTaken(int seat, const Card &card, int seats);
    void clear();
    // Nombre d'exemplaires de cette carte, sur les deux que compte le jeu, que
    // personne n'a encore vus.
    int unseenCopies(int rank, Suit suit) const;
};

struct Move {
    enum Kind { None, DrawStock, DrawPile, Lay, LayOff, BuyJoker, TakeBack, Discard };
    Kind kind = None;
    QVector<int> cards;    // Lay: the combination. The rest: one card.
    int meldId = -1;
};

// Détermine le coup à jouer dans l'état courant de la partie. Appelée
// successivement par la table jusqu'à l'achèvement du tour, afin que chaque
// action de l'adversaire puisse être affichée au moment où elle survient
// plutôt que toutes à la fois.
Move chooseMove(const Rami &game, int at, const Skill &skill, const Watch &watch);

// Décrit en français le coup que jouerait le meilleur joueur de la table avec
// la main du joueur. C'est la fonction du bouton Conseil ; elle emploie la
// même logique de décision que les adversaires, aucune logique distincte
// n'étant réservée au joueur.
QString advise(const Rami &game, int at, const Watch &watch);
