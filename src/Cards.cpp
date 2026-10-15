#include "Cards.h"

#include <QCoreApplication>
#include <QRandomGenerator>

#include <algorithm>

QVector<Card> freshPack() {
    QVector<Card> pack;
    pack.reserve(kPackSize);
    int id = 0;
    for (int copy = 0; copy < 2; ++copy) {
        for (int suit = 0; suit < 4; ++suit) {
            for (int rank = kAs; rank <= kRoi; ++rank) {
                Card card;
                card.id = id++;
                card.rank = rank;
                card.suit = Suit(suit);
                pack.append(card);
            }
        }
    }
    for (int i = 0; i < 4; ++i) {
        Card joker;
        joker.id = id++;
        joker.rank = kJoker;
        pack.append(joker);
    }
    return pack;
}

void shuffleCards(QVector<Card> &cards) {
    // Fisher-Yates, drawing from Qt's generator rather than from std::shuffle:
    // one source of randomness in the program, and it is the one the deal and
    // the rivals' hesitations both come out of.
    for (int i = cards.size() - 1; i > 0; --i) {
        const int j = QRandomGenerator::global()->bounded(i + 1);
        std::swap(cards[i], cards[j]);
    }
}

bool isRed(Suit suit) { return suit == Suit::Coeur || suit == Suit::Carreau; }

QString suitPip(Suit suit) {
    switch (suit) {
    case Suit::Pique: return QStringLiteral("♠");
    case Suit::Coeur: return QStringLiteral("♥");
    case Suit::Carreau: return QStringLiteral("♦");
    case Suit::Trefle: return QStringLiteral("♣");
    }
    return QString();
}

QString suitName(Suit suit) {
    switch (suit) {
    case Suit::Pique: return QCoreApplication::translate("cards", "pique");
    case Suit::Coeur: return QCoreApplication::translate("cards", "cœur");
    case Suit::Carreau: return QCoreApplication::translate("cards", "carreau");
    case Suit::Trefle: return QCoreApplication::translate("cards", "trèfle");
    }
    return QString();
}

QString rankLabel(int rank) {
    switch (rank) {
    case kJoker: return QStringLiteral("★");
    case kAs: return QCoreApplication::translate("cards", "A");
    case kValet: return QCoreApplication::translate("cards", "V");
    case kDame: return QCoreApplication::translate("cards", "D");
    case kRoi: return QCoreApplication::translate("cards", "R");
    default: break;
    }
    return QString::number(rank);
}

QString cardLabel(const Card &card) {
    if (!card.valid()) return QString();
    if (card.joker()) return QCoreApplication::translate("cards", "joker");
    return rankLabel(card.rank) + suitPip(card.suit);
}

QString cardName(const Card &card) {
    if (!card.valid()) return QString();
    if (card.joker()) return QCoreApplication::translate("cards", "un joker");

    QString head;
    switch (card.rank) {
    case kAs: head = QCoreApplication::translate("cards", "l'as"); break;
    case kValet: head = QCoreApplication::translate("cards", "le valet"); break;
    case kDame: head = QCoreApplication::translate("cards", "la dame"); break;
    case kRoi: head = QCoreApplication::translate("cards", "le roi"); break;
    default: head = QCoreApplication::translate("cards", "le %1").arg(card.rank); break;
    }
    return QCoreApplication::translate("cards", "%1 de %2").arg(head, suitName(card.suit));
}

int penaltyValue(const Card &card) {
    if (!card.valid()) return 0;
    if (card.joker()) return 25;
    if (card.rank == kAs) return 11;
    if (card.rank >= kValet) return 10;
    return card.rank;
}

int runRankValue(int runRank) {
    if (runRank <= 1) return 1;           // l'as sous le deux
    if (runRank >= 14) return 11;         // l'as au-dessus du roi
    if (runRank >= kValet) return 10;
    return runRank;
}

int setRankValue(int rank) {
    if (rank == kAs) return 11;
    if (rank >= kValet) return 10;
    return rank;
}

void sortBySuit(QVector<Card> &cards) {
    std::stable_sort(cards.begin(), cards.end(), [](const Card &a, const Card &b) {
        // Jokers at the right-hand end of the hand, where they are not in the
        // way of reading a suite off the cards beside them.
        if (a.joker() != b.joker()) return b.joker();
        if (a.joker()) return a.id < b.id;
        if (a.suit != b.suit) return int(a.suit) < int(b.suit);
        if (a.rank != b.rank) return a.rank < b.rank;
        return a.id < b.id;
    });
}

void sortByRank(QVector<Card> &cards) {
    std::stable_sort(cards.begin(), cards.end(), [](const Card &a, const Card &b) {
        if (a.joker() != b.joker()) return b.joker();
        if (a.joker()) return a.id < b.id;
        if (a.rank != b.rank) return a.rank < b.rank;
        if (a.suit != b.suit) return int(a.suit) < int(b.suit);
        return a.id < b.id;
    });
}

Card takeCard(QVector<Card> &cards, int id) {
    for (int i = 0; i < cards.size(); ++i) {
        if (cards.at(i).id != id) continue;
        const Card found = cards.at(i);
        cards.remove(i);
        return found;
    }
    return Card{};
}

const Card *findCard(const QVector<Card> &cards, int id) {
    for (const Card &card : cards) {
        if (card.id == id) return &card;
    }
    return nullptr;
}
