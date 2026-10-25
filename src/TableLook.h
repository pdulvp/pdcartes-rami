#pragma once

#include <QString>
#include <QVector>

class CardSet;

// Apparence du tapis et des cartes, propre à chaque profil.
//
// La couleur du tapis n'indique plus le nombre de joueurs : une table de six
// n'a pas une couleur différente d'une table de deux. Il s'agit donc d'une
// préférence, attachée au profil sous lequel on joue, au même titre que les
// noms donnés aux adversaires.
//
// La cartothèque, elle, ne connaît qu'une sélection courante : elle est
// partagée avec un autre programme, dépourvu de profils. Ce module lui
// applique la sélection du profil lors d'un changement, et enregistre ce qui
// vient d'être choisi dans les réglages.
//
// Le dos fait l'objet d'une sélection distincte. La cartothèque ne sait
// extraire un dos que du jeu qu'elle a chargé, c'est-à-dire celui des faces,
// alors que rien n'impose au dos d'en provenir. Un jeu peut fournir des
// figures satisfaisantes et des dos qui ne le sont pas ; un autre peut ne
// fournir que des dos, auquel cas la cartothèque le considère comme vide
// faute de faces à présenter ; et le treillis dessiné par le programme
// s'accorde avec tous. Ce module conserve donc l'origine du dos et se charge
// de le charger.

struct FeltColour {
    int hue;
    QString name;
};

// Les couleurs de tapis disponibles. La première est celle par défaut.
const QVector<FeltColour> &feltColours();
int defaultFeltHue();

// Couleur du profil courant, et modification de celle-ci.
int feltHue();
void setFeltHue(int hue);

// Applique à la cartothèque le jeu de cartes et le dos du profil courant, au
// démarrage et à chaque changement de profil.
void applyProfileLook();

// Enregistre pour le profil courant la sélection effectuée dans les réglages,
// et l'applique également à la cartothèque.
void rememberDeck(const QString &deckId);

// Jeu dont le dos est extrait. Une valeur vide désigne le jeu des faces, qui
// est le cas ordinaire : un jeu fournit généralement ses propres dos.
QString backDeckId();
void rememberBackDeck(const QString &deckId);

// Jeu dont le dos est extrait, chargé à la demande.
//
// Retourne un pointeur nul lorsqu'il s'agit du jeu dessiné, lorsque le fichier
// ne peut être lu, ou lorsque ce jeu ne fournit aucun dos ; dans ces trois cas,
// le treillis dessiné par le programme sert de recours. À la différence du jeu
// des faces, un jeu dépourvu de faces reste valide ici, seuls ses dos étant
// requis.
const CardSet *backCards();

// Index du dos sélectionné, toujours compris dans les bornes de
// `backCards()`. Négatif lorsqu'aucun dos n'est disponible.
int backIndex();
void rememberBack(int at);

// Vide le cache des dos mis à l'échelle lors d'un changement de taille des
// cartes : ces images étant indexées par taille, un changement global rendrait
// l'intégralité du cache inutilisable.
void forgetBackDrawings();
