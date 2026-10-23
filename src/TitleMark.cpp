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

// Les deux mots, écrits ici plutôt que traduits : c'est un nom.
const QLatin1String kUpper("PDCARTES");
const QLatin1String kLower("RAMI");

// Ce que la ligne du dessus fait de la hauteur de l'enseigne. Petite: c'est
// la raison sociale posée au-dessus du nom, et non un second nom. Zen Dots
// est ce qui la rend tenable à cette taille -- des lettres pleines et rondes,
// là où le trait rainuré de l'enseigne se brouillerait.
constexpr qreal kSmallShare = 0.20;
// Les deux lignes partent du même bord. Le bloc tenait avant en escalier,
// `Rami` rentré vers la droite ; une ligne du quart de la hauteur ne porte
// plus un décalage pareil -- elle se mettrait de travers au-dessus du vide.
constexpr qreal kIndent = 0.0;
// Ce qui sépare les deux lignes, en part de la hauteur de la grande. Serré :
// un logo, pas un paragraphe.
constexpr qreal kLeading = 0.07;
// L'ombre portée, en part de la hauteur de la grande ligne.
constexpr qreal kDrop = 0.022;

// La mesure d'essai dont on tire l'échelle. Les vraies tailles sont mesurées
// ensuite: une fonte ne grandit pas tout à fait proportionnellement.
constexpr qreal kTrial = 100.0;


// Le bloc mesuré à une taille donnée: les deux fontes, l'encre de chaque
// ligne, et où poser le coin haut-gauche de chacune.
//
// Mesuré sur l'encre -- `tightBoundingRect` et non la hauteur de ligne --
// parce qu'un logo se cale sur ce qu'on voit et non sur les blancs que la
// fonte réserve au-dessus et au-dessous.
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

// Une ligne et son ombre portée : sans elle l'or se perd dans le vert clair
// du milieu du tapis, où la lampe tombe -- et une carte qui passe dessous le
// mangerait.
void inkLine(QPainter *painter, const QFont &font, const QRectF &ink, const QPointF &at,
             const QString &text, const QColor &colour, const QColor &shade, qreal drop) {
    painter->setFont(font);
    // `drawText` prend la ligne de base ; on connaît le coin de l'encre.
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

    // L'échelle d'abord à l'essai, puis remesurée: la boîte est au rapport du
    // bloc, la hauteur suffit donc à le poser.
    const Block trial = measure(fallback, kTrial);
    const qreal room = box.height() / std::max(1.0, trial.size.height());
    const Block block = measure(fallback, std::max(7.0, kTrial * room));

    // Ce qui reste de jeu entre la boîte demandée et le bloc remesuré: on le
    // partage, plutôt que de laisser le bloc déborder d'un côté.
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
