#pragma once

#include <QString>
#include <QStringList>

class QMenu;
class QToolButton;
class QWidget;

// Habille un menu contextuel aux couleurs de la table.
//
// Un menu est une fenêtre à lui et n'hérite donc de rien: ses couleurs
// viennent de currentPalette(), comme celles des boîtes de dialogue. Deux
// endroits en ouvrent -- la plaque du profil en haut, les cartes des
// adversaires au menu -- et un menu qui ne ressemble pas à l'autre se
// remarque tout de suite.
void dressMenu(QMenu *menu);

// Le petit bouton carré qui porte une icône du bureau et rien d'autre.
//
// Deux boîtes s'en servent -- les réglages pour relire le dossier des cartes,
// la fiche d'un adversaire pour changer ce qu'il est et remettre son
// expérience -- et un bouton qui ne ressemble pas à l'autre se remarque tout
// de suite. Ce qu'il fait tient dans son infobulle, sauf sur un bureau dont
// le thème ne porte pas ces icônes: là, il reprend son mot.
QToolButton *iconButton(QWidget *parent, const QStringList &iconNames, const QString &fallback,
                        const QString &tip);
