#include "ProfileChip.h"

#include "Fonts.h"
#include "Palette.h"
#include "Profiles.h"

#include <QEnterEvent>
#include <QFontMetricsF>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>

namespace {

constexpr qreal kHeight = 32.0;
constexpr qreal kPadLeft = 5.0;
constexpr qreal kPadRight = 11.0;
constexpr qreal kDiscGap = 9.0;
constexpr qreal kChevron = 9.0;
constexpr qreal kChevronGap = 9.0;

} // namespace

ProfileChip::ProfileChip(QWidget *parent) : QAbstractButton(parent) {
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::NoFocus);
    setAttribute(Qt::WA_Hover);
    setToolTip(tr("Le profil sous lequel vous jouez. Cliquez pour en changer, le renommer, "
                  "ou l'oublier."));
    reread();
}

QFont ProfileChip::nameFont() const {
    QFont name = font();
    name.setBold(true);
    return name;
}

void ProfileChip::reread() {
    const QString now = profileName(currentProfile());
    if (now == m_name) return;
    m_name = now;
    updateGeometry();
    update();
}

QSize ProfileChip::sizeHint() const {
    const QFontMetricsF metrics(nameFont());
    const qreal disc = kHeight - 10.0;
    const qreal wide = kPadLeft + disc + kDiscGap + metrics.horizontalAdvance(m_name) +
                       kChevronGap + kChevron + kPadRight;
    return QSize(int(std::ceil(wide)), int(kHeight));
}

void ProfileChip::enterEvent(QEnterEvent *event) {
    m_hovered = true;
    update();
    QAbstractButton::enterEvent(event);
}

void ProfileChip::leaveEvent(QEvent *event) {
    m_hovered = false;
    update();
    QAbstractButton::leaveEvent(event);
}

void ProfileChip::setOnDark(bool onDark) {
    if (m_onDark == onDark) return;
    m_onDark = onDark;
    update();
}

void ProfileChip::paintEvent(QPaintEvent *) {
    const Palette &scheme = currentPalette();
    // Le fond de la bande, son encre, et la couleur de la pastille.
    const QColor face =
        m_onDark ? blendColours(scheme.feltEdge, QColor(0, 0, 0), 0.46) : scheme.face;
    const QColor ink = m_onDark ? scheme.feltText : scheme.faceText;
    const QColor medalFill = m_onDark ? scheme.gold : scheme.accent;
    const QColor medalInk = m_onDark ? readableTextOn(medalFill) : scheme.accentText;
    const QColor glow = m_onDark ? scheme.gold : scheme.accent;

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    const QRectF box = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    const qreal radius = box.height() / 2.0;

    // Le fond: rien au repos, pour que la bande reste une bande. Les deux
    // teintes du survol et de l'appui sont celles des boutons d'à côté --
    // c'est ce qui fait que la plaque en est une et pas une pièce rapportée.
    if (isDown() || m_hovered) {
        QPainterPath pill;
        pill.addRoundedRect(box, radius, radius);
        painter.fillPath(pill, blendColours(face, glow, isDown() ? 0.36 : 0.22));
    }

    // La pastille et l'initiale: le profil se reconnaît à sa lettre avant
    // qu'on ait lu son nom.
    const qreal disc = box.height() - 10.0;
    QRectF medal(box.left() + kPadLeft, box.center().y() - disc / 2.0, disc, disc);
    painter.setPen(Qt::NoPen);
    painter.setBrush(medalFill);
    painter.drawEllipse(medal);

    const QString initial = m_name.trimmed().left(1).toUpper();
    if (!initial.isEmpty()) {
        QFont letter = font();
        letter.setBold(true);
        letter.setPixelSize(int(disc * 0.62));
        painter.setFont(letter);
        painter.setPen(medalInk);
        painter.drawText(capCentred(medal, QFontMetricsF(letter)), Qt::AlignCenter, initial);
    }

    const QFont name = nameFont();
    const QFontMetricsF metrics(name);
    const qreal textLeft = medal.right() + kDiscGap;
    painter.setFont(name);
    painter.setPen(ink);
    painter.drawText(QRectF(textLeft, box.top(), metrics.horizontalAdvance(m_name), box.height()),
                     Qt::AlignLeft | Qt::AlignVCenter, m_name);

    // Le chevron, dessiné et non écrit: un caractère « v » dépend de la
    // police installée, un trait non.
    const qreal cx = box.right() - kPadRight - kChevron / 2.0;
    const qreal cy = box.center().y() + 1.0;
    QColor mark = ink;
    mark.setAlpha(m_hovered || isDown() ? 230 : 160);
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(mark, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    QPainterPath arrow;
    arrow.moveTo(cx - kChevron / 2.0, cy - kChevron / 4.0);
    arrow.lineTo(cx, cy + kChevron / 4.0);
    arrow.lineTo(cx + kChevron / 2.0, cy - kChevron / 4.0);
    painter.drawPath(arrow);
}
