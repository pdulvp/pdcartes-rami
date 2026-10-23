#include "TitlePage.h"

#include "CardArt.h"
#include "Fonts.h"
#include "Palette.h"
#include "TitleMark.h"

#include <QFontMetricsF>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QTimer>

#include <algorithm>
#include <cmath>

namespace {

constexpr int kFrameMs = 16;
// La respiration, en images : un peu moins de deux secondes pour aller et
// revenir. Assez lent pour qu'on la voie respirer et non clignoter.
constexpr int kBreathFrames = 108;

// La place que le logo s'accorde dans la fenêtre, et où il se tient.
constexpr qreal kMarkHeightShare = 0.26;
constexpr qreal kMarkMiddle = 0.42;
constexpr qreal kHintMiddle = 0.84;

// De combien une carte sort du cadre avant de revenir de l'autre côté, en
// part de la fenêtre. Assez pour qu'une carte de biais ait fini de sortir.
constexpr qreal kOffstage = 0.26;

// La ligne qui respire, en Zen Kaku Gothic New.
//
// Une fonte de texte et non une fonte d'affichage: elle se lit de travers, du
// coin de l'oeil, pendant qu'on regarde l'enseigne -- et les deux fontes du
// logo, qui sont faites pour être regardées, s'y brouilleraient. Du même
// atelier que les deux autres, ce qui fait tenir les trois ensemble.
QFont hintFont(const QFont &fallback, const QRectF &area) {
    QFont font = familyFont(fallback, proseFamily(),
                            std::min(area.height() * 0.028, area.width() * 0.020));
    font.setPixelSize(std::max(11, font.pixelSize()));
    // Le gras est la graisse qu'on a embarquée: demandée, Qt prend la vraie
    // et n'en fabrique pas une en épaississant le trait.
    font.setBold(true);
    font.setCapitalization(QFont::AllUppercase);
    font.setLetterSpacing(QFont::PercentageSpacing, 100);
    return font;
}

QRectF hintBand(const QFont &fallback, const QRectF &area) {
    const QFontMetricsF metrics(hintFont(fallback, area));
    const qreal height = metrics.height() * 1.6;
    return QRectF(area.left(), area.top() + area.height() * kHintMiddle - height / 2.0,
                  area.width(), height);
}

// Une carte au trait.
//
// Le contour, le rang dans deux coins et l'enseigne au milieu, tout en or et
// rien de rempli. C'est ce qui reste d'une carte quand on lui retire la
// couleur, et c'est assez pour qu'on la reconnaisse de loin -- là où une
// figure peinte, derrière un titre, n'est qu'une tache.
//
// Dessinée ici et non par `drawCardFace`: celle-là rend la carte du jeu
// installé, qui est une image, et une image ne se réduit pas à un trait.
void drawWireCard(QPainter *painter, const QRectF &box, const Card &card, bool back,
                  const QColor &ink) {
    const qreal corner = cardCornerFor(box);
    const qreal line = std::max(1.0, box.height() * 0.012);

    painter->setBrush(Qt::NoBrush);
    painter->setPen(QPen(ink, line));
    painter->drawRoundedRect(box, corner, corner);

    if (back) {
        // Le dos : un treillis, tracé dans la carte et coupé par elle.
        QPainterPath shape;
        shape.addRoundedRect(box.adjusted(line * 2, line * 2, -line * 2, -line * 2), corner * 0.8,
                             corner * 0.8);
        painter->save();
        painter->setClipPath(shape);
        QColor faint = ink;
        faint.setAlphaF(ink.alphaF() * 0.75);
        painter->setPen(QPen(faint, line * 0.7));
        const qreal step = box.height() * 0.11;
        for (qreal x = box.left() - box.height(); x < box.right() + box.height(); x += step) {
            painter->drawLine(QPointF(x, box.top()), QPointF(x + box.height(), box.bottom()));
            painter->drawLine(QPointF(x, box.bottom()), QPointF(x + box.height(), box.top()));
        }
        painter->restore();
        painter->setPen(QPen(ink, line * 0.8));
        painter->drawPath(shape);
        return;
    }

    // Le coin : le rang, et l'enseigne sous lui. Deux fois, tête-bêche, comme
    // une vraie carte est imprimée.
    const qreal indexHeight = box.height() * 0.17;
    QFont index = painter->font();
    index.setPixelSize(std::max(5, int(std::lround(indexHeight))));
    index.setBold(true);
    painter->setFont(index);

    const QString label = card.joker() ? QStringLiteral("JOKER") : rankLabel(card.rank);
    const qreal margin = box.width() * 0.09;

    if (card.joker()) {
        // Le joker n'a pas d'enseigne : son nom en travers, et une étoile au
        // milieu.
        painter->save();
        painter->translate(box.center());
        painter->rotate(-90.0);
        QFont name = index;
        name.setPixelSize(std::max(5, int(std::lround(box.height() * 0.12))));
        name.setLetterSpacing(QFont::PercentageSpacing, 100);
        painter->setFont(name);
        painter->setPen(ink);
        painter->drawText(QRectF(-box.height() / 2.0, -box.width() / 2.0, box.height(),
                                 box.width()),
                          Qt::AlignCenter, label);
        painter->restore();
        return;
    }

    painter->setPen(ink);
    const QRectF cornerBox(box.left() + margin, box.top() + margin, box.width() * 0.34,
                           indexHeight);
    painter->drawText(cornerBox, Qt::AlignLeft | Qt::AlignVCenter, label);
    drawSuitPip(painter,
                QRectF(cornerBox.left(), cornerBox.bottom() + indexHeight * 0.12,
                       indexHeight * 0.62, indexHeight * 0.62),
                card.suit, ink);

    painter->save();
    painter->translate(box.center());
    painter->rotate(180.0);
    painter->translate(-box.center());
    painter->drawText(cornerBox, Qt::AlignLeft | Qt::AlignVCenter, label);
    drawSuitPip(painter,
                QRectF(cornerBox.left(), cornerBox.bottom() + indexHeight * 0.12,
                       indexHeight * 0.62, indexHeight * 0.62),
                card.suit, ink);
    painter->restore();

    // L'enseigne au milieu, plus grande, pour que la carte se lise de loin.
    const qreal pip = box.height() * 0.30;
    QRectF middle(0, 0, pip, pip);
    middle.moveCenter(box.center());
    drawSuitPip(painter, middle, card.suit, ink);
}

Card faceCard(int rank, Suit suit) {
    Card card;
    // Un identifiant quelconque, mais valide : rien ici ne compte les cartes,
    // mais une carte sans identité ne se dessine pas.
    card.id = rank * 4 + int(suit);
    card.rank = rank;
    card.suit = suit;
    return card;
}

} // namespace

TitlePage::TitlePage(QWidget *parent) : QWidget(parent) {
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::PointingHandCursor);
    // Le tapis couvre tout : rien à effacer avant de le peindre.
    setAttribute(Qt::WA_OpaquePaintEvent);

    dealDrifters();

    m_beat = new QTimer(this);
    m_beat->setInterval(kFrameMs);
    connect(m_beat, &QTimer::timeout, this, [this] {
        ++m_frame;
        driftOn();
        update();
    });
}

// Les cartes qui passent, posées à la main et non tirées au hasard.
//
// Un écran titre se regarde longtemps et toujours au même endroit : on sait où
// le logo tombe, et des cartes tirées au sort finissent par en empiler trois
// dessus et laisser un coin vide. Celles-ci partent étalées sur le pourtour,
// dérivent chacune à son pas, et repassent.
void TitlePage::dealDrifters() {
    struct Seed {
        bool back;
        int rank;
        Suit suit;
        qreal x, y;      // d'où elle part
        qreal dx, dy;    // où elle va, en parts de fenêtre par seconde
        qreal height;
        qreal angle;
        qreal spin;      // degrés par seconde
        qreal ink;       // ce qu'elle a d'or
    };
    // Une carte traverse l'écran en une bonne minute: on la voit bouger sans
    // la regarder bouger.
    static const Seed kSeeds[] = {
        {false, kJoker, Suit::Coeur,    0.11, 0.20,  0.016, -0.007, 0.15, -14.0,  0.8, 0.85},
        {false, kRoi,   Suit::Coeur,    0.88, 0.16, -0.013,  0.010, 0.14,  11.0, -0.6, 0.75},
        {false, kAs,    Suit::Pique,    0.21, 0.83,  0.011,  0.012, 0.13,   8.0,  1.0, 0.80},
        {false, kDame,  Suit::Carreau,  0.79, 0.78, -0.015, -0.009, 0.13, -19.0, -0.9, 0.70},
        {false, kValet, Suit::Trefle,   0.06, 0.58,  0.014,  0.008, 0.12,  23.0,  0.7, 0.62},
        {false, 10,     Suit::Coeur,    0.94, 0.45, -0.010, -0.012, 0.11, -27.0,  1.1, 0.58},
        {true,  kJoker, Suit::Pique,    0.37, 0.07,  0.008,  0.013, 0.10,  16.0, -0.5, 0.48},
        {true,  kJoker, Suit::Pique,    0.63, 0.93, -0.009, -0.011, 0.10, -22.0,  0.6, 0.48},
        {false, 7,      Suit::Pique,    0.50, 0.63,  0.012, -0.010, 0.09,  31.0, -0.8, 0.40},
    };

    const qreal perFrame = kFrameMs / 1000.0;
    m_drifters.clear();
    for (const Seed &seed : kSeeds) {
        Drifter card;
        card.back = seed.back;
        card.card = faceCard(seed.rank, seed.suit);
        card.at = QPointF(seed.x, seed.y);
        card.speed = QPointF(seed.dx * perFrame, seed.dy * perFrame);
        card.height = seed.height;
        card.angle = seed.angle;
        card.spin = seed.spin * perFrame;
        card.ink = seed.ink;
        m_drifters.append(card);
    }
}

void TitlePage::driftOn() {
    for (Drifter &card : m_drifters) {
        card.at += card.speed;
        card.angle += card.spin;
        // Sortie d'un côté, rentrée de l'autre, en gardant l'autre axe: la
        // carte repasse plus tard et ailleurs, et non en boucle sur ses pas.
        if (card.at.x() < -kOffstage) card.at.setX(1.0 + kOffstage);
        if (card.at.x() > 1.0 + kOffstage) card.at.setX(-kOffstage);
        if (card.at.y() < -kOffstage) card.at.setY(1.0 + kOffstage);
        if (card.at.y() > 1.0 + kOffstage) card.at.setY(-kOffstage);
    }
}

void TitlePage::refresh() {
    rebuildFelt();
    rebuildMark();
    update();
}

QRectF TitlePage::markRect() const {
    return titleMarkBox(QRectF(rect()), font(), kMarkHeightShare, kMarkMiddle);
}

void TitlePage::setMarkHidden(bool hidden) {
    if (m_markHidden == hidden) return;
    m_markHidden = hidden;
    update();
}

// Les deux couches en cache. Transparente pour le logo, qui se pose sur les
// cartes et non sous elles.
void TitlePage::rebuildFelt() {
    if (width() <= 0 || height() <= 0) {
        m_felt = QPixmap();
        return;
    }
    const qreal ratio = devicePixelRatioF();
    m_felt = QPixmap(QSize(int(std::ceil(width() * ratio)), int(std::ceil(height() * ratio))));
    m_felt.setDevicePixelRatio(ratio);

    QPainter painter(&m_felt);
    painter.setRenderHint(QPainter::Antialiasing, true);
    drawFelt(&painter, QRectF(0, 0, width(), height()), currentPalette());
}

void TitlePage::rebuildMark() {
    if (width() <= 0 || height() <= 0) {
        m_mark = QPixmap();
        return;
    }
    const qreal ratio = devicePixelRatioF();
    m_mark = QPixmap(QSize(int(std::ceil(width() * ratio)), int(std::ceil(height() * ratio))));
    m_mark.setDevicePixelRatio(ratio);
    m_mark.fill(Qt::transparent);

    QPainter painter(&m_mark);
    painter.setRenderHint(QPainter::Antialiasing, true);
    drawTitleMark(&painter, markRect(), font(), currentPalette());
}

void TitlePage::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);
    if (m_felt.isNull()) rebuildFelt();
    if (m_mark.isNull()) rebuildMark();

    const Palette &scheme = currentPalette();
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.drawPixmap(0, 0, m_felt);

    // Les cartes, sur le tapis et sous le logo.
    for (const Drifter &card : m_drifters) {
        const QSizeF box = cardSizeForHeight(height() * card.height);
        QColor ink = scheme.gold;
        ink.setAlphaF(0.52 * card.ink);
        painter.save();
        painter.translate(card.at.x() * width(), card.at.y() * height());
        painter.rotate(card.angle);
        drawWireCard(&painter,
                     QRectF(-box.width() / 2.0, -box.height() / 2.0, box.width(), box.height()),
                     card.card, card.back, ink);
        painter.restore();
    }

    if (!m_markHidden) painter.drawPixmap(0, 0, m_mark);

    // La ligne qui respire. Une sinusoïde et non un clignotement : ce qui
    // s'allume et s'éteint d'un coup est une alarme.
    const qreal breath =
        0.5 - 0.5 * std::cos(2.0 * M_PI * (m_frame % kBreathFrames) / kBreathFrames);
    QColor ink = scheme.feltText;
    ink.setAlphaF(0.34 + 0.66 * breath);

    const QRectF area(rect());
    const QRectF band = hintBand(font(), area);
    const QString hint = tr("🂡  Cliquer pour commencer  🂡");
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.setFont(hintFont(font(), area));
    // Son ombre aussi : une carte passe là-dessous comme elle passe sous le
    // logo, et une ligne pâle sur un trait pâle ne se lit plus.
    QColor shade = scheme.feltEdge;
    shade.setAlphaF(0.55 * ink.alphaF());
    painter.setPen(shade);
    painter.drawText(band.translated(1.0, 2.0), Qt::AlignCenter, hint);
    painter.setPen(ink);
    painter.drawText(band, Qt::AlignCenter, hint);
}

void TitlePage::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    rebuildFelt();
    rebuildMark();
}

void TitlePage::mousePressEvent(QMouseEvent *event) {
    Q_UNUSED(event);
    Q_EMIT start();
}

void TitlePage::keyPressEvent(QKeyEvent *event) {
    // Une touche quelconque commence la partie -- un écran titre ne se
    // cherche pas. Mais les modificateurs seuls ne sont pas une touche, et
    // ce qui en porte un appartient à la fenêtre : Ctrl+Q doit encore
    // quitter.
    switch (event->key()) {
    case Qt::Key_Shift:
    case Qt::Key_Control:
    case Qt::Key_Alt:
    case Qt::Key_Meta:
    case Qt::Key_AltGr:
    case Qt::Key_CapsLock:
    case Qt::Key_NumLock:
        QWidget::keyPressEvent(event);
        return;
    default:
        break;
    }
    if (event->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier)) {
        QWidget::keyPressEvent(event);
        return;
    }
    Q_EMIT start();
}

void TitlePage::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
    m_beat->start();
    setFocus();
}

void TitlePage::hideEvent(QHideEvent *event) {
    QWidget::hideEvent(event);
    m_beat->stop();
}
