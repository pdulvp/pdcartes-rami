#include "CardArt.h"

#include "CardDeck.h"

#include <QCoreApplication>
#include <QFontMetricsF>
#include <QHash>
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>

#include <algorithm>
#include <cmath>

namespace {

// How round a card's corners are, as a fraction of its width.
// L'arrondi du coin d'une carte, en part de sa largeur.
//
// Petit, et pour une raison qui ne se voit qu'avec un jeu installé: une carte
// venue d'un fichier porte déjà son propre coin -- les CARDS.DLL de Windows
// ont un filet noir qui rentre de deux pixels sur soixante et onze, soit
// moins de trois centièmes. Découper par-dessus avec un arrondi plus large,
// c'est scier au travers de ce filet et laisser un moignon à chaque coin.
//
// Ce chiffre-là passe donc partout: il suit le filet des cartes de Windows,
// il ne mord pas dans un jeu dont les coins sont déjà transparents, il
// adoucit un jeu aux coins carrés -- et les cartes que le programme dessine
// le portent aussi, pour qu'un jeu installé et le jeu dessiné aient la même
// coupe.
constexpr qreal kCornerPart = 0.035;

// Les points du tapis.
//
// A weave is made of threads lying across one another, and the way to say
// that in two colours is one family of lines a shade lighter than the cloth
// and one a shade darker, half a pitch apart. Eight pixels square, so that
// the pattern tiles without a seam and the lines land on whole pixels.
struct Weave {
    QPixmap tile;
    int hue = -1;
    bool dark = false;
    bool made = false;
};

// Handed out by a function rather than kept as a variable at file scope: a
// QPixmap wants a QGuiApplication to exist before it is made, and a variable
// at file scope is made before main() has got as far as making one. A static
// inside a function is made the first time the function is called, which is
// long afterwards.
Weave &weave() {
    static Weave kept;
    return kept;
}

// Les dos dessinés, gardés en images.
//
// Le treillis est une cinquantaine de diagonales rognées à un coin arrondi,
// et un dos n'est jamais seul à l'écran: il y en a un sur la pioche, un sur
// la défausse et un par tas de cartes en face. Redessiné à chaque repaint il
// coûte plus cher qu'une figure -- deux fois plus, mesuré -- alors comme le
// treillis du tapis il est fait une fois par taille puis recopié.
//
// Rangés par taille et par état, et jetés en bloc quand le jeu de couleurs
// change, puisque c'est lui qui les colore.
struct Backs {
    QHash<quint64, QPixmap> tiles;
    int hue = -1;
    bool dark = false;
};

Backs &backs() {
    static Backs kept;
    return kept;
}

// Le corps d'un dos dessiné: tout sauf l'ombre dessous et le halo dessus,
// qui dépendent du coup qu'on est en train de jouer et non de la carte.
const QPixmap &drawnBack(const QSizeF &wanted, const Palette &scheme, bool lit) {
    static const QPixmap nothing;
    const QSize size = wanted.toSize();
    if (size.width() < 3 || size.height() < 5) return nothing;

    Backs &kept = backs();
    if (kept.hue != scheme.hue || kept.dark != darkMode()) {
        kept.tiles.clear();
        kept.hue = scheme.hue;
        kept.dark = darkMode();
    }

    const quint64 key = (quint64(size.width()) << 32) | (quint64(size.height()) << 1) |
                        quint64(lit ? 1 : 0);
    const auto found = kept.tiles.constFind(key);
    if (found != kept.tiles.constEnd()) return *found;

    // Un repaint se sert de cinq ou six tailles; la molette du zoom en
    // apporte une nouvelle à chaque cran. Passé de quoi tenir largement une
    // table, on repart de zéro plutôt que de garder toutes celles qu'on a
    // traversées.
    if (kept.tiles.size() > 64) kept.tiles.clear();

    QPixmap tile(size);
    tile.fill(Qt::transparent);
    QPainter painter(&tile);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QRectF box(0.0, 0.0, size.width(), size.height());
    const qreal corner = cardCornerFor(box);
    QPainterPath shape;
    shape.addRoundedRect(box, corner, corner);

    QColor back = scheme.cardBack;
    if (lit) back = blendColours(back, QColor(Qt::white), 0.18);
    painter.setPen(Qt::NoPen);
    painter.fillPath(shape, back);

    // Le dos: a white margin and a lattice inside it, which is what the back
    // of a card has looked like since there were backs on cards.
    const QRectF panel = box.adjusted(box.width() * 0.085, box.width() * 0.085,
                                      -box.width() * 0.085, -box.width() * 0.085);
    painter.setPen(QPen(blendColours(back, QColor(Qt::white), 0.45), 1.0));
    painter.setBrush(Qt::NoBrush);
    if (panel.width() > 6 && panel.height() > 6) {
        painter.drawRoundedRect(panel, corner * 0.55, corner * 0.55);

        painter.save();
        QPainterPath clip;
        clip.addRoundedRect(panel, corner * 0.55, corner * 0.55);
        painter.setClipPath(clip);
        painter.setPen(QPen(scheme.cardBackInk, 1.0));
        const qreal pitch = std::max(4.0, box.width() * 0.17);
        for (qreal at = -panel.height(); at < panel.width() + panel.height(); at += pitch) {
            painter.drawLine(QPointF(panel.left() + at, panel.top()),
                             QPointF(panel.left() + at + panel.height(), panel.bottom()));
            painter.drawLine(QPointF(panel.left() + at, panel.bottom()),
                             QPointF(panel.left() + at + panel.height(), panel.top()));
        }
        painter.restore();
    }
    painter.end();

    return kept.tiles.insert(key, tile).value();
}

// Un symbole dessiné, pas écrit.
//
// The suits exist in every font, and in every font they are a different
// shape and a different weight -- and on a machine whose fonts are thin they
// are a box. Drawn, a pique is a pique.
QPainterPath pipPath(Suit suit) {
    QPainterPath path;
    switch (suit) {
    case Suit::Coeur: {
        // Two lobes and a point, in one curve each side.
        path.moveTo(0.0, 0.92);
        path.cubicTo(-0.86, 0.22, -0.60, -0.58, -0.26, -0.58);
        path.cubicTo(-0.06, -0.58, 0.0, -0.36, 0.0, -0.22);
        path.cubicTo(0.0, -0.36, 0.06, -0.58, 0.26, -0.58);
        path.cubicTo(0.60, -0.58, 0.86, 0.22, 0.0, 0.92);
        path.closeSubpath();
        break;
    }
    case Suit::Carreau: {
        path.moveTo(0.0, -0.95);
        path.cubicTo(0.26, -0.44, 0.52, -0.16, 0.66, 0.0);
        path.cubicTo(0.52, 0.16, 0.26, 0.44, 0.0, 0.95);
        path.cubicTo(-0.26, 0.44, -0.52, 0.16, -0.66, 0.0);
        path.cubicTo(-0.52, -0.16, -0.26, -0.44, 0.0, -0.95);
        path.closeSubpath();
        break;
    }
    case Suit::Pique: {
        // A heart turned over, with a stem under it.
        path.moveTo(0.0, -0.95);
        path.cubicTo(0.86, -0.18, 0.62, 0.52, 0.26, 0.52);
        path.cubicTo(0.10, 0.52, 0.04, 0.42, 0.04, 0.34);
        path.lineTo(0.22, 0.90);
        path.lineTo(-0.22, 0.90);
        path.lineTo(-0.04, 0.34);
        path.cubicTo(-0.04, 0.42, -0.10, 0.52, -0.26, 0.52);
        path.cubicTo(-0.62, 0.52, -0.86, -0.18, 0.0, -0.95);
        path.closeSubpath();
        break;
    }
    case Suit::Trefle: {
        // Three rounds and a stem.
        path.addEllipse(QRectF(-0.30, -0.95, 0.60, 0.60));
        path.addEllipse(QRectF(-0.84, -0.26, 0.62, 0.62));
        path.addEllipse(QRectF(0.22, -0.26, 0.62, 0.62));
        QPainterPath stem;
        stem.moveTo(-0.06, -0.06);
        stem.lineTo(0.06, -0.06);
        stem.lineTo(0.24, 0.92);
        stem.lineTo(-0.24, 0.92);
        stem.closeSubpath();
        path.addPath(stem);
        break;
    }
    }
    return path;
}

void paintPip(QPainter *painter, const QPointF &middle, qreal span, Suit suit,
              const QColor &ink) {
    painter->save();
    painter->translate(middle);
    painter->scale(span / 2.0, span / 2.0);
    painter->setPen(Qt::NoPen);
    painter->setBrush(ink);
    painter->drawPath(pipPath(suit));
    painter->restore();
}

// Les figures. Not portraits -- a portrait at forty pixels is a smudge --
// but a mark each, drawn in the suit's own colour: a crown for the roi, a
// coronet for the dame, a fleur de lis for the valet.
void paintCourtMark(QPainter *painter, const QRectF &box, int rank, const QColor &ink,
                    const QColor &paper) {
    const QPointF middle = box.center();
    const qreal span = std::min(box.width(), box.height());
    painter->save();
    painter->translate(middle);
    painter->scale(span / 2.0, span / 2.0);
    painter->setPen(QPen(ink, 0.08, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter->setBrush(ink);

    if (rank == kRoi) {
        // La couronne du roi : cinq pointes et une croix dessus, qui est ce
        // qui la distingue du diadème de la dame d'un coup d'œil.
        QPainterPath crown;
        crown.moveTo(-0.72, 0.34);
        crown.lineTo(-0.86, -0.46);
        crown.lineTo(-0.34, -0.06);
        crown.lineTo(0.0, -0.56);
        crown.lineTo(0.34, -0.06);
        crown.lineTo(0.86, -0.46);
        crown.lineTo(0.72, 0.34);
        crown.closeSubpath();
        painter->drawPath(crown);
        painter->drawRect(QRectF(-0.09, -0.92, 0.18, 0.34));
        painter->drawRect(QRectF(-0.24, -0.80, 0.48, 0.16));
        painter->setBrush(paper);
        painter->drawRect(QRectF(-0.72, 0.44, 1.44, 0.18));
        painter->setBrush(ink);
        painter->drawRect(QRectF(-0.78, 0.40, 1.56, 0.26));
    } else if (rank == kDame) {
        // Le diadème de la dame : trois pointes, plus basses et plus rondes
        // que la couronne, et une perle au bout de chacune.
        QPainterPath tiara;
        tiara.moveTo(-0.66, 0.34);
        tiara.quadTo(-0.62, -0.18, -0.44, -0.30);
        tiara.lineTo(-0.22, 0.04);
        tiara.quadTo(0.0, -0.34, 0.0, -0.44);
        tiara.quadTo(0.0, -0.34, 0.22, 0.04);
        tiara.lineTo(0.44, -0.30);
        tiara.quadTo(0.62, -0.18, 0.66, 0.34);
        tiara.closeSubpath();
        painter->drawPath(tiara);
        for (const qreal at : {-0.44, 0.0, 0.44}) {
            const qreal top = (at == 0.0) ? -0.56 : -0.42;
            painter->drawEllipse(QRectF(at - 0.13, top - 0.13, 0.26, 0.26));
        }
        painter->setBrush(paper);
        painter->drawRect(QRectF(-0.66, 0.42, 1.32, 0.16));
        painter->setBrush(ink);
        painter->drawRect(QRectF(-0.72, 0.38, 1.44, 0.24));
    } else {
        // Le valet : une fleur de lis.
        QPainterPath lis;
        lis.moveTo(0.0, -0.80);
        lis.cubicTo(0.18, -0.46, 0.22, -0.26, 0.14, -0.10);
        lis.cubicTo(0.46, -0.44, 0.84, -0.20, 0.52, 0.22);
        lis.cubicTo(0.40, 0.00, 0.22, 0.02, 0.14, 0.14);
        lis.lineTo(0.30, 0.74);
        lis.lineTo(-0.30, 0.74);
        lis.lineTo(-0.14, 0.14);
        lis.cubicTo(-0.22, 0.02, -0.40, 0.00, -0.52, 0.22);
        lis.cubicTo(-0.84, -0.20, -0.46, -0.44, -0.14, -0.10);
        lis.cubicTo(-0.22, -0.26, -0.18, -0.46, 0.0, -0.80);
        lis.closeSubpath();
        painter->drawPath(lis);
        painter->setBrush(paper);
        painter->drawRect(QRectF(-0.34, 0.30, 0.68, 0.10));
    }
    painter->restore();
}

// Le joker : un bonnet à grelots, et une étoile.
void paintJoker(QPainter *painter, const QRectF &box, const Palette &scheme) {
    const qreal span = std::min(box.width(), box.height());
    painter->save();
    painter->translate(box.center());
    painter->scale(span / 2.0, span / 2.0);

    const QColor cloth = scheme.accent;
    const QColor bell = scheme.gold;

    QPainterPath hat;
    hat.moveTo(-0.70, 0.18);
    hat.cubicTo(-0.60, -0.48, 0.60, -0.48, 0.70, 0.18);
    hat.closeSubpath();
    painter->setPen(Qt::NoPen);
    painter->setBrush(cloth);
    painter->drawPath(hat);

    // Les trois pointes, et un grelot au bout de chacune.
    const QPointF tips[3] = {QPointF(-0.92, -0.42), QPointF(0.0, -0.80), QPointF(0.92, -0.42)};
    const QPointF roots[3] = {QPointF(-0.52, -0.14), QPointF(0.0, -0.34), QPointF(0.52, -0.14)};
    for (int i = 0; i < 3; ++i) {
        QPainterPath horn;
        horn.moveTo(roots[i].x() - 0.16, roots[i].y() + 0.10);
        horn.quadTo((roots[i].x() + tips[i].x()) / 2.0 - 0.10, tips[i].y() + 0.10, tips[i].x(),
                    tips[i].y());
        horn.quadTo((roots[i].x() + tips[i].x()) / 2.0 + 0.10, tips[i].y() + 0.26,
                    roots[i].x() + 0.16, roots[i].y() + 0.10);
        horn.closeSubpath();
        painter->setBrush(cloth);
        painter->drawPath(horn);
        painter->setBrush(bell);
        painter->drawEllipse(QRectF(tips[i].x() - 0.15, tips[i].y() - 0.15, 0.30, 0.30));
    }

    painter->setBrush(bell);
    painter->drawRect(QRectF(-0.74, 0.14, 1.48, 0.20));

    // L'étoile sous le bonnet, qui est ce qu'un joker est sur le tapis.
    QPainterPath star;
    for (int i = 0; i < 10; ++i) {
        const qreal angle = -M_PI / 2.0 + i * M_PI / 5.0;
        const qreal reach = (i % 2 == 0) ? 0.44 : 0.19;
        const QPointF at(std::cos(angle) * reach, 0.66 + std::sin(angle) * reach);
        if (i == 0) {
            star.moveTo(at);
        } else {
            star.lineTo(at);
        }
    }
    star.closeSubpath();
    painter->setBrush(cloth);
    painter->drawPath(star);
    painter->restore();
}

// Où les symboles vont sur une carte, de deux à dix: the layout every pack
// has used for two hundred years, as fractions of the card's inner box.
const QVector<QPointF> &pipLayout(int rank) {
    static const QVector<QVector<QPointF>> kLayouts = {
        {},                                                               // 0
        {{0.50, 0.50}},                                                   // as
        {{0.50, 0.16}, {0.50, 0.84}},                                     // 2
        {{0.50, 0.16}, {0.50, 0.50}, {0.50, 0.84}},                       // 3
        {{0.28, 0.16}, {0.72, 0.16}, {0.28, 0.84}, {0.72, 0.84}},         // 4
        {{0.28, 0.16}, {0.72, 0.16}, {0.50, 0.50}, {0.28, 0.84}, {0.72, 0.84}},
        {{0.28, 0.16}, {0.72, 0.16}, {0.28, 0.50}, {0.72, 0.50}, {0.28, 0.84}, {0.72, 0.84}},
        {{0.28, 0.16}, {0.72, 0.16}, {0.50, 0.33}, {0.28, 0.50}, {0.72, 0.50}, {0.28, 0.84},
         {0.72, 0.84}},
        {{0.28, 0.16}, {0.72, 0.16}, {0.50, 0.33}, {0.28, 0.50}, {0.72, 0.50}, {0.50, 0.67},
         {0.28, 0.84}, {0.72, 0.84}},
        {{0.28, 0.14}, {0.72, 0.14}, {0.28, 0.38}, {0.72, 0.38}, {0.50, 0.50}, {0.28, 0.62},
         {0.72, 0.62}, {0.28, 0.86}, {0.72, 0.86}},
        {{0.28, 0.14}, {0.72, 0.14}, {0.50, 0.27}, {0.28, 0.38}, {0.72, 0.38}, {0.28, 0.62},
         {0.72, 0.62}, {0.50, 0.73}, {0.28, 0.86}, {0.72, 0.86}},
    };
    static const QVector<QPointF> none;
    if (rank < 1 || rank > 10) return none;
    return kLayouts.at(rank);
}

QFont indexFont(const QFont &base, qreal height, bool bold) {
    QFont font = base;
    font.setBold(bold);
    font.setPixelSize(std::max(7, int(std::round(height))));
    return font;
}

} // namespace

qreal cardCornerFor(const QRectF &box) {
    return std::max(2.0, std::min(box.width(), box.height()) * kCornerPart);
}

QFont plainFont(const QPainter *painter) {
    QFont font = painter ? painter->font() : QFont();
    font.setBold(false);
    font.setItalic(false);
    font.setCapitalization(QFont::MixedCase);
    font.setLetterSpacing(QFont::PercentageSpacing, 100);
    return font;
}

qreal cardAspect() {
    if (const CardSet *set = CardDecks::instance().current()) {
        const qreal theirs = set->aspect();
        // Bornée : une proportion aberrante casserait la disposition.
        if (theirs > 0.35 && theirs < 1.60) return theirs;
    }
    return kCardRatio;
}

QSizeF cardSizeForHeight(qreal height) {
    const qreal aspect = cardAspect();
    // La hauteur demandée est un budget, et c'est la largeur qui le dépense.
    //
    // Tout le tapis se dispose en hauteur -- la main, les combinaisons, la
    // pioche -- et laisse la largeur suivre la proportion du jeu choisi. Tant
    // que le jeu est celui que le programme dessine, cela revient au même ;
    // mais un jeu installé apporte la sienne, et les CARDS.DLL de Windows
    // sont nettement plus larges que hautes (71 sur 96, contre 0,642). À
    // hauteur égale, leurs cartes prenaient un sixième de place en plus : la
    // main de quatorze cartes passait de rangée aérée à rangée qui touche les
    // deux bords de la fenêtre.
    //
    // La carte tient donc dans la largeur qu'aurait la carte dessinée à cette
    // hauteur-là, et c'est sa hauteur qui cède quand le jeu est plus large.
    // Un jeu plus étroit que celui dessiné garde la hauteur demandée : il ne
    // gênait personne.
    if (aspect <= kCardRatio) return QSizeF(height * aspect, height);
    const qreal wide = height * kCardRatio;
    return QSizeF(wide, wide / aspect);
}
QSizeF cardSizeForWidth(qreal width) { return QSizeF(width, width / cardAspect()); }

const QPixmap &weaveTile(const Palette &scheme) {
    Weave &kept = weave();
    if (kept.made && kept.hue == scheme.hue && kept.dark == darkMode()) return kept.tile;

    constexpr int kPitch = 8;
    QPixmap tile(kPitch, kPitch);
    tile.fill(Qt::transparent);

    QPainter painter(&tile);
    QColor thread = scheme.feltWeave;
    thread.setAlpha(58);
    painter.setPen(QPen(thread, 1.0));
    // One family of threads across, one down, half a pitch apart: that is
    // what a cloth looks like close up, and from a yard away it is a texture
    // rather than a grid.
    painter.drawLine(0, 0, kPitch, 0);
    painter.drawLine(0, 4, 0, kPitch);
    QColor shadow = scheme.feltEdge;
    shadow.setAlpha(44);
    painter.setPen(QPen(shadow, 1.0));
    painter.drawLine(0, 1, kPitch, 1);
    painter.drawLine(1, 4, 1, kPitch);
    painter.end();

    kept.tile = tile;
    kept.hue = scheme.hue;
    kept.dark = darkMode();
    kept.made = true;
    return kept.tile;
}

void drawFelt(QPainter *painter, const QRectF &box, const Palette &scheme) {
    painter->save();
    painter->setPen(Qt::NoPen);

    // La lumière au-dessus de la table: brighter where the lamp is, which is
    // over the middle. It is the one thing that makes a flat fill of green
    // look like cloth with a room around it.
    QRadialGradient light(box.center() - QPointF(0, box.height() * 0.10),
                          std::max(box.width(), box.height()) * 0.78);
    light.setColorAt(0.0, scheme.felt);
    light.setColorAt(0.62, blendColours(scheme.felt, scheme.feltEdge, 0.45));
    light.setColorAt(1.0, scheme.feltEdge);
    painter->setBrush(light);
    painter->drawRect(box);

    painter->setBrush(QBrush(weaveTile(scheme)));
    painter->drawRect(box);

    // La bordure: a braid a little in from the edge, and a darker line
    // outside it, so the cloth reads as cloth stretched over a rail.
    const qreal inset = std::max(5.0, std::min(box.width(), box.height()) * 0.012);
    QRectF braid = box.adjusted(inset, inset, -inset, -inset);
    painter->setBrush(Qt::NoBrush);
    QColor trim = scheme.trim;
    trim.setAlpha(150);
    painter->setPen(QPen(trim, 2.0));
    painter->drawRoundedRect(braid, 10, 10);
    trim.setAlpha(60);
    painter->setPen(QPen(trim, 1.0));
    painter->drawRoundedRect(braid.adjusted(-3, -3, 3, 3), 12, 12);
    painter->restore();
}

// Ce que le programme dessine lui-même sur une face. Mis à part pour que
// l'ombre, le liseré et le halo servent aussi aux cartes d'un jeu installé.
namespace {

void paintDrawnFace(QPainter *painter, const QRectF &box, const Card &card,
                    const Palette &scheme, const QColor &paper, const QColor &faded,
                    const CardLook &look) {
    Q_UNUSED(look);
    const QRectF inner = box.adjusted(box.width() * 0.09, box.height() * 0.07,
                                      -box.width() * 0.09, -box.height() * 0.07);

    if (card.joker()) {
        paintJoker(painter, inner.adjusted(0, inner.height() * 0.08, 0, -inner.height() * 0.08),
                   scheme);
        if (box.width() >= 42) {
            QFont name = indexFont(plainFont(painter), box.height() * 0.095, true);
            name.setCapitalization(QFont::AllUppercase);
            name.setLetterSpacing(QFont::PercentageSpacing, 130);
            painter->setFont(name);
            painter->setPen(scheme.accent);
            painter->drawText(QRectF(box.left(), box.bottom() - box.height() * 0.17,
                                     box.width(), box.height() * 0.13),
                              Qt::AlignCenter, QCoreApplication::translate("cardart", "Joker"));
        }
        return;
    }

    // Le coin: le rang, et le symbole sous lui. Twice over, head to tail, the
    // way a real card is printed -- which is what lets a fanned hand be read
    // with only the left edge of each card showing.
    const qreal indexHeight = std::max(8.0, box.height() * 0.158);
    QFont rank = indexFont(plainFont(painter), indexHeight, true);
    const QString label = rankLabel(card.rank);
    painter->setFont(rank);
    const QFontMetricsF metrics(rank);
    const qreal indexWidth = std::max(metrics.horizontalAdvance(QStringLiteral("10")),
                                      box.width() * 0.22);

    for (int turn = 0; turn < 2; ++turn) {
        painter->save();
        if (turn == 1) {
            painter->translate(box.center());
            painter->rotate(180);
            painter->translate(-box.center());
        }
        painter->setPen(faded);
        const QRectF where(inner.left(), inner.top(), indexWidth, indexHeight);
        painter->drawText(where, Qt::AlignHCenter | Qt::AlignVCenter, label);
        const qreal pip = std::min(indexWidth * 0.60, indexHeight * 0.66);
        paintPip(painter,
                 QPointF(where.center().x(), where.bottom() + pip * 0.60 + box.height() * 0.012),
                 pip, card.suit, faded);
        painter->restore();
        // A card too small for two corners gets one: better a legible corner
        // than two unreadable ones.
        if (box.width() < 40) break;
    }

    // Le milieu.
    //
    // Pas la bande laissée entre les deux colonnes d'index -- il n'en reste
    // rien. Sur une vraie carte le coin n'occupe que le coin, et les symboles
    // tiennent le milieu sur plus de la moitié de la largeur ; les colonnes
    // sont assez rentrées et les rangées assez descendues pour que le symbole
    // d'en haut à gauche ne vienne pas toucher celui du coin.
    // Rentré assez pour que les deux colonnes de symboles tombent entre les
    // deux coins et non dessus : l'index de ce jeu est large, parce qu'il doit
    // se lire quand la main est serrée et qu'un tiers de la carte dépasse, et
    // ce qui reste au milieu est d'autant plus étroit.
    const QRectF middle = box.adjusted(box.width() * 0.25, box.height() * 0.15,
                                       -box.width() * 0.25, -box.height() * 0.15);

    // Les figures et l'as ne se logent pas entre les deux colonnes d'index:
    // sur une vraie carte, le coin n'occupe que le coin, et la figure tient le
    // milieu sur presque toute la largeur. Les deux ont donc leur propre boîte
    // plutôt que celle, étroite, où les symboles sont rangés en colonnes.
    if (card.rank >= kValet) {
        const QRectF figure = box.adjusted(box.width() * 0.19, box.height() * 0.21,
                                           -box.width() * 0.19, -box.height() * 0.21);
        if (figure.width() > 9.0 && figure.height() > 12.0) {
            paintCourtMark(painter, figure, card.rank, faded, paper);
        } else {
            paintPip(painter, box.center(), std::min(box.width(), box.height()) * 0.42,
                     card.suit, faded);
        }
        return;
    }
    if (card.rank == kAs) {
        // Un seul symbole, et grand: c'est ce qu'est un as.
        paintPip(painter, box.center(), std::min(box.width(), box.height()) * 0.46, card.suit,
                 faded);
        return;
    }

    const QVector<QPointF> &layout = pipLayout(card.rank);
    const qreal pipSpan = std::min(middle.width() * 0.38, middle.height() * 0.15);
    if (middle.width() < 18 || pipSpan < 4.0 || layout.isEmpty()) {
        // Trop petite pour les symboles: one in the middle says the colour,
        // and the corner says the rank, which is all a card in a combination
        // on the tapis has to say.
        paintPip(painter, box.center(), std::min(box.width(), box.height()) * 0.40, card.suit,
                 faded);
        return;
    }
    for (const QPointF &at : layout) {
        paintPip(painter,
                 QPointF(middle.left() + at.x() * middle.width(),
                         middle.top() + at.y() * middle.height()),
                 pipSpan, card.suit, faded);
    }
}

} // namespace

void drawCardFace(QPainter *painter, const QRectF &box, const Card &card, const Palette &scheme,
                  const CardLook &look) {
    if (box.width() < 3 || box.height() < 5) return;
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    const qreal corner = cardCornerFor(box);
    QPainterPath shape;
    shape.addRoundedRect(box, corner, corner);

    // L'ombre portée: a card lies on cloth, and cloth takes a shadow.
    painter->setPen(Qt::NoPen);
    if (!look.noShadow) {
        QColor shade = scheme.feltEdge;
        shade.setAlpha(look.chosen ? 130 : 96);
        QPainterPath cast;
        cast.addRoundedRect(box.translated(0.0, look.chosen ? 3.5 : 2.0), corner, corner);
        painter->fillPath(cast, shade);
    }

    QColor paper = scheme.cardFace;
    if (look.dimmed) paper = blendColours(paper, scheme.felt, 0.34);
    if (look.lit) paper = blendColours(paper, QColor(Qt::white), darkMode() ? 0.22 : 0.55);
    painter->fillPath(shape, paper);

    const QColor ink = card.joker() ? scheme.cardInk
                                    : (isRed(card.suit) ? scheme.cardRed : scheme.cardInk);
    const QColor faded = look.dimmed ? blendColours(ink, paper, 0.45) : ink;

    // Le liseré. The chosen card wears the table's colour, which is how a
    // selection reads on a card that is otherwise all white.
    painter->setBrush(Qt::NoBrush);
    if (look.chosen) {
        painter->setPen(QPen(scheme.accent, std::max(1.6, box.width() * 0.035)));
    } else {
        QColor rim = blendColours(paper, scheme.cardInk, 0.22);
        painter->setPen(QPen(rim, 1.0));
    }
    painter->drawPath(shape);

    if (look.glow > 0.005) {
        // Ce qui vient d'être joué. A ring that fades, so that a card played
        // by somebody else across the table is a thing you saw happen.
        QColor halo = scheme.gold;
        halo.setAlphaF(std::clamp(look.glow, 0.0, 1.0) * 0.95);
        painter->setPen(QPen(halo, std::max(2.0, box.width() * 0.055)));
        painter->drawPath(shape);
    }

    // Une carte d'un jeu installé remplace celle que le programme dessinerait.
    QPixmap art;
    if (const CardSet *set = CardDecks::instance().current()) {
        const int slot = CardSet::slotFor(card.joker() ? 0 : card.rank, int(card.suit));
        art = set->faceImage(slot, box.size().toSize());
    }

    if (art.isNull()) {
        paintDrawnFace(painter, box, card, scheme, paper, faded, look);
    } else {
        painter->save();
        // Découpée aux coins arrondis.
        painter->setClipPath(shape);
        QRectF where(0, 0, art.width(), art.height());
        where.moveCenter(box.center());
        painter->drawPixmap(where.topLeft(), art);
        painter->restore();
    }
    painter->restore();
}

void drawCardBack(QPainter *painter, const QRectF &box, const Palette &scheme,
                  const CardLook &look) {
    if (box.width() < 3 || box.height() < 5) return;
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    const qreal corner = cardCornerFor(box);
    QPainterPath shape;
    shape.addRoundedRect(box, corner, corner);

    painter->setPen(Qt::NoPen);
    if (!look.noShadow) {
        QColor shade = scheme.feltEdge;
        shade.setAlpha(96);
        QPainterPath cast;
        cast.addRoundedRect(box.translated(0.0, 2.0), corner, corner);
        painter->fillPath(cast, shade);
    }

    // Le dos choisi dans les réglages, s'il y en a un, à la place du treillis.
    QPixmap art;
    if (const CardSet *set = CardDecks::instance().current()) {
        art = set->backImage(CardDecks::instance().chosenBack(), box.size().toSize());
    }

    if (art.isNull()) {
        // Le treillis, recopié plutôt que redessiné.
        const QPixmap &tile = drawnBack(box.size(), scheme, look.lit);
        if (!tile.isNull()) painter->drawPixmap(box.topLeft(), tile);
    } else {
        // La carte du jeu installé peut être transparente par endroits: le
        // corps dessous, puis l'image par-dessus.
        QColor back = scheme.cardBack;
        if (look.lit) back = blendColours(back, QColor(Qt::white), 0.18);
        painter->fillPath(shape, back);

        painter->save();
        painter->setClipPath(shape);
        QRectF where(0, 0, art.width(), art.height());
        where.moveCenter(box.center());
        painter->drawPixmap(where.topLeft(), art);
        painter->restore();
    }

    if (look.glow > 0.005) {
        QColor halo = scheme.gold;
        halo.setAlphaF(std::clamp(look.glow, 0.0, 1.0) * 0.9);
        painter->setPen(QPen(halo, std::max(2.0, box.width() * 0.055)));
        painter->setBrush(Qt::NoBrush);
        painter->drawPath(shape);
    }
    painter->restore();
}

void drawCardSlot(QPainter *painter, const QRectF &box, const Palette &scheme,
                  const QString &label) {
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    const qreal corner = cardCornerFor(box);

    QColor line = scheme.feltText;
    line.setAlpha(110);
    QPen dashes(line, 1.6, Qt::DashLine);
    dashes.setDashPattern({4.0, 3.5});
    painter->setPen(dashes);
    QColor hollow = scheme.feltEdge;
    hollow.setAlpha(70);
    painter->setBrush(hollow);
    painter->drawRoundedRect(box, corner, corner);

    if (!label.isEmpty() && box.width() > 36) {
        QFont word = plainFont(painter);
        word.setPixelSize(std::max(8, int(box.height() * 0.11)));
        word.setCapitalization(QFont::AllUppercase);
        word.setLetterSpacing(QFont::PercentageSpacing, 120);
        painter->setFont(word);
        painter->setPen(line);
        painter->drawText(box, Qt::AlignCenter | Qt::TextWordWrap, label);
    }
    painter->restore();
}

void drawSuitPip(QPainter *painter, const QRectF &box, Suit suit, const QColor &ink) {
    paintPip(painter, box.center(), std::min(box.width(), box.height()), suit, ink);
}

void drawRivalEmblem(QPainter *painter, const QRectF &box, int emblem, const QString &initial,
                     const QColor &ink, const QColor &back) {
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    const qreal span = std::min(box.width(), box.height());
    QRectF disc(0, 0, span, span);
    disc.moveCenter(box.center());

    painter->setPen(Qt::NoPen);
    painter->setBrush(back);
    painter->drawEllipse(disc);
    painter->setPen(QPen(ink, std::max(1.6, span * 0.075)));
    painter->setBrush(Qt::NoBrush);
    painter->drawEllipse(disc.adjusted(span * 0.045, span * 0.045, -span * 0.045, -span * 0.045));

    // L'initiale au milieu, et la marque en haut: a ring of six is told apart
    // by colour at a glance, by its mark at a look and by its letter for
    // certain.
    QFont letter = plainFont(painter);
    letter.setBold(true);
    letter.setPixelSize(std::max(8, int(span * 0.44)));
    painter->setFont(letter);
    painter->setPen(ink);
    painter->drawText(disc.adjusted(0, span * 0.06, 0, span * 0.06), Qt::AlignCenter,
                      initial.left(1).toUpper());

    const qreal mark = span * 0.22;
    const QPointF at(disc.center().x(), disc.top() + span * 0.21);
    painter->setPen(Qt::NoPen);
    painter->setBrush(ink);
    switch (qBound(0, emblem, 5)) {
    case 0: paintPip(painter, at, mark, Suit::Pique, ink); break;
    case 1: paintPip(painter, at, mark, Suit::Coeur, ink); break;
    case 2: paintPip(painter, at, mark, Suit::Carreau, ink); break;
    case 3: paintPip(painter, at, mark, Suit::Trefle, ink); break;
    case 4: {
        QPainterPath star;
        for (int i = 0; i < 10; ++i) {
            const qreal angle = -M_PI / 2.0 + i * M_PI / 5.0;
            const qreal reach = (i % 2 == 0) ? mark * 0.58 : mark * 0.25;
            const QPointF spoke(at.x() + std::cos(angle) * reach, at.y() + std::sin(angle) * reach);
            if (i == 0) {
                star.moveTo(spoke);
            } else {
                star.lineTo(spoke);
            }
        }
        star.closeSubpath();
        painter->drawPath(star);
        break;
    }
    default: {
        // Un croissant.
        QPainterPath moon;
        moon.addEllipse(QRectF(at.x() - mark * 0.55, at.y() - mark * 0.55, mark * 1.1, mark * 1.1));
        QPainterPath bite;
        bite.addEllipse(QRectF(at.x() - mark * 0.22, at.y() - mark * 0.62, mark * 1.1, mark * 1.1));
        painter->drawPath(moon.subtracted(bite));
        break;
    }
    }
    painter->restore();
}

void drawLevelStars(QPainter *painter, const QRectF &box, int stars, int outOf, const QColor &ink,
                    const QColor &empty) {
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(Qt::NoPen);

    const int count = std::max(1, outOf);
    const qreal step = box.width() / count;
    const qreal span = std::min(step * 0.78, box.height());
    for (int i = 0; i < count; ++i) {
        const QPointF at(box.left() + step * (i + 0.5), box.center().y());
        QPainterPath star;
        for (int corner = 0; corner < 10; ++corner) {
            const qreal angle = -M_PI / 2.0 + corner * M_PI / 5.0;
            const qreal reach = (corner % 2 == 0) ? span * 0.5 : span * 0.21;
            const QPointF spoke(at.x() + std::cos(angle) * reach, at.y() + std::sin(angle) * reach);
            if (corner == 0) {
                star.moveTo(spoke);
            } else {
                star.lineTo(spoke);
            }
        }
        star.closeSubpath();
        painter->setBrush(i < stars ? ink : empty);
        painter->drawPath(star);
    }
    painter->restore();
}

void drawGauge(QPainter *painter, const QRectF &box, qreal filled, const QColor &ink,
               const QColor &back) {
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(Qt::NoPen);
    const qreal round = box.height() / 2.0;
    painter->setBrush(back);
    painter->drawRoundedRect(box, round, round);
    const qreal part = std::clamp(filled, 0.0, 1.0);
    if (part > 0.001) {
        QRectF run = box;
        run.setWidth(std::max(box.height(), box.width() * part));
        painter->setBrush(ink);
        painter->drawRoundedRect(run, round, round);
    }
    painter->restore();
}

void drawHandPile(QPainter *painter, const QRectF &box, int cards, const Palette &scheme,
                  const QColor &tint) {
    if (cards <= 0) return;
    // Not one back per card -- fifteen of them in eighty pixels is a smear --
    // but enough of them that a thick hand looks thick.
    const int shown = std::min(cards, 7);
    const qreal height = box.height();
    const QSizeF card = cardSizeForHeight(height);
    const qreal step = shown > 1 ? std::min(card.width() * 0.42,
                                            (box.width() - card.width()) / (shown - 1))
                                 : 0.0;
    for (int i = 0; i < shown; ++i) {
        QRectF at(box.left() + step * i, box.top(), card.width(), card.height());
        drawCardBack(painter, at, scheme);
    }

    // Le compte, en chiffres, parce que c'est ce qu'on veut savoir.
    QFont count = plainFont(painter);
    count.setBold(true);
    count.setPixelSize(std::max(9, int(height * 0.42)));
    painter->save();
    painter->setFont(count);
    const QString text = QString::number(cards);
    const QFontMetricsF metrics(count);
    QRectF badge(0, 0, std::max(metrics.horizontalAdvance(text) + height * 0.34, height * 0.62),
                 height * 0.52);
    badge.moveCenter(QPointF(box.left() + step * (shown - 1) + card.width() * 0.5,
                             box.center().y()));
    painter->setPen(Qt::NoPen);
    painter->setBrush(tint);
    painter->drawRoundedRect(badge, badge.height() / 2.0, badge.height() / 2.0);
    painter->setPen(readableTextOn(tint));
    painter->drawText(badge, Qt::AlignCenter, text);
    painter->restore();
}
