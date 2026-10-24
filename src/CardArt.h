#pragma once

#include "Cards.h"
#include "Palette.h"

#include <QPixmap>
#include <QRectF>
#include <QSizeF>

class QPainter;
class QFont;

// Le dessin : le tapis, et les cartes posées dessus.
//
// Everything here is drawn rather than loaded. A card game wants fifty-four
// pictures, and fifty-four pictures is either an artist or a folder of files
// that has to be found at run time and is not there on somebody else's
// machine. Drawn, they come out in the table's own colours, they are sharp at
// any size, and the whole game is still one binary.

// The proportion a card keeps, whatever size it is drawn at. The real ones
// are 2.25 by 3.5 inches, and the eye knows it.
//
// Cette proportion est celle des cartes dessinées. Un jeu installé apporte la
// sienne ; tout le tapis se dispose d'après cardAspect().
inline constexpr qreal kCardRatio = 0.642;
qreal cardAspect();

// L'arrondi du coin d'une carte de cette taille. Partagé, parce qu'un aperçu
// de dos ailleurs dans le programme doit couper la sienne exactement comme le
// tapis coupe les siennes.
qreal cardCornerFor(const QRectF &box);
QSizeF cardSizeForHeight(qreal height);
QSizeF cardSizeForWidth(qreal width);

// Le tapis : the cloth, with the light falling on the middle of it, a weave
// you can see if you look, and a braid round the edge.
//
// `braid` pour le galon. On le laisse tomber quand le drap n'est pas une
// table mais un fond -- le menu d'ouverture est posé dessus, et un galon
// derrière une page de plaques est un cadre de plus qui ne cadre rien.
void drawFelt(QPainter *painter, const QRectF &box, const Palette &scheme, bool braid = true);

// Ce qu'on pose sur le drap, et ce qui le borde.
//
// Écrit ici et non dans chaque page, parce que le menu d'ouverture et la
// table portent la même tenue et qu'une recette répétée est une recette qui
// se met à diverger. Presque noir et translucide, pour que le tissage se
// devine dessous -- c'est ce qui fait que la plaque est posée sur le tapis et
// non découpée dedans.
//
// `lift` tire la plaque vers l'or quand elle est choisie ou allumée.
QColor feltPlaqueFill(const Palette &scheme, qreal lift = 0.0);
// L'or des filets, à la force qu'on veut: un cheveu au repos, franc quand la
// chose est choisie.
QColor feltGold(const Palette &scheme, qreal strength);
// Le fond des deux bandes, celle du haut et celle du bas: plus sombre encore
// que les plaques, parce qu'une bande n'est pas posée sur le drap -- elle le
// borde.
QColor feltBarFace(const Palette &scheme);

// The weave on its own, as a tile to brush other things with. Made once per
// scheme and kept: a cross-hatch drawn line by line over a window-sized area
// is a thousand lines a repaint.
const QPixmap &weaveTile(const Palette &scheme);

// Une carte, face en haut. `lit` for the one under the pointer, `chosen` for
// one the player has picked out of their hand, `glow` for the one that has
// just been played -- which is how a game played by five other people is
// followed at all.
struct CardLook {
    bool lit = false;
    bool chosen = false;
    qreal glow = 0.0;       // nought to one, fading
    bool dimmed = false;    // a card that cannot be played now
    bool noShadow = false;  // pour dessiner une carte hors du tapis
};

void drawCardFace(QPainter *painter, const QRectF &box, const Card &card, const Palette &scheme,
                  const CardLook &look = CardLook{});

// Une carte, face en bas: the back, in the table's colour.
void drawCardBack(QPainter *painter, const QRectF &box, const Palette &scheme,
                  const CardLook &look = CardLook{});

// La place vide: where the pioche was, or where the défausse has nothing on
// it yet. A dashed outline with a word in it, so that an empty place still
// says what belongs there.
void drawCardSlot(QPainter *painter, const QRectF &box, const Palette &scheme,
                  const QString &label);

// La police du peintre, remise à plat.
//
// QPainter keeps the font it was last given, and these drawings run one after
// another on the same painter -- so a heading set in spaced capitals leaves
// the next thing drawn in spaced capitals too, however little it wants to be.
// Every font used below is derived from this rather than from painter->font(),
// which is what keeps one player's plaque from catching the one before it.
QFont plainFont(const QPainter *painter);

// Le symbole, seul: for a legend, or the corner of a plaque.
void drawSuitPip(QPainter *painter, const QRectF &box, Suit suit, const QColor &ink);

// Le médaillon d'un adversaire: a ring in their own colour with a mark and
// their initial in it. Six marks, one each, so that a plaque is known by its
// shape as well as by its colour.
//
// Un `emblem` négatif ne dessine pas de marque: c'est le médaillon de votre
// propre plaque, et la marque est précisément ce qui sépare les six.
void drawRivalEmblem(QPainter *painter, const QRectF &box, int emblem, const QString &initial,
                     const QColor &ink, const QColor &back);

// Les étoiles du niveau: one for a beginner, two, three for an expert.
void drawLevelStars(QPainter *painter, const QRectF &box, int stars, int outOf,
                    const QColor &ink, const QColor &empty);

// Une petite jauge: how far along they are inside their level.
void drawGauge(QPainter *painter, const QRectF &box, qreal filled, const QColor &ink,
               const QColor &back);

// Un petit tas de cartes, de dos: what a rival's hand looks like from across
// the table. `cards` is how many they are holding.
void drawHandPile(QPainter *painter, const QRectF &box, int cards, const Palette &scheme,
                  const QColor &tint);
