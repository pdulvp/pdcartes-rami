#pragma once

#include <QRectF>
#include <QString>

class QFont;
class QFontMetricsF;

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
// Ce qui s'écrit pour être lu, en M PLUS 2: la ligne de l'écran titre, et
// tout le texte de la fenêtre.
//
// Deux graisses chargées sous ce nom-là, le normal et le gras, parce que
// c'est la fonte de toute l'application: avec le seul gras, Qt aurait
// fabriqué le normal en amincissant le trait, et tout le jeu aurait parlé
// fort.
QString proseFamily();

// Pose cette fonte-là sur toute l'application, en lui gardant le corps que
// le système a choisi. Appelée une fois au démarrage: tout ce qui se lit dans
// la fenêtre en hérite, et ce qui veut une fonte d'affichage la demande.
void dressApplicationFont();

// `fallback` habillé de `family` à `pixels` de haut. Quand `family` est vide,
// c'est le gras de la fonte du système qui tient lieu de fonte d'affichage.
QFont familyFont(const QFont &fallback, const QString &family, qreal pixels);

// La boîte à passer à `drawText` pour qu'un texte centré tombe sur le milieu
// de ses capitales, et non sur celui de la boîte de ligne de la fonte.
//
// Qt centre sur la boîte de ligne, qui va du haut des accents au bas des
// jambages. Dans une fonte latine les deux milieux sont presque le même et
// personne ne le remarque. Dans une fonte japonaise, non: sa boîte de ligne
// est haute, parce qu'elle doit loger un idéogramme et les signes qui le
// surmontent, et du latin centré dedans se retrouve posé bas. C'est d'autant
// plus visible que la pastille est serrée autour du mot -- « 144 pts » dans
// son ovale, le niveau d'un adversaire dans le sien.
QRectF capCentred(const QRectF &box, const QFontMetricsF &metrics);
