#include "Rami.h"

#include <algorithm>

namespace {

// As many times as the défausse is turned back into a pioche before a manche
// is given up on. Two: by then everyone has seen every card twice, and if
// nobody can go out they are not going to.
constexpr int kMostReshuffles = 2;

Seat emptySeat() { return Seat{}; }

} // namespace

Rami::Rami(QObject *parent) : QObject(parent) {}

const Seat &Rami::seat(int at) const {
    static const Seat nobody = emptySeat();
    if (at < 0 || at >= m_seats.size()) return nobody;
    return m_seats.at(at);
}

Seat &Rami::here() { return m_seats[qBound(0, m_turn, m_seats.size() - 1)]; }

const Meld *Rami::meldById(int id) const {
    for (const Meld &meld : m_table) {
        if (meld.id == id) return &meld;
    }
    for (const Seat &seat : m_seats) {
        for (const Meld &meld : seat.waiting) {
            if (meld.id == id) return &meld;
        }
    }
    return nullptr;
}

Card Rami::pileTop() const { return m_pile.isEmpty() ? Card{} : m_pile.last(); }

int Rami::poseValue(int at) const {
    int total = 0;
    for (const Meld &meld : seat(at).waiting) total += meld.points();
    return total;
}

bool Rami::owesJoker(int at) const {
    if (at != m_turn) return false;
    for (int id : m_jokerDebt) {
        if (findCard(seat(at).hand, id)) return true;
    }
    return false;
}

void Rami::startMatch(const QVector<SeatSpec> &specs, int targetPoints) {
    m_seats.clear();
    for (const SeatSpec &spec : specs) {
        Seat seat;
        seat.rivalId = spec.rivalId;
        seat.name = spec.name;
        seat.hue = spec.hue;
        m_seats.append(seat);
    }
    m_target = std::max(0, targetPoints);
    m_handNumber = 0;
    m_meldIds = 0;
    m_matchWinner = -1;
    // Whoever deals does not lead, so the first hand is led by the human: the
    // player should not open a game by watching five other people think.
    m_dealer = m_seats.size() - 1;
    startHand();
}

void Rami::startHand() {
    if (m_seats.isEmpty()) return;
    // The deal moves round one seat a manche, which moves the lead with it.
    if (m_handNumber > 0) m_dealer = (m_dealer + 1) % m_seats.size();
    ++m_handNumber;
    m_outSeat = -1;
    m_reshuffles = 0;
    m_table.clear();
    m_pile.clear();
    m_jokerDebt.clear();
    m_lastDrawn = Card{};
    m_drewFromPile = false;

    for (Seat &seat : m_seats) {
        seat.hand.clear();
        seat.waiting.clear();
        seat.opened = false;
        seat.wentOut = false;
        seat.lastHand = 0;
    }

    dealHand();
    m_turn = (m_dealer + 1) % m_seats.size();
    m_phase = Phase::Draw;
    Q_EMIT changed();
    Q_EMIT turnBegan(m_turn);
}

void Rami::dealHand() {
    m_stock = freshPack();
    shuffleCards(m_stock);

    // Round the table one card at a time, as it is dealt at a table. It makes
    // no difference to anything, and doing it the other way would be the sort
    // of shortcut that shows.
    for (int round = 0; round < kHandSize; ++round) {
        for (int at = 0; at < m_seats.size(); ++at) {
            const int who = (m_dealer + 1 + at) % m_seats.size();
            m_seats[who].hand.append(m_stock.takeLast());
        }
    }
    for (Seat &seat : m_seats) sortBySuit(seat.hand);

    // One card turned over to start the défausse, so that the first player
    // has the same choice as everybody else.
    if (!m_stock.isEmpty()) m_pile.append(m_stock.takeLast());
}

bool Rami::refillStock() {
    if (!m_stock.isEmpty()) return true;
    if (m_pile.size() < 2) return false;
    if (m_reshuffles >= kMostReshuffles) return false;

    // The card on top stays where it is -- it is still the one on offer --
    // and everything under it is turned over and shuffled.
    const Card top = m_pile.takeLast();
    m_stock = m_pile;
    m_pile.clear();
    m_pile.append(top);
    shuffleCards(m_stock);
    ++m_reshuffles;
    Q_EMIT acted(-1, tr("La défausse est retournée : la pioche repart."));
    return true;
}

bool Rami::drawStock(QString *why) {
    if (m_phase != Phase::Draw) {
        if (why) *why = tr("Ce n'est plus le moment de piocher.");
        return false;
    }
    if (!refillStock()) {
        if (why) *why = tr("Il n'y a plus rien à piocher.");
        // Nobody can go out of a manche with no cards left to find.
        finishHand();
        return false;
    }

    const Card drawn = m_stock.takeLast();
    here().hand.append(drawn);
    m_lastDrawn = drawn;
    m_drewFromPile = false;
    m_phase = Phase::Build;
    Q_EMIT acted(m_turn, tr("%1 pioche.").arg(here().name));
    Q_EMIT changed();
    return true;
}

bool Rami::drawPile(QString *why) {
    if (m_phase != Phase::Draw) {
        if (why) *why = tr("Ce n'est plus le moment de piocher.");
        return false;
    }
    if (m_pile.isEmpty()) {
        if (why) *why = tr("La défausse est vide.");
        return false;
    }

    const Card drawn = m_pile.takeLast();
    here().hand.append(drawn);
    m_lastDrawn = drawn;
    m_drewFromPile = true;
    m_phase = Phase::Build;
    Q_EMIT acted(m_turn, tr("%1 ramasse %2.").arg(here().name, cardName(drawn)));
    Q_EMIT changed();
    return true;
}

bool Rami::lay(const QVector<int> &cardIds, QString *why) {
    if (m_phase != Phase::Build) {
        if (why) *why = tr("Piochez d'abord.");
        return false;
    }
    Seat &seat = here();
    if (cardIds.size() < 3) {
        if (why) *why = tr("Une combinaison, c'est trois cartes au moins.");
        return false;
    }
    if (seat.hand.size() - cardIds.size() < 1) {
        if (why) {
            *why = tr("Il faut garder une carte à défausser pour finir le tour.");
        }
        return false;
    }

    QVector<Card> cards;
    for (int id : cardIds) {
        const Card *found = findCard(seat.hand, id);
        if (!found) {
            if (why) *why = tr("Cette carte n'est pas dans votre main.");
            return false;
        }
        cards.append(*found);
    }

    Meld meld;
    if (!makeMeld(cards, &meld)) {
        if (why) {
            *why = tr("Ni une suite ni une série.");
        }
        return false;
    }
    meld.id = nextMeldId();
    meld.owner = m_turn;

    for (int id : cardIds) takeCard(seat.hand, id);

    if (seat.opened) {
        m_table.append(meld);
        Q_EMIT acted(m_turn, tr("%1 pose %2.").arg(seat.name, meld.spell()));
    } else {
        seat.waiting.append(meld);
        const int value = poseValue(m_turn);
        if (value >= kOpeningPoints) {
            // Fifty-one reached: the whole pose goes on the tapis at once and
            // this player is open from here on.
            for (Meld &waiting : seat.waiting) m_table.append(waiting);
            seat.waiting.clear();
            seat.opened = true;
            Q_EMIT acted(m_turn, tr("%1 ouvre avec %2 points.").arg(seat.name).arg(value));
        } else {
            Q_EMIT acted(m_turn, tr("%1 prépare sa pose : %2 points sur 51.")
                                     .arg(seat.name)
                                     .arg(value));
        }
    }
    Q_EMIT changed();
    return true;
}

bool Rami::layOff(int cardId, int meldId, QString *why) {
    if (m_phase != Phase::Build) {
        if (why) *why = tr("Piochez d'abord.");
        return false;
    }
    Seat &seat = here();
    if (!seat.opened) {
        if (why) {
            *why = tr("Il faut avoir ouvert pour compléter une combinaison.");
        }
        return false;
    }
    if (seat.hand.size() < 2) {
        if (why) *why = tr("Il faut garder une carte à défausser pour finir le tour.");
        return false;
    }
    const Card *card = findCard(seat.hand, cardId);
    if (!card) {
        if (why) *why = tr("Cette carte n'est pas dans votre main.");
        return false;
    }

    for (int i = 0; i < m_table.size(); ++i) {
        if (m_table.at(i).id != meldId) continue;
        Meld grown;
        if (!canExtend(m_table.at(i), *card, &grown)) {
            if (why) {
                *why = tr("%1 ne va pas sur %2.").arg(cardName(*card), m_table.at(i).spell());
            }
            return false;
        }
        grown.id = m_table.at(i).id;
        grown.owner = m_table.at(i).owner;
        const Card played = takeCard(seat.hand, cardId);
        m_table[i] = grown;
        Q_EMIT acted(m_turn, tr("%1 complète %2 avec %3.")
                                 .arg(seat.name, grown.spell(), cardName(played)));
        Q_EMIT changed();
        return true;
    }
    if (why) *why = tr("Cette combinaison n'est pas sur le tapis.");
    return false;
}

bool Rami::buyJoker(int cardId, int meldId, QString *why) {
    if (m_phase != Phase::Build) {
        if (why) *why = tr("Piochez d'abord.");
        return false;
    }
    Seat &seat = here();
    if (!seat.opened) {
        if (why) *why = tr("Il faut avoir ouvert avant de racheter un joker.");
        return false;
    }
    const Card *card = findCard(seat.hand, cardId);
    if (!card || card->joker()) {
        if (why) *why = tr("Choisissez dans votre main la carte que le joker remplace.");
        return false;
    }

    for (int i = 0; i < m_table.size(); ++i) {
        if (m_table.at(i).id != meldId) continue;
        const Meld &meld = m_table.at(i);
        const int jokerAt = meld.jokerAt();
        if (jokerAt < 0) {
            if (why) *why = tr("Il n'y a pas de joker dans cette combinaison.");
            return false;
        }
        bool fits = false;
        for (const Card &wanted : jokerWants(meld)) {
            if (wanted.rank == card->rank && wanted.suit == card->suit) fits = true;
        }
        if (!fits) {
            if (why) {
                *why = tr("Ce joker ne tient pas la place de %1.").arg(cardName(*card));
            }
            return false;
        }

        const Card joker = meld.cards.at(jokerAt);
        Meld swapped = meld;
        if (meld.kind == MeldKind::Serie) {
            swapped.cards.remove(jokerAt);
            swapped.cards.append(*card);
            std::stable_sort(swapped.cards.begin(), swapped.cards.end(),
                             [](const Card &a, const Card &b) {
                                 return int(a.suit) < int(b.suit);
                             });
        } else {
            swapped.cards[jokerAt] = *card;
        }
        takeCard(seat.hand, cardId);
        seat.hand.append(joker);
        m_table[i] = swapped;
        // Bought, not borrowed: the joker has to be put back to work in this
        // same turn, and the turn will not end while it is still in hand.
        m_jokerDebt.append(joker.id);
        Q_EMIT acted(m_turn, tr("%1 rachète le joker de %2.").arg(seat.name, swapped.spell()));
        Q_EMIT changed();
        return true;
    }
    if (why) *why = tr("Cette combinaison n'est pas sur le tapis.");
    return false;
}

bool Rami::takeBackPose(QString *why) {
    if (m_phase != Phase::Build) {
        if (why) *why = tr("Ce n'est pas le moment.");
        return false;
    }
    Seat &seat = here();
    if (seat.waiting.isEmpty()) {
        if (why) *why = tr("Il n'y a pas de pose à reprendre.");
        return false;
    }
    for (const Meld &meld : seat.waiting) {
        for (const Card &card : meld.cards) seat.hand.append(card);
    }
    seat.waiting.clear();
    sortBySuit(seat.hand);
    Q_EMIT changed();
    return true;
}

void Rami::sortSeatHand(int at, bool byRank) {
    if (at < 0 || at >= m_seats.size()) return;
    if (byRank) {
        sortByRank(m_seats[at].hand);
    } else {
        sortBySuit(m_seats[at].hand);
    }
    Q_EMIT changed();
}

bool Rami::canDiscard(QString *why) const {
    if (m_phase != Phase::Build) {
        if (why) *why = tr("Piochez d'abord.");
        return false;
    }
    const Seat &seat = this->seat(m_turn);
    if (seat.hand.isEmpty()) {
        if (why) *why = tr("Il n'y a plus rien à défausser.");
        return false;
    }
    if (!seat.waiting.isEmpty() && poseValue(m_turn) < kOpeningPoints) {
        if (why) {
            *why = tr("Pose à %1 points : il en faut 51 pour ouvrir.").arg(poseValue(m_turn));
        }
        return false;
    }
    if (owesJoker(m_turn)) {
        if (why) {
            *why = tr("Le joker racheté doit servir avant la fin du tour.");
        }
        return false;
    }
    return true;
}

bool Rami::discard(int cardId, QString *why) {
    if (!canDiscard(why)) return false;
    Seat &seat = here();
    const Card *card = findCard(seat.hand, cardId);
    if (!card) {
        if (why) *why = tr("Cette carte n'est pas dans votre main.");
        return false;
    }

    const Card thrown = takeCard(seat.hand, cardId);
    m_pile.append(thrown);
    m_jokerDebt.clear();
    Q_EMIT acted(m_turn, tr("%1 défausse %2.").arg(seat.name, cardName(thrown)));

    if (seat.hand.isEmpty()) {
        seat.wentOut = true;
        m_outSeat = m_turn;
        Q_EMIT changed();
        finishHand();
        return true;
    }

    nextTurn();
    return true;
}

void Rami::nextTurn() {
    m_turn = (m_turn + 1) % m_seats.size();
    m_phase = Phase::Draw;
    m_lastDrawn = Card{};
    m_drewFromPile = false;
    m_jokerDebt.clear();
    Q_EMIT changed();
    Q_EMIT turnBegan(m_turn);
}

void Rami::finishHand() {
    m_phase = Phase::HandOver;

    // What everybody is still holding. A pose left short of fifty-one is
    // still in the hand that laid it, and counts against it: that is the
    // whole risk of beginning one.
    for (int at = 0; at < m_seats.size(); ++at) {
        Seat &seat = m_seats[at];
        int penalty = 0;
        for (const Card &card : seat.hand) penalty += penaltyValue(card);
        for (const Meld &meld : seat.waiting) {
            for (const Card &card : meld.cards) penalty += penaltyValue(card);
        }
        if (at == m_outSeat) penalty = 0;
        seat.lastHand = penalty;
        seat.score += penalty;
    }

    bool over = m_target <= 0;
    if (!over) {
        for (const Seat &seat : m_seats) {
            if (seat.score >= m_target) over = true;
        }
    }

    if (over) {
        // The fewest points wins. A tie goes to whoever went out of the last
        // manche, and failing that to the earliest seat, so that there is
        // always exactly one winner.
        int best = -1;
        for (int at = 0; at < m_seats.size(); ++at) {
            if (best < 0 || m_seats.at(at).score < m_seats.at(best).score) {
                best = at;
            } else if (m_seats.at(at).score == m_seats.at(best).score && at == m_outSeat) {
                best = at;
            }
        }
        m_matchWinner = best;
    }

    Q_EMIT changed();
    Q_EMIT handFinished();
    if (!over) return;

    m_phase = Phase::MatchOver;
    Q_EMIT matchFinished();
}
