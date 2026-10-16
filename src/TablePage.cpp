#include "TablePage.h"

#include "TableLook.h"

#include "CardArt.h"
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

// Combien de temps un adversaire a l'air de réfléchir. Pas pour faire
// semblant d'être lent : un coup qui arrive dans la même milliseconde que le
// précédent est un coup que personne n'a vu passer, et la moitié de ce jeu
// est de regarder ce que font les autres.
constexpr int kThinkMs = 620;
constexpr int kMoveMs = 330;

// Au-delà de ça, le cerveau tourne en rond. Il n'y a pas de raison connue
// pour que ça arrive, et c'est précisément pourquoi il y a un filet.
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

    // Par-dessus la table, et seulement quand il y a lieu.
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

    m_sortSuit = actionButton(m_actions, tr("Par couleur"), tr("Ranger la main par couleur"),
                              tr("T"));
    m_sortRank = actionButton(m_actions, tr("Par rang"), tr("Ranger la main par rang"), tr("R"));
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
    across->addWidget(m_sortSuit);
    across->addWidget(m_sortRank);
    across->addSpacing(12);
    across->addWidget(m_lay);
    across->addWidget(m_layOff);
    across->addWidget(m_takeBack);
    across->addSpacing(12);
    across->addWidget(m_hint);
    across->addWidget(m_discard);

    connect(m_sortSuit, &QPushButton::clicked, this, [this] { sort(false); });
    connect(m_sortRank, &QPushButton::clicked, this, [this] { sort(true); });
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
    // Vous, toujours en premier, pour que la première main de la partie soit
    // la vôtre plutôt qu'une attente.
    SeatSpec you;
    you.name = tr("Vous");
    you.hue = feltHue();
    seats.append(you);

    const int wanted = std::max(0, table.seats - 1);

    // Ceux qu'on a demandés d'abord, puis les moins joués: comme ça les six
    // progressent à peu près ensemble au lieu de laisser les trois derniers
    // débutants pour toujours.
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
        // Le nom donné sur ce profil, et non celui d'usine: c'est celui-là
        // qui est sur sa plaque et dans tout ce qui se dit de lui.
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

    m_skills.clear();
    for (const SeatSpec &spec : seats) {
        m_skills.append(spec.rivalId.isEmpty() ? bestSkill()
                                               : skillFor(learningFor(spec.rivalId).xp));
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
    // La table et la longueur de la partie ne changent pas une partie en
    // cours: jeter une manche à moitié jouée parce qu'un réglage a bougé
    // serait un mauvais échange. Elles valent pour la prochaine.
    m_settings.tableId = table;
    m_settings.targetId = target;
    m_hint->setVisible(m_settings.offerHints);
    m_view->setZoom(m_settings.zoom);
    updateActions();
}

void TablePage::refresh() {
    const Palette &scheme = currentPalette();
    m_actions->setStyleSheet(
        QStringLiteral("QWidget#actionBar { background: %1; border-top: 1px solid %2; }"
                       "QPushButton { background: %3; color: %4; border: 1px solid %2;"
                       " border-radius: 7px; padding: 6px 13px; }"
                       "QPushButton:hover:enabled { border: 1px solid %5; background: %6; }"
                       "QPushButton:pressed:enabled { background: %7; }"
                       "QPushButton:disabled { color: %2; background: %1; border-color: %1; }"
                       "QPushButton[strong=\"true\"]:enabled { background: %5; color: %8;"
                       " border: 1px solid %5; font-weight: bold; }"
                       "QLabel { color: %4; }")
            .arg(scheme.face.name(), scheme.edge.name(),
                 blendColours(scheme.face, scheme.surface, 0.55).name(), scheme.faceText.name(),
                 scheme.accent.name(), blendColours(scheme.face, scheme.accent, 0.16).name(),
                 blendColours(scheme.face, scheme.accent, 0.30).name(),
                 scheme.accentText.name()));

    QPalette bar = m_actions->palette();
    bar.setColor(QPalette::Window, scheme.face);
    bar.setColor(QPalette::WindowText, scheme.faceText);
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

void TablePage::playRival() {
    if (m_game->phase() != Phase::Draw && m_game->phase() != Phase::Build) return;
    const int at = m_game->turn();
    if (at < 0 || at >= m_skills.size()) return;
    if (m_game->seat(at).human()) return;

    if (++m_movesThisTurn > kMostMovesPerTurn) {
        // Le filet. Jamais vu se déclencher, mais une partie bloquée est bien
        // pire qu'un tour mal joué.
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
        if (done) m_view->noteDrawn(at, true, top);
        break;
    }
    case Move::Lay: {
        const bool wasOpen = m_game->seat(at).opened;
        const QVector<int> waiting = waitingCards(at);
        done = m_game->lay(move.cards, &why);
        if (done) m_view->noteLaid(landedCards(at, move.cards, waiting, wasOpen));
        break;
    }
    case Move::LayOff:
        if (!move.cards.isEmpty()) {
            done = m_game->layOff(move.cards.first(), move.meldId, &why);
            if (done) m_view->noteLaid(move.cards);
        }
        break;
    case Move::BuyJoker:
        if (!move.cards.isEmpty()) done = m_game->buyJoker(move.cards.first(), move.meldId, &why);
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
            if (done) m_view->noteDiscard(at, thrown);
        }
        break;
    case Move::None:
        break;
    }

    if (!done) {
        // Un coup refusé, ce qui veut dire que le cerveau a demandé quelque
        // chose que les règles ne permettent pas: on reprend la pose, sinon on
        // jette ce qui vient.
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

    // Toujours à lui ? Alors on enchaîne, un battement plus tard.
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

    // Le classement de la manche, pour l'expérience: le moins de points
    // d'abord, et celui qui est sorti devant à égalité.
    QVector<int> order;
    for (int at = 0; at < m_game->seatCount(); ++at) order.append(at);
    const Rami *game = m_game;
    std::stable_sort(order.begin(), order.end(), [game](int a, int b) {
        if (game->seat(a).lastHand != game->seat(b).lastHand) {
            return game->seat(a).lastHand < game->seat(b).lastHand;
        }
        return game->seat(a).wentOut && !game->seat(b).wentOut;
    });

    for (int place = 0; place < order.size(); ++place) {
        const Seat &seat = m_game->seat(order.at(place));
        if (seat.human()) {
            noteHandPlayed(seat.wentOut);
            continue;
        }
        noteRivalHand(seat.rivalId, place, m_game->seatCount(), seat.wentOut);
    }

    // Ce qu'ils viennent d'apprendre compte pour la manche suivante, pas pour
    // la prochaine fois qu'on lance le jeu.
    for (int at = 0; at < m_game->seatCount() && at < m_skills.size(); ++at) {
        const Seat &seat = m_game->seat(at);
        if (seat.human()) continue;
        m_skills[at] = skillFor(learningFor(seat.rivalId).xp);
    }

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

    // La feuille est remplie dans tous les cas. Quand il y a un feu
    // d'artifice elle attend derrière, et elle se montre quand il s'éteint.
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
    if (m_settings.autoSort) sort(false);
    m_view->noteDrawn(m_game->turn(), true, top);
    sayWhatToDo();
}

void TablePage::sort(bool byRank) {
    const int me = mySeat();
    if (me < 0) return;
    // Le moteur garde la main dans son ordre; trier est une affaire d'écran,
    // mais la main est au moteur, alors c'est lui qui la retourne triée.
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

// Poser une combinaison avant d'avoir ouvert ne la met pas sur le tapis: elle
// attend devant son joueur, et c'est celle qui atteint cinquante-et-un qui
// les y fait toutes descendre d'un coup. N'animer que la dernière laissait
// les autres paraître sur le tapis sans être venues de nulle part -- et comme
// on ouvre rarement avec une seule combinaison, c'était le cas ordinaire.
QVector<int> TablePage::landedCards(int seat, const QVector<int> &laid,
                                    const QVector<int> &waitingBefore, bool wasOpen) const {
    if (wasOpen || seat < 0 || !m_game->seat(seat).opened) return laid;
    // Celles qui attendaient d'abord: elles ont été posées avant, et elles
    // arrivent dans cet ordre-là.
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
    m_view->noteLaid(landedCards(me, chosen, waiting, wasOpen));
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
    // Chaque carte choisie sur la première combinaison qui la prend.
    for (int cardId : chosen) {
        bool done = false;
        for (const Meld &meld : m_game->table()) {
            if (m_game->layOff(cardId, meld.id, &lastWhy)) {
                m_view->noteLaid({cardId});
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
    // Quelles cartes vont revenir: relevées avant, puisque après la reprise
    // la pose n'existe plus et que c'est d'elle qu'elles repartent.
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

// Un clic sur une carte de la pose : elle revient en main.
//
// Celle-là seule quand la combinaison tient encore sans elle -- reprendre
// une pose entière pour en retirer une carte de trop était le geste qu'il
// fallait faire avant. Quand elle ne tient plus, la combinaison revient
// entière, parce que c'est la seule autre chose que les règles en laissent
// faire.
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

    // Quelles cartes vont revenir: relevées avant, puisque après la reprise
    // la combinaison n'existe plus et que c'est d'elle qu'elles repartent.
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
            m_view->noteLaid({cardId});
            ++placed;
        }
    }
    if (placed == 0) {
        // Rien n'a tenu : peut-être que la carte est le double du joker.
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
// Ce que les boutons disent.

void TablePage::updateActions() {
    const int me = mySeat();
    const bool mine = me >= 0 && m_game->turn() == me &&
                      (m_game->phase() == Phase::Draw || m_game->phase() == Phase::Build);
    const bool building = mine && m_game->phase() == Phase::Build;
    const Seat &seat = m_game->seat(std::max(0, me));
    const QVector<int> chosen = m_view->chosen();

    // Un bouton n'est allumé que quand le coup qu'il ferait est permis.
    //
    // L'autre façon -- tout allumer et refuser après coup avec une phrase en
    // rouge -- apprend les règles à coups de reproches : on choisit trois
    // cartes, on presse, on se fait dire non. Mieux vaut que le bouton reste
    // éteint tant que la sélection ne fait rien, et s'allume à la seconde où
    // elle fait quelque chose : c'est la sélection qui répond, pas le refus.

    // Poser : seulement si les cartes choisies font vraiment une suite ou une
    // série, et s'il en reste une à jeter pour finir le tour.
    bool canLay = false;
    if (building && chosen.size() >= 3 && seat.hand.size() - chosen.size() >= 1) {
        QVector<Card> picked;
        for (int id : chosen) {
            if (const Card *card = findCard(seat.hand, id)) picked.append(*card);
        }
        Meld meld;
        canLay = picked.size() == chosen.size() && makeMeld(picked, &meld);
    }

    // Compléter : seulement si au moins une des cartes choisies tient sur une
    // combinaison déjà posée.
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

    m_sortSuit->setEnabled(me >= 0 && !seat.hand.isEmpty());
    m_sortRank->setEnabled(me >= 0 && !seat.hand.isEmpty());
    m_lay->setEnabled(canLay);
    m_layOff->setEnabled(canLayOff);
    m_takeBack->setEnabled(building && !seat.waiting.isEmpty());
    m_takeBack->setVisible(!seat.waiting.isEmpty());
    // Et défausser reste éteint tant que le tour ne peut pas finir -- une pose
    // qui ne compte pas encore, un joker racheté qui n'a pas servi. Ce que la
    // bande dit sous la main explique déjà laquelle des deux.
    m_discard->setEnabled(building && chosen.size() == 1 && m_game->canDiscard());
    m_hint->setEnabled(mine);
    m_hint->setVisible(m_settings.offerHints);

    // Le bouton qui finit le tour est celui qu'on met en avant, sauf quand il
    // y a une pose à compléter: là, c'est elle qui presse.
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

    // Ce qui vient d'être pioché, nommé. La carte porte sa marque dorée dans
    // la main, mais la nommer est ce qui répond tout de suite à « laquelle ».
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
