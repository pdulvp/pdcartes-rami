#include "Melds.h"

#include <QCoreApplication>
#include <QStringList>

#include <algorithm>
#include <vector>

namespace {

// Étendue de l'échelle sur laquelle les suites sont comptées : un pour l'as
// placé sous le deux, quatorze pour l'as placé au-dessus du roi.
constexpr int kLowest = 1;
constexpr int kHighest = 14;

int runRankOf(const Card &card, bool aceHigh) {
    if (card.rank == kAs) return aceHigh ? kHighest : kLowest;
    return card.rank;
}

// Carte occupant une position de l'échelle, c'est-à-dire le rang auquel l'as
// est ramené à sa valeur propre.
int cardRankForRun(int runRank) { return runRank >= kHighest ? kAs : runRank; }

// Construit une suite à partir d'un intervalle de l'échelle : les cartes
// présentes occupent leur position, et le joker occupe la position manquante.
bool buildSuite(const QVector<Card> &cards, bool aceHigh, Meld *out) {
    const int length = cards.size();

    QVector<Card> real;
    Card joker;
    for (const Card &card : cards) {
        if (card.joker()) {
            if (joker.valid()) return false;   // one joker to a combination
            joker = card;
        } else {
            real.append(card);
        }
    }
    if (real.size() < 2) return false;

    const Suit suit = real.first().suit;
    int lowest = kHighest + 1;
    int highest = kLowest - 1;
    QVector<int> seen;
    for (const Card &card : real) {
        if (card.suit != suit) return false;
        const int at = runRankOf(card, aceHigh);
        if (seen.contains(at)) return false;   // a suite does not repeat a rank
        seen.append(at);
        lowest = std::min(lowest, at);
        highest = std::max(highest, at);
    }
    if (highest - lowest + 1 > length) return false;

    // Position de départ de l'intervalle, choisie la plus haute possible afin
    // qu'un joker pouvant occuper l'une ou l'autre extrémité occupe la plus
    // élevée : sa valeur y est supérieure et, surtout, le résultat doit être
    // identique chaque fois que les mêmes cartes sont posées.
    const int start = std::min(lowest, kHighest - length + 1);
    if (start < std::max(kLowest, highest - length + 1)) return false;
    if (start < kLowest || start + length - 1 > kHighest) return false;

    Meld built;
    built.kind = MeldKind::Suite;
    built.low = start;
    built.cards.reserve(length);
    for (int at = start; at < start + length; ++at) {
        const Card *found = nullptr;
        for (const Card &card : real) {
            if (runRankOf(card, aceHigh) == at) {
                found = &card;
                break;
            }
        }
        if (found) {
            built.cards.append(*found);
        } else if (joker.valid() && built.jokerAt() < 0) {
            built.cards.append(joker);
        } else {
            return false;
        }
    }
    if (built.cards.size() != length) return false;
    *out = built;
    return true;
}

bool buildSerie(const QVector<Card> &cards, Meld *out) {
    if (cards.size() > 4) return false;   // there are only four colours

    QVector<Card> real;
    Card joker;
    for (const Card &card : cards) {
        if (card.joker()) {
            if (joker.valid()) return false;
            joker = card;
        } else {
            real.append(card);
        }
    }
    if (real.size() < 2) return false;

    const int rank = real.first().rank;
    QVector<int> suits;
    for (const Card &card : real) {
        if (card.rank != rank) return false;
        if (suits.contains(int(card.suit))) return false;   // the same colour twice
        suits.append(int(card.suit));
    }

    Meld built;
    built.kind = MeldKind::Serie;
    built.low = 0;
    built.cards = real;
    std::stable_sort(built.cards.begin(), built.cards.end(),
                     [](const Card &a, const Card &b) { return int(a.suit) < int(b.suit); });
    if (joker.valid()) built.cards.append(joker);
    *out = built;
    return true;
}

// ---------------------------------------------------------------------------
// Décomposition d'une main.
//
// Toutes les combinaisons que contient la main sont d'abord énumérées, puis le
// meilleur sous-ensemble de combinaisons ne se disputant aucune carte est
// sélectionné. Cette sélection est exacte et non approchée : une main compte
// quinze cartes, l'ensemble des cartes déjà employées tient donc sur quinze
// bits, soit un peu plus de trente mille combinaisons possibles. La valeur
// maximale d'une main est ainsi obtenue par énumération exhaustive plutôt que
// par heuristique.

struct Candidate {
    quint32 mask = 0;
    int value = 0;
    int first = 0;      // the lowest card it uses, which is how they are filed
    Meld meld;
};

void addCandidate(QVector<Candidate> &into, const QVector<Card> &hand,
                  const QVector<int> &indices) {
    if (indices.size() < 3) return;

    QVector<Card> cards;
    quint32 mask = 0;
    int first = hand.size();
    for (int at : indices) {
        cards.append(hand.at(at));
        mask |= (quint32(1) << at);
        first = std::min(first, at);
    }

    Meld built;
    if (!makeMeld(cards, &built)) return;

    Candidate candidate;
    candidate.mask = mask;
    candidate.value = built.points();
    candidate.first = first;
    candidate.meld = built;
    into.append(candidate);
}

// Énumère toutes les façons de choisir un exemplaire de chaque carte requise.
// Deux jeux étant mélangés ensemble, une main peut contenir deux exemplaires
// d'une même carte, et une même combinaison peut alors être construite de deux
// manières. Ce cas n'a d'incidence que si l'autre exemplaire est simultanément
// requis par une autre combinaison. La situation étant rare, le produit
// cartésien est énuméré plutôt que traité analytiquement.
void eachChoice(const QVector<QVector<int>> &options, const QVector<int> &extra,
                QVector<Candidate> &into, const QVector<Card> &hand) {
    QVector<int> at(options.size(), 0);
    constexpr int kMostWays = 48;
    int ways = 0;
    while (true) {
        QVector<int> picked = extra;
        for (int i = 0; i < options.size(); ++i) picked.append(options.at(i).at(at.at(i)));
        addCandidate(into, hand, picked);
        if (++ways >= kMostWays) return;

        int digit = 0;
        while (digit < at.size()) {
            if (++at[digit] < options.at(digit).size()) break;
            at[digit] = 0;
            ++digit;
        }
        if (digit >= at.size()) return;
    }
}

QVector<Candidate> candidatesFor(const QVector<Card> &hand) {
    QVector<Candidate> found;
    const int count = hand.size();
    if (count < 3) return found;

    QVector<int> jokers;
    for (int i = 0; i < count; ++i) {
        if (hand.at(i).joker()) jokers.append(i);
    }

    // Les séries : un rang, décliné en trois ou quatre couleurs.
    for (int rank = kAs; rank <= kRoi; ++rank) {
        QVector<QVector<int>> bySuit(4);
        int present = 0;
        for (int i = 0; i < count; ++i) {
            const Card &card = hand.at(i);
            if (card.joker() || card.rank != rank) continue;
            if (bySuit[int(card.suit)].isEmpty()) ++present;
            bySuit[int(card.suit)].append(i);
        }
        if (present < 2) continue;

        for (int bits = 0; bits < 16; ++bits) {
            QVector<QVector<int>> chosen;
            for (int suit = 0; suit < 4; ++suit) {
                if (!(bits & (1 << suit))) continue;
                if (bySuit.at(suit).isEmpty()) {
                    chosen.clear();
                    break;
                }
                chosen.append(bySuit.at(suit));
            }
            if (chosen.size() >= 3) eachChoice(chosen, {}, found, hand);
            if (chosen.size() == 2) {
                // Deux couleurs, le joker tenant lieu de la troisième.
                for (int joker : jokers) eachChoice(chosen, {joker}, found, hand);
            }
        }
    }

    // Les suites : une couleur, et un intervalle de l'échelle.
    for (int suit = 0; suit < 4; ++suit) {
        for (int aceHigh = 0; aceHigh < 2; ++aceHigh) {
            QVector<QVector<int>> byRank(kHighest + 2);
            for (int i = 0; i < count; ++i) {
                const Card &card = hand.at(i);
                if (card.joker() || int(card.suit) != suit) continue;
                const int at = runRankOf(card, aceHigh != 0);
                byRank[at].append(i);
            }
            // L'as compté dans l'autre sens donne le même intervalle ; il n'est
            // donc utile de parcourir la partie haute de l'échelle que si elle
            // contient un as.
            if (aceHigh != 0 && byRank.at(kHighest).isEmpty()) continue;

            for (int start = kLowest; start <= kHighest - 2; ++start) {
                for (int length = 3; start + length - 1 <= kHighest; ++length) {
                    QVector<QVector<int>> chosen;
                    int gaps = 0;
                    int gapAt = -1;
                    for (int at = start; at < start + length; ++at) {
                        if (byRank.at(at).isEmpty()) {
                            ++gaps;
                            gapAt = at;
                        } else {
                            chosen.append(byRank.at(at));
                        }
                    }
                    if (gaps > 1) break;   // a longer window can only be worse
                    if (chosen.size() < 2) continue;
                    if (gaps == 0) {
                        eachChoice(chosen, {}, found, hand);
                    } else if (gapAt > 0) {
                        for (int joker : jokers) eachChoice(chosen, {joker}, found, hand);
                    }
                }
            }
        }
    }

    // Une même combinaison peut être obtenue deux fois, par exemple par un
    // intervalle parcouru depuis l'une ou l'autre extrémité de l'échelle. La
    // sélection effectuée plus bas y est indifférente, mais le volume de
    // calcul ne l'est pas.
    std::stable_sort(found.begin(), found.end(), [](const Candidate &a, const Candidate &b) {
        if (a.mask != b.mask) return a.mask < b.mask;
        return a.value > b.value;
    });
    found.erase(std::unique(found.begin(), found.end(),
                            [](const Candidate &a, const Candidate &b) {
                                return a.mask == b.mask;
                            }),
                found.end());
    return found;
}

} // namespace

bool Meld::hasJoker() const { return jokerAt() >= 0; }

int Meld::jokerAt() const {
    for (int i = 0; i < cards.size(); ++i) {
        if (cards.at(i).joker()) return i;
    }
    return -1;
}

int Meld::rank() const {
    for (const Card &card : cards) {
        if (!card.joker()) return card.rank;
    }
    return 0;
}

Suit Meld::suit() const {
    for (const Card &card : cards) {
        if (!card.joker()) return card.suit;
    }
    return Suit::Pique;
}

int Meld::points() const {
    if (kind == MeldKind::Serie) return cards.size() * setRankValue(rank());
    int total = 0;
    for (int i = 0; i < cards.size(); ++i) total += runRankValue(runRankAt(i));
    return total;
}

QString Meld::spell() const {
    QStringList parts;
    for (int i = 0; i < cards.size(); ++i) {
        const Card &card = cards.at(i);
        if (card.joker()) {
            parts.append(QStringLiteral("★"));
        } else if (kind == MeldKind::Serie) {
            parts.append(rankLabel(card.rank) + suitPip(card.suit));
        } else {
            parts.append(rankLabel(card.rank));
        }
    }
    if (kind == MeldKind::Suite) {
        return QCoreApplication::translate("melds", "%1 de %2")
            .arg(parts.join(QLatin1Char('-')), suitName(suit()));
    }
    return parts.join(QLatin1Char(' '));
}

bool makeMeld(const QVector<Card> &cards, Meld *out) {
    if (!out || cards.size() < 3) return false;

    // Deux cartes de même rang accompagnées d'un joker ne pourraient
    // s'interpréter des deux façons que si ces deux cartes étaient de même
    // couleur, ce qui est impossible : une série les exige de couleurs
    // différentes, une suite les exige de même couleur. Les deux tests ne
    // peuvent donc être satisfaits simultanément, et leur ordre est
    // indifférent.
    int firstRank = 0;
    bool oneRank = true;
    for (const Card &card : cards) {
        if (card.joker()) continue;
        if (firstRank == 0) {
            firstRank = card.rank;
        } else if (card.rank != firstRank) {
            oneRank = false;
        }
    }
    if (oneRank && buildSerie(cards, out)) return true;
    if (buildSuite(cards, false, out)) return true;
    return buildSuite(cards, true, out);
}

bool shorten(const Meld &meld, int cardId, Meld *out) {
    if (!meld.valid() || !out) return false;

    int at = -1;
    for (int i = 0; i < meld.cards.size(); ++i) {
        if (meld.cards.at(i).id == cardId) at = i;
    }
    if (at < 0) return false;

    QVector<Card> rest = meld.cards;
    rest.remove(at);
    if (rest.size() < 3) return false;

    // Suite dont on retire une extrémité : le reste est déjà ordonné et
    // demeure consécutif, et le joker continue de représenter la même carte.
    // Son rang vaut `low` augmenté de sa position ; lorsque la première carte
    // est retirée, les deux varient du même pas.
    if (meld.kind == MeldKind::Suite && (at == 0 || at == meld.cards.size() - 1)) {
        Meld kept = meld;
        kept.cards = rest;
        if (at == 0) kept.low = meld.low + 1;
        *out = kept;
        return true;
    }

    // Tous les autres cas — une série, ou une suite entamée en son milieu —
    // sont réinterprétés : une suite privée d'une carte médiane n'est valide
    // que si le joker comble le vide, ce que détermine `makeMeld`.
    return makeMeld(rest, out);
}

bool canExtend(const Meld &meld, const Card &card, Meld *out) {
    if (!meld.valid() || !card.valid() || !out) return false;

    if (meld.kind == MeldKind::Serie) {
        if (meld.cards.size() >= 4) return false;
        if (card.joker()) {
            if (meld.hasJoker()) return false;
            Meld grown = meld;
            grown.cards.append(card);
            *out = grown;
            return true;
        }
        if (card.rank != meld.rank()) return false;
        for (const Card &held : meld.cards) {
            if (!held.joker() && held.suit == card.suit) return false;
        }
        Meld grown = meld;
        const int joker = grown.jokerAt();
        if (joker >= 0) grown.cards.remove(joker);
        grown.cards.append(card);
        std::stable_sort(grown.cards.begin(), grown.cards.end(),
                         [](const Card &a, const Card &b) { return int(a.suit) < int(b.suit); });
        if (joker >= 0) grown.cards.append(meld.cards.at(joker));
        *out = grown;
        return true;
    }

    if (meld.kind != MeldKind::Suite) return false;
    const int top = meld.low + meld.cards.size() - 1;

    if (card.joker()) {
        if (meld.hasJoker()) return false;
        Meld grown = meld;
        if (top + 1 <= kHighest) {
            grown.cards.append(card);
        } else if (meld.low - 1 >= kLowest) {
            grown.cards.prepend(card);
            grown.low = meld.low - 1;
        } else {
            return false;
        }
        *out = grown;
        return true;
    }

    if (card.suit != meld.suit()) return false;
    // L'as peut occuper l'une ou l'autre extrémité de l'échelle ; les deux
    // positions sont donc essayées.
    for (int aceHigh = 0; aceHigh < 2; ++aceHigh) {
        const int at = runRankOf(card, aceHigh != 0);
        if (card.rank != kAs && aceHigh != 0) break;
        if (at == top + 1 && at <= kHighest) {
            Meld grown = meld;
            grown.cards.append(card);
            *out = grown;
            return true;
        }
        if (at == meld.low - 1 && at >= kLowest) {
            Meld grown = meld;
            grown.cards.prepend(card);
            grown.low = at;
            *out = grown;
            return true;
        }
    }
    return false;
}

QVector<Card> jokerWants(const Meld &meld) {
    QVector<Card> wanted;
    const int joker = meld.jokerAt();
    if (joker < 0) return wanted;

    if (meld.kind == MeldKind::Suite) {
        Card card;
        card.id = 0;
        card.rank = cardRankForRun(meld.runRankAt(joker));
        card.suit = meld.suit();
        wanted.append(card);
        return wanted;
    }

    // Série à laquelle il manque une ou deux couleurs : le joker peut
    // représenter n'importe laquelle d'entre elles.
    bool taken[4] = {false, false, false, false};
    for (const Card &held : meld.cards) {
        if (!held.joker()) taken[int(held.suit)] = true;
    }
    for (int suit = 0; suit < 4; ++suit) {
        if (taken[suit]) continue;
        Card card;
        card.id = 0;
        card.rank = meld.rank();
        card.suit = Suit(suit);
        wanted.append(card);
    }
    return wanted;
}

QVector<Meld> allMelds(const QVector<Card> &hand) {
    QVector<Meld> melds;
    for (const Candidate &candidate : candidatesFor(hand)) melds.append(candidate.meld);
    return melds;
}

Plan readHand(const QVector<Card> &hand, Reading how) {
    Plan plan;
    const int count = hand.size();
    for (const Card &card : hand) plan.deadwood += penaltyValue(card);
    plan.rest = hand;
    if (count < 3 || count > 20) return plan;

    const QVector<Candidate> candidates = candidatesFor(hand);
    if (candidates.isEmpty()) return plan;

    QVector<Meld> chosen;

    if (how == Reading::Greedy) {
        // La plus grande combinaison d'abord, puis celles qui s'y ajoutent.
        // C'est la façon de procéder d'un débutant, et elle est suffisamment
        // souvent sous-optimale pour avoir une incidence : trois combinaisons
        // de trois cartes peuvent valoir plus qu'une seule de cinq.
        QVector<Candidate> order = candidates;
        std::stable_sort(order.begin(), order.end(),
                         [](const Candidate &a, const Candidate &b) { return a.value > b.value; });
        quint32 used = 0;
        for (const Candidate &candidate : order) {
            if (candidate.mask & used) continue;
            used |= candidate.mask;
            chosen.append(candidate.meld);
        }
    } else {
        // Tous les ensembles de combinaisons ne se disputant aucune carte,
        // représentés par l'ensemble des cartes déjà employées. `best[mask]`
        // est la valeur maximale atteignable avec les cartes hors de `mask`.
        const int deadweight = (how == Reading::Balanced) ? 1 : 0;
        const quint32 full = (count >= 32) ? 0xffffffffu : ((quint32(1) << count) - 1);

        QVector<QVector<int>> filed(count);
        for (int i = 0; i < candidates.size(); ++i) filed[candidates.at(i).first].append(i);

        constexpr int kUnknown = -1000000;
        std::vector<int> best(size_t(full) + 1, kUnknown);
        std::vector<int> took(size_t(full) + 1, -1);
        best[size_t(full)] = 0;
        took[size_t(full)] = -1;

        // Parcours décroissant depuis le masque complet : la valeur d'un
        // masque ne dépend que de masques comportant davantage de bits.
        for (qint64 mask = qint64(full) - 1; mask >= 0; --mask) {
            const quint32 at = quint32(mask);
            int free1 = -1;
            for (int i = 0; i < count; ++i) {
                if (!(at & (quint32(1) << i))) {
                    free1 = i;
                    break;
                }
            }
            if (free1 < 0) continue;

            // Carte laissée en main, comptée pour sa pénalité.
            int answer = best[size_t(at | (quint32(1) << free1))] -
                         deadweight * penaltyValue(hand.at(free1));
            int by = -1;
            for (int which : filed.at(free1)) {
                const Candidate &candidate = candidates.at(which);
                if (candidate.mask & at) continue;
                const int trial = candidate.value + best[size_t(at | candidate.mask)];
                if (trial > answer) {
                    answer = trial;
                    by = which;
                }
            }
            best[size_t(at)] = answer;
            took[size_t(at)] = by;
        }

        quint32 walk = 0;
        while (walk != full) {
            const int which = took[size_t(walk)];
            if (which < 0) {
                for (int i = 0; i < count; ++i) {
                    if (!(walk & (quint32(1) << i))) {
                        walk |= (quint32(1) << i);
                        break;
                    }
                }
                continue;
            }
            chosen.append(candidates.at(which).meld);
            walk |= candidates.at(which).mask;
        }
    }

    // Cartes retenues dans les combinaisons, et cartes restant en main.
    plan.melds = chosen;
    plan.points = 0;
    plan.rest.clear();
    plan.deadwood = 0;
    QVector<int> spoken;
    for (const Meld &meld : chosen) {
        plan.points += meld.points();
        for (const Card &card : meld.cards) spoken.append(card.id);
    }
    for (const Card &card : hand) {
        if (spoken.contains(card.id)) continue;
        plan.rest.append(card);
        plan.deadwood += penaltyValue(card);
    }
    return plan;
}
