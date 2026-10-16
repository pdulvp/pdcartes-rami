#pragma once

#include <QString>
#include <QVector>

class CardSet;

// Le tapis et les cartes, tels que ce profil-là les veut.
//
// La couleur du drap ne dit plus combien on est -- une table de six n'est pas
// d'une autre couleur qu'une table de deux, elle est seulement plus pleine.
// C'est donc un goût, et un goût appartient à qui joue: chaque profil garde
// le sien, comme il garde le nom qu'il a donné à Lulu.
//
// La cartothèque, elle, ne connaît qu'un choix courant: elle est partagée
// avec un autre programme, qui n'a pas de profils. C'est ici qu'on lui pose
// celui du profil quand il change, et ici qu'on note ce qui vient d'être
// choisi dans les réglages.
//
// Et le dos est un choix à part. La cartothèque ne sait tirer un dos que du
// jeu qu'elle a sous la main, ce qui veut dire le jeu des faces ; mais rien
// n'oblige le dos à venir de là. Un jeu apporte de belles figures et des dos
// qu'on n'aime pas, un autre n'apporte que des dos -- la cartothèque le tient
// alors pour vide, n'ayant pas une seule face à montrer -- et le treillis que
// le programme dessine va avec tout. C'est donc ici qu'on garde de quel jeu le
// dos est tiré, et ici qu'il est chargé.

struct FeltColour {
    int hue;
    QString name;
};

// Les draps qu'on peut tendre. Le premier est celui d'origine.
const QVector<FeltColour> &feltColours();
int defaultFeltHue();

// La couleur du profil courant, et comment on en change.
int feltHue();
void setFeltHue(int hue);

// Pose sur la cartothèque le jeu et le dos de ce profil: au démarrage, et
// chaque fois qu'on change de profil.
void applyProfileLook();

// Retient pour ce profil ce que les réglages viennent de choisir, et le pose
// aussi sur la cartothèque.
void rememberDeck(const QString &deckId);

// De quel jeu le dos est tiré. Vide veut dire « celui des cartes », qui est le
// cas ordinaire: un jeu vient d'habitude avec ses propres dos, et on les prend
// avec lui.
QString backDeckId();
void rememberBackDeck(const QString &deckId);

// Le jeu d'où le dos est tiré, chargé à la demande.
//
// Nul quand c'est le jeu dessiné, quand le fichier ne se lit pas, ou quand ce
// jeu-là n'apporte aucun dos: le treillis que le programme dessine est le
// recours dans les trois cas. À la différence du jeu des faces, un jeu sans
// aucune face compte ici -- ce sont ses dos qu'on lui demande.
const CardSet *backCards();

// Lequel de ses dos, toujours dans les bornes de `backCards()`. Négatif quand
// il n'y a pas de dos à tirer.
int backIndex();
void rememberBack(int at);

// Jette les dos mis à l'échelle, quand la taille des cartes change: ils sont
// gardés par taille, et en changer toutes les tailles d'un coup laisse
// derrière soi un cache qui ne resservira plus.
void forgetBackDrawings();
