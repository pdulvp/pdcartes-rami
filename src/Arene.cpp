// L'arène : exécution du jeu sans interface, à des fins de mesure.
//
// Les six adversaires n'ayant pas le même comportement, les défauts de
// l'un d'eux ne se manifestent pas sur quelques manches jouées manuellement.
// Cette cible dispute donc quelques milliers de manches à chaque table et
// produit quatre indicateurs :
//
//   les sorties      proportion des manches achevées par un joueur s'étant
//                    défait de sa dernière carte. C'est l'indicateur
//                    principal : une manche interrompue par épuisement de la
//                    pioche est une manche sans vainqueur, et une table où ce
//                    cas survient la moitié du temps présente peu d'intérêt.
//   la pioche sèche  proportion complémentaire.
//   les coincées     manches ayant dû être abandonnées. Cet indicateur doit
//                    toujours être nul ; une seule occurrence suffit à
//                    rejeter une modification.
//   les tours        durée moyenne d'une manche. Une logique de jeu plus
//                    prudente l'allonge, et une manche deux fois plus longue
//                    n'est pas deux fois plus intéressante.
//
// Les duels répondent à une seconde question : l'expérience a-t-elle un effet
// mesurable ? Deux logiques de jeu, deux niveaux d'expérience, mille manches ;
// le taux de victoire doit croître avec l'expérience. Dans le cas contraire,
// le niveau affiché sur la plaque ne correspond à rien.
//
// L'arène se lance manuellement, avant et après toute modification de
// Brain.cpp :
//
//     ./build/pdcartes-rami-arene 150          mesure de référence (~5 min)
//     ./build/pdcartes-rami-arene 50 0         mesure rapide, pour un essai
//     ./build/pdcartes-rami-arene 0 400        duels seuls
//
// Aucune écriture n'est effectuée dans les réglages : les compétences sont
// définies dans le code et non lues sur un profil, afin que les mesures ne
// fassent pas progresser les adversaires.

#include "Brain.h"
#include "Rami.h"
#include "Rivals.h"
#include "Tables.h"

#include <QCoreApplication>
#include <QStringList>
#include <QTextStream>

#include <algorithm>

namespace {

// Même garde-fou que la table (TablePage.cpp:36) : au-delà de trente coups
// dans un tour, la logique de jeu est considérée comme bloquée.
constexpr int kMostMovesPerTurn = 30;

// Garde-fou équivalent pour une manche entière. Cent huit cartes et deux
// reconstitutions de la pioche ne représentent pas deux cents tours ; au-delà,
// la manche est comptabilisée comme bloquée, ce qui constitue précisément
// l'objet de la mesure.
constexpr int kMostTurnsPerHand = 200;

struct Tally {
    int hands = 0;
    int wentOut = 0;
    int dried = 0;
    int stuck = 0;          // abandonnées, et par laquelle des deux bornes
    int stuckTurns = 0;     // la manche n'en finissait pas
    int stuckMoves = 0;     // un tour tournait en rond
    qint64 turns = 0;
    qint64 reshuffles = 0;
    qint64 points = 0;        // points comptés, toutes mains de toutes manches
    int matches = 0;
    QVector<int> handWins;
    QVector<int> matchWins;

    void seats(int count) {
        handWins.fill(0, count);
        matchWins.fill(0, count);
    }
};

// Joue un coup selon le même enchaînement que `TablePage::playRival`, à
// l'exclusion de l'affichage. Cette identité est délibérée : la mesure doit
// porter sur le comportement réel du jeu et non sur une version simplifiée
// qui se comporterait différemment.
const char *moveName(Move::Kind kind) {
    switch (kind) {
    case Move::None: return "rien";
    case Move::DrawStock: return "pioche";
    case Move::DrawPile: return "ramasse";
    case Move::Lay: return "pose";
    case Move::LayOff: return "complete";
    case Move::BuyJoker: return "rachete";
    case Move::TakeBack: return "reprend";
    case Move::Discard: return "jette";
    }
    return "?";
}

bool playMove(Rami &game, const Skill &skill, Watch &watch, QStringList *trail) {
    const int at = game.turn();
    const Move move = chooseMove(game, at, skill, watch);
    bool done = false;

    switch (move.kind) {
    case Move::DrawStock:
        done = game.drawStock();
        break;
    case Move::DrawPile:
        watch.noteTaken(at, game.pileTop(), game.seatCount());
        done = game.drawPile();
        break;
    case Move::Lay:
        done = game.lay(move.cards);
        break;
    case Move::LayOff:
        if (!move.cards.isEmpty()) done = game.layOff(move.cards.first(), move.meldId);
        break;
    case Move::BuyJoker:
        if (!move.cards.isEmpty()) done = game.buyJoker(move.cards.first(), move.meldId);
        break;
    case Move::TakeBack:
        done = game.takeBackPose();
        break;
    case Move::Discard:
        if (!move.cards.isEmpty() && move.cards.first() >= 0) {
            if (const Card *card = findCard(game.seat(at).hand, move.cards.first())) {
                watch.note(*card);
            }
            done = game.discard(move.cards.first());
        }
        break;
    case Move::None:
        break;
    }
    if (trail) {
        if (trail->size() > 48) trail->removeFirst();
        trail->append(QStringLiteral("%1:%2%3")
                          .arg(at)
                          .arg(QLatin1String(moveName(move.kind)))
                          .arg(done ? QString() : QStringLiteral("(refusé)")));
    }
    if (done) return true;

    // Coup refusé : la pose est reprise en main ; à défaut, le joueur pioche ;
    // à défaut, il défausse la première carte disponible. Même mécanisme de
    // rattrapage que la table.
    if (game.takeBackPose()) return true;
    if (game.phase() == Phase::Draw) return game.drawStock();
    for (const Card &card : game.seat(at).hand) {
        if (game.discard(card.id)) return true;
    }
    return false;
}

// Joue une manche, du premier tour au décompte. Retourne faux si la manche a
// dû être abandonnée.
// Trace imprimée lorsqu'une manche a dû être abandonnée : l'historique des
// coups et l'état de la table. Sans cette trace, une manche bloquée sur mille
// serait inexploitable.
void sayStuck(const Rami &game, const QStringList &trail, const char *why) {
    QTextStream out(stdout);
    const int at = game.turn();
    out << "  coincée (" << why << ") : siège " << at << ", "
        << (game.phase() == Phase::Draw ? "à piocher" : "à poser")
        << ", main " << game.seat(at).hand.size() << ", pose en attente "
        << game.seat(at).waiting.size() << " (" << game.poseValue(at) << " pts)"
        << ", joker dû " << game.jokerDebt(at).size() << ", pioche " << game.stockLeft()
        << ", défausse " << game.pile().size() << ", tapis " << game.table().size()
        << ", retournements " << game.reshuffles() << "\n    " << trail.join(QStringLiteral(" "))
        << "\n";
    out.flush();
}

bool playHand(Rami &game, const QVector<Skill> &skills, Tally &tally) {
    Watch watch;
    watch.taken.resize(game.seatCount());
    QStringList trail;

    int turns = 0;
    int moves = 0;
    int lastSeat = game.turn();

    while (game.phase() == Phase::Draw || game.phase() == Phase::Build) {
        const int at = game.turn();
        if (at != lastSeat) {
            ++turns;
            moves = 0;
            lastSeat = at;
        }
        if (turns > kMostTurnsPerHand) {
            ++tally.hands;
            ++tally.stuck;
            ++tally.stuckTurns;
            sayStuck(game, trail, "manche interminable");
            return false;
        }
        if (++moves > kMostMovesPerTurn) {
            ++tally.hands;
            ++tally.stuck;
            ++tally.stuckMoves;
            sayStuck(game, trail, "un tour qui tourne en rond");
            return false;
        }
        if (!playMove(game, skills.at(at), watch, &trail)) {
            // Un coup refusé ne signale pas nécessairement une manche
            // bloquée : l'épuisement de la pioche met fin à la manche tout en
            // retournant faux (Rami.cpp:138). Il s'agit alors d'une manche
            // interrompue faute de cartes et non d'une logique de jeu en
            // échec. Le critère distinctif est l'état d'avancement de la
            // manche.
            if (game.phase() == Phase::Draw || game.phase() == Phase::Build) {
                ++tally.hands;
                ++tally.stuck;
                sayStuck(game, trail, "plus un coup légal");
                return false;
            }
            break;
        }
    }

    ++tally.hands;
    tally.turns += turns + 1;
    tally.reshuffles += game.reshuffles();
    if (game.outSeat() >= 0) ++tally.wentOut;
    else ++tally.dried;

    // Vainqueur de la manche : le total le plus faible, le joueur sorti étant
    // départagé en sa faveur en cas d'égalité. C'est le classement dont dépend
    // l'attribution de l'expérience (TablePage.cpp).
    int best = -1;
    for (int at = 0; at < game.seatCount(); ++at) {
        tally.points += game.seat(at).lastHand;
        if (best < 0 || game.seat(at).lastHand < game.seat(best).lastHand) best = at;
    }
    if (game.outSeat() >= 0) best = game.outSeat();
    if (best >= 0 && best < tally.handWins.size()) ++tally.handWins[best];
    return true;
}

// ---------------------------------------------------------------------------
// Les tables.

// Le joueur humain, puis les adversaires dans leur ordre de définition. Les
// niveaux initiaux étant volontairement étalés (Rivals.cpp), une table de
// quatre comporte des débutants et des intermédiaires, et une table de six
// comporte l'adversaire de plus haut niveau.
QVector<SeatSpec> seatsFor(int count) {
    QVector<SeatSpec> specs;
    SeatSpec you;
    you.name = QStringLiteral("Vous");
    specs.append(you);
    for (int i = 0; i + 1 < count && i < rivals().size(); ++i) {
        const Rival &who = rivals().at(i);
        SeatSpec spec;
        spec.rivalId = who.id;
        spec.name = who.name;
        spec.hue = who.hue;
        specs.append(spec);
    }
    return specs;
}

QVector<Skill> skillsFor(const QVector<SeatSpec> &specs) {
    QVector<Skill> skills;
    for (const SeatSpec &spec : specs) {
        // La place du joueur humain est tenue par la logique du bouton
        // Conseil, c'est-à-dire le meilleur joueur que le programme sache
        // simuler, ce qui constitue la référence la plus exigeante pour les
        // six adversaires.
        if (spec.rivalId.isEmpty()) {
            skills.append(bestSkill());
            continue;
        }
        // Tempérament et expérience initiale, définis ici plutôt que lus sur
        // un profil, afin que les mesures ne fassent progresser aucun
        // adversaire.
        const Rival &who = rivalById(spec.rivalId);
        skills.append(composeSkill(who.innate, poolsFromXp(who.startXp, who.innate)));
    }
    return skills;
}

Tally runTable(int seats, int matches) {
    const QVector<SeatSpec> specs = seatsFor(seats);
    const QVector<Skill> skills = skillsFor(specs);
    const Target &normale = targetById(QStringLiteral("normale"));
    const int points = targetPointsFor(normale, seats);

    Tally tally;
    tally.seats(seats);
    for (int m = 0; m < matches; ++m) {
        Rami game;
        game.startMatch(specs, points);
        while (true) {
            if (!playHand(game, skills, tally)) break;
            if (game.phase() == Phase::MatchOver) {
                ++tally.matches;
                const int winner = game.matchWinner();
                if (winner >= 0 && winner < tally.matchWins.size()) ++tally.matchWins[winner];
                break;
            }
            game.startHand();
        }
    }
    return tally;
}

// ---------------------------------------------------------------------------
// Les duels : une même table opposant deux niveaux d'expérience, toutes
// choses égales par ailleurs.

// Les manches sont jouées par paires, les places étant permutées entre les
// deux manches d'une paire : le donneur n'entamant pas, l'avantage d'un
// demi-tour serait sensible sur mille manches.
void duel(int xpLeft, int xpRight, int hands, int *leftWins, int *rightWins) {
    *leftWins = 0;
    *rightWins = 0;
    for (int h = 0; h < hands; ++h) {
        const bool swapped = (h % 2) == 1;
        QVector<SeatSpec> specs;
        SeatSpec a;
        a.rivalId = QStringLiteral("gauche");
        a.name = QStringLiteral("gauche");
        SeatSpec b;
        b.rivalId = QStringLiteral("droite");
        b.name = QStringLiteral("droite");
        specs.append(swapped ? b : a);
        specs.append(swapped ? a : b);

        // Deux logiques de jeu au tempérament neutre : seule l'expérience
        // diffère, ce qui constitue l'objet de la mesure.
        const Innates even;
        QVector<Skill> skills;
        skills.append(composeSkill(even, poolsFromXp(swapped ? xpRight : xpLeft, even)));
        skills.append(composeSkill(even, poolsFromXp(swapped ? xpLeft : xpRight, even)));

        Tally quiet;
        quiet.seats(2);
        Rami game;
        game.startMatch(specs, 0);   // une manche, et c'est la partie
        if (!playHand(game, skills, quiet)) continue;
        if (quiet.handWins.isEmpty()) continue;
        const int leftSeat = swapped ? 1 : 0;
        if (quiet.handWins.at(leftSeat) > 0) ++(*leftWins);
        else ++(*rightWins);
    }
}

QString percent(qint64 part, qint64 whole) {
    if (whole <= 0) return QStringLiteral("    -");
    return QStringLiteral("%1 %").arg(100.0 * qreal(part) / qreal(whole), 5, 'f', 1);
}

QString average(qint64 total, qint64 count, int digits = 1) {
    if (count <= 0) return QStringLiteral("    -");
    return QStringLiteral("%1").arg(qreal(total) / qreal(count), 6, 'f', digits);
}

} // namespace

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("pdcartes-rami-arene"));

    const QStringList args = QCoreApplication::arguments();
    // Un nombre de parties nul fait ignorer les tables et n'exécute que les
    // duels : les deux séries étant longues, elles ne sont pas toujours
    // relancées ensemble.
    const int matches = args.size() > 1 ? std::max(0, args.at(1).toInt()) : 300;
    const int duels = args.size() > 2 ? std::max(1, args.at(2).toInt()) : 1000;

    QTextStream out(stdout);
    if (matches > 0) {
        out << "Arène — " << matches << " parties normales par table\n\n";
        out << "table      manches  sorties  pioche sèche  coincées  tours/manche  "
               "points/main  retournements\n";
    }

    for (const TableKind &kind : tableKinds()) {
        if (matches <= 0) break;
        const Tally tally = runTable(kind.seats, matches);
        out << QStringLiteral("%1").arg(kind.id, -10)
            << QStringLiteral("%1").arg(tally.hands, 9) << "  "
            << percent(tally.wentOut, tally.hands) << "       "
            << percent(tally.dried, tally.hands) << "   "
            << QStringLiteral("%1").arg(tally.stuck, 7) << "   "
            << average(tally.turns, tally.hands) << "       "
            << average(tally.points, qint64(tally.hands) * kind.seats) << "        "
            << average(tally.reshuffles, tally.hands, 2) << "\n";

        // Répartition des victoires à cette table. La place du joueur humain
        // étant tenue par la logique du bouton Conseil, la ligne « Vous »
        // indique le résultat qu'obtiendrait un très bon joueur.
        const QVector<SeatSpec> specs = seatsFor(kind.seats);
        if (tally.stuck > 0) {
            out << "           coincées : " << tally.stuckMoves << " en tournant en rond dans un tour, "
                << tally.stuckTurns << " en manche interminable\n";
        }
        out << "           parties : ";
        for (int at = 0; at < specs.size(); ++at) {
            out << specs.at(at).name << " " << percent(tally.matchWins.at(at), tally.matches);
            if (at + 1 < specs.size()) out << " · ";
        }
        out << "\n";
        out.flush();
    }

    out << "\nDuels — " << duels << " manches en tête-à-tête, à expérience fixée\n\n";
    const QVector<int> ladder = {0, 200, 400, 600, 900, 1200};
    for (int i = 0; i + 1 < ladder.size(); ++i) {
        const int low = ladder.at(i);
        const int high = ladder.at(i + 1);
        int lowWins = 0;
        int highWins = 0;
        duel(low, high, duels, &lowWins, &highWins);
        out << QStringLiteral("%1 xp (%2) contre %3 xp (%4) : %5 contre %6\n")
                   .arg(low, 4)
                   .arg(levelName(levelOf(low)).toLower(), -13)
                   .arg(high, 4)
                   .arg(levelName(levelOf(high)).toLower(), -13)
                   .arg(percent(lowWins, lowWins + highWins))
                   .arg(percent(highWins, lowWins + highWins));
        out.flush();
    }

    // Les deux extrémités de l'échelle, pour couvrir toute sa plage.
    int worst = 0;
    int bestWins = 0;
    duel(0, 1200, duels, &worst, &bestWins);
    out << QStringLiteral("   0 xp contre 1200 xp : %1 contre %2\n")
               .arg(percent(worst, worst + bestWins))
               .arg(percent(bestWins, worst + bestWins));
    return 0;
}
