#include "Brain.h"

#include <QCoreApplication>
#include <QRandomGenerator>

#include <algorithm>
#include <cmath>

namespace {

qreal roll() { return QRandomGenerator::global()->generateDouble(); }

bool happens(qreal chance) { return roll() < chance; }

// ---------------------------------------------------------------------------
// Conditions prioritaires sur le style de jeu.

// Indique si la manche est en situation d'urgence : pioche proche de
// l'épuisement, ou main qui se vide, celle de l'adversaire comme la sienne.
//
// Cette condition borne l'effet de tous les tempéraments. Un joueur audacieux
// qui attendrait son coup pendant qu'un autre pose sa dernière carte
// prolongerait indéfiniment la manche ; il en irait de même d'un joueur
// prudent conservant ses jokers alors que la manche se conclut. Dès que
// l'urgence est établie, tous les joueurs jouent pour sortir.
bool pressing(const Rami &game, int at) {
    if (game.stockLeft() <= 8 || game.reshuffles() > 0) return true;
    // Au-delà de six tours de table, la manche est considérée comme avancée
    // et tous les joueurs jouent pour sortir, quel que soit leur tempérament.
    // Ce critère est retenu parce qu'il progresse en toutes circonstances :
    // une manche où personne ne pioche n'épuise aucune réserve, et deux
    // joueurs qui s'attendent mutuellement pourraient s'y maintenir
    // indéfiniment.
    if (game.turnsPlayed() >= 6 * game.seatCount()) return true;
    for (int seat = 0; seat < game.seatCount(); ++seat) {
        if (game.seat(seat).hand.size() <= 4) return true;
    }
    Q_UNUSED(at);
    return false;
}

// Total attendu pour ouvrir : les cinquante et un points, augmentés de
// quelques points chez un joueur audacieux. Cette marge n'est observée qu'en
// début de manche : dès qu'un joueur a ouvert, que la pioche est largement
// entamée ou que l'urgence est établie, l'ouverture se fait au total
// disponible.
int openingBar(const Rami &game, int at, const Skill &skill) {
    if (skill.openingSlack <= 0) return kOpeningPoints;
    if (game.stockLeft() <= 24 || pressing(game, at)) return kOpeningPoints;
    for (int seat = 0; seat < game.seatCount(); ++seat) {
        if (seat != at && game.seat(seat).opened) return kOpeningPoints;
    }
    return kOpeningPoints + skill.openingSlack;
}

// Mémoire effectivement mobilisée. Un joueur peu calme dispose de la même
// mémoire mais ne l'exploite pas systématiquement, ce qui correspond à un
// comptage des cartes effectué de façon irrégulière.
bool remembers(const Skill &skill) { return skill.watches && happens(skill.recall); }
bool remembersTakes(const Skill &skill) { return skill.readsTakes && happens(skill.recall); }

bool usesJoker(const Meld &meld) {
    for (const Card &card : meld.cards) {
        if (card.joker()) return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Évaluation d'une main.
//
// Trois termes sont additionnés : les combinaisons déjà constituées, les
// cartes restantes et leur pénalité, et les combinaisons auxquelles il ne
// manque qu'une carte. C'est ce troisième terme qui distingue les niveaux de
// jeu : un débutant ne compte que les deux premiers et se défausse de la carte
// qui aurait complété le troisième.

struct Partial {
    int worth = 0;      // what finishing it would be worth
    int ways = 0;       // how many cards in the pack would finish it
};

// Combinaisons incomplètes : deux cartes de même rang dans des couleurs
// différentes, ou deux cartes d'une même couleur presque consécutives. Indique
// le rôle de chaque carte isolée de la main.
qreal potentialOf(const QVector<Card> &rest, const Watch &watch, bool counts) {
    qreal total = 0.0;
    const int count = rest.size();

    for (const Card &card : rest) {
        // Un joker isolé constitue une combinaison en attente de deux cartes
        // quelconques.
        if (card.joker()) total += 14.0;
    }

    for (int i = 0; i < count; ++i) {
        const Card &a = rest.at(i);
        if (a.joker()) continue;
        for (int j = i + 1; j < count; ++j) {
            const Card &b = rest.at(j);
            if (b.joker()) continue;

            if (a.rank == b.rank && a.suit != b.suit) {
                // Deux cartes de même rang : l'une ou l'autre des deux couleurs
                // restantes complète la combinaison.
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
    // Avant l'ouverture, seul le total de points importe, les cinquante et un
    // points en conditionnant l'accès. Après l'ouverture, l'objectif devient
    // de se défaire de ses cartes.
    qreal worth = plan.points * (opened ? 0.85 : 1.25);
    worth -= plan.deadwood * 0.80;
    worth += skill.potentialWeight * potentialOf(plan.rest, watch, remembers(skill));
    return worth;
}

// ---------------------------------------------------------------------------
// Coups disponibles dans l'état courant.

// Combinaisons de la main effectivement posables : une pose doit laisser au
// moins une carte à défausser en fin de tour.
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

// Carte de la main pouvant compléter une combinaison déjà posée sur le tapis.
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
            // Un joker est conservé pour une combinaison personnelle, sauf
            // s'il n'a aucun autre emploi ; un joueur prudent le conserve
            // même dans ce cas.
            if (card.joker() && happens(skill.jokerGrip)) continue;
            chances.append({card.id, meld.id, card.joker() ? 2 : penaltyValue(card) + 10});
        }
    }
    if (chances.isEmpty()) return false;

    std::stable_sort(chances.begin(), chances.end(),
                     [](const Chance &a, const Chance &b) { return a.worth > b.worth; });

    // Une carte plaçable n'est pas repérée par tous les joueurs.
    for (const Chance &chance : chances) {
        if (!happens(skill.layoffSight)) continue;
        out->kind = Move::LayOff;
        out->cards = {chance.cardId};
        out->meldId = chance.meldId;
        return true;
    }
    return false;
}

// Rachat d'un joker dans une combinaison, suivi de son emploi immédiat à
// l'extrémité d'une autre. La carte cédée en échange était inutilisée : la
// main se trouve donc allégée d'une carte, et le joker est posé sur le tapis
// plutôt que décompté en pénalité.
bool findJokerBuy(const Rami &game, const Seat &seat, const QVector<Card> &loose, Move *out) {
    for (const Meld &meld : game.table()) {
        if (!meld.hasJoker()) continue;
        for (const Card &wanted : jokerWants(meld)) {
            for (const Card &card : loose) {
                if (card.joker() || card.rank != wanted.rank || card.suit != wanted.suit) continue;
                // L'opération n'a d'intérêt que si le joker peut ensuite être
                // placé : les règles interdisent d'achever le tour en le
                // conservant en main.
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
// Choix de la défausse.

// Évalue le bénéfice que cette carte procurerait à un adversaire. La valeur
// est nulle pour un joueur qui n'observe pas le jeu des autres, ce qui est le
// cas de la plupart d'entre eux.
qreal feedRisk(const Rami &game, int at, const Card &card, const Watch &watch,
               const Skill &skill) {
    if (!skill.guardsFeed) return 0.0;
    qreal risk = 0.0;

    // Carte plaçable directement sur une combinaison posée : c'est la pire
    // défausse possible, tout joueur ayant ouvert pouvant l'employer
    // immédiatement.
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

    // Sont également pris en compte les ramassages antérieurs. Un joueur ayant
    // pris le neuf de cœur sur la défausse recherche le huit et le dix de cœur
    // ainsi que les autres neuf. Il ne s'agit pas d'une supposition : la carte
    // a été ramassée au vu de tous.
    if (remembersTakes(skill)) {
        for (int seat = 0; seat < watch.taken.size(); ++seat) {
            if (seat == at) continue;
            for (const Card &want : watch.taken.at(seat)) {
                if (want.joker()) continue;
                if (want.rank == card.rank && want.suit != card.suit) risk += 3.0;
                if (want.suit == card.suit && std::abs(want.rank - card.rank) <= 2) risk += 3.0;
            }
        }
    }

    // Lorsqu'un joueur est sur le point de sortir, toute défausse devient
    // risquée.
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

    // Cartes isolées. Si la main entière se décompose en combinaisons — cas
    // qui se produit et impose d'en rompre une — c'est la plus longue qui cède
    // une carte d'extrémité.
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

    // Occasionnellement, un choix erroné.
    if (happens(skill.blunder)) {
        return loose.at(QRandomGenerator::global()->bounded(loose.size())).id;
    }

    // En situation d'urgence, un joueur sachant interpréter une fin de manche
    // se défait de ses cartes les plus coûteuses : les cartes restées en main
    // constituent la pénalité, et la réduire vaut mieux que conserver une
    // combinaison qui n'aura pas le temps d'aboutir.
    if (skill.readsEndgame && pressing(game, at)) {
        int dearest = -1;
        int cost = -1;
        for (const Card &card : loose) {
            if (card.joker()) continue;
            if (penaltyValue(card) > cost) {
                cost = penaltyValue(card);
                dearest = card.id;
            }
        }
        if (dearest >= 0) return dearest;
    }

    // La carte qui vient d'être prise sur la défausse est exclue. La reprendre
    // puis la redéfausser laisserait le tour dans son état initial — aucune
    // pose, aucune pioche, la même carte au sommet — et deux joueurs procédant
    // ainsi ne feraient jamais progresser la manche. Ce choix n'est autorisé
    // que s'il n'existe effectivement aucune autre défausse possible.
    const int justTaken = game.drewFromPile() ? game.lastDrawn().id : -1;
    bool somethingElse = false;
    for (const Card &card : loose) {
        if (!card.joker() && card.id != justTaken) somethingElse = true;
    }

    int chosen = loose.first().id;
    qreal lowest = 0.0;
    bool first = true;
    for (const Card &card : loose) {
        // Le joker n'est en aucun cas défaussé : ce serait céder vingt-cinq
        // points au joueur suivant.
        if (card.joker()) continue;
        if (somethingElse && card.id == justTaken) continue;

        QVector<Card> without;
        for (const Card &other : loose) {
            if (other.id != card.id) without.append(other);
        }
        // Valeur de la main après le retrait de cette carte : les combinaisons
        // à une carte près qui subsistent, diminuées de la pénalité que la
        // carte représentait, augmentées du bénéfice qu'elle procurerait aux
        // adversaires. Une valeur faible désigne la carte à défausser.
        //
        // Les deux poids traduisent le tempérament : un joueur prudent
        // conserve ce qui peut servir et en accepte la pénalité, un joueur
        // insouciant fait l'inverse. Ils varient en sens contraire l'un de
        // l'autre, afin qu'aucun tempérament ne constitue un avantage net.
        const qreal verdict =
            -skill.potentialWeight * skill.keepWeight *
                potentialOf(without, watch, remembers(skill)) -
            penaltyValue(card) * 0.70 * (2.0 - skill.keepWeight) +
            feedRisk(game, at, card, watch, skill);
        if (first || verdict < lowest) {
            lowest = verdict;
            chosen = card.id;
            first = false;
        }
    }
    return chosen;
}

// ---------------------------------------------------------------------------
// Piocher ou ramasser la défausse.

bool wantsPile(const Rami &game, int at, const Skill &skill, const Watch &watch) {
    const Card top = game.pileTop();
    if (!top.valid()) return false;
    const Seat &seat = game.seat(at);

    // Aucun joueur ne laisse passer un joker.
    if (top.joker()) return true;

    // Une main réduite à ses dernières cartes ne peut exploiter une
    // combinaison incomplète : poser une combinaison de trois cartes en
    // requiert quatre, une carte devant rester pour la défausse. À ce stade,
    // seule une carte directement plaçable sur le tapis présente un intérêt.
    // La prendre pour la combinaison incomplète qu'elle forme reviendrait à la
    // redéfausser ensuite, ce qui, à deux joueurs, empêche la manche de
    // s'achever : les deux mains se renvoient la même carte sans jamais
    // entamer la pioche.
    if (seat.hand.size() + 1 < 4) {
        Meld grown;
        for (const Meld &meld : game.table()) {
            if (canExtend(meld, top, &grown)) return true;
        }
        return false;
    }

    QVector<Card> with = seat.hand;
    with.append(top);

    const qreal before = evaluate(seat.hand, skill, watch, seat.opened);
    const qreal after = evaluate(with, skill, watch, seat.opened);

    // Un débutant ne prend la défausse que lorsqu'elle complète manifestement
    // une combinaison. Le seuil quantifie cette exigence et s'abaisse à mesure
    // que le joueur progresse.
    qreal wanted = skill.pileThreshold;
    // Lorsque la pioche est presque épuisée, la défausse constitue
    // l'essentiel des cartes disponibles.
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
    // Seuls les ramassages récents sont pris en compte : une carte prise huit
    // tours plus tôt ne renseigne plus sur les attentes actuelles du joueur.
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

Move chooseMove(const Rami &game, int at, const Skill &skill, const Watch &watch) {
    Move move;
    if (at != game.turn()) return move;
    const Seat &seat = game.seat(at);

    if (game.phase() == Phase::Draw) {
        move.kind = wantsPile(game, at, skill, watch) ? Move::DrawPile : Move::DrawStock;
        return move;
    }
    if (game.phase() != Phase::Build) return move;

    // Un joker racheté doit être placé en priorité : le tour ne peut s'achever
    // tant qu'il reste en main.
    const QVector<int> owed = game.jokerDebt(at);
    if (!owed.isEmpty()) {
        // C'est bien le joker dû qui doit être placé, et non un autre. Une
        // main détenant deux jokers pourrait placer celui qui n'est pas dû :
        // l'obligation subsisterait, le tour ne pourrait plus s'achever — ni
        // par une pose ni par une défausse — et la manche serait bloquée pour
        // tous les joueurs. Le cas s'est produit une fois sur mille manches à
        // l'arène.
        Skill sure = skill;
        sure.layoffSight = 1.0;
        for (int id : owed) {
            const Card *joker = findCard(seat.hand, id);
            if (!joker) continue;
            for (const Meld &meld : game.table()) {
                Meld grown;
                if (!canExtend(meld, *joker, &grown)) continue;
                move.kind = Move::LayOff;
                move.cards = {id};
                move.meldId = meld.id;
                return move;
            }
        }
        // À défaut d'emplacement disponible, une combinaison le contenant est
        // posée ; cette combinaison doit contenir le joker dû, pour la raison
        // exposée ci-dessus.
        for (const Meld &meld : layableMelds(seat.hand, sure, false)) {
            bool paysDebt = false;
            for (const Card &card : meld.cards) {
                if (owed.contains(card.id)) paysDebt = true;
            }
            if (!paysDebt) continue;
            if (seat.hand.size() - meld.cards.size() < 1) continue;
            move.kind = Move::Lay;
            move.cards = idsOf(meld);
            return move;
        }
    }

    if (!seat.opened) {
        // L'ouverture requiert cinquante et un points en un seul tour. La pose
        // n'est donc entamée qu'une fois ce total acquis : une pose
        // insuffisante devrait être reprise en main, ce qu'aucun joueur
        // avisé ne provoque délibérément.
        const int already = game.poseValue(at);
        const QVector<Meld> opening = layableMelds(seat.hand, skill, true);

        // Combinaisons effectivement posables. Une carte doit rester
        // disponible pour la défausse de fin de tour : une combinaison qui
        // emploierait la dernière carte ne peut être posée, et la compter dans
        // les cinquante et un points conduirait à poser un total insuffisant
        // sans pouvoir achever le tour.
        QVector<Meld> laying;
        int taken = 0;
        int sure = already;
        for (const Meld &meld : opening) {
            if (seat.hand.size() - taken - meld.cards.size() < 1) continue;
            laying.append(meld);
            taken += meld.cards.size();
            sure += meld.points();
        }

        // Total tel que l'analyse de la main le perçoit, que les combinaisons
        // soient posables ou non. Un joueur n'ayant pas acquis le rang
        // Lecture III se fie à cette valeur : il commence sa pose, n'atteint
        // pas le total requis et doit tout reprendre, perdant ainsi un tour.
        int hoped = already;
        for (const Meld &meld : opening) hoped += meld.points();

        const int worth = skill.countsOpening ? sure : hoped;
        // Une pose reprise en main n'est pas redéposée durant le même tour.
        if (worth >= openingBar(game, at, skill) && !laying.isEmpty() &&
            !game.poseTakenBack()) {
            move.kind = Move::Lay;
            move.cards = idsOf(laying.first());
            return move;
        }

        // Une pose qui ne peut plus être complétée doit revenir en main : le
        // tour ne peut s'achever sur une pose n'atteignant pas cinquante et un
        // points, et lorsque plus aucune combinaison ne peut être posée, la
        // reprise est le seul coup disponible.
        if (already > 0 && already < kOpeningPoints) {
            move.kind = Move::TakeBack;
            return move;
        }

        move.kind = Move::Discard;
        move.cards = {chooseDiscard(game, at, skill, watch)};
        return move;
    }

    // Joueur ouvert : tout ce qui peut être posé l'est, sous réserve des
    // réticences liées au tempérament.
    const bool urgent = pressing(game, at);
    for (const Meld &meld : layableMelds(seat.hand, skill, false)) {
        if (seat.hand.size() - meld.cards.size() < 1) continue;
        if (happens(skill.blunder * 0.4)) continue;   // and sometimes it is missed
        // Cette pose permet de sortir : aucun joueur ne diffère une sortie,
        // quel que soit son tempérament.
        const bool goingOut = seat.hand.size() - meld.cards.size() <= 1;
        // Conserver sa pose n'est possible qu'en début de manche, comme le
        // fait d'attendre pour ouvrir : passé la moitié de la pioche, tous les
        // joueurs posent ce qu'ils détiennent. Une manche où deux joueurs
        // attendent mutuellement ne s'achève pas ; l'arène a relevé ce cas une
        // fois sur deux mille cinq cents manches.
        if (!goingOut && !urgent && game.stockLeft() > 24) {
            // Un joueur prudent n'emploie pas un joker dans une pose
            // réalisable autrement, et un joueur audacieux conserve sa pose en
            // main, celle-ci pouvant encore s'étoffer, une sortie en un seul
            // coup étant préférable à une pose fractionnée. Ces deux
            // comportements sont abandonnés en situation d'urgence.
            if (usesJoker(meld) && happens(skill.jokerGrip)) continue;
            if (skill.eagerness < 1.0 && happens(1.0 - skill.eagerness)) continue;
        }
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
