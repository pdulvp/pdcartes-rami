#include "CardArt.h"

#include "Fonts.h"

#include "CardDeck.h"
#include "TableLook.h"

#include <QCoreApplication>
#include <QFontMetricsF>
#include <QHash>
#include <QGuiApplication>
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>

#include <algorithm>
#include <cmath>

namespace {

// Rayon d'arrondi des coins d'une carte, en proportion de sa largeur.
//
// La valeur est faible, pour une raison qui n'apparaît qu'avec un jeu
// installé : une carte provenant d'un fichier possède déjà ses propres coins.
// Les fichiers CARDS.DLL de Windows comportent ainsi un filet noir situé à
// deux pixels du bord sur soixante et onze, soit moins de trois centièmes de
// la largeur. Appliquer par-dessus un arrondi plus large couperait ce filet
// et laisserait un fragment visible à chaque coin.
//
// Cette valeur convient donc à tous les cas : elle suit le filet des cartes
// de Windows, n'entame pas un jeu dont les coins sont déjà transparents, et
// adoucit un jeu aux coins vifs. Les cartes dessinées par le programme
// l'emploient également, afin qu'un jeu installé et le jeu dessiné présentent
// la même découpe.
constexpr qreal kCornerPart = 0.035;

// Motif de trame du tapis.
//
// Une trame est formée de fils se croisant. Elle est rendue au moyen de deux
// familles de lignes, l'une légèrement plus claire que le tissu et l'autre
// légèrement plus sombre, décalées d'un demi-pas. La tuile mesure huit pixels
// de côté, de sorte que le motif se répète sans raccord visible et que les
// lignes tombent sur des pixels entiers.
struct Weave {
    QPixmap tile;
    int hue = -1;
    bool dark = false;
    bool made = false;
};

// La tuile est fournie par une fonction plutôt que conservée dans une
// variable de portée fichier : la construction d'un `QPixmap` suppose qu'une
// `QGuiApplication` existe déjà, alors qu'une variable de portée fichier est
// construite avant que `main()` n'en ait créé une. Une variable statique
// locale à une fonction est construite au premier appel, bien plus tard.
Weave &weave() {
    static Weave kept;
    return kept;
}

// Cache des dos de cartes dessinés.
//
// Le treillis se compose d'une cinquantaine de diagonales découpées selon un
// coin arrondi, et un dos n'apparaît jamais seul à l'écran : il y en a un sur
// la pioche, un sur la défausse et un par tas de cartes adverse. Redessiné à
// chaque rafraîchissement, il coûterait deux fois plus cher qu'une figure
// — valeur mesurée. Comme la trame du tapis, il est donc produit une fois par
// taille puis recopié.
//
// Les images sont indexées par taille et par état, et l'ensemble du cache est
// vidé lorsque la palette change, celle-ci déterminant leur coloration.
struct Backs {
    QHash<quint64, QPixmap> tiles;
    int hue = -1;
    bool dark = false;
};

Backs &backs() {
    static Backs kept;
    return kept;
}

// Corps d'un dos dessiné : tout sauf l'ombre portée et le halo, qui dépendent
// du coup en cours et non de la carte elle-même.
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

    // Un rafraîchissement emploie cinq ou six tailles, et chaque cran de la
    // molette de zoom en introduit une nouvelle. Au-delà d'un seuil largement
    // suffisant pour une table, le cache est vidé plutôt que de conserver
    // toutes les tailles traversées.
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

    // Le dos : une marge blanche encadrant un treillis, disposition
    // traditionnelle du dos d'une carte à jouer.
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

// Les symboles de couleur sont tracés et non composés.
//
// Ces symboles existent dans toutes les polices, mais leur forme et leur
// graisse y varient, et une police dépourvue du caractère affiche un
// rectangle de substitution. Tracés par le programme, ils ont une apparence
// constante.
QPainterPath pipPath(Suit suit) {
    QPainterPath path;
    switch (suit) {
    case Suit::Coeur: {
        // Deux lobes et une pointe, formés d'une courbe de chaque côté.
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
        // Un cœur renversé, surmontant une tige.
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
        // Trois lobes circulaires et une tige.
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

// Les figures. Elles ne sont pas représentées par des portraits, illisibles à
// quarante pixels, mais par un emblème propre à chacune, tracé dans la
// couleur de la carte : une couronne pour le roi, un diadème pour la dame,
// une fleur de lis pour le valet.
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
        // Couronne du roi : cinq pointes surmontées d'une croix, qui la
        // distingue immédiatement du diadème de la dame.
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
        // Diadème de la dame : trois pointes, plus basses et plus arrondies
        // que celles de la couronne, terminées chacune par une perle.
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
        // Valet : une fleur de lis.
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

// Joker : un bonnet à grelots surmontant une étoile.
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

    // Les trois pointes du bonnet, terminées chacune par un grelot.
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

    // L'étoile placée sous le bonnet, symbole du joker sur le tapis.
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

// Disposition des symboles sur les cartes de deux à dix : la disposition
// employée par les jeux de cartes depuis deux siècles, exprimée en fractions
// du rectangle intérieur de la carte.
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
    // La famille est réinitialisée au même titre que la graisse et
    // l'interlettrage : un titre composé sur le peintre — nom d'une pile,
    // intitulé du tapis — laissait sinon sa police d'affichage à tout ce qui
    // était dessiné ensuite. Il en résultait des zéros barrés sur les cartes
    // et un accent manquant sur une plaque, ces polices d'affichage ne
    // comportant ni l'un ni l'autre.
    font.setFamily(QGuiApplication::font().family());
    font.setBold(false);
    font.setItalic(false);
    font.setCapitalization(QFont::MixedCase);
    font.setLetterSpacing(QFont::PercentageSpacing, 100);
    return font;
}

qreal cardAspect() {
    if (const CardSet *set = CardDecks::instance().current()) {
        const qreal theirs = set->aspect();
        // Bornée : une proportion aberrante romprait la disposition.
        if (theirs > 0.35 && theirs < 1.60) return theirs;
    }
    return kCardRatio;
}

QSizeF cardSizeForHeight(qreal height) {
    const qreal aspect = cardAspect();
    // La hauteur demandée fixe l'encombrement disponible, que la largeur
    // consomme.
    //
    // Toute la disposition du tapis — la main, les combinaisons, la pioche —
    // est calculée en hauteur, la largeur découlant de la proportion du jeu
    // choisi. Tant qu'il s'agit du jeu dessiné par le programme, les deux
    // reviennent au même. Un jeu installé impose en revanche sa propre
    // proportion, et les fichiers CARDS.DLL de Windows sont nettement plus
    // larges que hauts (71 sur 96, contre 0,642). À hauteur égale, leurs
    // cartes occupaient un sixième de largeur en plus : la main de quatorze
    // cartes passait d'une rangée aérée à une rangée touchant les deux bords
    // de la fenêtre.
    //
    // La carte est donc contenue dans la largeur qu'aurait la carte dessinée à
    // cette hauteur, et c'est sa hauteur qui est réduite lorsque le jeu est
    // plus large. Un jeu plus étroit que le jeu dessiné conserve la hauteur
    // demandée, ce cas ne posant pas de difficulté.
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
    // Une famille de fils horizontaux et une famille de fils verticaux,
    // décalées d'un demi-pas : c'est l'aspect d'un tissu vu de près, qui à
    // distance normale se lit comme une texture et non comme une grille.
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

QColor feltPlaqueFill(const Palette &scheme, qreal lift) {
    QColor fill = blendColours(scheme.feltEdge, QColor(0, 0, 0), 0.52);
    if (lift > 0.0) fill = blendColours(fill, scheme.gold, lift);
    fill.setAlpha(205);
    return fill;
}

QColor feltGold(const Palette &scheme, qreal strength) {
    QColor line = scheme.gold;
    line.setAlphaF(qBound(0.0, strength, 1.0));
    return line;
}

QColor feltBarFace(const Palette &scheme) {
    return blendColours(scheme.feltEdge, QColor(0, 0, 0), 0.46);
}

void drawFelt(QPainter *painter, const QRectF &box, const Palette &scheme, bool withBraid) {
    painter->save();
    painter->setPen(Qt::NoPen);

    // Éclairage de la table : plus clair à l'aplomb de la source lumineuse,
    // située au-dessus du centre. C'est ce dégradé qui distingue un aplat vert
    // uniforme d'un tissu éclairé dans une pièce.
    QRadialGradient light(box.center() - QPointF(0, box.height() * 0.10),
                          std::max(box.width(), box.height()) * 0.78);
    light.setColorAt(0.0, scheme.felt);
    light.setColorAt(0.62, blendColours(scheme.felt, scheme.feltEdge, 0.45));
    light.setColorAt(1.0, scheme.feltEdge);
    painter->setBrush(light);
    painter->drawRect(box);

    painter->setBrush(QBrush(weaveTile(scheme)));
    painter->drawRect(box);

    if (!withBraid) {
        painter->restore();
        return;
    }

    // Bordure : un galon légèrement en retrait du bord, doublé d'une ligne
    // plus sombre à l'extérieur, afin que le tissu se lise comme tendu sur un
    // cadre.
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

// Contenu dessiné par le programme sur une face de carte. Cette partie est
// isolée afin que l'ombre, le liseré et le halo s'appliquent également aux
// cartes d'un jeu installé.
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

    // Le coin : le rang, surmontant le symbole de couleur. Il est répété
    // deux fois, tête-bêche, comme sur une carte imprimée ; c'est ce qui
    // permet de lire une main en éventail dont seul le bord gauche de chaque
    // carte est visible.
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
        // Une carte trop petite pour deux coins n'en reçoit qu'un : un coin
        // lisible vaut mieux que deux illisibles.
        if (box.width() < 40) break;
    }

    // La zone centrale.
    //
    // Il ne s'agit pas de la bande résiduelle entre les deux colonnes d'index,
    // qui serait trop étroite. Sur une carte imprimée, le coin n'occupe que le
    // coin et les symboles tiennent le centre sur plus de la moitié de la
    // largeur ; les colonnes sont suffisamment rentrées et les rangées
    // suffisamment abaissées pour que le symbole supérieur gauche ne touche
    // pas celui du coin.
    //
    // Le retrait est calculé pour que les deux colonnes de symboles se placent
    // entre les deux coins et non par-dessus. L'index de ce jeu est large,
    // devant rester lisible lorsque la main est resserrée et qu'un tiers
    // seulement de la carte dépasse, ce qui réduit d'autant la zone
    // centrale.
    const QRectF middle = box.adjusted(box.width() * 0.25, box.height() * 0.15,
                                       -box.width() * 0.25, -box.height() * 0.15);

    // Les figures et l'as ne s'inscrivent pas entre les deux colonnes
    // d'index : sur une carte imprimée, le coin n'occupe que le coin et la
    // figure tient le centre sur presque toute la largeur. L'un et l'autre
    // disposent donc de leur propre rectangle, distinct du rectangle étroit
    // où les symboles sont disposés en colonnes.
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
        // Un symbole unique et de grande taille, conformément à l'usage pour
        // un as.
        paintPip(painter, box.center(), std::min(box.width(), box.height()) * 0.46, card.suit,
                 faded);
        return;
    }

    const QVector<QPointF> &layout = pipLayout(card.rank);
    const qreal pipSpan = std::min(middle.width() * 0.38, middle.height() * 0.15);
    if (middle.width() < 18 || pipSpan < 4.0 || layout.isEmpty()) {
        // Carte trop petite pour la disposition complète : un symbole central
        // indique la couleur et le coin indique le rang, ce qui suffit pour
        // une carte intégrée à une combinaison posée sur le tapis.
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

    // Ombre portée : la carte repose sur un tissu, qui reçoit son ombre.
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

    // Liseré : la carte sélectionnée est bordée de la couleur de la table,
    // seul moyen de signaler une sélection sur une carte par ailleurs
    // entièrement blanche.
    painter->setBrush(Qt::NoBrush);
    if (look.chosen) {
        painter->setPen(QPen(scheme.accent, std::max(1.6, box.width() * 0.035)));
    } else {
        QColor rim = blendColours(paper, scheme.cardInk, 0.22);
        painter->setPen(QPen(rim, 1.0));
    }
    painter->drawPath(shape);

    if (look.glow > 0.005) {
        // Carte qui vient d'être jouée : un anneau qui s'estompe, afin qu'un
        // coup joué par un adversaire soit perçu au moment où il survient.
        QColor halo = scheme.gold;
        halo.setAlphaF(std::clamp(look.glow, 0.0, 1.0) * 0.95);
        painter->setPen(QPen(halo, std::max(2.0, box.width() * 0.055)));
        painter->drawPath(shape);
    }

    // Une carte issue d'un jeu installé se substitue à celle que le programme
    // dessinerait.
    QPixmap art;
    if (const CardSet *set = CardDecks::instance().current()) {
        const int slot = CardSet::slotFor(card.joker() ? 0 : card.rank, int(card.suit));
        art = set->faceImage(slot, box.size().toSize());
    }

    if (art.isNull()) {
        paintDrawnFace(painter, box, card, scheme, paper, faded, look);
    } else {
        painter->save();
        // Découpée selon les coins arrondis.
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

    // Le dos sélectionné dans les réglages, s'il en existe un, remplace le
    // treillis.
    //
    // Il est demandé à `TableLook` et non à la cartothèque : le dos ne
    // provient pas nécessairement du jeu dont sont tirées les faces, et seul
    // `TableLook` connaît son origine.
    QPixmap art;
    if (const CardSet *set = backCards()) {
        art = set->backImage(backIndex(), box.size().toSize());
    }

    if (art.isNull()) {
        // Le treillis est recopié depuis le cache plutôt que redessiné.
        const QPixmap &tile = drawnBack(box.size(), scheme, look.lit);
        if (!tile.isNull()) painter->drawPixmap(box.topLeft(), tile);
    } else {
        // La carte d'un jeu installé peut comporter des zones transparentes :
        // le corps est dessiné d'abord, l'image par-dessus.
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

    // Une valeur négative supprime le symbole. C'est le cas de la plaque du
    // joueur : le symbole sert à distinguer les six adversaires entre eux, et
    // le joueur n'en fait pas partie ; le cercle et l'initiale suffisent.
    const bool marked = emblem >= 0;

    // L'initiale est centrée et le symbole placé au-dessus : parmi six
    // médaillons, la couleur permet une identification immédiate, le symbole
    // une identification rapide, et la lettre une identification certaine.
    //
    // L'initiale est légèrement abaissée pour laisser la place au symbole. Ce
    // décalage est lié à la présence du symbole : un médaillon qui n'en porte
    // pas conserve sa lettre au centre du cercle, là où le regard la
    // cherche.
    const qreal drop = marked ? span * 0.06 : 0.0;
    QFont letter = plainFont(painter);
    letter.setBold(true);
    letter.setPixelSize(std::max(8, int(span * 0.44)));
    painter->setFont(letter);
    painter->setPen(ink);
    painter->drawText(capCentred(disc.adjusted(0, drop, 0, drop), QFontMetricsF(letter)),
                      Qt::AlignCenter, initial.left(1).toUpper());

    if (!marked) {
        painter->restore();
        return;
    }

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
    // Le tas ne comporte pas un dos par carte — quinze dos sur quatre-vingts
    // pixels seraient indistincts — mais un nombre suffisant pour qu'une main
    // nombreuse paraisse telle.
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

    // Le nombre exact de cartes, affiché en chiffres.
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
    painter->drawText(capCentred(badge, metrics), Qt::AlignCenter, text);
    painter->restore();
}
