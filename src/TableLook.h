#pragma once

#include <QString>
#include <QVector>

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
void rememberBack(int at);
