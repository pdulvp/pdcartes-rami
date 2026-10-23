#include "ScoreBoard.h"

#include "CardArt.h"
#include "Palette.h"
#include "Rami.h"

#include <QFontMetricsF>
#include <QKeyEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>

#include <algorithm>

namespace {

constexpr qreal kPad = 26.0;
constexpr qreal kRowHeight = 30.0;
constexpr qreal kNameWidth = 190.0;
constexpr qreal kNumberWidth = 92.0;

} // namespace

ScoreBoard::ScoreBoard(QWidget *parent) : QWidget(parent) {
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_NoSystemBackground);
    hide();

    m_onwards = new QPushButton(this);
    m_away = new QPushButton(this);
    m_onwards->setCursor(Qt::PointingHandCursor);
    m_away->setCursor(Qt::PointingHandCursor);
    connect(m_onwards, &QPushButton::clicked, this, &ScoreBoard::goOn);
    connect(m_away, &QPushButton::clicked, this, &ScoreBoard::leave);
}

void ScoreBoard::showHand(const Rami *game) {
    if (!game) return;
    m_matchOver = false;
    m_lines.clear();

    const int out = game->outSeat();
    for (int at = 0; at < game->seatCount(); ++at) {
        const Seat &seat = game->seat(at);
        m_lines.append({seat.name, seat.hue, seat.lastHand, seat.score, at == out, seat.human(),
                        false});
    }
    // Celui qui mène, c'est-à-dire celui qui a le moins.
    int best = 0;
    for (int i = 1; i < m_lines.size(); ++i) {
        if (m_lines.at(i).total < m_lines.at(best).total) best = i;
    }
    if (!m_lines.isEmpty()) m_lines[best].winner = true;

    if (out >= 0) {
        m_headline = game->seat(out).human() ? tr("Vous sortez")
                                             : tr("%1 sort").arg(game->seat(out).name);
    } else {
        m_headline = tr("Manche nulle");
    }
    const int target = game->targetPoints();
    m_aside = target > 0 ? tr("Manche %1 — la partie se joue en %2 points")
                               .arg(game->handNumber())
                               .arg(target)
                         : tr("Manche %1").arg(game->handNumber());
    m_youWon = false;

    m_onwards->setText(tr("Manche suivante"));
    m_away->setText(tr("Quitter la table"));
    dressButtons();
    show();
    raise();
    setFocus();
    placeButtons();
    update();
}

void ScoreBoard::showMatch(const Rami *game) {
    if (!game) return;
    showHand(game);
    m_matchOver = true;

    const int winner = game->matchWinner();
    for (int i = 0; i < m_lines.size(); ++i) m_lines[i].winner = (i == winner);
    m_youWon = winner >= 0 && game->seat(winner).human();

    m_headline = m_youWon ? tr("Partie gagnée") : tr("%1 gagne la partie").arg(m_lines.isEmpty() ? QString() : m_lines.at(std::max(0, winner)).name);
    m_aside = tr("%1 manches — le moins de points l'emporte").arg(game->handNumber());

    m_onwards->setText(tr("Rejouer"));
    m_away->setText(tr("Quitter la table"));
    dressButtons();
    placeButtons();
    update();
}

void ScoreBoard::hideBoard() { hide(); }

void ScoreBoard::refresh() {
    dressButtons();
    update();
}

void ScoreBoard::dressButtons() {
    const Palette &scheme = currentPalette();
    const QString common = QStringLiteral("QPushButton { border-radius: 7px; padding: 7px 20px 9px 20px;"
                                          " font-weight: bold; border: 1px solid %1; }");
    m_onwards->setStyleSheet(common.arg(scheme.accent.name()) +
                             QStringLiteral("QPushButton { background: %1; color: %2; }"
                                            "QPushButton:hover { background: %3; }")
                                 .arg(scheme.accent.name(), scheme.accentText.name(),
                                      blendColours(scheme.accent, QColor(Qt::white), 0.16).name()));
    m_away->setStyleSheet(common.arg(scheme.edge.name()) +
                          QStringLiteral("QPushButton { background: %1; color: %2; }"
                                         "QPushButton:hover { border: 1px solid %3; }")
                              .arg(scheme.surface.name(), scheme.surfaceText.name(),
                                   scheme.accent.name()));
}

QSizeF ScoreBoard::measure() const {
    const qreal rows = kRowHeight * m_lines.size();
    const qreal width = kPad * 2 + kNameWidth + kNumberWidth * 2;
    const qreal height = kPad * 2 + 46.0 + 22.0 + rows + 26.0 + 46.0;
    return QSizeF(width, height);
}

void ScoreBoard::placeButtons() {
    const QSizeF card = measure();
    QRectF box(0, 0, card.width(), card.height());
    box.moveCenter(QPointF(width() / 2.0, height() / 2.0));

    const qreal buttonTop = box.bottom() - kPad - 36.0;
    m_onwards->adjustSize();
    m_away->adjustSize();
    m_onwards->move(int(box.right() - kPad - m_onwards->width()), int(buttonTop));
    m_away->move(int(box.left() + kPad), int(buttonTop));
}

void ScoreBoard::resizeEvent(QResizeEvent *) { placeButtons(); }

void ScoreBoard::keyPressEvent(QKeyEvent *event) {
    switch (event->key()) {
    case Qt::Key_Return:
    case Qt::Key_Enter:
    case Qt::Key_Space:
        Q_EMIT goOn();
        return;
    case Qt::Key_Escape:
        Q_EMIT leave();
        return;
    default:
        break;
    }
    QWidget::keyPressEvent(event);
}

void ScoreBoard::paintEvent(QPaintEvent *) {
    const Palette &scheme = currentPalette();
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    QColor veil = scheme.feltEdge;
    veil.setAlpha(206);
    painter.fillRect(rect(), veil);

    const QSizeF size = measure();
    QRectF box(0, 0, size.width(), size.height());
    box.moveCenter(QPointF(width() / 2.0, height() / 2.0));

    QPainterPath shape;
    shape.addRoundedRect(box, 16, 16);
    painter.setPen(Qt::NoPen);
    painter.fillPath(shape, scheme.surface);
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(m_matchOver && m_youWon ? scheme.gold : scheme.accent, 2.2));
    painter.drawPath(shape);

    const QRectF inner = box.adjusted(kPad, kPad, -kPad, -kPad);

    QFont headline = plainFont(&painter);
    headline.setBold(true);
    headline.setPixelSize(m_matchOver ? 26 : 21);
    painter.setFont(headline);
    painter.setPen(m_matchOver && m_youWon ? scheme.gold : scheme.accent);
    const QFontMetricsF headMetrics(headline);
    painter.drawText(QRectF(inner.left(), inner.top(), inner.width(), headMetrics.height()),
                     Qt::AlignCenter, m_headline);
    qreal y = inner.top() + headMetrics.height() + 3.0;

    QFont aside = plainFont(&painter);
    aside.setBold(false);
    aside.setPixelSize(12);
    painter.setFont(aside);
    painter.setPen(scheme.hint);
    const QFontMetricsF asideMetrics(aside);
    painter.drawText(QRectF(inner.left(), y, inner.width(), asideMetrics.height()),
                     Qt::AlignCenter, m_aside);
    y += asideMetrics.height() + 14.0;

    // L'en-tête des colonnes.
    QFont heading = plainFont(&painter);
    heading.setBold(true);
    heading.setPixelSize(10);
    heading.setCapitalization(QFont::AllUppercase);
    heading.setLetterSpacing(QFont::PercentageSpacing, 120);
    painter.setFont(heading);
    painter.setPen(scheme.hint);
    painter.drawText(QRectF(inner.left() + kNameWidth, y, kNumberWidth, 14.0),
                     Qt::AlignRight | Qt::AlignVCenter, tr("manche"));
    painter.drawText(QRectF(inner.left() + kNameWidth + kNumberWidth, y, kNumberWidth, 14.0),
                     Qt::AlignRight | Qt::AlignVCenter, tr("total"));
    y += 16.0;
    painter.setPen(QPen(scheme.edge, 1.0));
    painter.drawLine(QPointF(inner.left(), y), QPointF(inner.right(), y));
    y += 3.0;

    QFont row = plainFont(&painter);
    row.setBold(false);
    row.setCapitalization(QFont::MixedCase);
    row.setLetterSpacing(QFont::PercentageSpacing, 100);
    row.setPixelSize(14);
    QFont rowBold = row;
    rowBold.setBold(true);

    for (const Line &line : m_lines) {
        const QRectF band(inner.left(), y, inner.width(), kRowHeight);
        if (line.winner) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(blendColours(scheme.surface, scheme.accent, 0.14));
            painter.drawRoundedRect(band.adjusted(-6, 1, 6, -1), 6, 6);
        }

        // Le jeton de couleur, puis le nom.
        painter.setPen(Qt::NoPen);
        painter.setBrush(rivalColour(line.hue));
        painter.drawEllipse(QRectF(band.left(), band.center().y() - 4.5, 9.0, 9.0));

        painter.setFont(line.human ? rowBold : row);
        painter.setPen(scheme.surfaceText);
        QString name = line.name;
        if (line.wentOut) name += tr("  — sort");
        painter.drawText(QRectF(band.left() + 16.0, band.top(), kNameWidth - 16.0, kRowHeight),
                         Qt::AlignLeft | Qt::AlignVCenter, name);

        painter.setFont(row);
        painter.setPen(line.handPoints == 0 ? scheme.accent : scheme.surfaceText);
        painter.drawText(QRectF(band.left() + kNameWidth, band.top(), kNumberWidth, kRowHeight),
                         Qt::AlignRight | Qt::AlignVCenter, QString::number(line.handPoints));

        painter.setFont(rowBold);
        painter.setPen(line.winner ? scheme.accent : scheme.surfaceText);
        painter.drawText(
            QRectF(band.left() + kNameWidth + kNumberWidth, band.top(), kNumberWidth, kRowHeight),
            Qt::AlignRight | Qt::AlignVCenter, QString::number(line.total));
        y += kRowHeight;
    }
}
