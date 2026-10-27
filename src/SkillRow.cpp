#include "SkillRow.h"

#include "FeltArt.h"
#include "Palette.h"

#include <QFontMetricsF>
#include <QMouseEvent>
#include <QPainter>

#include <algorithm>

namespace {

// Position des colonnes, mesurée depuis la gauche. Ces valeurs sont fixes et
// non calculées : huit lignes alignées se lisent comme un tableau, alors que
// huit lignes alignées chacune sur son propre texte se lisent isolément.
constexpr qreal kTitleWide = 112.0;
constexpr qreal kPipStep = 18.0;
constexpr qreal kPipSize = 11.0;
constexpr qreal kRomanWide = 34.0;
constexpr qreal kWordWide = 160.0;
constexpr qreal kGaugeWide = 140.0;
constexpr qreal kRowHigh = 34.0;

// Mode compact : largeur des mêmes colonnes dans l'aperçu. Les deux colonnes
// de compétences y figurent côte à côte, et huit lignes à l'interligne de la
// fiche produiraient un panneau plus haut que la plaque survolée.
constexpr qreal kTightTitle = 76.0;
constexpr qreal kTightRoman = 24.0;
constexpr qreal kTightWord = 172.0;
constexpr qreal kTightHigh = 21.0;
// Corps du texte en mode compact : deux points de moins que sur la fiche.
constexpr int kTightType = 11;

} // namespace

SkillRow::SkillRow(QWidget *parent) : QWidget(parent) {
    setAttribute(Qt::WA_Hover);
    setMinimumHeight(int(kRowHigh));
}

qreal SkillRow::titleWide() const { return m_compact ? kTightTitle : kTitleWide; }

// La colonne du rang est réservée même vide sur la fiche, où les deux colonnes
// de compétences sont superposées et doivent s'aligner. En mode compact,
// elles sont côte à côte : celle des innées, dépourvue de rang, n'a pas à en
// réserver la place.
qreal SkillRow::romanWide() const {
    if (!m_compact) return kRomanWide;
    return m_roman.isEmpty() ? 0.0 : kTightRoman;
}

qreal SkillRow::wordWide() const { return m_compact ? kTightWord : kWordWide; }

qreal SkillRow::rowHigh() const { return m_compact ? kTightHigh : kRowHigh; }

QSize SkillRow::sizeHint() const {
    // En mode compact, la ligne se limite à ses trois colonnes, la dernière
    // étant ajustée à son libellé : c'est ce qui détermine la largeur de
    // l'aperçu, quatre libellés courts n'ayant pas à réserver la place de
    // quatre libellés longs.
    if (m_compact) {
        QFont word = font();
        word.setBold(true);
        word.setPixelSize(kTightType);
        const qreal mine = QFontMetricsF(word).horizontalAdvance(m_word) + 4.0;
        return QSize(int(titleWide() + m_outOf * kPipStep + 8.0 + romanWide() +
                         std::clamp(mine, 40.0, kTightWord)),
                     int(kTightHigh));
    }
    return QSize(640, int(kRowHigh));
}

void SkillRow::setTitle(const QString &title) {
    m_title = title;
    update();
}

void SkillRow::setHue(int hue) {
    m_hue = hue;
    update();
}

void SkillRow::setLevel(int level, int outOf) {
    m_outOf = std::max(1, outOf);
    m_level = qBound(0, level, m_outOf - 1);
    update();
}

void SkillRow::setWord(const QString &word) {
    m_word = word;
    // En mode compact, la largeur de la ligne est déterminée par son
    // libellé.
    if (m_compact) updateGeometry();
    update();
}

void SkillRow::setSays(const QString &says) {
    m_says = says;
    update();
}

void SkillRow::setRank(const QString &roman) {
    m_roman = roman;
    if (m_compact) updateGeometry();
    update();
}

void SkillRow::setGauge(qreal filled, const QString &right) {
    m_filled = filled;
    m_right = right;
    update();
}

void SkillRow::setEditable(bool editable) {
    m_editable = editable;
    setCursor(editable ? Qt::PointingHandCursor : Qt::ArrowCursor);
    if (!editable) m_hovered = -1;
    update();
}

void SkillRow::setDim(bool dim) {
    m_dim = dim;
    update();
}

void SkillRow::setCompact(bool compact) {
    if (m_compact == compact) return;
    m_compact = compact;
    setMinimumHeight(int(rowHigh()));
    updateGeometry();
    update();
}

QRectF SkillRow::pipBox(int index) const {
    const qreal top = (height() - kPipSize) / 2.0;
    return QRectF(titleWide() + index * kPipStep, top, kPipSize, kPipSize);
}

int SkillRow::pipAt(const QPointF &where) const {
    for (int i = 0; i < m_outOf; ++i) {
        // Zone de clic légèrement plus large que la pastille : celle-ci mesure
        // neuf pixels, ce qui serait insuffisant comme cible.
        if (pipBox(i).adjusted(-3.0, -6.0, 3.0, 6.0).contains(where)) return i;
    }
    return -1;
}

void SkillRow::mousePressEvent(QMouseEvent *event) {
    if (!m_editable || event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }
    const int pip = pipAt(event->position());
    if (pip < 0) return;
    if (pip != m_level) Q_EMIT levelChosen(pip);
}

void SkillRow::mouseMoveEvent(QMouseEvent *event) {
    if (!m_editable) return;
    const int pip = pipAt(event->position());
    if (pip == m_hovered) return;
    m_hovered = pip;
    update();
}

void SkillRow::leaveEvent(QEvent *) {
    if (m_hovered < 0) return;
    m_hovered = -1;
    update();
}

void SkillRow::paintEvent(QPaintEvent *) {
    const Palette &scheme = currentPalette();
    const QColor mine = rivalColour(m_hue);
    const QColor empty = blendColours(scheme.surface, scheme.surfaceText, 0.18);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    if (m_dim) painter.setOpacity(0.38);

    // Ligne modifiable : un fond très clair dans sa couleur signale les lignes
    // qui répondent au clic.
    if (m_editable) {
        QRectF box = QRectF(rect()).adjusted(0.0, 1.0, -1.0, -1.0);
        painter.setPen(Qt::NoPen);
        painter.setBrush(blendColours(scheme.surface, mine, 0.10));
        painter.drawRoundedRect(box, 6.0, 6.0);
    }

    QFont name = plainFont(&painter);
    name.setBold(true);
    name.setPixelSize(m_compact ? kTightType + 1 : 14);
    painter.setFont(name);
    painter.setPen(scheme.surfaceText);
    painter.drawText(QRectF(0.0, 0.0, titleWide() - 8.0, height()),
                     Qt::AlignLeft | Qt::AlignVCenter, m_title);

    // Les pastilles : pleines jusqu'au niveau atteint, évidées au-delà. Celle
    // qui est survolée est représentée comme sélectionnée par anticipation.
    for (int i = 0; i < m_outOf; ++i) {
        const QRectF box = pipBox(i);
        const bool filled = i <= m_level;
        painter.setPen(Qt::NoPen);
        painter.setBrush(filled ? mine : empty);
        painter.drawEllipse(box);
        if (m_editable && i == m_hovered && i != m_level) {
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(mine, 1.6));
            painter.drawEllipse(box.adjusted(-3.0, -3.0, 3.0, 3.0));
        }
    }

    qreal left = titleWide() + m_outOf * kPipStep + 8.0;

    // Le rang, lorsqu'il existe. Sur la fiche, sa colonne est réservée même
    // vide : les innées n'ont pas de rang et les acquises en ont un, et deux
    // colonnes dont les libellés ne commencent pas au même endroit se liraient
    // comme deux tableaux distincts. Voir `romanWide()`.
    if (!m_roman.isEmpty()) {
        QFont rank = plainFont(&painter);
        rank.setBold(true);
        rank.setPixelSize(m_compact ? kTightType : 13);
        painter.setFont(rank);
        painter.setPen(mine);
        painter.drawText(QRectF(left, 0.0, romanWide(), height()),
                         Qt::AlignLeft | Qt::AlignVCenter, m_roman);
    }
    left += romanWide();

    QFont word = plainFont(&painter);
    word.setBold(true);
    word.setPixelSize(m_compact ? kTightType : 13);
    painter.setFont(word);
    painter.setPen(mine);
    const QFontMetricsF wordMetrics(word);
    // En mode compact, le libellé occupe tout l'espace restant : la largeur de
    // la colonne est celle de la colonne de compétences, déterminée par le plus
    // long de ses quatre libellés.
    const qreal wordRoom = m_compact ? width() - left : wordWide() - 8.0;
    painter.drawText(QRectF(left, 0.0, wordRoom, height()),
                     Qt::AlignLeft | Qt::AlignVCenter,
                     wordMetrics.elidedText(m_word, Qt::ElideRight, wordRoom));
    left += wordWide();

    // En mode compact, la ligne s'arrête ici : la description du niveau et la
    // jauge de progression relèvent de la fiche, que l'aperçu ne remplace
    // pas.
    if (m_compact) return;

    // La jauge, placée à l'extrême droite, accompagnée de l'expérience
    // manquante. Sa colonne est également réservée, pour la même raison : la
    // description se termine au même endroit sur les huit lignes, qu'une jauge
    // la suive ou non.
    const qreal gaugeLeft = width() - kGaugeWide;
    const qreal room = gaugeLeft - left - 8.0;
    if (m_filled >= 0.0) {
        drawGauge(&painter, QRectF(gaugeLeft, height() / 2.0 - 3.0, 52.0, 6.0),
                  qBound(0.0, m_filled, 1.0), mine, empty);
        QFont figure = plainFont(&painter);
        figure.setPixelSize(12);
        painter.setFont(figure);
        painter.setPen(scheme.hint);
        painter.drawText(QRectF(gaugeLeft + 60.0, 0.0, kGaugeWide - 60.0, height()),
                         Qt::AlignLeft | Qt::AlignVCenter, m_right);
    }

    QFont says = plainFont(&painter);
    says.setPixelSize(13);
    painter.setFont(says);
    painter.setPen(scheme.hint);
    const QFontMetricsF metrics(says);
    painter.drawText(QRectF(left, 0.0, std::max(40.0, room), height()),
                     Qt::AlignLeft | Qt::AlignVCenter,
                     metrics.elidedText(m_says, Qt::ElideRight, std::max(40.0, room)));
}
