#pragma once

#include <QString>
#include <QVector>

// Les tables disponibles, et la durée d'une partie.
//
// Cinq tables, correspondant à cinq nombres de joueurs. Les règles ne varient
// pas de l'une à l'autre ; ce qui varie, c'est la proportion du jeu distribuée
// en mains. À deux joueurs, la pioche est profonde et une carte recherchée
// finit par réapparaître. À six, quatre-vingt-quatre cartes sur cent huit sont
// distribuées avant le premier tour, et la défausse devient presque la seule
// source d'approvisionnement.
//
// Toutes les tables partagent la même apparence : la couleur du tapis est une
// préférence, choisie dans les réglages et attachée au profil (voir
// TableLook.h). Une table de six n'a pas une couleur différente d'une table de
// deux ; elle compte seulement plus de joueurs.
struct TableKind {
    QString id;
    QString name;
    int seats;      // joueur humain compris
};

const QVector<TableKind> &tableKinds();
const TableKind &tableKindById(const QString &id);
QString defaultTableId();

// Durée d'une partie.
//
// Les scores sont des pénalités — les points restés en main lorsqu'un joueur
// sort — de sorte que le vainqueur est celui qui en totalise le moins, et que
// la partie s'arrête dès qu'un joueur dépasse l'objectif.
//
// L'objectif est exprimé en nombre de manches et non en points, le nombre de
// points correspondant à une partie de huit manches variant selon la table :
// en tête-à-tête, le perdant d'une manche a une carte de retard et paie sept
// ou huit points, alors qu'à six les cartes non placées totalisent une
// quarantaine de points par manche. Un objectif fixe de trois cents points
// représenterait quatre manches à une table et trente à l'autre. Le nombre de
// manches est donc choisi et les points en sont déduits d'après la table ; les
// deux sont affichés, le tableau des scores comptant en points.
struct Target {
    QString id;
    QString name;
    QString blurb;
    int manches;    // un pour une donne unique, la partie se limitant alors à
                    // cette donne
};

const QVector<Target> &targets();
const Target &targetById(const QString &id);
QString defaultTargetId();

// Objectif exprimé en points pour une table de `seats` joueurs. Retourne zéro
// pour la partie d'une seule manche, qui s'achève avec celle-ci.
int targetPointsFor(const Target &target, int seats);
