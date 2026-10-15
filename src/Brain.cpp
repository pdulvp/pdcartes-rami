#include "Brain.h"

#include <QCoreApplication>
#include <QRandomGenerator>

#include <algorithm>
#include <cmath>

namespace {

qreal roll() { return QRandomGenerator::global()->generateDouble(); }

bool happens(qreal chance) { return roll() < chance; }

// ---------------------------------------------------------------------------
// Ce qu'une main vaut.
//
// Three things, added up: the combinations that are already there, what is
// left over and would be paid for, and the combinations that are one card
// away. The third is the one that separates the players -- a beginner counts
// the first two and throws away the card that would have made the third.

struct Partial {
    int worth = 0;      // what finishing it would be worth
    int ways = 0;       // how many cards in the pack would finish it
};

// A pair of the same rank in different colours, or two cards of a colour that
// nearly follow on: what each loose card in the hand is part of.
qreal potentialOf(const QVector<Card> &rest, const Watch &watch, bool counts) {
    qreal total = 0.0;
    const int count = rest.size();

    for (const Card &card : rest) {
        // A loose joker is a combination waiting for any two cards at all.
        if (card.joker()) total += 14.0;
    }

    for (int i = 0; i < count; ++i) {
        const Card &a = rest.at(i);
        if (a.joker()) continue;
        for (int j = i + 1; j < count; ++j) {
            const Card &b = rest.at(j);
            if (b.joker()) continue;

            if (a.rank == b.rank && a.suit != b.suit) {
                // Two of a rank: either of the other two colours finishes it.
                int ways = 0;
                for (int suit = 0; suit < 4; ++suit) {
                    if (Suit(suit) == a.suit || Suit(suit) == b.suit) continue;
                    ways += counts ? watch.unseenCopies(a.rank, Suit(suit)) : 2;
                }
                total += 3.0 + 0.9 * ways + 0.12 * setRankValue(a.rank);
                continue;
            }
            if (a.suit != b.suit) continue;

            const int gap = std::abs(a.rank - b.rank);
            if (gap != 1 && gap != 2) continue;
            const int low = std::min(a.rank, b.rank);
            const int high = std::max(a.rank, b.rank);
            int ways = 0;
            if (gap == 2) {
                ways = counts ? watch.unseenCopies(low + 1, a.suit) : 2;
            } else {
                if (low - 1 >= kAs) ways += counts ? watch.unseenCopies(low - 1, a.suit) : 2;
                if (high + 1 <= kRoi) ways += counts ? watch.unseenCopies(high + 1, a.suit) : 2;
            }
            total += 2.4 + 0.8 * ways;
        }
    }
    return total;
}

qreal evaluate(const QVector<Card> &hand, const Skill &skill, const Watch &watch, bool opened) {
    const Plan plan = readHand(hand, skill.thorough ? Reading::Balanced : Reading::Greedy);
    // Before opening, points are the whole problem: fifty-one of them is the
    // door. Afterwards what matters is getting rid of cards.
    qreal worth = plan.points * (opened ? 0.85 : 1.25);
    worth -= plan.deadwood * 0.80;
    worth += skill.potentialWeight * potentialOf(plan.rest, watch, skill.watches);
    return worth;
}

// ---------------------------------------------------------------------------
// Ce qu'on peut faire maintenant.

// The combinations in the hand that could actually be laid: laying one must
// leave a card to throw at the end of the turn.
QVector<Meld> layableMelds(const QVector<Card> &hand, const Skill &skill, bool mostPoints) {
    const Reading how = !skill.thorough ? Reading::Greedy
                                        : (mostPoints ? Reading::MostPoints : Reading::Balanced);
    return readHand(hand, how).melds;
}

QVector<int> idsOf(const Meld &meld) {
    QVector<int> ids;
    for (const Card &card : meld.cards) ids.append(card.id);
    return ids;
}

// The card in hand that goes onto something already on the tapis.
bool findLayOff(const Rami &game, const Seat &seat, const Skill &skill, Move *out) {
    struct Chance {
        int cardId;
        int meldId;
        int worth;
    };
    QVector<Chance> chances;
    for (const Card &card : seat.hand) {
        for (const Meld &meld : game.table()) {
            Meld grown;
            if (!canExtend(meld, card, &grown)) continue;
            // A joker is worth keeping for a combination of one's own unless
            // there is nothing else to do with it.
            chances.append({card.id, meld.id, card.joker() ? 2 : penaltyValue(card) + 10});
        }
    }
    if (chances.isEmpty()) return false;

    std::stable_sort(chances.begin(), chances.end(),
                     [](const Chance &a, const Chance &b) { return a.worth > b.worth; });

    // A card that fits is not a card everybody notices.
    for (const Chance &chance : chances) {
        if (!happens(skill.layoffSight)) continue;
        out->kind = Move::LayOff;
        out->cards = {chance.cardId};
        out->meldId = chance.meldId;
        return true;
    }
    return false;
}

// A joker bought back out of a combination, and put straight back to work on
// the end of one. The card handed over is one that was doing nothing, so the
// hand comes out a card shorter and the joker is on the tapis instead of in
// somebody's score.
bool findJokerBuy(const Rami &game, const Seat &seat, const QVector<Card> &loose, Move *out) {
    for (const Meld &meld : game.table()) {
        if (!meld.hasJoker()) continue;
        for (const Card &wanted : jokerWants(meld)) {
            for (const Card &card : loose) {
                if (card.joker() || card.rank != wanted.rank || card.suit != wanted.suit) continue;
                // Only worth it if the joker has somewhere to go afterwards --
                // the rules will not let the turn end with it in hand.
                Meld swapped = meld;
                const int jokerAt = swapped.jokerAt();
                if (jokerAt < 0) continue;
                if (swapped.kind == MeldKind::Serie) {
                    swapped.cards.remove(jokerAt);
                    swapped.cards.append(card);
                } else {
                    swapped.cards[jokerAt] = card;
                }
                Card joker;
                joker.id = meld.cards.at(jokerAt).id;
                joker.rank = kJoker;
                bool somewhere = false;
                Meld grown;
                if (canExtend(swapped, joker, &grown)) somewhere = true;
                for (const Meld &other : game.table()) {
                    if (other.id == meld.id) continue;
                    if (canExtend(other, joker, &grown)) somewhere = true;
                }
                if (!somewhere) continue;
                out->kind = Move::BuyJoker;
                out->cards = {card.id};
                out->meldId = meld.id;
                return true;
            }
        }
    }
    Q_UNUSED(seat);
    return false;
}

// ---------------------------------------------------------------------------
// Ce qu'on jette.

// How much giving this card away would help somebody else. Nought for a
// player who is not watching -- which is most of them.
qreal feedRisk(const Rami &game, int at, const Card &card, const Watch &watch,
               const Skill &skill) {
    if (!skill.guardsFeed) return 0.0;
    qreal risk = 0.0;

    // Straight onto something already down: the worst card there is to throw,
    // because anybody who has opened can use it the moment it lands.
    bool anyoneOpen = false;
    for (int seat = 0; seat < game.seatCount(); ++seat) {
        if (seat != at && game.seat(seat).opened) anyoneOpen = true;
    }
    if (anyoneOpen) {
        for (const Meld &meld : game.table()) {
            Meld grown;
            if (meld.owner == at) continue;
            if (canExtend(meld, card, &grown)) risk += 7.0;
        }
    }

    // And what they have been picking up. A player who took the nine de cœur
    // off the défausse wants the eight and the ten of it, and the other
    // nines; that is not a guess, it is a card they showed everybody.
    if (skill.watches) {
        for (int seat = 0; seat < watch.taken.size(); ++seat) {
            if (seat == at) continue;
            for (const Card &want : watch.taken.at(seat)) {
                if (want.joker()) continue;
                if (want.rank == card.rank && want.suit != card.suit) risk += 3.0;
                if (want.suit == card.suit && std::abs(want.rank - card.rank) <= 2) risk += 3.0;
            }
        }
    }

    // Somebody about to go out makes every card dangerous.
    int fewest = kHandSize + 2;
    for (int seat = 0; seat < game.seatCount(); ++seat) {
        if (seat == at) continue;
        fewest = std::min(fewest, int(game.seat(seat).hand.size()));
    }
    if (fewest <= 4) risk *= 1.6;
    return risk;
}

int chooseDiscard(const Rami &game, int at, const Skill &skill, const Watch &watch) {
    const Seat &seat = game.seat(at);
    if (seat.hand.isEmpty()) return -1;
    if (seat.hand.size() == 1) return seat.hand.first().id;

    const Plan plan = readHand(seat.hand, skill.thorough ? Reading::Balanced : Reading::Greedy);

    // What is loose. If the whole hand reads as combinations -- it happens,
    // and then one of them has to be broken -- the longest one gives up an
    // end card.
    QVector<Card> loose = plan.rest;
    if (loose.isEmpty()) {
        const Meld *longest = nullptr;
        for (const Meld &meld : plan.melds) {
            if (!longest || meld.cards.size() > longest->cards.size()) longest = &meld;
        }
        if (!longest) return seat.hand.first().id;
        for (const Card &card : longest->cards) {
            if (!card.joker()) loose.append(card);
        }
        if (loose.isEmpty()) return seat.hand.first().id;
    }

    // Now and then, the wrong one.
    if (happens(skill.blunder)) {
        return loose.at(QRandomGenerator::global()->bounded(loose.size())).id;
    }

    int chosen = loose.first().id;
    qreal lowest = 0.0;
    bool first = true;
    for (const Card &card : loose) {
        // Never, under any circumstances, the joker: twenty-five points
        // thrown away, and handed to whoever is next.
        if (card.joker()) continue;

        QVector<Card> without;
        for (const Card &other : loose) {
            if (other.id != card.id) without.append(other);
        }
        // What the hand keeps if this one goes: the combinations one card
        // away that survive, less what the card would have cost to hold, plus
        // what giving it away is worth to the others.
        // Low means "throw this one". What is left behind counts for it, what
        // it would cost to keep counts for it, and what it would be worth to
        // the others counts against it.
        const qreal verdict =
            -skill.potentialWeight * potentialOf(without, watch, skill.watches) -
            penaltyValue(card) * 0.70 + feedRisk(game, at, card, watch, skill);
        if (first || verdict < lowest) {
            lowest = verdict;
            chosen = card.id;
            first = false;
        }
    }
    return chosen;
}

// ---------------------------------------------------------------------------
// Piocher, ou ramasser.

bool wantsPile(const Rami &game, int at, const Skill &skill, const Watch &watch) {
    const Card top = game.pileTop();
    if (!top.valid()) return false;
    const Seat &seat = game.seat(at);

    // Nobody passes up a joker.
    if (top.joker()) return true;

    QVector<Card> with = seat.hand;
    with.append(top);

    const qreal before = evaluate(seat.hand, skill, watch, seat.opened);
    const qreal after = evaluate(with, skill, watch, seat.opened);

    // A beginner only reaches for it when it plainly finishes something; the
    // threshold is what "plainly" means, and it comes down as they learn.
    qreal wanted = skill.pileThreshold;
    // With the pioche nearly gone, the défausse is most of what is left.
    if (game.stockLeft() <= 4) wanted *= 0.5;
    if (happens(skill.blunder * 0.5)) return happens(0.5);
    return after - before >= wanted;
}

} // namespace

// ---------------------------------------------------------------------------

void Watch::note(const Card &card) {
    if (!card.valid()) return;
    for (const Card &known : seen) {
        if (known.id == card.id) return;
    }
    seen.append(card);
}

void Watch::noteTaken(int seat, const Card &card, int seats) {
    if (taken.size() < seats) taken.resize(seats);
    if (seat < 0 || seat >= taken.size()) return;
    taken[seat].append(card);
    // Only the last few: what somebody picked up eight turns ago is not what
    // they are still waiting for.
    while (taken[seat].size() > 5) taken[seat].removeFirst();
    note(card);
}

void Watch::clear() {
    seen.clear();
    for (QVector<Card> &list : taken) list.clear();
}

int Watch::unseenCopies(int rank, Suit suit) const {
    int gone = 0;
    for (const Card &card : seen) {
        if (!card.joker() && card.rank == rank && card.suit == suit) ++gone;
    }
    return std::max(0, 2 - gone);
}

Skill skillFor(int xp) {
    const qreal mastery = masteryOf(xp);
    Skill skill;
    skill.mastery = mastery;
    // Steep at the bottom and flat at the top: the difference between a brand
    // new player and one who has sat through ten manches should be the thing
    // you notice, and the difference between two experts should not be luck.
    skill.blunder = 0.30 * std::pow(1.0 - mastery, 1.6);
    skill.thorough = xp >= levelFloor(Level::Intermediaire);
    skill.watches = xp >= levelFloor(Level::Expert);
    skill.guardsFeed = mastery >= 0.34;
    skill.buysJokers = xp >= levelFloor(Level::Expert);
    skill.layoffSight = 0.55 + 0.45 * mastery;
    skill.pileThreshold = 10.0 - 8.5 * mastery;
    skill.potentialWeight = 0.22 + 0.78 * mastery;
    return skill;
}

Skill bestSkill() {
    Skill skill = skillFor(kMaxExperience);
    skill.blunder = 0.0;
    skill.layoffSight = 1.0;
    return skill;
}

Move chooseMove(const Rami &game, int at, const Skill &skill, const Watch &watch) {
    Move move;
    if (at != game.turn()) return move;
    const Seat &seat = game.seat(at);

    if (game.phase() == Phase::Draw) {
        move.kind = wantsPile(game, at, skill, watch) ? Move::DrawPile : Move::DrawStock;
        return move;
    }
    if (game.phase() != Phase::Build) return move;

    // A joker bought back has to be put somewhere before anything else: the
    // turn cannot end while it is in hand.
    if (game.owesJoker(at)) {
        Skill sure = skill;
        sure.layoffSight = 1.0;
        for (const Card &card : seat.hand) {
            if (!card.joker()) continue;
            for (const Meld &meld : game.table()) {
                Meld grown;
                if (!canExtend(meld, card, &grown)) continue;
                move.kind = Move::LayOff;
                move.cards = {card.id};
                move.meldId = meld.id;
                return move;
            }
        }
        // Nowhere to put it after all: lay a combination with it in, and if
        // even that is impossible the table will fall back on a plain throw.
        for (const Meld &meld : layableMelds(seat.hand, sure, false)) {
            bool usesJoker = false;
            for (const Card &card : meld.cards) {
                if (card.joker()) usesJoker = true;
            }
            if (!usesJoker) continue;
            if (seat.hand.size() - meld.cards.size() < 1) continue;
            move.kind = Move::Lay;
            move.cards = idsOf(meld);
            return move;
        }
    }

    if (!seat.opened) {
        // Fifty-one in one turn, or nothing. The pose is only begun once it
        // is known to be there -- beginning one that falls short means
        // picking it all back up, and no sensible player does that on
        // purpose.
        const int already = game.poseValue(at);
        QVector<Meld> opening = layableMelds(seat.hand, skill, true);
        int worth = already;
        for (const Meld &meld : opening) worth += meld.points();
        if (worth >= kOpeningPoints) {
            for (const Meld &meld : opening) {
                if (seat.hand.size() - meld.cards.size() < 1) continue;
                move.kind = Move::Lay;
                move.cards = idsOf(meld);
                return move;
            }
        } else if (already > 0) {
            // A pose begun and not finished. It happens to a beginner and to
            // nobody else: reading the hand greedily can find fifty-one
            // points, lay the first combination of them, and then fail to
            // find the rest in what is left. The pose goes back in the hand,
            // and the turn is a turn wasted -- which is exactly what the
            // mistake costs at a real table.
            move.kind = Move::TakeBack;
            return move;
        }
        move.kind = Move::Discard;
        move.cards = {chooseDiscard(game, at, skill, watch)};
        return move;
    }

    // Open: everything down that can go down.
    for (const Meld &meld : layableMelds(seat.hand, skill, false)) {
        if (seat.hand.size() - meld.cards.size() < 1) continue;
        if (happens(skill.blunder * 0.4)) continue;   // and sometimes it is missed
        move.kind = Move::Lay;
        move.cards = idsOf(meld);
        return move;
    }

    if (seat.hand.size() >= 2 && findLayOff(game, seat, skill, &move)) return move;

    if (skill.buysJokers && seat.hand.size() >= 2) {
        const Plan plan = readHand(seat.hand, Reading::Balanced);
        if (findJokerBuy(game, seat, plan.rest, &move)) return move;
    }

    move.kind = Move::Discard;
    move.cards = {chooseDiscard(game, at, skill, watch)};
    return move;
}

QString advise(const Rami &game, int at, const Watch &watch) {
    if (at != game.turn()) {
        return QCoreApplication::translate("brain", "Ce n'est pas à vous de jouer.");
    }
    const Seat &seat = game.seat(at);
    const Skill skill = bestSkill();
    const Move move = chooseMove(game, at, skill, watch);

    switch (move.kind) {
    case Move::DrawStock:
        return QCoreApplication::translate(
            "brain", "Piochez : la défausse n'apporte rien à cette main.");
    case Move::DrawPile:
        return QCoreApplication::translate("brain", "Ramassez %1 sur la défausse.")
            .arg(cardName(game.pileTop()));
    case Move::Lay: {
        QVector<Card> cards;
        for (int id : move.cards) {
            if (const Card *found = findCard(seat.hand, id)) cards.append(*found);
        }
        Meld meld;
        if (!makeMeld(cards, &meld)) break;
        if (!seat.opened) {
            const Plan plan = readHand(seat.hand, Reading::MostPoints);
            return QCoreApplication::translate(
                       "brain", "Posez %1. Votre main vaut %2 points de combinaisons : "
                                "de quoi ouvrir.")
                .arg(meld.spell())
                .arg(plan.points + game.poseValue(at));
        }
        return QCoreApplication::translate("brain", "Posez %1.").arg(meld.spell());
    }
    case Move::LayOff: {
        const Meld *meld = game.meldById(move.meldId);
        const Card *card = move.cards.isEmpty() ? nullptr : findCard(seat.hand, move.cards.first());
        if (!meld || !card) break;
        return QCoreApplication::translate("brain", "Mettez %1 sur %2.")
            .arg(cardName(*card), meld->spell());
    }
    case Move::BuyJoker: {
        const Meld *meld = game.meldById(move.meldId);
        const Card *card = move.cards.isEmpty() ? nullptr : findCard(seat.hand, move.cards.first());
        if (!meld || !card) break;
        return QCoreApplication::translate("brain", "Rachetez le joker de %1 avec %2.")
            .arg(meld->spell(), cardName(*card));
    }
    case Move::Discard: {
        const Card *card = move.cards.isEmpty() ? nullptr : findCard(seat.hand, move.cards.first());
        if (!card) break;
        if (!seat.opened) {
            const Plan plan = readHand(seat.hand, Reading::MostPoints);
            return QCoreApplication::translate(
                       "brain", "Défaussez %1. Il vous manque %2 points pour ouvrir.")
                .arg(cardName(*card))
                .arg(std::max(1, kOpeningPoints - plan.points - game.poseValue(at)));
        }
        return QCoreApplication::translate("brain", "Défaussez %1 : c'est la carte qui sert "
                                                    "le moins et qui coûte le plus.")
            .arg(cardName(*card));
    }
    case Move::TakeBack:
        return QCoreApplication::translate(
            "brain", "Reprenez votre pose en main : elle ne fait pas 51 points.");
    case Move::None:
        break;
    }
    return QCoreApplication::translate("brain", "Rien d'évident : défaussez ce qui coûte le plus.");
}
