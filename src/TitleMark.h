#pragma once

#include <QRectF>

class QFont;
class QPainter;
struct Palette;

// Logo de l'application : « PDCartes » placé en haut à gauche de « Rami »,
// à la moitié de sa taille.
//
// Le logo est dessiné et non composé à partir de widgets. L'écart entre les
// deux lignes est calculé au pixel près d'après les métriques des polices,
// aucune disposition de `QLabel` ne permettant de maintenir deux lignes de
// tailles différentes aussi resserrées. Cet écart reste proportionnel à
// toutes les tailles, ce qui est l'intérêt du procédé : l'écran titre affiche
// le logo en grand, le menu l'affiche en petit en tête de page, et
// l'animation qui relie les deux l'affiche à toutes les tailles
// intermédiaires.
//
// Deux polices d'affichage sont employées, chargées depuis les ressources du
// binaire (`:/fonts/`) : Zen Dots pour la ligne supérieure, Zen Tokyo Zoo
// pour le titre. Si une ressource est absente — cas qui ne devrait pas se
// produire —, la ligne concernée est rendue en gras dans la police passée en
// argument.

// Rapport largeur sur hauteur du bloc. Mesuré une fois puis conservé ; il
// détermine les proportions du rectangle que le logo requiert.
qreal titleMarkAspect(const QFont &fallback);

// Rectangle effectivement occupé par le logo dans `area`, pour une hauteur
// exprimée en proportion de `area`. Le bloc conserve son rapport et est
// centré horizontalement. Ce rectangle sert également de position de départ
// et d'arrivée à l'animation.
QRectF titleMarkBox(const QRectF &area, const QFont &fallback, qreal heightShare,
                    qreal middleShare);

// Dessine le logo dans `box`, rectangle au rapport du bloc tel que le
// retourne `titleMarkBox()`. `fade` varie de zéro à un et commande
// l'apparition progressive.
void drawTitleMark(QPainter *painter, const QRectF &box, const QFont &fallback,
                   const Palette &scheme, qreal fade = 1.0);
