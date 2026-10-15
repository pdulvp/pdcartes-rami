#include "TableView.h"

#include "CardArt.h"
#include "Palette.h"
#include "Rami.h"
#include "Rivals.h"

#include <QFontMetricsF>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QTimer>

#include <algorithm>

namespace {

constexpr int kFrameMs = 16;

// What each band of the cloth is given, before anything is squeezed.
constexpr qreal kSeatsHeight = 104.0;
constexpr qreal kStacksWidth = 178.0;
constexpr qreal kRailHeight = 62.0;
constexpr qreal kHandHeight = 134.0;
constexpr qreal kMargin = 14.0;
constexpr qreal kGap = 10.0;

// How far apart the cards of a combination sit. Enough of each one shows for
// its corner to be read, which is the whole trick of a fanned hand.
constexpr qreal kMeldStep = 0.60;

// How much a chosen card rises out of the hand.
constexpr qreal kLift = 18.0;

qreal plateau(qreal along) {
    // Starts quickly and lands softly: a card thrown onto a table does not
    // arrive at a constant speed.
    const qreal at = std::clamp(along, 0.0, 1.0);
    return 1.0 - (1.0 - at) * (1.0 - at);
}

void paintPlaqueBody(QPainter *painter, const QRectF &box, const Palette &scheme,
                     const QColor &mine, bool active) {
    QPainterPath shape;
    shape.addRoundedRect(box, 11, 11);

    QColor shade = scheme.feltEdge;
    shade.setAlpha(120);
    QPainterPath cast;
    cast.addRoundedRect(box.translated(0, 2.5), 11, 11);
    painter->setPen(Qt::NoPen);
    painter->fillPath(cast, shade);

    QColor body = scheme.surface;
    if (active) body = blendColours(body, mine, 0.18);
    painter->fillPath(shape, body);

    painter->setBrush(Qt::NoBrush);
    painter->setPen(QPen(active ? mine : blendColours(scheme.surface, scheme.surfaceText, 0.18),
                         active ? 2.4 : 1.0));
    painter->drawPath(shape);
}

} // namespace

TableView::TableView(QWidget *parent) : QWidget(parent) {
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setMinimumSize(760, 520);
    setAutoFillBackground(false);

    m_ticker = new QTimer(this);
    m_ticker->setInterval(kFrameMs);
    connect(m_ticker, &QTimer::timeout, this, &TableView::tick);
}

void TableView::watch(const Rami *game) {
    m_game = game;
    m_chosen.clear();
    m_glow.clear();
    m_flights.clear();
    relayout();
    update();
}

void TableView::setChosen(const QVector<int> &cards) {
    m_chosen = cards;
    Q_EMIT chosenChanged();
    update();
}

void TableView::clearChosen() {
    if (m_chosen.isEmpty()) return;
    m_chosen.clear();
    Q_EMIT chosenChanged();
    update();
}

void TableView::say(const QString &line) {
    m_line = line;
    m_trouble = false;
    update();
}

void TableView::sayTrouble(const QString &line) {
    m_line = line;
    m_trouble = true;
    update();
}

void TableView::refresh() {
    relayout();
    update();
}

void TableView::cardsOnScreen(QVector<Card> *cards, QVector<QPointF> *at) const {
    if (!cards || !at || !m_game) return;
    const QSizeF card = cardSizeForHeight(m_meldHeight);
    for (const Spot &spot : m_meldBoxes) {
        const Meld *meld = m_game->meldById(spot.id);
        if (!meld) continue;
        qreal x = spot.box.left() + 11.0;
        for (const Card &held : meld->cards) {
            cards->append(held);
            at->append(QPointF(x + card.width() / 2.0, spot.box.center().y()));
            x += card.width() * kMeldStep;
        }
    }
}

void TableView::stopAnimations() {
    m_flights.clear();
    m_glow.clear();
    m_ticker->stop();
    update();
}

void TableView::wake() {
    if (!m_ticker->isActive()) m_ticker->start();
}

QPointF TableView::seatAnchor(int at) const {
    for (const Spot &spot : m_seatBoxes) {
        if (spot.id == at) return spot.box.center();
    }
    // Your own cards come up out of your hand.
    return QPointF(m_handRow.center().x(), m_handRow.center().y());
}

void TableView::noteDiscard(int seat, const Card &card) {
    if (!card.valid() || m_pileBox.isNull()) return;
    Flight flight;
    flight.card = card;
    flight.faceUp = true;
    flight.size = m_pileBox.size();
    flight.from = seatAnchor(seat) - QPointF(flight.size.width() / 2, flight.size.height() / 2);
    flight.to = m_pileBox.topLeft();
    flight.speed = 0.075;
    m_flights.append(flight);
    m_glow[card.id] = 1.0;
    wake();
}

void TableView::noteDrawn(int seat, bool fromPile, const Card &card) {
    if (!card.valid() || !m_game) return;

    // La vôtre vole du tas jusqu'à la place qu'elle vient de prendre dans
    // votre main. C'est la seule façon de voir où elle est allée: la main se
    // range toute seule après la pioche, et une quinzième carte rangée par
    // couleur atterrit au milieu des quatorze autres.
    if (m_game->seat(seat).human()) {
        // La disposition d'abord, parce que c'est là qu'on cherche la place.
        relayout();
        QRectF landing;
        for (const Spot &spot : m_handBoxes) {
            if (spot.id == card.id) landing = spot.box;
        }
        if (!landing.isNull()) {
            Flight flight;
            flight.card = card;
            flight.faceUp = true;
            flight.size = (fromPile ? m_pileBox : m_stockBox).size();
            flight.from = (fromPile ? m_pileBox : m_stockBox).topLeft();
            flight.to = landing.topLeft();
            // Un peu plus lent qu'une défausse: celle-là, on veut la suivre.
            flight.speed = 0.048;
            flight.grow = landing.size();
            m_flights.append(flight);
        }
        wake();
        return;
    }

    // Celle de quelqu'un d'autre vole du tas vers sa plaque, face en haut si
    // elle vient de la défausse -- une carte ramassée là est une carte que
    // tout le monde a vue -- et face en bas si elle vient de la pioche, où
    // tout ce qu'on apprend est de quel tas il a pris.
    Flight flight;
    flight.card = card;
    flight.faceUp = fromPile;
    flight.size = m_pileBox.size();
    flight.from = (fromPile ? m_pileBox : m_stockBox).topLeft();
    flight.to = seatAnchor(seat) - QPointF(flight.size.width() / 2, flight.size.height() / 2);
    flight.speed = 0.072;
    m_flights.append(flight);
    wake();
}

void TableView::noteLaid(const QVector<int> &cardIds) {
    for (int id : cardIds) m_glow[id] = 1.0;
    wake();
}

void TableView::tick() {
    bool busy = false;

    for (Flight &flight : m_flights) {
        flight.along += flight.speed;
        if (flight.along < 1.0) busy = true;
    }
    m_flights.erase(std::remove_if(m_flights.begin(), m_flights.end(),
                                   [](const Flight &flight) { return flight.along >= 1.0; }),
                    m_flights.end());

    for (auto it = m_glow.begin(); it != m_glow.end();) {
        it.value() -= 0.016;
        if (it.value() <= 0.0) {
            it = m_glow.erase(it);
        } else {
            busy = true;
            ++it;
        }
    }

    update();
    if (!busy) m_ticker->stop();
}

// ---------------------------------------------------------------------------
// Où tout se place.

void TableView::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    relayout();
}

void TableView::relayout() {
    const QRectF all = QRectF(rect()).adjusted(kMargin, kMargin, -kMargin, -kMargin);
    if (all.width() < 80 || all.height() < 80) return;

    const qreal seats = std::min(kSeatsHeight, all.height() * 0.22);
    const qreal hand = std::min(kHandHeight, all.height() * 0.26);
    const qreal rail = std::min(kRailHeight, all.height() * 0.12);

    m_seatsRow = QRectF(all.left(), all.top(), all.width(), seats);
    m_handRow = QRectF(all.left(), all.bottom() - hand, all.width(), hand);
    m_rail = QRectF(all.left(), m_handRow.top() - rail - kGap * 0.4, all.width(), rail);

    const qreal middleTop = m_seatsRow.bottom() + kGap;
    const qreal middleHeight = m_rail.top() - kGap - middleTop;
    const qreal stacks = std::min(kStacksWidth, all.width() * 0.22);
    m_tapis = QRectF(all.left(), middleTop, all.width() - stacks - kGap,
                     std::max(60.0, middleHeight));
    m_stacks = QRectF(m_tapis.right() + kGap, middleTop, stacks, std::max(60.0, middleHeight));

    layoutSeats();
    layoutMelds();
    layoutHand();

    // La pioche et la défausse, l'une sous l'autre dans la colonne de droite,
    // avec de la place sous elles pour votre propre plaque.
    const qreal cardHeight = std::min(104.0, std::max(56.0, m_stacks.height() * 0.30));
    const QSizeF card = cardSizeForHeight(cardHeight);
    const qreal left = m_stacks.left() + (m_stacks.width() - card.width()) / 2.0;
    const qreal labels = 17.0;
    m_stockBox = QRectF(left, m_stacks.top() + labels, card.width(), card.height());
    m_pileBox = QRectF(left, m_stockBox.bottom() + labels + 18.0, card.width(), card.height());
}

void TableView::layoutSeats() {
    m_seatBoxes.clear();
    if (!m_game || m_game->seatCount() < 2) return;

    // Everybody but you. Your own cards are in front of you and your own
    // plaque is under the défausse, where there is room for it.
    QVector<int> others;
    for (int at = 0; at < m_game->seatCount(); ++at) {
        if (!m_game->seat(at).human()) others.append(at);
    }
    if (others.isEmpty()) return;

    const qreal gap = 10.0;
    const qreal widest = 250.0;
    qreal width = (m_seatsRow.width() - gap * (others.size() - 1)) / others.size();
    width = std::min(width, widest);
    const qreal span = width * others.size() + gap * (others.size() - 1);
    qreal at = m_seatsRow.left() + (m_seatsRow.width() - span) / 2.0;
    for (int seat : others) {
        m_seatBoxes.append({seat, QRectF(at, m_seatsRow.top(), width, m_seatsRow.height())});
        at += width + gap;
    }
}

void TableView::layoutMelds() {
    m_meldBoxes.clear();
    if (!m_game) return;

    // Par joueur, pour que le tapis se lise: twenty combinations in a heap
    // say nothing, the same twenty in six groups say who is where.
    QVector<Meld> order = m_game->table();
    std::stable_sort(order.begin(), order.end(),
                     [](const Meld &a, const Meld &b) { return a.owner < b.owner; });
    if (order.isEmpty()) return;

    // The cards come down in size until the whole tapis fits. Two passes at
    // most in practice, but the loop is cheap and the alternative is a tapis
    // that runs off the bottom of the cloth.
    for (qreal height = 56.0; height >= 26.0; height -= 4.0) {
        const QSizeF card = cardSizeForHeight(height);
        const qreal bar = 5.0;
        const qreal pad = 5.0;
        const qreal rowHeight = card.height() + pad * 2;
        QVector<Spot> laid;
        qreal x = m_tapis.left() + 6.0;
        qreal y = m_tapis.top() + 6.0;
        bool fits = true;
        int lastOwner = -2;
        for (const Meld &meld : order) {
            const qreal width =
                bar + pad * 2 + card.width() + card.width() * kMeldStep * (meld.cards.size() - 1);
            // A new player starts on a new row when there is not much left of
            // this one, so that a group is not split for the sake of a card.
            const bool fresh = meld.owner != lastOwner;
            if (x + width > m_tapis.right() - 6.0 ||
                (fresh && lastOwner != -2 && x + width > m_tapis.right() - 60.0)) {
                x = m_tapis.left() + 6.0;
                y += rowHeight + 8.0;
            }
            if (y + rowHeight > m_tapis.bottom() - 4.0) {
                fits = false;
                break;
            }
            laid.append({meld.id, QRectF(x, y, width, rowHeight)});
            x += width + 9.0;
            lastOwner = meld.owner;
        }
        if (fits) {
            m_meldBoxes = laid;
            m_meldHeight = height;
            return;
        }
    }
    m_meldHeight = 26.0;
}

void TableView::layoutHand() {
    m_handBoxes.clear();
    m_poseBoxes.clear();
    if (!m_game) return;

    int me = -1;
    for (int at = 0; at < m_game->seatCount(); ++at) {
        if (m_game->seat(at).human()) me = at;
    }
    if (me < 0) return;

    const Seat &mine = m_game->seat(me);
    const qreal height = std::min(m_handRow.height() - kLift - 6.0, 118.0);
    const QSizeF card = cardSizeForHeight(std::max(52.0, height));
    const int count = mine.hand.size();
    if (count > 0) {
        const qreal step = count > 1 ? std::min(card.width() + 7.0,
                                                (m_handRow.width() - card.width()) / (count - 1))
                                     : 0.0;
        const qreal span = card.width() + step * (count - 1);
        const qreal left = m_handRow.left() + std::max(0.0, (m_handRow.width() - span) / 2.0);
        const qreal top = m_handRow.bottom() - card.height();
        for (int i = 0; i < count; ++i) {
            m_handBoxes.append(
                {mine.hand.at(i).id, QRectF(left + step * i, top, card.width(), card.height())});
        }
    }

    // La pose en cours, sur le rail: smaller, and to the left of the line of
    // commentary, because it is yours and it is not finished.
    if (mine.waiting.isEmpty()) return;
    const QSizeF small = cardSizeForHeight(std::min(44.0, m_rail.height() - 10.0));
    qreal x = m_rail.left() + 8.0;
    for (const Meld &meld : mine.waiting) {
        const qreal width = small.width() + small.width() * kMeldStep * (meld.cards.size() - 1);
        m_poseBoxes.append({meld.id, QRectF(x, m_rail.center().y() - small.height() / 2.0, width,
                                            small.height())});
        x += width + 12.0;
    }
}

// ---------------------------------------------------------------------------
// Le dessin.

void TableView::paintEvent(QPaintEvent *) {
    const Palette &scheme = currentPalette();
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    drawFelt(&painter, QRectF(rect()), scheme);
    if (!m_game || m_game->seatCount() < 2) return;

    paintSeats(&painter);
    paintMelds(&painter);
    paintStacks(&painter);
    paintRail(&painter);
    paintHand(&painter);
    paintFlights(&painter);
}

void TableView::paintSeatPlaque(QPainter *painter, const QRectF &box, int at) {
    const Palette &scheme = currentPalette();
    const Seat &seat = m_game->seat(at);
    const QColor mine = rivalColour(seat.hue);
    const bool active = m_game->turn() == at && m_game->phase() != Phase::HandOver &&
                        m_game->phase() != Phase::MatchOver;

    paintPlaqueBody(painter, box, scheme, mine, active);

    const QRectF inner = box.adjusted(9, 7, -9, -7);
    const qreal disc = std::min(inner.height() * 0.62, 44.0);
    const QRectF medallion(inner.left(), inner.top(), disc, disc);
    QString initial = seat.name.left(1);
    int emblem = 0;
    if (!seat.rivalId.isEmpty()) emblem = rivalById(seat.rivalId).emblem;
    drawRivalEmblem(painter, medallion, emblem, initial, mine,
                    blendColours(scheme.surface, mine, 0.14));

    const qreal textLeft = medallion.right() + 9.0;
    const qreal textWidth = inner.right() - textLeft;

    QFont name = plainFont(painter);
    name.setBold(true);
    name.setPixelSize(std::max(11, int(inner.height() * 0.23)));
    painter->setFont(name);
    painter->setPen(scheme.surfaceText);
    const QFontMetricsF nameMetrics(name);
    painter->drawText(QRectF(textLeft, inner.top(), textWidth * 0.62, nameMetrics.height()),
                      Qt::AlignLeft | Qt::AlignVCenter, seat.name);

    // Le score, à droite, en gros: it is the only number that decides
    // anything, and the manche just gone is written under it.
    QFont score = plainFont(painter);
    score.setBold(true);
    score.setPixelSize(std::max(12, int(inner.height() * 0.27)));
    painter->setFont(score);
    painter->setPen(active ? mine : scheme.hint);
    painter->drawText(QRectF(inner.left(), inner.top(), inner.width(), nameMetrics.height()),
                      Qt::AlignRight | Qt::AlignVCenter,
                      tr("%1 pts").arg(seat.score));

    // Le niveau, et les étoiles.
    qreal y = inner.top() + nameMetrics.height() + 2.0;
    if (!seat.rivalId.isEmpty()) {
        const Learning learning = learningFor(seat.rivalId);
        const Level level = levelOf(learning.xp);
        QFont small = plainFont(painter);
        small.setBold(false);
        small.setPixelSize(std::max(9, int(inner.height() * 0.185)));
        painter->setFont(small);
        const QFontMetricsF smallMetrics(small);

        const qreal starSpan = std::min(44.0, textWidth * 0.34);
        drawLevelStars(painter, QRectF(textLeft, y + 1.0, starSpan, smallMetrics.height() - 2.0),
                       levelStars(level), 3, scheme.gold,
                       blendColours(scheme.surface, scheme.surfaceText, 0.16));
        painter->setPen(scheme.hint);
        painter->drawText(QRectF(textLeft + starSpan + 6.0, y, textWidth - starSpan - 6.0,
                                 smallMetrics.height()),
                          Qt::AlignLeft | Qt::AlignVCenter, levelName(level));
        y += smallMetrics.height() + 2.0;

        drawGauge(painter, QRectF(textLeft, y + 2.0, std::min(textWidth, 96.0), 4.0),
                  progressWithin(learning.xp), mine,
                  blendColours(scheme.surface, scheme.surfaceText, 0.14));
        y += 9.0;
    }

    // Les cartes en main, de dos, et le compte dessus.
    const qreal pileHeight = std::max(20.0, inner.bottom() - y - 1.0);
    drawHandPile(painter, QRectF(textLeft, inner.bottom() - pileHeight,
                                 std::max(50.0, textWidth * 0.64), pileHeight),
                 seat.hand.size(), scheme, mine);

    // Ouvert ou pas: the one fact about an opponent that changes how you play
    // against them, because an opened player can use anything you throw.
    QFont flagFont = plainFont(painter);
    flagFont.setPixelSize(std::max(9, int(inner.height() * 0.17)));
    flagFont.setBold(true);
    flagFont.setCapitalization(QFont::AllUppercase);
    flagFont.setLetterSpacing(QFont::PercentageSpacing, 110);
    painter->setFont(flagFont);
    const QFontMetricsF flagMetrics(flagFont);
    const QString flag = seat.opened ? tr("ouvert") : tr("fermé");
    QRectF badge(0, 0, flagMetrics.horizontalAdvance(flag) + 14.0,
                 flagMetrics.height() + 3.0);
    badge.moveBottomRight(QPointF(inner.right(), inner.bottom()));
    painter->setPen(Qt::NoPen);
    painter->setBrush(seat.opened ? mine : blendColours(scheme.surface, scheme.surfaceText, 0.12));
    painter->drawRoundedRect(badge, badge.height() / 2.0, badge.height() / 2.0);
    painter->setPen(seat.opened ? readableTextOn(mine) : scheme.hint);
    painter->drawText(badge, Qt::AlignCenter, flag);
}

void TableView::paintSeats(QPainter *painter) {
    for (const Spot &spot : m_seatBoxes) paintSeatPlaque(painter, spot.box, spot.id);
}

void TableView::paintStacks(QPainter *painter) {
    const Palette &scheme = currentPalette();
    const bool yours = m_game->seat(m_game->turn()).human();
    const bool drawing = yours && m_game->phase() == Phase::Draw;

    QFont label = plainFont(painter);
    label.setBold(true);
    label.setPixelSize(11);
    label.setCapitalization(QFont::AllUppercase);
    label.setLetterSpacing(QFont::PercentageSpacing, 130);
    painter->setFont(label);

    // La pioche: trois dos décalés, pour que ce soit un tas.
    painter->setPen(scheme.feltText);
    painter->drawText(QRectF(m_stacks.left(), m_stockBox.top() - 17.0, m_stacks.width(), 15.0),
                      Qt::AlignCenter, tr("Pioche"));
    const int left = m_game->stockLeft();
    if (left <= 0) {
        drawCardSlot(painter, m_stockBox, scheme, tr("vide"));
    } else {
        const int layers = std::min(3, std::max(1, left / 6 + 1));
        for (int i = layers - 1; i >= 0; --i) {
            CardLook look;
            look.lit = i == 0 && m_hoverStock && drawing;
            drawCardBack(painter, m_stockBox.translated(-1.6 * i, -1.6 * i), scheme, look);
        }
    }
    QFont count = plainFont(painter);
    count.setCapitalization(QFont::MixedCase);
    count.setLetterSpacing(QFont::PercentageSpacing, 100);
    painter->setFont(count);
    painter->setPen(scheme.feltText);
    painter->drawText(QRectF(m_stacks.left(), m_stockBox.bottom() + 2.0, m_stacks.width(), 15.0),
                      Qt::AlignCenter, tr("%1 cartes").arg(left));

    // La défausse: la carte du dessus, et un bord de celle d'en dessous.
    painter->setFont(label);
    painter->setPen(scheme.feltText);
    painter->drawText(QRectF(m_stacks.left(), m_pileBox.top() - 17.0, m_stacks.width(), 15.0),
                      Qt::AlignCenter, tr("Défausse"));
    const QVector<Card> &pile = m_game->pile();
    if (pile.isEmpty()) {
        drawCardSlot(painter, m_pileBox, scheme, tr("vide"));
    } else {
        if (pile.size() > 1) {
            CardLook under;
            under.dimmed = true;
            drawCardFace(painter, m_pileBox.translated(3.0, -3.0), pile.at(pile.size() - 2),
                         scheme, under);
        }
        CardLook look;
        look.lit = m_hoverPile && drawing;
        look.glow = m_glow.value(pile.last().id, 0.0);
        drawCardFace(painter, m_pileBox, pile.last(), scheme, look);
    }

    // Le tour de piste pendant lequel on peut cliquer.
    if (drawing) {
        QColor ring = scheme.gold;
        ring.setAlpha(190);
        painter->setPen(QPen(ring, 2.2, Qt::DashLine));
        painter->setBrush(Qt::NoBrush);
        if (left > 0) painter->drawRoundedRect(m_stockBox.adjusted(-4, -4, 4, 4), 9, 9);
        if (!pile.isEmpty()) painter->drawRoundedRect(m_pileBox.adjusted(-4, -4, 4, 4), 9, 9);
    }

    // Et votre propre plaque, sous les deux tas.
    int me = -1;
    for (int at = 0; at < m_game->seatCount(); ++at) {
        if (m_game->seat(at).human()) me = at;
    }
    if (me < 0) return;
    const qreal top = m_pileBox.bottom() + 20.0;
    if (m_stacks.bottom() - top < 44.0) return;
    const QRectF plaque(m_stacks.left(), top, m_stacks.width(),
                        std::min(72.0, m_stacks.bottom() - top));
    const Seat &mine = m_game->seat(me);
    const QColor colour = rivalColour(currentPalette().hue);
    paintPlaqueBody(painter, plaque, scheme,
                    blendColours(colour, QColor(Qt::white), 0.18),
                    m_game->turn() == me && m_game->phase() != Phase::HandOver);

    const QRectF inner = plaque.adjusted(9, 6, -9, -6);
    QFont name = plainFont(painter);
    name.setBold(true);
    name.setPixelSize(13);
    painter->setFont(name);
    painter->setPen(scheme.surfaceText);
    const QFontMetricsF nameMetrics(name);
    painter->drawText(QRectF(inner.left(), inner.top(), inner.width(), nameMetrics.height()),
                      Qt::AlignLeft | Qt::AlignVCenter, mine.name);
    QFont big = plainFont(painter);
    big.setPixelSize(17);
    painter->setFont(big);
    painter->setPen(currentPalette().accent);
    painter->drawText(QRectF(inner.left(), inner.top() + nameMetrics.height(), inner.width(),
                             inner.height() - nameMetrics.height()),
                      Qt::AlignLeft | Qt::AlignTop, tr("%1 pts").arg(mine.score));
    QFont small = plainFont(painter);
    small.setPixelSize(10);
    small.setBold(true);
    small.setCapitalization(QFont::AllUppercase);
    painter->setFont(small);
    painter->setPen(mine.opened ? currentPalette().accent : scheme.hint);
    painter->drawText(inner, Qt::AlignRight | Qt::AlignBottom,
                      mine.opened ? tr("ouvert") : tr("fermé"));
}

void TableView::paintMelds(QPainter *painter) {
    const Palette &scheme = currentPalette();

    QFont label = plainFont(painter);
    label.setBold(true);
    label.setPixelSize(11);
    label.setCapitalization(QFont::AllUppercase);
    label.setLetterSpacing(QFont::PercentageSpacing, 130);
    painter->setFont(label);
    painter->setPen(scheme.feltText);
    painter->drawText(QRectF(m_tapis.left() + 8.0, m_tapis.top() - 15.0, m_tapis.width(), 14.0),
                      Qt::AlignLeft | Qt::AlignVCenter, tr("Le tapis"));

    if (m_game->table().isEmpty()) {
        QFont empty = plainFont(painter);
        empty.setBold(false);
        empty.setCapitalization(QFont::MixedCase);
        empty.setLetterSpacing(QFont::PercentageSpacing, 100);
        empty.setPixelSize(13);
        painter->setFont(empty);
        QColor faint = scheme.feltText;
        faint.setAlpha(150);
        painter->setPen(faint);
        painter->drawText(m_tapis, Qt::AlignCenter,
                          tr("Rien de posé : il faut 51 points pour ouvrir."));
        return;
    }

    const QSizeF card = cardSizeForHeight(m_meldHeight);
    for (const Spot &spot : m_meldBoxes) {
        const Meld *meld = m_game->meldById(spot.id);
        if (!meld) continue;
        const QColor mine = rivalColour(m_game->seat(meld->owner).hue);
        const bool lit = m_hoveredMeld == spot.id;

        QPainterPath shape;
        shape.addRoundedRect(spot.box, 8, 8);
        QColor bed = scheme.feltEdge;
        bed.setAlpha(lit ? 170 : 110);
        painter->setPen(Qt::NoPen);
        painter->fillPath(shape, bed);
        painter->setBrush(Qt::NoBrush);
        painter->setPen(QPen(lit ? scheme.gold : blendColours(mine, scheme.feltEdge, 0.3),
                             lit ? 2.0 : 1.0));
        painter->drawPath(shape);

        // Le trait du propriétaire, à gauche: whose combination this is, said
        // in their own colour, which is the only way a tapis of twenty reads.
        painter->setPen(Qt::NoPen);
        painter->setBrush(mine);
        painter->drawRoundedRect(QRectF(spot.box.left() + 2.0, spot.box.top() + 4.0, 4.0,
                                        spot.box.height() - 8.0),
                                 2, 2);

        qreal x = spot.box.left() + 11.0;
        const qreal top = spot.box.center().y() - card.height() / 2.0;
        for (const Card &held : meld->cards) {
            CardLook look;
            look.glow = m_glow.value(held.id, 0.0);
            drawCardFace(painter, QRectF(x, top, card.width(), card.height()), held, scheme, look);
            x += card.width() * kMeldStep;
        }
    }
}

void TableView::paintRail(QPainter *painter) {
    const Palette &scheme = currentPalette();
    int me = -1;
    for (int at = 0; at < m_game->seatCount(); ++at) {
        if (m_game->seat(at).human()) me = at;
    }

    qreal textLeft = m_rail.left() + 6.0;

    if (!m_poseBoxes.isEmpty() && me >= 0) {
        const QSizeF card = cardSizeForHeight(m_poseBoxes.first().box.height());
        for (const Spot &spot : m_poseBoxes) {
            const Meld *meld = m_game->meldById(spot.id);
            if (!meld) continue;
            qreal x = spot.box.left();
            for (const Card &held : meld->cards) {
                CardLook look;
                look.glow = m_glow.value(held.id, 0.0);
                drawCardFace(painter, QRectF(x, spot.box.top(), card.width(), card.height()), held,
                             scheme, look);
                x += card.width() * kMeldStep;
            }
        }
        // Combien il manque, parce que c'est la seule question.
        const int value = m_game->poseValue(me);
        QFont tally = plainFont(painter);
        tally.setBold(true);
        tally.setPixelSize(14);
        painter->setFont(tally);
        const QString text = tr("%1 / %2").arg(value).arg(kOpeningPoints);
        const QFontMetricsF metrics(tally);
        QRectF badge(m_poseBoxes.last().box.right() + 12.0, 0,
                     metrics.horizontalAdvance(text) + 18.0, 24.0);
        badge.moveCenter(QPointF(badge.center().x(), m_rail.center().y()));
        painter->setPen(Qt::NoPen);
        painter->setBrush(value >= kOpeningPoints ? scheme.accent : scheme.danger);
        painter->drawRoundedRect(badge, 12, 12);
        painter->setPen(readableTextOn(value >= kOpeningPoints ? scheme.accent : scheme.danger));
        painter->drawText(badge, Qt::AlignCenter, text);
        textLeft = badge.right() + 14.0;
    }

    if (m_line.isEmpty()) return;
    QFont line = plainFont(painter);
    line.setPixelSize(14);
    line.setBold(m_trouble);
    painter->setFont(line);
    painter->setPen(m_trouble ? blendColours(scheme.danger, QColor(Qt::white), 0.35)
                              : scheme.feltText);
    painter->drawText(
        QRectF(textLeft, m_rail.top(), std::max(60.0, m_rail.right() - textLeft - 6.0),
               m_rail.height()),
        Qt::AlignRight | Qt::AlignVCenter | Qt::TextWordWrap, m_line);
}

void TableView::paintHand(QPainter *painter) {
    const Palette &scheme = currentPalette();
    int me = -1;
    for (int at = 0; at < m_game->seatCount(); ++at) {
        if (m_game->seat(at).human()) me = at;
    }
    if (me < 0) return;
    const Seat &mine = m_game->seat(me);

    for (const Spot &spot : m_handBoxes) {
        const Card *card = findCard(mine.hand, spot.id);
        if (!card) continue;
        CardLook look;
        look.chosen = m_chosen.contains(spot.id);
        look.lit = m_hovered == spot.id;
        look.glow = m_glow.value(spot.id, 0.0);
        QRectF box = spot.box;
        if (look.chosen) box.translate(0, -kLift);
        else if (look.lit) box.translate(0, -6.0);
        drawCardFace(painter, box, *card, scheme, look);
    }

    if (mine.hand.isEmpty()) {
        QFont empty = plainFont(painter);
        empty.setPixelSize(15);
        painter->setFont(empty);
        painter->setPen(scheme.feltText);
        painter->drawText(m_handRow, Qt::AlignCenter, tr("Plus une carte en main."));
    }
}

void TableView::paintFlights(QPainter *painter) {
    const Palette &scheme = currentPalette();
    for (const Flight &flight : m_flights) {
        const qreal along = plateau(flight.along);
        const QPointF at = flight.from + (flight.to - flight.from) * along;
        // Les deux tas et la main ne sont pas à la même échelle, alors une
        // carte qui va de l'un à l'autre grandit en chemin.
        QSizeF size = flight.size;
        if (flight.grow.isValid()) {
            size = QSizeF(flight.size.width() + (flight.grow.width() - flight.size.width()) * along,
                          flight.size.height() +
                              (flight.grow.height() - flight.size.height()) * along);
        }
        const QRectF box(at, size);
        painter->save();
        // Une carte lancée tourne un peu, et se redresse en arrivant.
        painter->translate(box.center());
        painter->rotate((1.0 - along) * 14.0);
        painter->translate(-box.center());
        if (flight.faceUp) {
            drawCardFace(painter, box, flight.card, scheme);
        } else {
            drawCardBack(painter, box, scheme);
        }
        painter->restore();
    }
}

// ---------------------------------------------------------------------------
// La souris.

int TableView::cardAt(const QPointF &where) const {
    // À l'envers: the one drawn last is the one on top, and the one on top is
    // the one you meant.
    for (int i = m_handBoxes.size() - 1; i >= 0; --i) {
        const int id = m_handBoxes.at(i).id;
        QRectF box = m_handBoxes.at(i).box;
        // Une carte soulevée se clique là où elle est, pas là où elle était.
        if (m_chosen.contains(id)) box.translate(0, -kLift);
        if (box.contains(where)) return id;
    }
    return -1;
}

int TableView::meldAt(const QPointF &where) const {
    for (const Spot &spot : m_meldBoxes) {
        if (spot.box.contains(where)) return spot.id;
    }
    return -1;
}

void TableView::mousePressEvent(QMouseEvent *event) {
    const QPointF where = event->position();
    if (event->button() != Qt::LeftButton) {
        const int meld = meldAt(where);
        if (meld >= 0) {
            Q_EMIT meldRightPressed(meld);
            return;
        }
        clearChosen();
        return;
    }
    if (m_stockBox.adjusted(-4, -4, 4, 4).contains(where)) {
        Q_EMIT stockPressed();
        return;
    }
    if (m_pileBox.adjusted(-4, -4, 4, 4).contains(where)) {
        Q_EMIT pilePressed();
        return;
    }
    const int card = cardAt(where);
    if (card >= 0) {
        if (m_chosen.contains(card)) {
            m_chosen.removeAll(card);
        } else {
            m_chosen.append(card);
        }
        Q_EMIT chosenChanged();
        Q_EMIT cardPressed(card);
        update();
        return;
    }
    const int meld = meldAt(where);
    if (meld >= 0) {
        Q_EMIT meldPressed(meld);
        return;
    }
    clearChosen();
}

void TableView::mouseDoubleClickEvent(QMouseEvent *event) {
    const int card = cardAt(event->position());
    if (card < 0) return;
    // A double click on a card chose it and then unchose it on the way
    // through; it is meant as "that one, go on".
    if (!m_chosen.contains(card)) {
        m_chosen.append(card);
        Q_EMIT chosenChanged();
    }
    Q_EMIT cardDoubleClicked(card);
}

void TableView::mouseMoveEvent(QMouseEvent *event) {
    const QPointF where = event->position();
    const int card = cardAt(where);
    const int meld = card >= 0 ? -1 : meldAt(where);
    const bool stock = m_stockBox.adjusted(-4, -4, 4, 4).contains(where);
    const bool pile = m_pileBox.adjusted(-4, -4, 4, 4).contains(where);
    if (card == m_hovered && meld == m_hoveredMeld && stock == m_hoverStock &&
        pile == m_hoverPile) {
        return;
    }
    m_hovered = card;
    m_hoveredMeld = meld;
    m_hoverStock = stock;
    m_hoverPile = pile;
    setCursor(card >= 0 || meld >= 0 || stock || pile ? Qt::PointingHandCursor : Qt::ArrowCursor);
    update();
}

void TableView::leaveEvent(QEvent *) {
    m_hovered = -1;
    m_hoveredMeld = -1;
    m_hoverStock = false;
    m_hoverPile = false;
    update();
}
