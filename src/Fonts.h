#pragma once

#include <QRectF>
#include <QString>

class QFont;
class QFontMetricsF;

// Polices d'affichage de l'application, chargées depuis les ressources du
// binaire.
//
// Elles sont incluses dans l'exécutable plutôt que livrées à côté : la
// distribution se réduit ainsi à un seul fichier, et aucune police ne peut
// manquer au démarrage.
//
// Il y en a trois, pour trois usages distincts. Le titre réclame une police
// haute et ornée ; la ligne qui le surmonte, affichée au cinquième de sa
// taille, réclame des lettres pleines ; le texte courant réclame une police
// de labeur, lisible sans attirer l'attention. Une police unique ne
// conviendrait à aucun des trois.
//
// Chacune est chargée une seule fois par session, à la première demande. Un
// nom vide signale une ressource absente — cas qui ne devrait pas se
// produire, les polices étant compilées dans le binaire ; l'appelant se
// rabat alors sur la police qui lui est passée en argument.

// Police du titre : `RAMI`, en Zen Tokyo Zoo.
QString signFamily();
// Police de la ligne supérieure : `PDCARTES`, en Zen Dots.
QString capsFamily();
// Police du texte courant, en M PLUS 2 : la ligne de l'écran titre et
// l'ensemble du texte de l'interface.
//
// Deux graisses sont chargées sous ce nom, le romain et le gras, cette
// police étant celle de toute l'application. Si le gras seul était fourni,
// Qt produirait le romain en amincissant le tracé, ce qui dégraderait
// l'ensemble du texte.
QString proseFamily();

// Applique cette police à l'ensemble de l'application, en conservant le
// corps défini par le système. Appelée une fois au démarrage : tout le texte
// de l'interface en hérite, les polices d'affichage étant demandées
// explicitement là où elles sont nécessaires.
void dressApplicationFont();

// Retourne `fallback` dans la famille `family`, au corps `pixels`. Si
// `family` est vide, le gras de la police système tient lieu de police
// d'affichage.
QFont familyFont(const QFont &fallback, const QString &family, qreal pixels);

// Retourne le rectangle à passer à `drawText` pour qu'un texte centré le soit
// sur la hauteur de ses capitales plutôt que sur la hauteur de ligne de la
// police.
//
// Qt centre sur la hauteur de ligne, qui s'étend du sommet des accents au bas
// des jambages. Dans une police latine, les deux centres coïncident presque
// et l'écart passe inaperçu. Dans une police japonaise, la hauteur de ligne
// est bien plus grande, devant loger un idéogramme et les signes qui le
// surmontent : un texte latin centré selon ce critère apparaît trop bas.
// L'écart est d'autant plus visible que le cadre est ajusté au texte, comme
// pour « 144 pts » dans sa pastille ou le niveau d'un adversaire dans la
// sienne.
QRectF capCentred(const QRectF &box, const QFontMetricsF &metrics);
