#include "TileButton.h"

#include "CardArt.h"
#include "Fonts.h"
#include "Palette.h"

#include <QEnterEvent>
#include <QFontMetricsF>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>
#include <cmath>

namespace {

constexpr qreal kCorner = 14.0;
constexpr int kArtHeight = 86;
constexpr int kPadding = 14;

// L'air entre le portrait de la table et son nom. Large, parce qu'il n'y a
// plus que ces deux choses sur la tuile et que les serrer l'une contre
// l'autre en haut laisserait la moitié basse vide.
constexpr qreal kArtToName = 16.0;

// Le rapport du drap, largeur sur hauteur.
//
// Tenu plutôt que tiré de la place disponible: la hauteur du portrait est
// plafonnée et sa largeur suivait celle de la tuile, si bien que le même
// ovale était rond sur une tuile étroite et en flaque sur une large. Une
// table se reconnaît à sa forme; elle ne change pas avec le cadre.
constexpr qreal kClothRatio = 1.35;

// La petite table: un ovale de drap, des places autour, et un tas au milieu.
// Ce n'est pas la table qu'on va jouer -- c'est son portrait, et ce qu'il
// dit, c'est combien on sera.
void paintLittleTable(QPainter *painter, const QRectF &box, int seats, const Palette &scheme) {
    const qreal clothHeight = box.height() - 8.0;
    QRectF cloth(0.0, 0.0, std::min(box.width(), clothHeight * kClothRatio), clothHeight);
    cloth.moveCenter(box.center());
    if (cloth.width() < 20 || cloth.height() < 20) return;

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    painter->setPen(Qt::NoPen);
    painter->setBrush(blendColours(scheme.felt, scheme.feltEdge, 0.3));
    painter->drawEllipse(cloth.translated(0, 2.0));
    painter->setBrush(scheme.felt);
    painter->drawEllipse(cloth);
    painter->setBrush(Qt::NoBrush);
    QColor braid = scheme.trim;
    braid.setAlpha(170);
    painter->setPen(QPen(braid, 1.6));
    painter->drawEllipse(cloth.adjusted(3, 3, -3, -3));

    // Un petit tas au milieu, pour que l'ovale soit une table de jeu et pas
    // une flaque.
    const QSizeF middle = cardSizeForHeight(cloth.height() * 0.30);
    QRectF stack(0, 0, middle.width(), middle.height());
    stack.moveCenter(cloth.center());
    for (int i = 2; i >= 0; --i) {
        drawCardBack(painter, stack.translated(-1.2 * i, -1.2 * i), scheme);
    }

    // Les places, réparties sur le pourtour, la vôtre en bas et dans la
    // couleur de la table.
    const QSizeF seat = cardSizeForHeight(cloth.height() * 0.26);
    const int count = std::max(2, seats);
    for (int i = 0; i < count; ++i) {
        const qreal angle = M_PI / 2.0 + i * 2.0 * M_PI / count;
        const QPointF at(cloth.center().x() + std::cos(angle) * cloth.width() * 0.40,
                         cloth.center().y() + std::sin(angle) * cloth.height() * 0.42);
        QRectF place(0, 0, seat.width(), seat.height());
        place.moveCenter(at);
        painter->save();
        painter->translate(at);
        // Face à la table: chaque place s'aligne sur le rayon qui va du
        // milieu jusqu'à elle. Deux raisons de le prendre là plutôt que de
        // reprendre `angle` -- il était compté à l'envers, et le drap est un
        // ovale et non un rond, si bien que l'angle qui a servi à poser la
        // place n'est pas la direction où elle se trouve. On tient ses cartes
        // face à soi, et personne n'est assis en travers.
        const QPointF away = at - cloth.center();
        painter->rotate(std::atan2(away.y(), away.x()) * 180.0 / M_PI - 90.0);
        painter->translate(-at);
        if (i == 0) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(scheme.accent);
            painter->drawRoundedRect(place, 3, 3);
        } else {
            drawCardBack(painter, place, scheme);
        }
        painter->restore();
    }
    painter->restore();
}

} // namespace

TileButton::TileButton(QWidget *parent) : QAbstractButton(parent) {
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_Hover);
}

void TileButton::setSeats(int seats) {
    m_seats = seats;
    update();
}

void TileButton::setCorner(const QString &corner) {
    m_corner = corner;
    update();
}

void TileButton::setOnFelt(bool onFelt) {
    m_onFelt = onFelt;
    update();
}

void TileButton::setTileSize(const QSize &size) {
    m_tileSize = size;
    setMinimumSize(size);
    updateGeometry();
}

QSize TileButton::sizeHint() const { return m_tileSize; }

void TileButton::enterEvent(QEnterEvent *event) {
    m_hovered = true;
    update();
    Q_EMIT pointerIn();
    QAbstractButton::enterEvent(event);
}

void TileButton::leaveEvent(QEvent *event) {
    m_hovered = false;
    update();
    Q_EMIT pointerOut();
    QAbstractButton::leaveEvent(event);
}

void TileButton::paintEvent(QPaintEvent *) {
    const Palette &scheme = currentPalette();
    const bool lit = m_hovered || hasFocus();

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    QRectF box = QRectF(rect()).adjusted(1.5, 1.5, -1.5, -1.5);
    if (isDown()) box.translate(0, 1);

    QColor fill;
    QColor edge;
    qreal line = 1.2;
    if (m_onFelt) {
        // Sur le drap: presque noir et translucide, bordé d'un filet d'or qui
        // s'allume au survol. Pas d'ombre portée -- une plaque sombre sur un
        // drap sombre n'en a pas besoin, et l'ombre ne ferait qu'un halo.
        fill = blendColours(scheme.feltEdge, QColor(0, 0, 0), lit ? 0.42 : 0.52);
        fill.setAlpha(lit ? 215 : 205);
        edge = scheme.gold;
        edge.setAlphaF(isChecked() ? 0.95 : (lit ? 0.80 : 0.26));
        line = isChecked() ? 2.2 : (lit ? 2.0 : 1.2);
    } else {
        fill = isChecked() ? blendColours(scheme.surface, scheme.accent, 0.20)
                           : (lit ? blendColours(scheme.surface, scheme.accent, 0.10)
                                  : scheme.surface);
        edge = isChecked() || lit ? scheme.accent : scheme.edge;
        line = isChecked() ? 2.2 : (lit ? 2.0 : 1.2);
    }

    QPainterPath shape;
    shape.addRoundedRect(box, kCorner, kCorner);

    if (!m_onFelt && (lit || isChecked())) {
        QPainterPath lift;
        lift.addRoundedRect(box.translated(0, 2.5), kCorner, kCorner);
        painter.fillPath(lift, blendColours(scheme.desk, scheme.accent, 0.28));
    }

    painter.fillPath(shape, fill);
    painter.setPen(QPen(edge, line));
    painter.drawPath(shape);

    const QRectF inner = box.adjusted(kPadding, kPadding, -kPadding, -kPadding);

    QFont name = font();
    name.setBold(true);
    if (name.pointSizeF() > 0) name.setPointSizeF(name.pointSizeF() * 1.08);
    const QFontMetricsF nameMetrics(name);

    // Le portrait et le nom, pris ensemble et posés au milieu de la hauteur.
    const qreal artHeight = std::min(qreal(kArtHeight), inner.height() * 0.52);
    const qreal together = artHeight + kArtToName + nameMetrics.height();
    const qreal top = inner.top() + std::max(0.0, (inner.height() - together) / 2.0);

    const QRectF art(inner.left(), top, inner.width(), artHeight);
    paintLittleTable(&painter, art, m_seats, scheme);

    painter.setFont(name);
    painter.setPen(m_onFelt ? (lit ? scheme.gold : scheme.feltText) : scheme.surfaceText);
    painter.drawText(
        QRectF(inner.left(), art.bottom() + kArtToName, inner.width(), nameMetrics.height()),
        Qt::AlignHCenter | Qt::AlignVCenter, text());

    if (!m_corner.isEmpty()) {
        QFont badge = font();
        badge.setBold(true);
        if (badge.pointSizeF() > 0) badge.setPointSizeF(std::max(7.0, badge.pointSizeF() - 1.6));
        painter.setFont(badge);
        const QFontMetricsF metrics(badge);
        QRectF chip(0, 0, metrics.horizontalAdvance(m_corner) + 16.0, metrics.height() + 4.0);
        chip.moveTopRight(QPointF(box.right() - 9.0, box.top() + 9.0));
        const QColor badgeFill = m_onFelt ? scheme.gold : scheme.accent;
        painter.setPen(Qt::NoPen);
        painter.setBrush(badgeFill);
        painter.drawRoundedRect(chip, chip.height() / 2.0, chip.height() / 2.0);
        painter.setPen(m_onFelt ? readableTextOn(badgeFill) : scheme.accentText);
        painter.drawText(capCentred(chip, metrics), Qt::AlignCenter, m_corner);
    }
}
