#pragma once

#include <QString>
#include <QVector>

// What a card is, and what it is worth.
//
// The rami is played with two full packs shuffled together and four jokers on
// top of them -- a hundred and eight cards -- which is why a card carries an
// identity of its own as well as a face: there are two kings of hearts in the
// pack, they are not the same card, and a hand that holds both has to be able
// to say so.

enum class Suit { Pique = 0, Coeur = 1, Carreau = 2, Trefle = 3 };

// The ranks, as the French pack names them: l'as, puis deux à dix, le valet,
// la dame, le roi. Nought is the joker, which has no suit either.
inline constexpr int kAs = 1;
inline constexpr int kValet = 11;
inline constexpr int kDame = 12;
inline constexpr int kRoi = 13;
inline constexpr int kJoker = 0;

struct Card {
    int id = -1;        // which card of the hundred and eight this is
    int rank = kJoker;  // 1 = as ... 13 = roi, 0 = joker
    Suit suit = Suit::Pique;

    bool joker() const { return rank == kJoker; }
    bool valid() const { return id >= 0; }

    // The same card to look at -- the two kings of hearts are, and that is
    // what matters when one is being swapped for a joker.
    bool sameFace(const Card &other) const {
        return rank == other.rank && (joker() || suit == other.suit);
    }
};

inline bool operator==(const Card &a, const Card &b) { return a.id == b.id; }

// The pack: two of each of the fifty-two, then four jokers. In order, so
// shuffling it is the only randomness there is.
QVector<Card> freshPack();
void shuffleCards(QVector<Card> &cards);

// How many cards the pack holds, and how many are dealt to each player.
inline constexpr int kPackSize = 108;
inline constexpr int kHandSize = 14;

bool isRed(Suit suit);
QString suitPip(Suit suit);            // the symbol itself
QString suitName(Suit suit);           // "pique", for a sentence
QString rankLabel(int rank);           // "A", "2".."10", "V", "D", "R"
QString cardLabel(const Card &card);   // "R♥", for a tight space
QString cardName(const Card &card);    // "le roi de cœur", for a sentence

// What a card left in the hand costs when the hand ends. The joker is the
// expensive one -- twenty-five points -- which is the whole reason nobody
// hoards it.
int penaltyValue(const Card &card);

// What a rank is worth inside a combination, read in the space a suite is
// counted in: one to fourteen, where fourteen is the as placed above the roi.
// The as is worth eleven at either end and one only in as-deux-trois.
int runRankValue(int runRank);

// What a rank is worth inside a série, where the as is always eleven.
int setRankValue(int rank);

// Tidying the hand. By suit for finding suites, by rank for finding séries:
// the two orders answer different questions, which is why there are two.
void sortBySuit(QVector<Card> &cards);
void sortByRank(QVector<Card> &cards);

// Takes the card with this identity out of `cards` and hands it back; an
// invalid card when it was not there.
Card takeCard(QVector<Card> &cards, int id);
const Card *findCard(const QVector<Card> &cards, int id);
