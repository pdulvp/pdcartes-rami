#include "TitlePage.h"

#include "FeltArt.h"
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
// Période du fondu de la ligne d'invite, exprimée en images : un peu moins de
// deux secondes par cycle complet. Cette durée produit une variation
// progressive et non un clignotement.
constexpr int kBreathFrames = 108;

// Proportion de la fenêtre occupée par le logo, et sa position.
constexpr qreal kMarkHeightShare = 0.26;
constexpr qreal kMarkMiddle = 0.42;
constexpr qreal kHintMiddle = 0.84;

// Distance dont une carte dépasse le cadre avant de réapparaître du côté
// opposé, exprimée en proportion de la fenêtre. Elle est suffisante pour
// qu'une carte inclinée soit entièrement sortie.
constexpr qreal kOffstage = 0.26;

// Police de la ligne d'invite : celle du texte courant.
//
// Une police de labeur est employée et non une police d'affichage : cette
// ligne est lue de façon périphérique pendant que le regard porte sur le
// logo, et les deux polices d'affichage, conçues pour être regardées,
// seraient peu lisibles dans cet emploi. Elle est demandée par son nom plutôt
// que reprise de la fenêtre : il s'agit de la même police, mais l'écran titre
// ne dépend ainsi pas des réglages de la fenêtre.
QFont hintFont(const QFont &fallback, const QRectF &area) {
    QFont font = familyFont(fallback, proseFamily(),
                            std::min(area.height() * 0.028, area.width() * 0.020));
    font.setPixelSize(std::max(11, font.pixelSize()));
    // Le gras correspond à une graisse réellement embarquée : demandé
    // explicitement, Qt emploie la fonte correspondante au lieu d'épaissir le
    // tracé du romain.
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

// Carte tracée au trait.
//
// Seuls sont dessinés le contour, le rang dans deux coins et le symbole de
// couleur au centre, en doré et sans aplat. Cette représentation suffit à
// identifier une carte à distance, alors qu'une figure peinte serait trop
// visible derrière un titre.
//
// Elle est dessinée ici et non par `drawCardFace` : cette dernière rend la
// carte du jeu installé, qui est une image et ne peut être réduite à un
// tracé.
void drawWireCard(QPainter *painter, const QRectF &box, const Card &card, bool back,
                  const QColor &ink) {
    const qreal corner = cardCornerFor(box);
    const qreal line = std::max(1.0, box.height() * 0.012);

    painter->setBrush(Qt::NoBrush);
    painter->setPen(QPen(ink, line));
    painter->drawRoundedRect(box, corner, corner);

    if (back) {
        // Le dos : un treillis tracé à l'intérieur de la carte et découpé par
        // son contour.
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

    // Le coin : le rang, surmontant le symbole de couleur. Répété deux fois,
    // tête-bêche, comme sur une carte imprimée.
    const qreal indexHeight = box.height() * 0.17;
    QFont index = painter->font();
    index.setPixelSize(std::max(5, int(std::lround(indexHeight))));
    index.setBold(true);
    painter->setFont(index);

    const QString label = card.joker() ? QStringLiteral("JOKER") : rankLabel(card.rank);
    const qreal margin = box.width() * 0.09;

    if (card.joker()) {
        // Le joker n'a pas de couleur : son nom est inscrit en diagonale et
        // une étoile figure au centre.
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

    // Le symbole de couleur est placé au centre, à une taille supérieure,
    // afin que la carte reste identifiable à distance.
    const qreal pip = box.height() * 0.30;
    QRectF middle(0, 0, pip, pip);
    middle.moveCenter(box.center());
    drawSuitPip(painter, middle, card.suit, ink);
}

Card faceCard(int rank, Suit suit) {
    Card card;
    // Un identifiant arbitraire mais valide : aucun décompte n'est effectué
    // ici, mais une carte sans identifiant ne peut être dessinée.
    card.id = rank * 4 + int(suit);
    card.rank = rank;
    card.suit = suit;
    return card;
}

} // namespace

TitlePage::TitlePage(QWidget *parent) : QWidget(parent) {
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::PointingHandCursor);
    // Le tapis couvre toute la surface : aucun effacement préalable n'est
    // nécessaire.
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

// Positions initiales des cartes, définies explicitement et non tirées au
// hasard.
//
// Un écran titre est affiché longtemps et sa composition est fixe : la
// position du logo est connue, et un tirage aléatoire finirait par superposer
// plusieurs cartes à cet endroit tout en laissant une zone vide. Les cartes
// sont donc réparties sur le pourtour et se déplacent chacune à sa propre
// vitesse.
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
    // Une carte traverse l'écran en une minute environ : le déplacement est
    // perceptible sans attirer l'attention.
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
        // La carte sortie d'un côté réapparaît du côté opposé, sa coordonnée
        // sur l'autre axe étant conservée : elle repasse donc à un autre
        // endroit et non selon un cycle identique.
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

// Les deux couches mises en cache. Celle du logo est transparente, le logo
// étant dessiné par-dessus les cartes et non en dessous.
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

    // Les cartes, dessinées sur le tapis et sous le logo.
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

    // Ligne d'invite en fondu. La variation suit une sinusoïde et non un
    // créneau : une alternance brusque serait perçue comme une alerte.
    const qreal breath =
        0.5 - 0.5 * std::cos(2.0 * M_PI * (m_frame % kBreathFrames) / kBreathFrames);
    QColor ink = scheme.feltText;
    ink.setAlphaF(0.34 + 0.66 * breath);

    const QRectF area(rect());
    const QRectF band = hintBand(font(), area);
    const QString hint = tr("🂡  Cliquer pour commencer  🂡");
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.setFont(hintFont(font(), area));
    // Son ombre portée également : une carte peut passer sous cette ligne
    // comme sous le logo, et un texte clair sur un tracé clair deviendrait
    // illisible.
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
    // Toute touche déclenche le démarrage. Les modificateurs seuls sont
    // toutefois exclus, de même que les combinaisons en comportant un,
    // celles-ci étant traitées par la fenêtre : Ctrl+Q doit continuer de
    // quitter l'application.
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
