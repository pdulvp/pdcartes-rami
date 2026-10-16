#pragma once

class QMenu;

// Habille un menu contextuel aux couleurs de la table.
//
// Un menu est une fenêtre à lui et n'hérite donc de rien: ses couleurs
// viennent de currentPalette(), comme celles des boîtes de dialogue. Deux
// endroits en ouvrent -- la plaque du profil en haut, les cartes des
// adversaires au menu -- et un menu qui ne ressemble pas à l'autre se
// remarque tout de suite.
void dressMenu(QMenu *menu);
