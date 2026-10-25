#pragma once

#include "RivalSkills.h"

#include <QString>
#include <QVector>

// Les six adversaires et leur expérience acquise.
//
// Ils ne constituent pas six réglages d'un même curseur de difficulté. Chacun
// dispose de sa propre progression : deux débutent en ne connaissant presque
// rien du jeu, trois le maîtrisent, et Oriane le pratique de longue date.
// L'expérience acquise au cours des parties jouées est conservée entre les
// sessions, de sorte que le niveau des adversaires continue de s'élever au fil
// du temps.
//
// Deux catégories les distinguent, décrites dans RivalSkills.h : quatre
// compétences innées, qui constituent leur tempérament et demeurent fixes, et
// quatre compétences acquises, chacune dotée de sa propre expérience, qui
// progressent au fil des parties. Ce fichier se borne à enregistrer ces
// valeurs par profil et à les relire : les six adversaires sont communs à tous
// les profils, mais leur expérience appartient au profil sous lequel elle a
// été acquise.
//
// Chacun peut être réinitialisé individuellement depuis sa fiche : son
// expérience seule, ou son tempérament, cette seconde opération entraînant la
// perte de son expérience.

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

// Résultats d'un adversaire pour les parties jouées sous ce profil.
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

// Tempérament de l'adversaire sur ce profil, et phrase correspondante.
Innates rivalInnate(const QString &rivalId);
Innates rivalInnate(const QString &rivalId, int slot);

// Modifie le tempérament d'un adversaire, ce qui annule son expérience.
//
// Les innées déterminent la vitesse de progression des acquises : l'expérience
// obtenue avec un tempérament donné n'est donc pas transposable à un autre.
// Modifier le tempérament remet à zéro les quatre expériences et le palmarès,
// opération effectuée par cette fonction afin que les deux ne puissent être
// dissociées, quel que soit l'appelant. Sans changement effectif, la fonction
// n'a aucun effet.
void setRivalInnate(const QString &rivalId, const Innates &innate);

// Restaure le tempérament initial de l'adversaire, avec les mêmes
// conséquences sur son expérience.
void resetRivalInnate(const QString &rivalId);

// Phrase affichée sur sa carte : la phrase d'origine tant que son tempérament
// n'a pas été modifié, et celle déduite de ses innées dans le cas contraire,
// afin que la carte ne décrive pas un joueur différent de celui qui est en
// jeu.
QString rivalBlurb(const QString &rivalId);

// Paramètres de jeu de cet adversaire dans son état courant.
Skill skillFor(const QString &rivalId);
Skill skillFor(const QString &rivalId, int slot);

// Nom sous lequel l'adversaire apparaît sur ce profil.
//
// Les six adversaires restent les mêmes : même couleur, même emblème, même
// logique de jeu, même expérience. Seul le nom varie, et il varie par profil,
// comme toute donnée attachée à un profil. En l'absence de nom attribué, le
// nom d'origine est employé.
QString rivalName(const QString &rivalId);
QString rivalName(const QString &rivalId, int slot);
void setRivalName(const QString &rivalId, const QString &name);
void clearRivalName(const QString &rivalId);

// Longueur maximale d'un nom attribué, déterminée par la place disponible sur
// la plaque, à côté de l'indicateur de niveau.
inline constexpr int kRivalNameLength = 18;

// Comptabilisé à la fin de chaque manche. `place` vaut zéro pour le joueur
// ayant obtenu le meilleur résultat ; `wentOut` vaut vrai pour celui qui s'est
// débarrassé de sa dernière carte, ce qui rapporte davantage qu'une deuxième
// place serrée.
void noteRivalHand(const QString &rivalId, int place, int seats, bool wentOut,
                   const Deeds &deeds);
void noteRivalMatch(const QString &rivalId, bool won);

// Ramène cet adversaire à son niveau initial, pour ce profil uniquement. La
// fiche propose cette opération adversaire par adversaire et non globalement,
// afin de pouvoir réinitialiser l'un sans affecter les autres. Le tempérament
// est conservé.
void resetRivalExperience(const QString &rivalId);
