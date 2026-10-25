#pragma once

#include <QString>
#include <QVector>

// Définition d'une carte et de sa valeur.
//
// Le rami se joue avec deux jeux de cinquante-deux cartes mélangés et quatre
// jokers, soit cent huit cartes. Chaque carte porte donc une identité propre
// en plus de sa face : le jeu contient deux rois de cœur, qui sont deux
// cartes distinctes, et une main détenant les deux doit pouvoir les
// distinguer.

enum class Suit { Pique = 0, Coeur = 1, Carreau = 2, Trefle = 3 };

// Les rangs, selon la nomenclature du jeu français : l'as, les cartes de deux
// à dix, le valet, la dame et le roi. La valeur zéro désigne le joker, qui
// n'a pas non plus de couleur.
inline constexpr int kAs = 1;
inline constexpr int kValet = 11;
inline constexpr int kDame = 12;
inline constexpr int kRoi = 13;
inline constexpr int kJoker = 0;

struct Card {
    int id = -1;        // numéro de la carte parmi les cent huit
    int rank = kJoker;  // 1 = as ... 13 = roi, 0 = joker
    Suit suit = Suit::Pique;

    bool joker() const { return rank == kJoker; }
    bool valid() const { return id >= 0; }

    // Indique si les deux cartes ont la même face. C'est le cas des deux rois
    // de cœur, et c'est le seul critère qui importe lors du rachat d'un
    // joker.
    bool sameFace(const Card &other) const {
        return rank == other.rank && (joker() || suit == other.suit);
    }
};

inline bool operator==(const Card &a, const Card &b) { return a.id == b.id; }

// Le jeu complet : deux exemplaires de chacune des cinquante-deux cartes,
// puis quatre jokers. Il est produit dans l'ordre, le mélange constituant la
// seule source d'aléa.
QVector<Card> freshPack();
void shuffleCards(QVector<Card> &cards);

// Nombre de cartes du jeu, et nombre de cartes distribuées à chaque joueur.
inline constexpr int kPackSize = 108;
inline constexpr int kHandSize = 14;

bool isRed(Suit suit);
QString suitPip(Suit suit);            // le symbole seul
QString suitName(Suit suit);           // « pique », pour une phrase
QString rankLabel(int rank);           // « A », « 2 » à « 10 », « V », « D », « R »
QString cardLabel(const Card &card);   // « R♥ », pour un espace réduit
QString cardName(const Card &card);    // « le roi de cœur », pour une phrase

// Pénalité d'une carte restée en main à la fin de la manche. Le joker est le
// plus coûteux, avec vingt-cinq points, ce qui dissuade de le conserver.
int penaltyValue(const Card &card);

// Valeur d'un rang au sein d'une suite, exprimée dans l'échelle de un à
// quatorze propre aux suites, où quatorze désigne l'as placé au-dessus du
// roi. L'as vaut onze aux deux extrémités, et un uniquement dans la suite
// as-deux-trois.
int runRankValue(int runRank);

// Valeur d'un rang au sein d'une série, où l'as vaut toujours onze.
int setRankValue(int rank);

// Tri de la main. Le tri par couleur sert à repérer les suites, le tri par
// rang à repérer les séries : les deux ordres répondent à des questions
// différentes, d'où leur coexistence.
void sortBySuit(QVector<Card> &cards);
void sortByRank(QVector<Card> &cards);

// Retire de `cards` la carte portant cet identifiant et la retourne.
// Retourne une carte invalide si elle ne s'y trouvait pas.
Card takeCard(QVector<Card> &cards, int id);
const Card *findCard(const QVector<Card> &cards, int id);
