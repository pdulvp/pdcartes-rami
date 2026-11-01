#include "TablePage.h"

#include "TableLook.h"

#include "FeltArt.h"
#include "Palette.h"
#include "Profiles.h"
#include "Rivals.h"
#include "ScoreBoard.h"
#include "TableView.h"
#include "VictoryEffect.h"

#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QPushButton>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

namespace {

constexpr int kActionsHeight = 50;

// Durée de temporisation avant le coup d'un adversaire. Elle ne vise pas à
// simuler une réflexion : un coup joué immédiatement après le précédent ne
// serait pas perceptible, alors que l'observation du jeu des adversaires
// constitue une part importante de la partie.
constexpr int kThinkMs = 620;
constexpr int kMoveMs = 330;

// Au-delà de ce nombre de coups, la logique de décision est considérée comme
// bloquée. Aucune cause connue ne doit provoquer ce cas ; ce garde-fou existe
// précisément pour cette raison.
constexpr int kMostMovesPerTurn = 30;

QPushButton *actionButton(QWidget *parent, const QString &text, const QString &tip,
                          const QString &keys) {
    auto *button = new QPushButton(text, parent);
    button->setToolTip(keys.isEmpty() ? tip : QStringLiteral("%1 (%2)").arg(tip, keys));
    button->setCursor(Qt::PointingHandCursor);
    button->setFocusPolicy(Qt::NoFocus);
    return button;
}

} // namespace

TablePage::TablePage(QWidget *parent) : QWidget(parent) {
    m_game = new Rami(this);
    m_view = new TableView(this);
    m_view->watch(m_game);

    auto *rows = new QVBoxLayout(this);
    rows->setContentsMargins(0, 0, 0, 0);
    rows->setSpacing(0);
    rows->addWidget(m_view, 1);
    rows->addWidget(buildActions());

    // Affiché par-dessus la table, et uniquement lorsque c'est nécessaire.
    m_board = new ScoreBoard(this);
    m_victory = new VictoryEffect(this);

    m_beat = new QTimer(this);
    m_beat->setSingleShot(true);
    connect(m_beat, &QTimer::timeout, this, &TablePage::playRival);

    connect(m_game, &Rami::changed, this, [this] {
        m_view->refresh();
        updateActions();
        Q_EMIT stateChanged();
    });
    connect(m_game, &Rami::turnBegan, this, &TablePage::onTurnBegan);
    connect(m_game, &Rami::acted, this, &TablePage::onActed);
    connect(m_game, &Rami::handFinished, this, &TablePage::onHandFinished);
    connect(m_game, &Rami::matchFinished, this, &TablePage::onMatchFinished);

    connect(m_view, &TableView::stockPressed, this, &TablePage::onStockPressed);
    connect(m_view, &TableView::pilePressed, this, &TablePage::onPilePressed);
    connect(m_view, &TableView::meldPressed, this, &TablePage::onMeldPressed);
    connect(m_view, &TableView::meldRightPressed, this, &TablePage::onMeldRightPressed);
    connect(m_view, &TableView::poseCardPressed, this, &TablePage::onPoseCardPressed);
    connect(m_view, &TableView::cardDoubleClicked, this, &TablePage::onCardDoubleClicked);
    connect(m_view, &TableView::chosenChanged, this, &TablePage::updateActions);
    connect(m_view, &TableView::sortChosen, this, &TablePage::sort);
    connect(m_view, &TableView::zoomChanged, this, [this](int percent) {
        m_settings.zoom = percent;
        Q_EMIT zoomChanged(percent);
    });

    connect(m_board, &ScoreBoard::goOn, this, [this] {
        m_board->hideBoard();
        m_victory->stop();
        if (m_game->phase() == Phase::MatchOver) {
            startMatch(m_settings);
        } else {
            m_watch.clear();
            forgetDeeds();
            m_handCounted = false;
            m_game->startHand();
        }
    });
    connect(m_board, &ScoreBoard::leave, this, [this] {
        m_board->hideBoard();
        m_victory->stop();
        Q_EMIT wantsMenu();
    });
    connect(m_victory, &VictoryEffect::finished, this, [this] {
        m_victory->stop();
        m_board->raise();
        m_board->show();
    });

    setFocusPolicy(Qt::StrongFocus);
}

QWidget *TablePage::buildActions() {
    m_actions = new QWidget(this);
    m_actions->setObjectName(QStringLiteral("actionBar"));
    m_actions->setFixedHeight(kActionsHeight);

    m_pose = new QLabel(m_actions);

    m_lay = actionButton(m_actions, tr("Poser"),
                         tr("Poser les cartes choisies en une combinaison"), tr("Entrée"));
    m_layOff = actionButton(m_actions, tr("Compléter"),
                            tr("Mettre les cartes choisies sur les combinaisons du tapis"),
                            tr("C"));
    m_takeBack = actionButton(m_actions, tr("Reprendre"),
                              tr("Reprendre en main toute la pose — ou cliquez une seule "
                                 "carte de la pose pour ne reprendre qu'elle"),
                              {});
    m_discard = actionButton(m_actions, tr("Défausser"),
                             tr("Jeter la carte choisie et finir le tour"), tr("D"));
    m_hint = actionButton(m_actions, tr("Conseil"), tr("Ce que le meilleur joueur ferait"),
                          tr("H"));

    auto *across = new QHBoxLayout(m_actions);
    across->setContentsMargins(14, 0, 14, 0);
    across->setSpacing(7);
    across->addWidget(m_pose);
    across->addStretch();
    across->addWidget(m_lay);
    across->addWidget(m_layOff);
    across->addWidget(m_takeBack);
    across->addSpacing(12);
    across->addWidget(m_hint);
    across->addWidget(m_discard);

    connect(m_lay, &QPushButton::clicked, this, &TablePage::doLay);
    connect(m_layOff, &QPushButton::clicked, this, &TablePage::doLayOff);
    connect(m_takeBack, &QPushButton::clicked, this, &TablePage::doTakeBack);
    connect(m_discard, &QPushButton::clicked, this, &TablePage::doDiscard);
    connect(m_hint, &QPushButton::clicked, this, &TablePage::doHint);

    return m_actions;
}

// ---------------------------------------------------------------------------
// Qui est à table.

QVector<SeatSpec> TablePage::chooseSeats(const TableKind &table) const {
    QVector<SeatSpec> seats;
    // Le joueur humain occupe toujours la première place, afin que la partie
    // débute par son tour plutôt que par une attente.
    SeatSpec you;
    you.name = tr("Vous");
    you.hue = feltHue();
    seats.append(you);

    const int wanted = std::max(0, table.seats - 1);

    // Les adversaires invités sont placés en premier, puis les moins
    // sollicités, afin que les six progressent à un rythme comparable.
    QVector<QString> order;
    for (const QString &id : m_settings.invited) {
        if (order.size() >= wanted) break;
        if (!order.contains(id)) order.append(id);
    }

    QVector<Rival> rest;
    for (const Rival &rival : rivals()) {
        if (!order.contains(rival.id)) rest.append(rival);
    }
    std::stable_sort(rest.begin(), rest.end(), [](const Rival &a, const Rival &b) {
        return learningFor(a.id).handsPlayed < learningFor(b.id).handsPlayed;
    });
    for (const Rival &rival : rest) {
        if (order.size() >= wanted) break;
        order.append(rival.id);
    }

    for (const QString &id : order) {
        const Rival &rival = rivalById(id);
        SeatSpec spec;
        spec.rivalId = rival.id;
        // Le nom attribué sur ce profil est employé plutôt que le nom
        // d'origine : c'est celui qui figure sur sa plaque et dans tous les
        // messages le concernant.
        spec.name = rivalName(rival.id);
        spec.hue = rival.hue;
        seats.append(spec);
    }
    return seats;
}

void TablePage::startMatch(const GameSettings &settings) {
    m_settings = settings;
    const TableKind &table = tableKindById(settings.tableId);
    const Target &target = targetById(settings.targetId);

    m_board->hideBoard();
    m_victory->stop();
    m_beat->stop();
    m_view->setZoom(m_settings.zoom);
    m_view->stopAnimations();
    m_watch.clear();
    m_handCounted = false;

    const QVector<SeatSpec> seats = chooseSeats(table);
    m_watch.taken.resize(seats.size());
    m_deeds.resize(seats.size());
    forgetDeeds();

    m_skills.clear();
    for (const SeatSpec &spec : seats) {
        m_skills.append(spec.rivalId.isEmpty() ? bestSkill()
                                               : skillFor(spec.rivalId));
    }

    m_game->startMatch(seats, targetPointsFor(target, seats.size()));
    m_view->watch(m_game);
    m_view->clearChosen();
    updateActions();
    setFocus();
}

void TablePage::applySettings(const GameSettings &settings) {
    const QString table = m_settings.tableId;
    const QString target = m_settings.targetId;
    m_settings = settings;
    // La table et la durée de partie ne s'appliquent pas à une partie en
    // cours : un changement de réglage ne doit pas interrompre une manche
    // entamée. Ces valeurs prennent effet à la partie suivante.
    m_settings.tableId = table;
    m_settings.targetId = target;
    m_hint->setVisible(m_settings.offerHints);
    m_view->setZoom(m_settings.zoom);
    updateActions();
}

void TablePage::refresh() {
    const Palette &scheme = currentPalette();
    // Le bandeau inférieur reprend les couleurs du bandeau supérieur : ce sont
    // les deux bords de la même fenêtre, et un bandeau clair sous un tapis
    // sombre donnait l'impression d'un panneau étranger à l'application.
    const QColor barFace = feltBarFace(scheme);
    const QColor barInk = scheme.feltText;
    QColor barAside = barInk;
    barAside.setAlphaF(0.62);
    m_actions->setStyleSheet(
        QStringLiteral("QWidget#actionBar { background: %1; border-top: 1px solid %2; }"
                       "QPushButton { background: %3; color: %4; border: 1px solid %2;"
                       " border-radius: 7px; padding: 5px 13px 7px 13px; }"
                       "QPushButton:hover:enabled { border: 1px solid %5; background: %6; }"
                       "QPushButton:pressed:enabled { background: %7; }"
                       "QPushButton:disabled { color: %2; background: %1; border-color: %1; }"
                       "QPushButton[strong=\"true\"]:enabled { background: %5; color: %8;"
                       " border: 1px solid %5; font-weight: bold; }"
                       "QLabel { color: %9; }")
            .arg(barFace.name(), feltGold(scheme, 0.30).name(QColor::HexArgb),
                 feltPlaqueFill(scheme).name(QColor::HexArgb), barInk.name(),
                 scheme.gold.name(), feltPlaqueFill(scheme, 0.14).name(QColor::HexArgb),
                 feltPlaqueFill(scheme, 0.26).name(QColor::HexArgb),
                 readableTextOn(scheme.gold).name(), barAside.name(QColor::HexArgb)));

    QPalette bar = m_actions->palette();
    bar.setColor(QPalette::Window, barFace);
    bar.setColor(QPalette::WindowText, barInk);
    m_actions->setPalette(bar);
    m_actions->setAutoFillBackground(true);

    m_board->refresh();
    m_view->refresh();
    updateActions();
}

int TablePage::mySeat() const {
    for (int at = 0; at < m_game->seatCount(); ++at) {
        if (m_game->seat(at).human()) return at;
    }
    return -1;
}

bool TablePage::playing() const {
    return m_game->seatCount() >= 2 && m_game->phase() != Phase::Idle;
}

QString TablePage::subtitle() const {
    if (!playing()) return QString();
    const TableKind &table = tableKindById(m_settings.tableId);
    const Target &target = targetById(m_settings.targetId);
    const int points = m_game->targetPoints();
    if (m_game->phase() == Phase::MatchOver) {
        const int winner = m_game->matchWinner();
        return tr("%1 — %2 gagne")
            .arg(table.name, winner >= 0 ? m_game->seat(winner).name : tr("personne"));
    }
    if (points > 0) {
        return tr("%1 — manche %2, en %3 points").arg(table.name).arg(m_game->handNumber()).arg(points);
    }
    return tr("%1 — une manche").arg(table.name);
}

int TablePage::thinkingMs(bool first) const {
    const qreal part = m_settings.pace == 0 ? 1.7 : (m_settings.pace == 2 ? 0.42 : 1.0);
    return std::max(40, int((first ? kThinkMs : kMoveMs) * part));
}

// ---------------------------------------------------------------------------
// Le tour des autres.

void TablePage::onTurnBegan(int seat) {
    m_movesThisTurn = 0;
    m_view->clearChosen();
    if (m_game->seat(seat).human()) {
        sayWhatToDo();
        updateActions();
        return;
    }
    m_view->say(tr("À %1 de jouer…").arg(m_game->seat(seat).name));
    m_beat->start(thinkingMs(true));
}

void TablePage::onActed(int seat, const QString &tale) {
    Q_UNUSED(seat);
    if (!tale.isEmpty()) m_view->say(tale);
}

void TablePage::forgetDeeds() {
    for (Deeds &deeds : m_deeds) deeds.clear();
    m_pileFrom = -1;
}

// Une carte ramassée sur la défausse fait progresser deux joueurs : celui qui
// la prend, dans l'exploitation de la défausse ; celui qui l'avait défaussée,
// dans le choix de sa défausse. C'est le seul événement du jeu qui attribue de
// l'expérience à deux joueurs simultanément.
void TablePage::noteTakenFromPile(int at) {
    if (at >= 0 && at < m_deeds.size()) m_deeds[at].did(Learned::Memoire);
    if (m_pileFrom >= 0 && m_pileFrom != at && m_pileFrom < m_deeds.size()) {
        m_deeds[m_pileFrom].did(Learned::Defausse);
    }
    m_pileFrom = -1;
}

void TablePage::playRival() {
    if (m_game->phase() != Phase::Draw && m_game->phase() != Phase::Build) return;
    const int at = m_game->turn();
    if (at < 0 || at >= m_skills.size()) return;
    if (m_game->seat(at).human()) return;

    if (++m_movesThisTurn > kMostMovesPerTurn) {
        // Garde-fou. Ce cas n'a jamais été observé, mais une partie bloquée
        // serait bien plus préjudiciable qu'un tour mal joué.
        m_game->takeBackPose();
        for (const Card &card : m_game->seat(at).hand) {
            if (m_game->discard(card.id)) return;
        }
        return;
    }

    const Move move = chooseMove(*m_game, at, m_skills.at(at), m_watch);
    bool done = false;
    QString why;

    switch (move.kind) {
    case Move::DrawStock:
        done = m_game->drawStock(&why);
        if (done) m_view->noteDrawn(at, false, m_game->lastDrawn());
        break;
    case Move::DrawPile: {
        const Card top = m_game->pileTop();
        m_watch.noteTaken(at, top, m_game->seatCount());
        done = m_game->drawPile(&why);
        if (done) {
            m_view->noteDrawn(at, true, top);
            noteTakenFromPile(at);
        }
        break;
    }
    case Move::Lay: {
        const bool wasOpen = m_game->seat(at).opened;
        const QVector<int> waiting = waitingCards(at);
        done = m_game->lay(move.cards, &why);
        if (done) {
            m_view->noteLaid(landedCards(at, move.cards, waiting, wasOpen), at);
            // L'ouverture relève de la compétence Lecture, qui porte sur le
            // décompte de la main.
            if (!wasOpen && m_game->seat(at).opened && at < m_deeds.size()) {
                m_deeds[at].did(Learned::Lecture);
            }
        }
        break;
    }
    case Move::LayOff:
        if (!move.cards.isEmpty()) {
            done = m_game->layOff(move.cards.first(), move.meldId, &why);
            if (done) {
                m_view->noteLaid(move.cards, at);
                if (at < m_deeds.size()) m_deeds[at].did(Learned::Maitrise);
            }
        }
        break;
    case Move::BuyJoker:
        if (!move.cards.isEmpty()) {
            done = m_game->buyJoker(move.cards.first(), move.meldId, &why);
            if (done && at < m_deeds.size()) m_deeds[at].did(Learned::Maitrise);
        }
        break;
    case Move::TakeBack:
        done = m_game->takeBackPose(&why);
        break;
    case Move::Discard:
        if (!move.cards.isEmpty() && move.cards.first() >= 0) {
            const Card *card = findCard(m_game->seat(at).hand, move.cards.first());
            const Card thrown = card ? *card : Card{};
            m_watch.note(thrown);
            done = m_game->discard(move.cards.first(), &why);
            if (done) {
                m_view->noteDiscard(at, thrown);
                m_pileFrom = at;
            }
        }
        break;
    case Move::None:
        break;
    }

    if (!done) {
        // Coup refusé : la logique de décision a demandé une action non
        // conforme aux règles. La pose est alors reprise en main ; à défaut,
        // la première carte disponible est défaussée.
        if (m_game->takeBackPose()) {
            m_beat->start(thinkingMs(false));
            return;
        }
        if (m_game->phase() == Phase::Draw) {
            if (!m_game->drawStock()) return;
        } else {
            for (const Card &card : m_game->seat(at).hand) {
                if (m_game->discard(card.id)) return;
            }
            return;
        }
    }

    // Si c'est encore à lui de jouer, le coup suivant est enchaîné après une
    // temporisation.
    if ((m_game->phase() == Phase::Draw || m_game->phase() == Phase::Build) &&
        m_game->turn() == at && !m_game->seat(at).human()) {
        m_beat->start(thinkingMs(false));
    }
}

// ---------------------------------------------------------------------------
// La fin d'une manche.

void TablePage::onHandFinished() {
    m_beat->stop();
    m_view->clearChosen();
    if (m_handCounted) return;
    m_handCounted = true;

    // Classement de la manche, servant à l'attribution de l'expérience : le
    // total le plus faible d'abord, le joueur sorti étant départagé en sa
    // faveur en cas d'égalité.
    QVector<int> order;
    for (int at = 0; at < m_game->seatCount(); ++at) order.append(at);
    const Rami *game = m_game;
    std::stable_sort(order.begin(), order.end(), [game](int a, int b) {
        if (game->seat(a).lastHand != game->seat(b).lastHand) {
            return game->seat(a).lastHand < game->seat(b).lastHand;
        }
        return game->seat(a).wentOut && !game->seat(b).wentOut;
    });

    // Expérience détenue avant cette manche, afin de pouvoir signaler ce qui
    // vient d'être acquis.
    QVector<Pools> before(m_game->seatCount());
    for (int at = 0; at < m_game->seatCount(); ++at) {
        const Seat &seat = m_game->seat(at);
        if (!seat.human()) before[at] = learningFor(seat.rivalId).pools;
    }

    for (int place = 0; place < order.size(); ++place) {
        const int at = order.at(place);
        const Seat &seat = m_game->seat(at);
        if (seat.human()) {
            noteHandPlayed(seat.wentOut);
            continue;
        }
        Deeds deeds = at < m_deeds.size() ? m_deeds.at(at) : Deeds{};
        // La sortie relève de la compétence Lecture : c'est l'analyse complète
        // de la main qui permet de s'en défaire.
        if (seat.wentOut) deeds.did(Learned::Lecture);
        noteRivalHand(seat.rivalId, place, m_game->seatCount(), seat.wentOut, deeds);
    }

    // L'expérience acquise prend effet dès la manche suivante, et non au
    // prochain démarrage de l'application.
    QStringList learnt;
    for (int at = 0; at < m_game->seatCount() && at < m_skills.size(); ++at) {
        const Seat &seat = m_game->seat(at);
        if (seat.human()) continue;
        m_skills[at] = skillFor(seat.rivalId);

        // Le franchissement d'un rang est annoncé : une progression non
        // signalée serait interprétée comme un comportement arbitraire du
        // programme.
        const Pools after = learningFor(seat.rivalId).pools;
        for (int i = 0; i < kLearnedCount; ++i) {
            const Learned which = Learned(i);
            const int rung = rungOf(after.at(which));
            if (rung <= rungOf(before.at(at).at(which))) continue;
            learnt.append(tr("%1 a appris : %2 %3 — %4")
                              .arg(seat.name, learnedSpec(which).name,
                                   QStringLiteral("I").repeated(rung + 1),
                                   learnedSpec(which).says[rung]));
        }
    }
    if (!learnt.isEmpty()) m_view->say(learnt.join(QStringLiteral("  ·  ")));

    if (m_game->phase() == Phase::MatchOver) return;   // la feuille suit
    m_board->setGeometry(rect());
    m_board->showHand(m_game);
    Q_EMIT stateChanged();
}

void TablePage::onMatchFinished() {
    m_beat->stop();
    const int winner = m_game->matchWinner();
    const bool youWon = winner >= 0 && m_game->seat(winner).human();

    for (int at = 0; at < m_game->seatCount(); ++at) {
        const Seat &seat = m_game->seat(at);
        if (seat.human()) {
            noteMatchPlayed(youWon, seat.score);
            continue;
        }
        noteRivalMatch(seat.rivalId, at == winner);
    }

    m_board->setGeometry(rect());
    m_victory->setGeometry(rect());

    // La feuille de marque est renseignée dans tous les cas. En présence de
    // l'animation de victoire, elle reste masquée et s'affiche à la fin de
    // celle-ci.
    m_board->showMatch(m_game);
    if (youWon && m_settings.celebrate) {
        m_board->hide();
        QVector<Card> cards;
        QVector<QPointF> at;
        m_view->cardsOnScreen(&cards, &at);
        m_victory->start(cards, at, m_view->meldCardHeight(), tr("Partie gagnée"),
                         tr("%1 points contre vous").arg(m_game->seat(winner).score));
    }
    Q_EMIT stateChanged();
}

// ---------------------------------------------------------------------------
// Votre tour.

void TablePage::onStockPressed() {
    if (!m_game->seat(m_game->turn()).human()) return;
    QString why;
    if (!m_game->drawStock(&why)) {
        grumble(why);
        return;
    }
    if (m_settings.autoSort) sort(false);
    m_view->noteDrawn(m_game->turn(), false, m_game->lastDrawn());
    sayWhatToDo();
}

void TablePage::onPilePressed() {
    if (!m_game->seat(m_game->turn()).human()) return;
    const Card top = m_game->pileTop();
    QString why;
    if (!m_game->drawPile(&why)) {
        grumble(why);
        return;
    }
    m_watch.noteTaken(m_game->turn(), top, m_game->seatCount());
    noteTakenFromPile(m_game->turn());
    if (m_settings.autoSort) sort(false);
    m_view->noteDrawn(m_game->turn(), true, top);
    sayWhatToDo();
}

void TablePage::sort(bool byRank) {
    const int me = mySeat();
    if (me < 0) return;
    // Les règles conservent la main dans leur propre ordre. Le tri relève de
    // l'affichage, mais la main appartenant aux règles, c'est à elles qu'il
    // est demandé.
    m_game->sortSeatHand(me, byRank);
    m_view->refresh();
}

QVector<int> TablePage::waitingCards(int seat) const {
    QVector<int> ids;
    for (const Meld &meld : m_game->seat(seat).waiting) {
        for (const Card &card : meld.cards) ids.append(card.id);
    }
    return ids;
}

// Avant l'ouverture, une combinaison posée n'est pas placée sur le tapis :
// elle reste en attente devant son joueur, et c'est celle qui porte le total à
// cinquante et un qui les y fait toutes descendre simultanément. N'animer que
// la dernière faisait apparaître les autres sans déplacement visible ; une
// ouverture comportant une seule combinaison étant rare, ce défaut se
// manifestait dans la plupart des cas.
QVector<int> TablePage::landedCards(int seat, const QVector<int> &laid,
                                    const QVector<int> &waitingBefore, bool wasOpen) const {
    if (wasOpen || seat < 0 || !m_game->seat(seat).opened) return laid;
    // Les combinaisons en attente sont animées en premier, dans leur ordre de
    // pose.
    QVector<int> all = waitingBefore;
    all.append(laid);
    return all;
}

void TablePage::doLay() {
    const QVector<int> chosen = m_view->chosen();
    const int me = mySeat();
    const bool wasOpen = me >= 0 && m_game->seat(me).opened;
    const QVector<int> waiting = me >= 0 ? waitingCards(me) : QVector<int>();

    QString why;
    if (!m_game->lay(chosen, &why)) {
        grumble(why);
        return;
    }
    m_view->noteLaid(landedCards(me, chosen, waiting, wasOpen), me);
    m_view->clearChosen();
    sayWhatToDo();
}

void TablePage::doLayOff() {
    const QVector<int> chosen = m_view->chosen();
    if (chosen.isEmpty()) {
        grumble(tr("Choisissez d'abord une carte à mettre sur une combinaison."));
        return;
    }
    QString lastWhy;
    int placed = 0;
    // Chaque carte sélectionnée est placée sur la première combinaison qui
    // l'accepte.
    for (int cardId : chosen) {
        bool done = false;
        for (const Meld &meld : m_game->table()) {
            if (m_game->layOff(cardId, meld.id, &lastWhy)) {
                m_view->noteLaid({cardId}, mySeat());
                ++placed;
                done = true;
                break;
            }
        }
        if (!done && lastWhy.isEmpty()) lastWhy = tr("Aucune combinaison du tapis ne la prend.");
    }
    if (placed == 0) {
        grumble(lastWhy.isEmpty() ? tr("Aucune combinaison du tapis ne la prend.") : lastWhy);
        return;
    }
    m_view->clearChosen();
    sayWhatToDo();
}

void TablePage::doTakeBack() {
    // Cartes concernées par la reprise, relevées au préalable : après
    // l'opération, la pose n'existe plus alors que l'animation en part.
    const int me = mySeat();
    QVector<int> coming;
    if (me >= 0) {
        for (const Meld &meld : m_game->seat(me).waiting) {
            for (const Card &card : meld.cards) coming.append(card.id);
        }
    }

    QString why;
    if (!m_game->takeBackPose(&why)) {
        grumble(why);
        return;
    }
    if (m_settings.autoSort) sort(false);
    m_view->noteTakenBack(coming);
    sayWhatToDo();
}

// Clic sur une carte de la pose : la carte revient en main.
//
// Seule cette carte est reprise lorsque sa combinaison reste valide sans elle,
// ce qui évite d'avoir à reprendre la pose entière pour en retirer une carte.
// Lorsque la combinaison n'est plus valide, elle revient en main dans son
// intégralité, les règles n'autorisant aucune autre opération.
void TablePage::onPoseCardPressed(int cardId) {
    const int me = mySeat();
    if (me < 0 || m_game->turn() != me) return;

    if (m_game->canTakeBackCard(cardId)) {
        QString why;
        if (!m_game->takeBackCard(cardId, &why)) {
            grumble(why);
            return;
        }
        if (m_settings.autoSort) sort(false);
        m_view->noteTakenBack({cardId});
        sayWhatToDo();
        return;
    }

    // Cartes concernées par la reprise, relevées au préalable : après
    // l'opération, la combinaison n'existe plus alors que l'animation en
    // part.
    const int meldId = m_game->poseMeldOf(cardId);
    const Meld *meld = meldId < 0 ? nullptr : m_game->meldById(meldId);
    if (!meld) return;
    const QString was = meld->spell();
    QVector<int> coming;
    for (const Card &card : meld->cards) coming.append(card.id);

    QString why;
    if (!m_game->takeBackMeld(meldId, &why)) {
        grumble(why);
        return;
    }
    if (m_settings.autoSort) sort(false);
    m_view->noteTakenBack(coming);
    m_view->say(tr("%1 ne tenait plus sans cette carte : la combinaison revient "
                   "entière en main.")
                    .arg(was));
}

void TablePage::doDiscard() {
    const QVector<int> chosen = m_view->chosen();
    if (chosen.size() != 1) {
        grumble(tr("Choisissez une seule carte à défausser."));
        return;
    }
    const int me = mySeat();
    const Card *card = me < 0 ? nullptr : findCard(m_game->seat(me).hand, chosen.first());
    const Card thrown = card ? *card : Card{};
    QString why;
    if (!m_game->discard(chosen.first(), &why)) {
        grumble(why);
        return;
    }
    m_watch.note(thrown);
    m_pileFrom = me;
    m_view->noteDiscard(me, thrown);
    m_view->clearChosen();
}

void TablePage::doHint() {
    const int me = mySeat();
    if (me < 0 || m_game->turn() != me) {
        m_view->say(tr("Ce n'est pas à vous de jouer."));
        return;
    }
    m_view->say(advise(*m_game, me, m_watch));
}

void TablePage::onMeldPressed(int meldId) {
    const QVector<int> chosen = m_view->chosen();
    if (chosen.isEmpty()) {
        const Meld *meld = m_game->meldById(meldId);
        if (!meld) return;
        m_view->say(tr("%1 — posée par %2. Choisissez une carte pour la compléter.")
                        .arg(meld->spell(), m_game->seat(meld->owner).name));
        return;
    }
    QString why;
    int placed = 0;
    for (int cardId : chosen) {
        if (m_game->layOff(cardId, meldId, &why)) {
            m_view->noteLaid({cardId}, mySeat());
            ++placed;
        }
    }
    if (placed == 0) {
        // Aucune opération n'a abouti : la carte est peut-être le second
        // exemplaire de celle que représente le joker.
        if (m_game->buyJoker(chosen.first(), meldId, &why)) {
            m_view->clearChosen();
            sayWhatToDo();
            return;
        }
        grumble(why);
        return;
    }
    m_view->clearChosen();
    sayWhatToDo();
}

void TablePage::onMeldRightPressed(int meldId) {
    const QVector<int> chosen = m_view->chosen();
    if (chosen.size() != 1) {
        m_view->say(tr("Choisissez la carte que le joker remplace, puis clic droit."));
        return;
    }
    QString why;
    if (!m_game->buyJoker(chosen.first(), meldId, &why)) {
        grumble(why);
        return;
    }
    m_view->clearChosen();
    sayWhatToDo();
}

void TablePage::onCardDoubleClicked(int cardId) {
    const int me = mySeat();
    if (me < 0 || m_game->turn() != me) return;
    if (m_game->phase() != Phase::Build) {
        grumble(tr("Piochez d'abord."));
        return;
    }
    m_view->setChosen({cardId});
    doDiscard();
}

// ---------------------------------------------------------------------------
// État des boutons.

void TablePage::updateActions() {
    const int me = mySeat();
    const bool mine = me >= 0 && m_game->turn() == me &&
                      (m_game->phase() == Phase::Draw || m_game->phase() == Phase::Build);
    const bool building = mine && m_game->phase() == Phase::Build;
    const Seat &seat = m_game->seat(std::max(0, me));
    const QVector<int> chosen = m_view->chosen();

    // Un bouton n'est actif que si le coup correspondant est autorisé.
    //
    // L'approche inverse — activer tous les boutons et refuser le coup après
    // coup par un message d'erreur — oblige le joueur à découvrir les règles
    // par essais successifs. Il est préférable que le bouton reste inactif
    // tant que la sélection ne permet aucun coup, et s'active dès qu'elle en
    // permet un : c'est alors la sélection qui renseigne le joueur, et non le
    // refus.

    // Poser : actif uniquement si les cartes sélectionnées forment une suite
    // ou une série valide, et s'il reste une carte à défausser pour achever le
    // tour.
    bool canLay = false;
    if (building && chosen.size() >= 3 && seat.hand.size() - chosen.size() >= 1) {
        QVector<Card> picked;
        for (int id : chosen) {
            if (const Card *card = findCard(seat.hand, id)) picked.append(*card);
        }
        Meld meld;
        canLay = picked.size() == chosen.size() && makeMeld(picked, &meld);
    }

    // Compléter : actif uniquement si au moins une des cartes sélectionnées
    // peut être placée sur une combinaison déjà posée.
    bool canLayOff = false;
    if (building && seat.opened && !chosen.isEmpty() && seat.hand.size() >= 2) {
        for (int id : chosen) {
            const Card *card = findCard(seat.hand, id);
            if (!card) continue;
            Meld grown;
            for (const Meld &meld : m_game->table()) {
                if (!canExtend(meld, *card, &grown)) continue;
                canLayOff = true;
                break;
            }
            if (canLayOff) break;
        }
    }

    m_lay->setEnabled(canLay);
    m_layOff->setEnabled(canLayOff);
    m_takeBack->setEnabled(building && !seat.waiting.isEmpty());
    m_takeBack->setVisible(!seat.waiting.isEmpty());
    // Défausser reste inactif tant que le tour ne peut s'achever : pose
    // n'atteignant pas le total requis, ou joker racheté non réemployé. Le
    // bandeau situé sous la main précise lequel des deux cas s'applique.
    m_discard->setEnabled(building && chosen.size() == 1 && m_game->canDiscard());
    m_hint->setEnabled(mine);
    m_hint->setVisible(m_settings.offerHints);

    // Le bouton achevant le tour est mis en évidence, sauf lorsqu'une pose
    // reste à compléter, auquel cas c'est celle-ci qui est prioritaire.
    const bool poseShort = !seat.waiting.isEmpty() && m_game->poseValue(me) < kOpeningPoints;
    m_discard->setProperty("strong", m_discard->isEnabled() && !poseShort);
    m_lay->setProperty("strong", canLay && poseShort);
    for (QPushButton *button : {m_discard, m_lay}) {
        button->style()->unpolish(button);
        button->style()->polish(button);
    }

    if (me < 0) {
        m_pose->setText(QString());
        return;
    }
    if (!seat.waiting.isEmpty()) {
        m_pose->setText(tr("Pose en cours : %1 points sur %2")
                            .arg(m_game->poseValue(me))
                            .arg(kOpeningPoints));
    } else if (!seat.opened) {
        const Plan plan = readHand(seat.hand, Reading::MostPoints);
        m_pose->setText(tr("Pas encore ouvert — votre main vaut %1 points de combinaisons")
                            .arg(plan.points));
    } else {
        m_pose->setText(tr("Ouvert — %1 cartes en main").arg(seat.hand.size()));
    }
}

void TablePage::sayWhatToDo() {
    const int me = mySeat();
    if (me < 0 || m_game->turn() != me) return;
    const Seat &seat = m_game->seat(me);

    if (m_game->phase() == Phase::Draw) {
        const Card top = m_game->pileTop();
        if (top.valid()) {
            m_view->say(tr("À vous : piochez, ou ramassez %1.").arg(cardName(top)));
        } else {
            m_view->say(tr("À vous : piochez."));
        }
        return;
    }
    if (m_game->phase() != Phase::Build) return;

    // La carte qui vient d'être piochée est nommée explicitement. Elle est
    // également signalée par une marque dorée dans la main, mais son nom
    // l'identifie immédiatement.
    const Card drawn = m_game->lastDrawn();
    const QString took = drawn.valid()
                             ? tr("Pioché : %1. ").arg(cardName(drawn))
                             : QString();

    if (!seat.waiting.isEmpty() && m_game->poseValue(me) < kOpeningPoints) {
        m_view->say(tr("Il manque %1 points à votre pose, ou reprenez-la en main.")
                        .arg(kOpeningPoints - m_game->poseValue(me)));
        return;
    }
    if (m_game->owesJoker(me)) {
        m_view->say(tr("Le joker racheté doit servir avant la fin du tour."));
        return;
    }
    if (!seat.opened) {
        const Plan plan = readHand(seat.hand, Reading::MostPoints);
        if (plan.points >= kOpeningPoints) {
            m_view->say(took + tr("Vous avez de quoi ouvrir : %1 points de combinaisons.")
                                   .arg(plan.points));
        } else {
            m_view->say(took + tr("Il vous manque %1 points pour ouvrir.")
                                   .arg(kOpeningPoints - plan.points));
        }
        return;
    }
    m_view->say(took + tr("Posez, complétez le tapis, puis défaussez."));
}

void TablePage::grumble(const QString &why) {
    m_view->sayTrouble(why.isEmpty() ? tr("Ce coup-là n'est pas permis.") : why);
}

// ---------------------------------------------------------------------------

void TablePage::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    if (m_board) m_board->setGeometry(rect());
    if (m_victory) m_victory->setGeometry(rect());
}

void TablePage::keyPressEvent(QKeyEvent *event) {
    const int me = mySeat();
    const bool mine = me >= 0 && m_game->turn() == me;
    switch (event->key()) {
    case Qt::Key_T: sort(false); return;
    case Qt::Key_R: sort(true); return;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        if (mine) doLay();
        return;
    case Qt::Key_C:
        if (mine) doLayOff();
        return;
    case Qt::Key_D:
        if (mine) doDiscard();
        return;
    case Qt::Key_H:
        if (m_settings.offerHints) doHint();
        return;
    case Qt::Key_Space:
        if (mine && m_game->phase() == Phase::Draw) onStockPressed();
        return;
    default:
        break;
    }
    QWidget::keyPressEvent(event);
}
