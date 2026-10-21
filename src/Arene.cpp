// L'arène : le jeu joué par personne, pour voir s'il tient.
//
// Six adversaires qui ne jouent pas pareil, c'est six façons de gâcher une
// partie, et aucune ne se voit en jouant trois manches à la main. Cette cible
// fait donc ce que personne n'a le temps de faire : quelques milliers de
// manches à chaque table, et quatre chiffres qui disent si le jeu va bien.
//
//   les sorties      la part des manches que quelqu'un termine en se
//                    débarrassant de sa dernière carte. C'est le chiffre qui
//                    compte : une manche qui s'arrête parce que la pioche est
//                    morte est une manche sans fin heureuse, et une table où
//                    cela arrive la moitié du temps n'est pas une table où
//                    l'on a envie de s'asseoir.
//   la pioche sèche  le reste, par opposition.
//   les coincées     celles qu'il a fallu abandonner, et qui devraient
//                    toujours être à zéro. Un seul suffit à refuser un
//                    changement.
//   les tours        la longueur d'une manche. Un cerveau plus prudent la
//                    rallonge, et une manche deux fois plus longue n'est pas
//                    deux fois plus intéressante.
//
// Et les duels, qui répondent à l'autre question : l'expérience sert-elle à
// quelque chose ? Deux cerveaux, deux expériences, mille manches, et le taux
// de victoire doit monter avec les points. S'il ne monte pas, le niveau
// affiché sur la plaque est un mensonge.
//
// Elle se lance à la main, avant et après avoir touché à Brain.cpp :
//
//     ./build/pdcartes-rami-arene 150          la mesure de référence (~5 min)
//     ./build/pdcartes-rami-arene 50 0         plus vite, pour un essai
//     ./build/pdcartes-rami-arene 0 400        les duels seuls
//
// Rien ici n'écrit dans les réglages : les compétences sont posées dans le
// code, pas lues sur le profil, pour que mesurer une partie ne fasse pas
// progresser les six.

#include "Brain.h"
#include "Rami.h"
#include "Rivals.h"
#include "Tables.h"

#include <QCoreApplication>
#include <QStringList>
#include <QTextStream>

#include <algorithm>

namespace {

// Le même filet que la table (TablePage.cpp:36) : trente coups dans un tour,
// et le cerveau est manifestement en train de tourner en rond.
constexpr int kMostMovesPerTurn = 30;

// Et le même pour une manche entière. Cent-huit cartes et deux retournements
// ne font pas deux cents tours ; au-delà, la manche est comptée coincée, ce
// qui est précisément ce qu'on vient mesurer.
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

// Un coup, joué comme la table le joue -- moins l'écran. C'est volontairement
// le même enchaînement que TablePage::playRival : ce qu'on mesure doit être
// ce qui se passe quand on joue, et pas une version simplifiée qui se
// comporterait mieux.
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

    // Un coup refusé : la pose revient en main, sinon on pioche, sinon on
    // jette ce qui vient. Le même rattrapage que la table.
    if (game.takeBackPose()) return true;
    if (game.phase() == Phase::Draw) return game.drawStock();
    for (const Card &card : game.seat(at).hand) {
        if (game.discard(card.id)) return true;
    }
    return false;
}

// Une manche, du premier tour au décompte. Faux quand il a fallu l'abandonner.
// Ce qu'on imprime quand une manche a dû être abandonnée. Un fil de coups et
// l'état de la table: sans cela, une manche coincée sur mille est une rumeur.
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
            // Un coup qui échoue n'est pas forcément une manche coincée: la
            // pioche morte fait finir la manche et renvoie faux quand même
            // (Rami.cpp:138) -- c'est la manche appelée faute de cartes, pas
            // un cerveau qui ne sait plus quoi faire. Ce qui distingue les
            // deux, c'est si la manche est encore en cours.
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

    // Qui a gagné la manche : le moins de points, et le sorti devant à
    // égalité -- le classement dont l'expérience se nourrit (TablePage.cpp).
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

// Vous, puis les adversaires dans l'ordre où ils arrivent : les niveaux de
// départ sont étalés exprès (Rivals.cpp), donc une table de quatre a du
// débutant et de l'intermédiaire, et une table de six a Oriane.
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
        // Votre siège est tenu par le cerveau du bouton Conseil : c'est le
        // meilleur joueur que le jeu sache faire, et donc l'étalon le plus
        // dur que les six auront en face.
        if (spec.rivalId.isEmpty()) {
            skills.append(bestSkill());
            continue;
        }
        // Ce qu'il est, et l'expérience avec laquelle il s'assied: composés
        // ici plutôt que lus sur un profil, pour que mesurer ne fasse
        // progresser personne.
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
// Les duels : la même table pour deux expériences, et rien d'autre qui change.

// Les manches sont jouées par paires, les sièges échangés entre les deux :
// celui qui donne ne mène pas, et un demi-tour d'avance se voit sur mille
// manches.
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

        // Deux cerveaux sans caractère: seule l'expérience diffère, qui est
        // très exactement la question posée.
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
    // Zéro parties : on saute les tables et on ne fait que les duels, parce
    // que les deux moitiés sont longues et qu'on ne remesure pas toujours les
    // deux.
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

        // Qui gagne, à cette table. Votre siège est tenu par le Conseil, donc
        // "Vous" est ce qu'un très bon joueur obtiendrait.
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

    // Et les deux bouts, pour l'échelle entière.
    int worst = 0;
    int bestWins = 0;
    duel(0, 1200, duels, &worst, &bestWins);
    out << QStringLiteral("   0 xp contre 1200 xp : %1 contre %2\n")
               .arg(percent(worst, worst + bestWins))
               .arg(percent(bestWins, worst + bestWins));
    return 0;
}
