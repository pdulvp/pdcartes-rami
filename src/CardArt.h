#pragma once

#include "Cards.h"
#include "Palette.h"

#include <QPixmap>
#include <QRectF>
#include <QSizeF>

class QPainter;
class QFont;

// Dessin du tapis et des cartes qui y sont posées.
//
// Tout est dessiné par le programme plutôt que chargé depuis des images. Un
// jeu de cartes en demanderait cinquante-quatre, ce qui supposerait soit un
// travail d'illustration, soit un dossier de fichiers à localiser à
// l'exécution et susceptible d'être absent. Dessinées, les cartes adoptent
// les couleurs de la palette courante, restent nettes à toute taille, et
// l'application demeure un fichier unique.

// Proportion conservée par une carte quelle que soit sa taille de rendu. Les
// cartes réelles mesurent 2,25 sur 3,5 pouces, rapport auquel l'œil est
// habitué.
//
// Cette valeur est celle des cartes dessinées par le programme. Un jeu
// installé fournit la sienne ; la disposition du tapis repose sur
// `cardAspect()`.
inline constexpr qreal kCardRatio = 0.642;
qreal cardAspect();

// Rayon d'arrondi des coins d'une carte de cette taille. La fonction est
// partagée afin qu'un aperçu de dos affiché ailleurs dans le programme
// adopte exactement le même arrondi que les cartes du tapis.
qreal cardCornerFor(const QRectF &box);
QSizeF cardSizeForHeight(qreal height);
QSizeF cardSizeForWidth(qreal width);

// Dessine le tapis : le tissu, éclairé en son centre, dont la trame est
// visible de près, bordé d'un galon.
//
// `braid` commande le galon. Il est omis lorsque le tapis sert de fond et non
// de table : le menu d'ouverture est affiché par-dessus, et un galon derrière
// une page de plaques ajouterait un encadrement sans objet.
void drawFelt(QPainter *painter, const QRectF &box, const Palette &scheme, bool braid = true);

// Couleur de remplissage d'une plaque posée sur le tapis.
//
// Définie ici plutôt que dans chaque page, le menu d'ouverture et la table
// partageant la même apparence : une formule répétée finirait par diverger
// d'un endroit à l'autre. La couleur est presque noire et translucide, afin
// que la trame reste perceptible au travers ; c'est ce qui donne à la plaque
// l'aspect d'un élément posé sur le tapis plutôt que découpé dedans.
//
// `lift` déplace la couleur vers l'or lorsque la plaque est sélectionnée ou
// mise en évidence.
QColor feltPlaqueFill(const Palette &scheme, qreal lift = 0.0);
// Couleur dorée des filets, à l'intensité demandée : discrète au repos,
// marquée lorsque l'élément est sélectionné.
QColor feltGold(const Palette &scheme, qreal strength);
// Couleur de fond des deux bandeaux, supérieur et inférieur. Plus sombre que
// celle des plaques, ces bandeaux bordant le tapis au lieu d'y être posés.
QColor feltBarFace(const Palette &scheme);

// Motif de trame seul, sous forme de tuile servant de brosse. Il est produit
// une fois par palette puis conservé : tracer ce quadrillage ligne à ligne
// sur une surface de la taille de la fenêtre représenterait un millier de
// segments à chaque rafraîchissement.
const QPixmap &weaveTile(const Palette &scheme);

// État de rendu d'une carte face visible. `lit` désigne la carte survolée par
// le pointeur, `chosen` une carte sélectionnée dans la main du joueur, et
// `glow` la carte qui vient d'être jouée — mise en évidence sans laquelle les
// coups des cinq autres joueurs seraient difficiles à suivre.
struct CardLook {
    bool lit = false;
    bool chosen = false;
    qreal glow = 0.0;       // de zéro à un, en fondu
    bool dimmed = false;    // carte non jouable dans l'état courant
    bool noShadow = false;  // pour dessiner une carte hors du tapis
};

void drawCardFace(QPainter *painter, const QRectF &box, const Card &card, const Palette &scheme,
                  const CardLook &look = CardLook{});

// Dessine une carte face cachée : le dos, dans la couleur de la palette.
void drawCardBack(QPainter *painter, const QRectF &box, const Palette &scheme,
                  const CardLook &look = CardLook{});

// Dessine un emplacement vide : la pioche épuisée, ou la défausse encore
// vierge. Le contour est en pointillés et porte un libellé, afin qu'un
// emplacement vide indique malgré tout ce qui doit s'y trouver.
void drawCardSlot(QPainter *painter, const QRectF &box, const Palette &scheme,
                  const QString &label);

// Retourne la police du peintre réinitialisée.
//
// `QPainter` conserve la dernière police qui lui a été donnée, et ces
// fonctions de dessin s'enchaînent sur le même peintre : un titre composé en
// capitales espacées laisserait le dessin suivant dans la même composition.
// Toutes les polices employées ci-dessous dérivent de cette fonction plutôt
// que de `painter->font()`, ce qui évite qu'une plaque hérite des réglages de
// la précédente.
QFont plainFont(const QPainter *painter);

// Dessine le symbole de couleur seul, pour une légende ou le coin d'une plaque.
void drawSuitPip(QPainter *painter, const QRectF &box, Suit suit, const QColor &ink);

// Dessine le médaillon d'un adversaire : un anneau à sa couleur, contenant un
// symbole et son initiale. Les six adversaires ont chacun leur symbole, de
// sorte qu'une plaque se reconnaisse à sa forme autant qu'à sa couleur.
//
// Un `emblem` négatif supprime le symbole : c'est le cas du médaillon de la
// plaque du joueur, le symbole servant précisément à distinguer les six
// adversaires entre eux.
void drawRivalEmblem(QPainter *painter, const QRectF &box, int emblem, const QString &initial,
                     const QColor &ink, const QColor &back);

// Dessine les étoiles de niveau : une pour un débutant, deux pour un
// intermédiaire, trois pour un expert.
void drawLevelStars(QPainter *painter, const QRectF &box, int stars, int outOf,
                    const QColor &ink, const QColor &empty);

// Dessine une jauge indiquant la progression à l'intérieur du niveau.
void drawGauge(QPainter *painter, const QRectF &box, qreal filled, const QColor &ink,
               const QColor &back);

// Dessine un petit tas de cartes face cachée, représentant la main d'un
// adversaire vue depuis l'autre côté de la table. `cards` est le nombre de
// cartes qu'il détient.
void drawHandPile(QPainter *painter, const QRectF &box, int cards, const Palette &scheme,
                  const QColor &tint);
