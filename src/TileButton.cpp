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

// Espacement entre la représentation de la table et son nom. Il est ample
// parce que la tuile ne comporte plus que ces deux éléments : les resserrer
// dans la partie supérieure laisserait la moitié inférieure vide.
constexpr qreal kArtToName = 16.0;

// Rapport largeur sur hauteur du tapis représenté.
//
// Cette valeur est fixe plutôt que déduite de la place disponible. La hauteur
// de la représentation étant plafonnée et sa largeur suivant celle de la
// tuile, le même ovale apparaissait presque circulaire sur une tuile étroite
// et très aplati sur une tuile large. Une table se reconnaissant à sa forme,
// celle-ci ne doit pas dépendre du cadre.
constexpr qreal kClothRatio = 1.35;

// Représentation réduite de la table : un ovale de tapis, les places
// réparties autour, et un tas de cartes au centre. Il ne s'agit pas d'un
// aperçu de la partie à venir, mais d'une illustration dont la fonction est
// d'indiquer le nombre de joueurs.
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

    // Un tas de cartes au centre, afin que l'ovale se lise comme une table de
    // jeu.
    const QSizeF middle = cardSizeForHeight(cloth.height() * 0.30);
    QRectF stack(0, 0, middle.width(), middle.height());
    stack.moveCenter(cloth.center());
    for (int i = 2; i >= 0; --i) {
        drawCardBack(painter, stack.translated(-1.2 * i, -1.2 * i), scheme);
    }

    // Les places réparties sur le pourtour ; celle du joueur figure en bas,
    // dans la couleur de la table.
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
        // Chaque place est orientée selon le rayon joignant le centre à sa
        // position. Cette direction est recalculée plutôt que reprise de
        // `angle`, pour deux raisons : `angle` est compté en sens inverse, et
        // le tapis est un ovale et non un cercle, de sorte que l'angle ayant
        // servi à positionner la place ne correspond pas à la direction où
        // elle se trouve. Chaque joueur fait face au centre de la table.
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
        // Sur le tapis : fond presque noir et translucide, bordé d'un filet
        // doré qui s'intensifie au survol. Aucune ombre portée : elle serait
        // sans effet entre une plaque sombre et un tapis sombre, et ne
        // produirait qu'un halo.
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

    // La représentation et le nom sont centrés ensemble sur la hauteur.
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
