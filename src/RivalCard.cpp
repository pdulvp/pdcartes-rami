#include "RivalCard.h"

#include "FeltArt.h"
#include "Fonts.h"
#include "Palette.h"
#include "RivalGlance.h"
#include "Rivals.h"

#include <QContextMenuEvent>
#include <QFontMetricsF>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QTimer>

#include <algorithm>

namespace {

constexpr qreal kPad = 12.0;
constexpr qreal kCorner = 12.0;

} // namespace

RivalCard::RivalCard(const QString &rivalId, QWidget *parent)
    : QWidget(parent), m_rivalId(rivalId) {
    setAttribute(Qt::WA_Hover);
    setMinimumSize(286, 112);

    // L'aperçu n'apparaît qu'après un maintien du pointeur sur la carte : un
    // déplacement à travers les six n'en déclenche aucun.
    m_glanceBeat = new QTimer(this);
    m_glanceBeat->setSingleShot(true);
    m_glanceBeat->setInterval(kGlanceDelayMs);
    connect(m_glanceBeat, &QTimer::timeout, this, [this] {
        showRivalGlance(m_rivalId, QRect(mapToGlobal(QPoint(0, 0)), size()), this);
    });
}

QSize RivalCard::sizeHint() const { return QSize(300, m_onFelt ? 78 : 118); }

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

void RivalCard::setOnFelt(bool onFelt) {
    m_onFelt = onFelt;
    // Deux lignes de texte en moins : la carte est réduite d'autant.
    setMinimumSize(286, onFelt ? 74 : 112);
    updateGeometry();
    update();
}

void RivalCard::setGlanceWanted(bool wanted) {
    m_glanceWanted = wanted;
    if (!wanted) {
        m_glanceBeat->stop();
        hideRivalGlance(this);
    }
}

void RivalCard::reread() { update(); }

void RivalCard::mousePressEvent(QMouseEvent *event) {
    // Après un clic, l'aperçu n'a plus lieu d'être affiché, quelle que soit
    // l'action demandée : invitation, menu contextuel ou ouverture de la
    // fiche.
    m_glanceBeat->stop();
    hideRivalGlance(this);
    if (!m_pickable || event->button() != Qt::LeftButton) return;
    setInvited(!m_invited);
}

void RivalCard::mouseDoubleClickEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton) return;
    // Les deux clics du double-clic ont déjà produit leur effet — invitation
    // puis annulation — ce qui laisse la carte dans son état initial. Ce
    // comportement est souhaitable : ouvrir une fiche n'équivaut pas à inviter
    // l'adversaire.
    Q_EMIT sheetWanted();
    event->accept();
}

void RivalCard::contextMenuEvent(QContextMenuEvent *event) {
    m_glanceBeat->stop();
    hideRivalGlance(this);
    // La position est exprimée en coordonnées d'écran et non relativement à la
    // carte : la page qui construit le menu n'a pas à connaître la position de
    // celle-ci.
    Q_EMIT menuWanted(event->globalPos());
    event->accept();
}

void RivalCard::enterEvent(QEnterEvent *) {
    m_hovered = true;
    if (m_glanceWanted) m_glanceBeat->start();
    update();
}

void RivalCard::leaveEvent(QEvent *) {
    m_hovered = false;
    m_glanceBeat->stop();
    // L'aperçu est libéré et non fermé : le pointeur peut simplement être
    // passé sur celui-ci, l'aperçu ayant pu être placé sur la carte faute de
    // place ailleurs.
    releaseRivalGlance(this);
    update();
}

void RivalCard::hideEvent(QHideEvent *event) {
    // Changement de page ou fermeture de la fenêtre : l'aperçu doit être
    // fermé, faute de quoi il subsisterait sans la carte à laquelle il se
    // rapporte.
    m_glanceBeat->stop();
    hideRivalGlance(this);
    QWidget::hideEvent(event);
}

void RivalCard::paintEvent(QPaintEvent *) {
    const Palette &scheme = currentPalette();
    const Rival &who = rivalById(m_rivalId);
    // Nom de l'adversaire sur ce profil. Le médaillon en reprend l'initiale,
    // de sorte qu'un changement de nom modifie également son affichage.
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
    QColor body;
    QColor edge;
    if (m_onFelt) {
        // Sur le tapis : fond presque noir, légèrement éclairci au survol, et
        // filet doré lorsque l'adversaire est invité, l'or servant de couleur
        // de sélection dans tout ce menu.
        body = blendColours(scheme.feltEdge, QColor(0, 0, 0), lit ? 0.42 : 0.52);
        if (m_invited) body = blendColours(body, mine, 0.18);
        body.setAlpha(lit || m_invited ? 215 : 205);
        edge = m_invited ? scheme.gold : (lit ? scheme.gold : scheme.gold);
        edge.setAlphaF(m_invited ? 0.95 : (lit ? 0.70 : 0.22));
    } else {
        body = scheme.surface;
        if (m_invited) body = blendColours(body, mine, 0.16);
        else if (lit) body = blendColours(body, mine, 0.08);
        edge = m_invited ? mine : (lit ? blendColours(mine, scheme.edge, 0.4) : scheme.edge);
    }
    painter.setPen(Qt::NoPen);
    painter.fillPath(shape, body);
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(edge, m_invited ? 2.2 : 1.0));
    painter.drawPath(shape);

    // Couleur du texte de la carte, selon son fond.
    const QColor ink = m_onFelt ? scheme.feltText : scheme.surfaceText;
    QColor aside = m_onFelt ? scheme.feltText : scheme.hint;
    if (m_onFelt) aside.setAlphaF(0.60);
    const QColor behind = m_onFelt ? blendColours(scheme.feltEdge, QColor(0, 0, 0), 0.52)
                                   : scheme.surface;

    const QRectF inner = box.adjusted(kPad, kPad, -kPad, -kPad);
    const qreal disc = std::min(inner.height() * 0.72, 56.0);
    drawRivalEmblem(&painter, QRectF(inner.left(), inner.top(), disc, disc), who.emblem,
                    called, mine, blendColours(behind, mine, 0.14));

    const qreal left = inner.left() + disc + 12.0;
    const qreal wide = inner.right() - left;

    QFont name = plainFont(&painter);
    name.setBold(true);
    name.setPixelSize(15);
    painter.setFont(name);
    painter.setPen(ink);
    const QFontMetricsF nameMetrics(name);
    painter.drawText(QRectF(left, inner.top(), wide, nameMetrics.height()),
                     Qt::AlignLeft | Qt::AlignVCenter, called);

    // Le niveau, affiché en pastille dans sa couleur propre.
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
    if (m_onFelt) {
        // La pastille est évidée et non pleine : six pastilles pleines en
        // couleurs vives seraient trop visibles sur un menu reposant sur l'or
        // et le noir. Le contour conserve la couleur, ce qui suffit à
        // identifier le niveau.
        painter.setPen(QPen(mine, 1.2));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(chip, chip.height() / 2.0, chip.height() / 2.0);
        painter.setPen(mine);
    } else {
        painter.setPen(Qt::NoPen);
        painter.setBrush(mine);
        painter.drawRoundedRect(chip, chip.height() / 2.0, chip.height() / 2.0);
        painter.setPen(readableTextOn(mine));
    }
    painter.drawText(capCentred(chip, pillMetrics), Qt::AlignCenter, badge);

    qreal y = inner.top() + nameMetrics.height() + 1.0;

    QFont small = plainFont(&painter);
    small.setBold(false);
    small.setPixelSize(11);
    small.setCapitalization(QFont::MixedCase);
    small.setLetterSpacing(QFont::PercentageSpacing, 100);
    const QFontMetricsF asideMetrics(small);
    // La phrase descriptive n'est affichée que sur la fiche, où elle est
    // consultée ; dans le menu, six phrases de ce type formeraient un bloc de
    // texte peu lisible.
    if (!m_onFelt) {
        painter.setFont(small);
        painter.setPen(aside);
        painter.drawText(QRectF(left, y, wide, asideMetrics.height()),
                         Qt::AlignLeft | Qt::AlignVCenter, rivalBlurb(m_rivalId));
        y += asideMetrics.height() + 5.0;
    } else {
        y += 4.0;
    }

    // Les étoiles, la jauge et la valeur numérique associée : la jauge indique
    // la progression de façon approximative, la valeur la précise.
    drawLevelStars(&painter, QRectF(left, y, 46.0, 12.0), levelStars(level), 3, scheme.gold,
                   blendColours(behind, ink, 0.16));
    const qreal gaugeLeft = left + 54.0;
    const qreal gaugeWide = std::max(40.0, wide - 54.0 - 64.0);
    drawGauge(&painter, QRectF(gaugeLeft, y + 4.0, gaugeWide, 5.0), progressWithin(learning.xp),
              m_onFelt ? scheme.gold : mine, blendColours(behind, ink, 0.14));
    painter.setFont(small);
    painter.setPen(aside);
    painter.drawText(QRectF(gaugeLeft + gaugeWide + 8.0, y - 1.0, 60.0, 14.0),
                     Qt::AlignLeft | Qt::AlignVCenter,
                     tr("%1 xp").arg(learning.xp));
    y += 16.0;

    // Résultats sous ce profil, affichés sur la fiche uniquement : dans le
    // menu, cette ligne serait répétée six fois et la jauge fournit déjà cette
    // information.
    if (!m_onFelt) {
        QString record;
        if (learning.handsPlayed == 0) {
            record = tr("pas encore joué sous ce profil");
        } else {
            record = tr("%1 manches, %2 sorties, %3 parties gagnées")
                         .arg(learning.handsPlayed)
                         .arg(learning.handsWon)
                         .arg(learning.matchesWon);
        }
        painter.setFont(small);
        painter.setPen(aside);
        painter.drawText(QRectF(left, y, wide, asideMetrics.height()),
                         Qt::AlignLeft | Qt::AlignVCenter, record);
    }

    if (m_pickable && m_invited) {
        QFont mark = plainFont(&painter);
        mark.setBold(true);
        mark.setPixelSize(11);
        painter.setFont(mark);
        painter.setPen(m_onFelt ? scheme.gold : mine);
        painter.drawText(inner, Qt::AlignRight | Qt::AlignBottom, tr("invité"));
    }
}
