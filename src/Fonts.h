#pragma once

#include <QString>

class QFont;

// Les fontes d'affichage du jeu, tirées du binaire.
//
// Dedans et non à côté: le jeu est un fichier, le zip de Windows n'a rien de
// plus à emporter, et une fonte qu'on ne trouve pas au démarrage ne laisse
// pas l'écran titre en Sans.
//
// Trois, parce qu'il y a trois métiers sur le tapis. L'enseigne veut de la
// hauteur et des rainures; la ligne de dessus est au cinquième de sa taille
// et veut des lettres pleines; et ce qui s'écrit en petit veut une fonte de
// texte, qui se lit sans qu'on la regarde. Une seule fonte pour les trois
// ferait soit une enseigne tiède, soit une ligne illisible.
//
// Chacune est chargée une fois pour la séance, à la première demande. Un nom
// vide veut dire que la ressource manque -- ce qui ne devrait pas arriver,
// elles sont compilées dedans -- et l'appelant se rabat alors sur la fonte
// qu'on lui passe: moins beau, pas cassé.

// L'enseigne: `RAMI`, en Zen Tokyo Zoo.
QString signFamily();
// La ligne du dessus: `PDCARTES`, en Zen Dots.
QString capsFamily();
// Ce qui s'écrit en petit sur le drap, en Zen Kaku Gothic New.
QString proseFamily();

// `fallback` habillé de `family` à `pixels` de haut. Quand `family` est vide,
// c'est le gras de la fonte du système qui tient lieu de fonte d'affichage.
QFont familyFont(const QFont &fallback, const QString &family, qreal pixels);
