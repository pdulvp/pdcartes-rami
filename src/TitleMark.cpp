#include "TitleMark.h"

#include "Fonts.h"
#include "Palette.h"

#include <QFont>
#include <QFontMetricsF>
#include <QLatin1String>
#include <QPainter>
#include <QPointF>
#include <QSizeF>
#include <QString>
#include <QStringList>

#include <algorithm>
#include <cmath>

namespace {

// Les deux mots sont inscrits en clair et non traduits : c'est un nom propre.
const QLatin1String kUpper("PDCARTES");
const QLatin1String kLower("RAMI");

// Hauteur de la ligne supérieure, en proportion de celle du titre. Elle est
// réduite car il s'agit d'une mention secondaire placée au-dessus du nom et
// non d'un second nom. La police Zen Dots reste lisible à cette taille, ses
// lettres étant pleines et arrondies, là où le tracé rainuré de la police du
// titre deviendrait confus.
constexpr qreal kSmallShare = 0.20;
// Les deux lignes sont alignées sur le même bord gauche. Le bloc était
// auparavant disposé en décalage, « Rami » étant rentré vers la droite ; une
// ligne supérieure réduite au quart de la hauteur ne supporte plus un tel
// décalage, qui la laisserait en porte-à-faux au-dessus du vide.
constexpr qreal kIndent = 0.0;
// Interligne séparant les deux lignes, en proportion de la hauteur de la
// grande. La valeur est faible, s'agissant d'un logo et non d'un texte
// courant.
constexpr qreal kLeading = 0.07;
// Décalage de l'ombre portée, en proportion de la hauteur de la grande ligne.
constexpr qreal kDrop = 0.022;

// Corps de référence servant à calculer le facteur d'échelle. Les dimensions
// définitives sont mesurées ensuite, le rendu d'une police n'étant pas
// exactement proportionnel à son corps.
constexpr qreal kTrial = 100.0;


// Le bloc mesuré à un corps donné : les deux polices, le rectangle d'encrage
// de chaque ligne, et la position du coin supérieur gauche de chacune.
//
// La mesure porte sur l'encrage réel — `tightBoundingRect` et non la hauteur
// de ligne — car un logo se cale sur les formes visibles et non sur les blancs
// que la police réserve au-dessus et au-dessous.
struct Block {
    QFont upper;
    QFont lower;
    QRectF upperInk;
    QRectF lowerInk;
    QSizeF size;
    qreal indent = 0.0;
};

Block measure(const QFont &fallback, qreal pixels) {
    Block block;
    block.lower = familyFont(fallback, signFamily(), pixels);
    block.upper = familyFont(fallback, capsFamily(), pixels * kSmallShare);
    block.upper.setLetterSpacing(QFont::PercentageSpacing, 104);
    block.upperInk = QFontMetricsF(block.upper).tightBoundingRect(kUpper);
    block.lowerInk = QFontMetricsF(block.lower).tightBoundingRect(kLower);
    block.indent = block.upperInk.width() * kIndent;
    block.size = QSizeF(std::max(block.upperInk.width(), block.indent + block.lowerInk.width()),
                        block.upperInk.height() + pixels * kLeading + block.lowerInk.height());
    return block;
}

// Dessine une ligne et son ombre portée. Sans cette ombre, l'or se confond
// avec le vert clair du centre du tapis, où porte l'éclairage, et une carte
// passant dessous ferait disparaître le texte.
void inkLine(QPainter *painter, const QFont &font, const QRectF &ink, const QPointF &at,
             const QString &text, const QColor &colour, const QColor &shade, qreal drop) {
    painter->setFont(font);
    // `drawText` attend la ligne de base, alors que l'on dispose du coin du
    // rectangle d'encrage.
    const QPointF baseline = at - ink.topLeft();
    painter->setPen(shade);
    painter->drawText(baseline + QPointF(drop * 0.5, drop), text);
    painter->setPen(colour);
    painter->drawText(baseline, text);
}

} // namespace

qreal titleMarkAspect(const QFont &fallback) {
    const Block block = measure(fallback, kTrial);
    if (block.size.height() <= 0.0) return 1.6;
    return block.size.width() / block.size.height();
}

QRectF titleMarkBox(const QRectF &area, const QFont &fallback, qreal heightShare,
                    qreal middleShare) {
    const qreal height = area.height() * heightShare;
    const qreal width = height * titleMarkAspect(fallback);
    return QRectF(area.left() + (area.width() - width) / 2.0,
                  area.top() + area.height() * middleShare - height / 2.0, width, height);
}

void drawTitleMark(QPainter *painter, const QRectF &box, const QFont &fallback,
                   const Palette &scheme, qreal fade) {
    if (box.height() <= 1.0 || fade <= 0.0) return;

    // L'échelle est d'abord estimée au corps de référence, puis le bloc est
    // remesuré. Le rectangle étant au rapport du bloc, sa hauteur suffit à le
    // positionner.
    const Block trial = measure(fallback, kTrial);
    const qreal room = box.height() / std::max(1.0, trial.size.height());
    const Block block = measure(fallback, std::max(7.0, kTrial * room));

    // L'écart résiduel entre le rectangle demandé et le bloc remesuré est
    // réparti de part et d'autre, plutôt que laissé d'un seul côté.
    const qreal left = box.left() + (box.width() - block.size.width()) / 2.0;
    const qreal top = box.top() + (box.height() - block.size.height()) / 2.0;
    const qreal pixels = block.lower.pixelSize();

    painter->save();
    painter->setRenderHint(QPainter::TextAntialiasing, true);
    if (fade < 1.0) painter->setOpacity(painter->opacity() * fade);

    QColor shade = scheme.feltEdge;
    shade.setAlpha(185);
    const qreal drop = std::max(1.0, pixels * kDrop);
    inkLine(painter, block.upper, block.upperInk, QPointF(left, top), kUpper, scheme.feltText,
            shade, drop * kSmallShare);
    inkLine(painter, block.lower, block.lowerInk,
            QPointF(left + block.indent, top + block.upperInk.height() + pixels * kLeading),
            kLower, scheme.gold, shade, drop);
    painter->restore();
}
