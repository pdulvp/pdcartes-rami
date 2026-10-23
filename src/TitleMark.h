#pragma once

#include <QRectF>

class QFont;
class QPainter;
struct Palette;

// Le logo : `PDCartes` posé en haut à gauche de `Rami`, deux fois plus petit.
//
// Dessiné et non composé. Le décalage des deux lignes se décide au pixel
// d'après ce que mesure la fonte -- aucune disposition de QLabel ne tient deux
// lignes de tailles différentes serrées comme un logo -- et il est le même à
// toutes les tailles, ce qui est tout l'intérêt : l'écran titre le dessine en
// grand, le menu le dessine petit en tête de page, et le calque qui va de
// l'un à l'autre le dessine à chaque taille entre les deux. Une seule chose,
// dessinée trois fois.
//
// Deux fontes d'affichage, tirées du binaire (`:/fonts/`) : Zen Dots pour la
// ligne du dessus, Zen Tokyo Zoo pour l'enseigne. Quand une ressource manque
// -- ce qui ne devrait pas arriver, elles sont compilées dedans -- la ligne
// se rabat sur la fonte qu'on lui passe, en gras : moins beau, pas cassé.

// Le rapport largeur sur hauteur du bloc. Mesuré une fois et gardé: c'est lui
// qui dit quelle boîte le logo veut.
qreal titleMarkAspect(const QFont &fallback);

// La boîte que le logo occupe vraiment dans `area`, à hauteur donnée en part
// de `area`: le bloc garde son rapport et se centre sur la largeur. C'est
// aussi d'où et vers où le vol se fait.
QRectF titleMarkBox(const QRectF &area, const QFont &fallback, qreal heightShare,
                    qreal middleShare);

// Le logo, mis dans `box` -- qui est une boîte au rapport du bloc, telle que
// `titleMarkBox()` la rend. `fade` de zéro à un pour le faire paraître.
void drawTitleMark(QPainter *painter, const QRectF &box, const QFont &fallback,
                   const Palette &scheme, qreal fade = 1.0);
