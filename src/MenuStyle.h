#pragma once

#include <QIcon>
#include <QString>
#include <QStringList>

class QMenu;
class QToolButton;
class QWidget;

// Applique à un menu contextuel les couleurs de la palette courante.
//
// Un menu constitue une fenêtre distincte et n'hérite donc d'aucun style :
// ses couleurs sont prises dans `currentPalette()`, comme celles des boîtes
// de dialogue. Deux éléments en ouvrent — la plaque de profil du bandeau
// supérieur et les cartes d'adversaire du menu — et une différence
// d'apparence entre les deux serait immédiatement visible.
void dressMenu(QMenu *menu);

// Retourne l'icône d'un bouton, recolorée : celle du thème du bureau s'il en
// fournit une, sinon celle que l'application embarque.
//
// Un bureau Linux fournit son thème d'icônes, que l'application reprend afin
// de s'y intégrer ; Windows n'en propose pas, et les boutons y resteraient
// sans icône. Les mêmes icônes sont donc incluses dans le binaire
// (`:/icons/`) à titre de recours, ce qui préserve l'apparence du bureau là
// où il en existe un sans laisser l'autre plateforme dépourvue.
//
// Les noms sont essayés dans l'ordre, pour les deux sources : un bureau ne
// connaissant qu'une graphie ancienne obtient malgré tout son icône.
QIcon appIcon(const QStringList &names, const QColor &ink);

// Crée le petit bouton carré ne portant qu'une icône.
//
// Deux boîtes de dialogue l'emploient : les réglages, pour relire le dossier
// des jeux de cartes, et la fiche d'un adversaire, pour modifier son niveau
// et réinitialiser son expérience. Sa fonction est indiquée par son
// infobulle ; sur un bureau dont le thème ne fournit pas ces icônes, le
// bouton affiche à la place le libellé passé en `fallback`.
QToolButton *iconButton(QWidget *parent, const QStringList &iconNames, const QString &fallback,
                        const QString &tip);
