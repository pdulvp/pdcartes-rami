#include "RivalCard.h"

#include "CardArt.h"
#include "Palette.h"
#include "Rivals.h"

#include <QContextMenuEvent>
#include <QFontMetricsF>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>

namespace {

constexpr qreal kPad = 12.0;
constexpr qreal kCorner = 12.0;

} // namespace

RivalCard::RivalCard(const QString &rivalId, QWidget *parent)
    : QWidget(parent), m_rivalId(rivalId) {
    setAttribute(Qt::WA_Hover);
    setMinimumSize(286, 112);
}

QSize RivalCard::sizeHint() const { return QSize(300, 118); }

void RivalCard::setInvited(bool invited) {
    if (m_invited == invited) return;
    m_invited = invited;
    update();
    Q_EMIT invitedChanged();
}

void RivalCard::setPickable(bool pickable) {
    m_pickable = pickable;
    setCursor(pickable ? Qt::PointingHandCursor : Qt::ArrowCursor);
    update();
}

void RivalCard::reread() { update(); }

void RivalCard::mousePressEvent(QMouseEvent *event) {
    if (!m_pickable || event->button() != Qt::LeftButton) return;
    setInvited(!m_invited);
}

void RivalCard::contextMenuEvent(QContextMenuEvent *event) {
    // La position à l'écran plutôt que dans la carte: la page qui dresse le
    // menu n'a pas à savoir où la carte se trouve.
    Q_EMIT menuWanted(event->globalPos());
    event->accept();
}

void RivalCard::enterEvent(QEnterEvent *) {
    m_hovered = true;
    update();
}

void RivalCard::leaveEvent(QEvent *) {
    m_hovered = false;
    update();
}

void RivalCard::paintEvent(QPaintEvent *) {
    const Palette &scheme = currentPalette();
    const Rival &who = rivalById(m_rivalId);
    // Le nom qu'il porte sur ce profil. Le médaillon en prend l'initiale, si
    // bien que le rebaptiser redessine sa pastille.
    const QString called = rivalName(m_rivalId);
    const Learning learning = learningFor(who.id);
    const Level level = levelOf(learning.xp);
    const QColor mine = rivalColour(who.hue);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    QRectF box = QRectF(rect()).adjusted(1.0, 1.0, -1.0, -1.0);
    QPainterPath shape;
    shape.addRoundedRect(box, kCorner, kCorner);

    const bool lit = m_hovered && m_pickable;
    QColor body = scheme.surface;
    if (m_invited) body = blendColours(body, mine, 0.16);
    else if (lit) body = blendColours(body, mine, 0.08);
    painter.setPen(Qt::NoPen);
    painter.fillPath(shape, body);
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(m_invited ? mine : (lit ? blendColours(mine, scheme.edge, 0.4)
                                               : scheme.edge),
                        m_invited ? 2.2 : 1.0));
    painter.drawPath(shape);

    const QRectF inner = box.adjusted(kPad, kPad, -kPad, -kPad);
    const qreal disc = std::min(inner.height() * 0.72, 56.0);
    drawRivalEmblem(&painter, QRectF(inner.left(), inner.top(), disc, disc), who.emblem,
                    called, mine, blendColours(scheme.surface, mine, 0.14));

    const qreal left = inner.left() + disc + 12.0;
    const qreal wide = inner.right() - left;

    QFont name = plainFont(&painter);
    name.setBold(true);
    name.setPixelSize(15);
    painter.setFont(name);
    painter.setPen(scheme.surfaceText);
    const QFontMetricsF nameMetrics(name);
    painter.drawText(QRectF(left, inner.top(), wide, nameMetrics.height()),
                     Qt::AlignLeft | Qt::AlignVCenter, called);

    // Le niveau, en pastille, dans sa couleur.
    QFont pill = plainFont(&painter);
    pill.setPixelSize(10);
    pill.setBold(true);
    pill.setCapitalization(QFont::AllUppercase);
    pill.setLetterSpacing(QFont::PercentageSpacing, 115);
    painter.setFont(pill);
    const QFontMetricsF pillMetrics(pill);
    const QString badge = levelName(level);
    QRectF chip(0, 0, pillMetrics.horizontalAdvance(badge) + 16.0, pillMetrics.height() + 4.0);
    chip.moveTopRight(QPointF(inner.right(), inner.top() + 1.0));
    painter.setPen(Qt::NoPen);
    painter.setBrush(mine);
    painter.drawRoundedRect(chip, chip.height() / 2.0, chip.height() / 2.0);
    painter.setPen(readableTextOn(mine));
    painter.drawText(chip, Qt::AlignCenter, badge);

    qreal y = inner.top() + nameMetrics.height() + 1.0;

    QFont aside = plainFont(&painter);
    aside.setBold(false);
    aside.setPixelSize(11);
    aside.setCapitalization(QFont::MixedCase);
    aside.setLetterSpacing(QFont::PercentageSpacing, 100);
    painter.setFont(aside);
    painter.setPen(scheme.hint);
    const QFontMetricsF asideMetrics(aside);
    painter.drawText(QRectF(left, y, wide, asideMetrics.height()),
                     Qt::AlignLeft | Qt::AlignVCenter, who.blurb);
    y += asideMetrics.height() + 5.0;

    // Les étoiles, la jauge, et le chiffre derrière elles -- parce que la
    // jauge dit "bientôt" et le chiffre dit de combien.
    drawLevelStars(&painter, QRectF(left, y, 46.0, 12.0), levelStars(level), 3, scheme.gold,
                   blendColours(scheme.surface, scheme.surfaceText, 0.16));
    const qreal gaugeLeft = left + 54.0;
    const qreal gaugeWide = std::max(40.0, wide - 54.0 - 64.0);
    drawGauge(&painter, QRectF(gaugeLeft, y + 4.0, gaugeWide, 5.0), progressWithin(learning.xp),
              mine, blendColours(scheme.surface, scheme.surfaceText, 0.14));
    painter.setFont(aside);
    painter.setPen(scheme.hint);
    painter.drawText(QRectF(gaugeLeft + gaugeWide + 8.0, y - 1.0, 60.0, 14.0),
                     Qt::AlignLeft | Qt::AlignVCenter,
                     tr("%1 xp").arg(learning.xp));
    y += 16.0;

    // Ce qu'il a fait sous ce profil. Un adversaire qui n'a jamais joué le
    // dit, parce que c'est la réponse à "pourquoi est-il encore débutant".
    QString record;
    if (learning.handsPlayed == 0) {
        record = tr("pas encore joué sous ce profil");
    } else {
        record = tr("%1 manches, %2 sorties, %3 parties gagnées")
                     .arg(learning.handsPlayed)
                     .arg(learning.handsWon)
                     .arg(learning.matchesWon);
    }
    painter.setPen(scheme.hint);
    painter.drawText(QRectF(left, y, wide, asideMetrics.height()),
                     Qt::AlignLeft | Qt::AlignVCenter, record);

    if (m_pickable && m_invited) {
        QFont mark = plainFont(&painter);
        mark.setBold(true);
        mark.setPixelSize(11);
        painter.setFont(mark);
        painter.setPen(mine);
        painter.drawText(inner, Qt::AlignRight | Qt::AlignBottom, tr("invité"));
    }
}
