#include "TableView.h"

#include "FeltArt.h"
#include "Fonts.h"
#include "Palette.h"
#include "Profiles.h"
#include "Rami.h"
#include "RivalGlance.h"
#include "Rivals.h"

#include "CardDeck.h"
#include "TableLook.h"

#include <QFontMetricsF>
#include <QLineF>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QPainter>
#include <QPainterPath>
#include <QTimer>

#include <algorithm>
#include <cmath>

namespace {

constexpr int kFrameMs = 16;

// Hauteur allouée à chaque zone du tapis, avant ajustement.
constexpr qreal kSeatsHeight = 104.0;
constexpr qreal kStacksWidth = 178.0;
// Le rail, situé au-dessus de la main, où la pose en cours est mise en
// attente.
//
// Ses cartes mesuraient 44 pixels lorsque celles du tapis en mesuraient 56 ;
// l'écart est devenu excessif depuis que celles du tapis en mesurent 96. Or
// ce sont précisément ces cartes qui vont être déposées sur le tapis : les
// afficher réduites puis au double de leur taille un instant plus tard
// produisait une discontinuité inutile.
constexpr qreal kRailHeight = 82.0;
constexpr qreal kPoseTall = 62.0;

// Oscillation des flèches de pioche : amplitude et période.
//
// Trois pixels, pour un aller-retour d'une seconde et demie. L'amplitude est
// suffisante pour que le mouvement soit perçu en vision périphérique et assez
// faible pour ne pas devenir gênant, les flèches restant affichées tant que le
// joueur n'a pas pioché.
// Vitesse des cartes animées, exprimée en fraction du trajet par image.
//
// Une seule valeur est employée pour tous les joueurs. La carte piochée par le
// joueur se déplaçait auparavant à 0,048 et celle d'un adversaire à 0,072,
// l'intention étant de rendre la première plus facile à suivre ;
// l'écartement de la main était également à 0,048. Deux vitesses différentes
// sur une même table sont toutefois perceptibles, et c'est la plus lente qui
// est remarquée : tout ce qui concernait la main du joueur paraissait plus
// lent que le reste.
//
// La pose conserve une vitesse légèrement inférieure, identique pour tous les
// joueurs.
constexpr qreal kFlightSpeed = 0.072;
constexpr qreal kLaySpeed = 0.055;

// Décalage entre deux combinaisons successives, exprimé en fraction du
// trajet.
//
// L'ouverture dépose l'intégralité de la pose sur le tapis en une seule
// opération, soit fréquemment trois combinaisons. Trois animations simultanées
// ne sont pas lisibles : le tapis se remplit sans que le détail soit
// perceptible. Un décalage les rend distinctes. À 0,055 par image, une valeur
// de 0,45 représente environ six images d'écart.
constexpr qreal kLayStagger = 0.45;

// Durée de l'animation de pioche du joueur, seule animation comportant un
// retournement de carte. Cette durée dépend de la distance à parcourir.
//
// Une vitesse exprimée en fraction du trajet par image fait aboutir toutes les
// animations en un même nombre d'images, quelle que soit leur longueur : une
// carte se rangeant près de la pioche et une carte traversant toute la main
// prennent le même temps, la seconde se déplaçant donc trois à quatre fois
// plus vite. C'est précisément celle-là dont le retournement n'était pas
// perceptible, alors que le trajet court convenait.
//
// La durée est donc constituée d'une valeur fixe augmentée d'un nombre de
// millisecondes par largeur de table parcourue, la distance étant rapportée à
// la largeur afin que le réglage reste valable quelle que soit la taille de la
// fenêtre.
//
// Les deux valeurs sont déduites de la géométrie de la table. La pioche se
// trouvant en haut à droite et la main en bas, la composante verticale
// intervient autant que la composante horizontale : pour une fenêtre de 1220
// sur 850, la position la plus proche est à 582 pixels et la plus éloignée à
// 1202, soit un rapport de deux et non de cinq.
//
//     près de la pioche      582 px    242 ms    38 px par image
//     position médiane       772 px    269 ms    46
//     extrémité opposée    1 202 px    330 ms    58
//
// Le trajet court conserve les 242 ms qui convenaient. Le trajet long reçoit
// le tiers supplémentaire nécessaire à la perception du retournement, sans
// aller au-delà : à une demi-seconde, l'animation devenait trop lente, et une
// pioche que l'on attend est plus gênante qu'une pioche mal perçue. L'écart de
// vitesse apparente passe ainsi d'un rapport de deux à un rapport d'un et
// demi, ce qui le rend imperceptible.
//
// C'est la seule animation réglée de cette façon ; les autres conservent la
// vitesse unique de la table, ce qui est précisément ce qui la rend
// imperceptible.
constexpr qreal kDrawMsBase = 159.0;
constexpr qreal kDrawMsPerWidth = 173.0;
constexpr qreal kDrawMsLeast = 200.0;
constexpr qreal kDrawMsMost = 380.0;

constexpr qreal kBeckonSwing = 3.0;
constexpr qreal kBeckonSpeed = 0.07;

// Débordement des tas par rapport au rectangle de leur carte : la pioche est
// représentée par un empilement dont chaque épaisseur dépasse en haut à
// gauche, la défausse laisse apparaître un bord de la carte inférieure, en
// haut à droite.
constexpr qreal kStockLayerStep = 1.6;
constexpr qreal kPileUnderStep = 3.0;

// Espacement entre les deux tas, sous le libellé de chacun.
//
// Il doit être plus important qu'il n'y paraît : le nombre de cartes de la
// pioche est inscrit sous celle-ci et le libellé « défausse » au-dessus de
// l'autre, de sorte que l'espacement doit loger deux lignes de texte et non
// une. À dix-huit pixels, la seconde ligne commençait là où la première se
// terminait et les deux tas paraissaient confondus.
constexpr qreal kStackGap = 36.0;
// Bandeau surmontant un tas, portant son libellé, et espacement conservé sous
// le texte, sans lequel le libellé toucherait la carte. Cette valeur est
// partagée entre le calcul de disposition et le rendu, qui doivent
// concorder.
constexpr qreal kStackLabel = 27.0;
constexpr qreal kStackLabelAir = 8.0;
// Corps des trois libellés du tapis. La police Zen Dots étant large, un corps
// réduit y reste lisible ; ces libellés désignant des zones et non des
// détails, ils doivent être lisibles à distance.
constexpr qreal kStackLabelSize = 13.0;

// Retrait de la plaque du joueur par rapport aux bords de sa colonne.
//
// Les deux cartes sont centrées et laissent déjà un espace de chaque côté,
// alors que la plaque occupait toute la largeur et venait au contact du bord
// de la fenêtre, à la marge près.
constexpr qreal kPlaqueInset = 13.0;

// Corps de texte d'une plaque d'adversaire, et hauteur de référence pour
// laquelle ils sont définis. Ce sont ceux employés par la carte du même
// adversaire dans le menu d'ouverture (`RivalCard`) : il s'agit du même
// adversaire décrit de la même façon, et des corps différents entre la table
// et le menu les feraient percevoir comme deux éléments distincts. Le score
// n'ayant pas d'équivalent au menu, il reprend le corps de la plaque du
// joueur, située juste en dessous, seul autre endroit où un score est
// affiché.
constexpr int kPlaqueNameSize = 15;
constexpr int kPlaqueScoreSize = 17;
constexpr int kPlaqueAsideSize = 11;
constexpr int kPlaqueBadgeSize = 10;
// Hauteur utile d'une plaque, marges supérieure et inférieure déduites,
// lorsque la fenêtre lui accorde toute sa place.
constexpr qreal kPlaqueInnerFull = kSeatsHeight - 14.0;

// Plaque du joueur, située sous la défausse : hauteur réservée dans la
// colonne et hauteur maximale accordée. Elle comporte moins d'éléments que
// celle d'un adversaire — ni étoiles, ni jauge, ni représentation de la main —
// et tient donc en deux lignes à côté de son médaillon.
constexpr qreal kMinePlaqueHeight = 60.0;
constexpr qreal kMinePlaqueMax = 72.0;
constexpr qreal kMinePlaqueInnerFull = kMinePlaqueHeight - 12.0;

// Retrait du libellé « LE TAPIS » par rapport au bord gauche du tapis.
constexpr qreal kTapisLabelPad = 14.0;

// Corps du texte d'une plaque, pour la hauteur dont elle dispose. La rangée
// de plaques se réduit avec la fenêtre (`min(kSeatsHeight, hauteur * 0.22)`),
// et un corps fixe y déborderait : le texte est donc réduit en proportion. Il
// ne peut en revanche pas être agrandi, le corps employé par le menu
// constituant un plafond et non une valeur de référence qu'une plaque de
// pleine hauteur multipliait d'un tiers.
int plaqueTextSize(qreal innerHeight, int atFullHeight, int floor,
                   qreal fullHeight = kPlaqueInnerFull) {
    const qreal scale = std::min(1.0, innerHeight / fullHeight);
    return std::max(floor, int(std::lround(atFullHeight * scale)));
}

int stockLayersFor(int left) { return std::min(3, std::max(1, left / 6 + 1)); }
constexpr qreal kHandHeight = 134.0;
constexpr qreal kMargin = 14.0;
constexpr qreal kGap = 10.0;

// Écartement des cartes d'une combinaison. Il laisse apparaître de chaque
// carte la portion nécessaire à la lecture de son coin, selon le principe
// d'une main disposée en éventail.
constexpr qreal kMeldStep = 0.60;

// Hauteur maximale et minimale accordée à une carte posée sur le tapis.
//
// Le plafond était fixé à 56 pixels, et la boucle de réduction par pas de
// quatre n'avait jamais à s'appliquer : même trente combinaisons tiennent à
// cette taille dans une fenêtre de dimensions courantes. Les cartes posées
// étaient donc systématiquement réduites sans nécessité, à la moitié de la
// taille de celles de la main, sur un tapis très largement vide.
//
// À 96 pixels, elles restent lisibles tant que le tapis est peu rempli, et la
// boucle de réduction retrouve son utilité lorsqu'il se remplit : 92 pixels à
// dix-huit combinaisons, 80 à vingt-quatre, 72 à trente, valeur qui demeure
// supérieure à celle appliquée auparavant en permanence.
constexpr qreal kMeldTall = 96.0;
constexpr qreal kMeldShort = 26.0;

// Plus grande carte tenant dans le rectangle donné, dans les deux dimensions.
//
// La disposition du tapis est calculée en hauteur, et la colonne de droite est
// le seul emplacement dont la largeur est fixée a priori : une carte plus
// large que sa colonne y débordait sur le tapis, situation que produisent
// précisément le zoom et un jeu aux cartes larges.
// Flèche indiquant un emplacement où piocher : un trait terminé par une
// pointe, orienté vers la droite, dans l'espace qui lui est réservé.
//
// Elle remplace le contour en pointillés qui entourait les deux tas. Un
// contour en pointillés autour d'une carte évoque une sélection, et il en
// entourait deux simultanément sans désigner l'une ou l'autre. Une flèche
// désigne un emplacement, et deux flèches côte à côte s'interprètent comme un
// choix entre les deux.
void drawTurnArrow(QPainter *painter, const QRectF &box, const QColor &ink) {
    // La flèche est courte et épaisse plutôt que longue et fine : l'information
    // réside dans sa pointe, et un trait long traversant la marge éloignait le
    // regard de la carte désignée. Elle n'occupe donc que la moitié de
    // l'espace disponible et reste proche de la carte.
    const qreal tall = std::min(box.height(), 26.0);
    const qreal head = tall * 0.64;
    const qreal span = box.width() * 0.5 + 4.0;
    if (span < head + 3.0) return;

    const qreal y = box.center().y();
    const qreal from = box.right() - span;
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(QPen(ink, std::max(2.5, tall * 0.26), Qt::SolidLine, Qt::RoundCap));
    painter->drawLine(QPointF(from, y), QPointF(box.right() - head * 0.8, y));

    painter->setPen(Qt::NoPen);
    painter->setBrush(ink);
    QPainterPath tip;
    tip.moveTo(box.right(), y);
    tip.lineTo(box.right() - head, y - head * 0.62);
    tip.lineTo(box.right() - head, y + head * 0.62);
    tip.closeSubpath();
    painter->drawPath(tip);
    painter->restore();
}

QSizeF cardWithin(qreal height, qreal width) {
    QSizeF card = cardSizeForHeight(height);
    if (card.width() > width && card.width() > 0.0) {
        card = cardSizeForHeight(height * width / card.width());
    }
    return card;
}

// Élévation d'une carte sélectionnée au-dessus de la main.
constexpr qreal kLift = 18.0;

// Bornes du zoom et pas de la molette. En deçà de la borne inférieure, les
// figures deviennent illisibles ; au-delà de la borne supérieure, la main de
// quatorze cartes ne tient plus en largeur.
constexpr int kZoomMin = 80;
constexpr int kZoomMax = 200;
constexpr int kZoomStep = 10;

// Durée d'une animation de pioche couvrant `reach` pixels sur une table large
// de `across`.
qreal drawSpeedFor(qreal reach, qreal across) {
    const qreal part = across > 1.0 ? std::clamp(reach / across, 0.0, 2.0) : 0.5;
    const qreal ms = std::clamp(kDrawMsBase + kDrawMsPerWidth * part, kDrawMsLeast, kDrawMsMost);
    return qreal(kFrameMs) / ms;
}

qreal plateau(qreal along) {
    // Départ rapide et arrivée amortie : une carte lancée sur une table ne se
    // déplace pas à vitesse constante.
    const qreal at = std::clamp(along, 0.0, 1.0);
    return 1.0 - (1.0 - at) * (1.0 - at);
}

// Fond d'une plaque, qu'il s'agisse de celle d'un adversaire ou de celle du
// joueur.
//
// Sombre et translucide, bordé d'un filet doré, comme les cartes du menu
// d'ouverture : un panneau clair sur un tapis sombre romprait la continuité
// visuelle, et les deux pages doivent présenter la même apparence. La plaque
// du joueur dont c'est le tour est légèrement éclaircie et son filet prend sa
// couleur propre, seul élément permettant de distinguer les six adversaires
// d'un coup d'œil.
void paintPlaqueBody(QPainter *painter, const QRectF &box, const Palette &scheme,
                     const QColor &mine, bool active) {
    QPainterPath shape;
    shape.addRoundedRect(box, 11, 11);

    painter->setPen(Qt::NoPen);
    QColor body = feltPlaqueFill(scheme, active ? 0.10 : 0.0);
    if (active) body = blendColours(body, mine, 0.14);
    painter->fillPath(shape, body);

    painter->setBrush(Qt::NoBrush);
    painter->setPen(QPen(active ? mine : feltGold(scheme, 0.26), active ? 2.4 : 1.0));
    painter->drawPath(shape);
}

// Couleurs du texte d'une plaque, principale et secondaire : claires, le fond
// étant sombre.
QColor plaqueInk(const Palette &scheme) { return scheme.feltText; }

QColor plaqueAside(const Palette &scheme) {
    QColor ink = scheme.feltText;
    ink.setAlphaF(0.62);
    return ink;
}

} // namespace

TableView::TableView(QWidget *parent) : QWidget(parent) {
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setMinimumSize(760, 520);
    setAutoFillBackground(false);

    m_ticker = new QTimer(this);
    m_ticker->setInterval(kFrameMs);
    connect(m_ticker, &QTimer::timeout, this, &TableView::tick);

    // Aperçu d'un adversaire, affiché pendant le survol de sa plaque : son
    // tempérament et son expérience, consultables sans ouvrir sa fiche au
    // cours d'une manche.
    m_glanceBeat = new QTimer(this);
    m_glanceBeat->setSingleShot(true);
    m_glanceBeat->setInterval(kGlanceDelayMs);
    connect(m_glanceBeat, &QTimer::timeout, this, [this] {
        if (!m_game || m_hoveredSeat < 0 || m_hoveredSeat >= m_game->seatCount()) return;
        const QString who = m_game->seat(m_hoveredSeat).rivalId;
        for (const Spot &spot : m_seatBoxes) {
            if (spot.id != m_hoveredSeat) continue;
            // Position de la plaque en coordonnées d'écran : l'aperçu est
            // affiché en dessous, sur le tapis, où il ne masque ni la plaque
            // ni la main du joueur.
            showRivalGlance(who, QRect(mapToGlobal(spot.box.topLeft().toPoint()),
                                       spot.box.size().toSize()),
                            this);
            return;
        }
    });
}

void TableView::watch(const Rami *game) {
    m_game = game;
    m_chosen.clear();
    m_glow.clear();
    m_flights.clear();
    m_handWas.clear();
    m_slideFrom.clear();
    m_slideAlong = 1.0;
    relayout();
    update();
}

void TableView::setChosen(const QVector<int> &cards) {
    m_chosen = cards;
    Q_EMIT chosenChanged();
    update();
}

void TableView::clearChosen() {
    if (m_chosen.isEmpty()) return;
    m_chosen.clear();
    Q_EMIT chosenChanged();
    update();
}

void TableView::say(const QString &line) {
    m_line = line;
    m_trouble = false;
    // La ligne d'état change au moment où le tour passe : c'est l'occasion de
    // relancer la minuterie afin que les flèches reprennent leur oscillation.
    // Elle s'arrête d'elle-même en l'absence d'animation.
    wake();
    update();
}

void TableView::sayTrouble(const QString &line) {
    m_line = line;
    m_trouble = true;
    update();
}

void TableView::refresh() {
    relayout();
    update();
}

void TableView::cardsOnScreen(QVector<Card> *cards, QVector<QPointF> *at) const {
    if (!cards || !at || !m_game) return;
    const QSizeF card = cardSizeForHeight(m_meldHeight);
    for (const Spot &spot : m_meldBoxes) {
        const Meld *meld = m_game->meldById(spot.id);
        if (!meld) continue;
        qreal x = spot.box.left() + 11.0;
        for (const Card &held : meld->cards) {
            cards->append(held);
            at->append(QPointF(x + card.width() / 2.0, spot.box.center().y()));
            x += card.width() * kMeldStep;
        }
    }
}

void TableView::stopAnimations() {
    m_flights.clear();
    m_glow.clear();
    m_ticker->stop();
    update();
}

QRectF TableView::arrowBox(const QRectF &card) const {
    // Treize pixels entre la pointe et la carte, la flèche étant centrée
    // verticalement sur celle-ci.
    //
    // La position est calculée sur le rectangle de la carte, identique pour les
    // deux tas, et non sur le débordement qui l'entoure — l'empilement de la
    // pioche, le bord de la carte sous la défausse. Ce second calcul a été
    // essayé : les dessins de dos et de face ne ménageant pas la même marge au
    // bord de leur image, il ne rapprochait rien et décalait durablement les
    // deux flèches de trois pixels.
    const qreal from = m_stacks.left() + 2.0;
    return QRectF(from, card.center().y() - 13.0, card.left() - 13.0 - from, 26.0);
}

bool TableView::beckoning() const {
    if (!m_game || m_game->seatCount() < 2) return false;
    if (m_game->phase() != Phase::Draw) return false;
    if (!m_game->seat(m_game->turn()).human()) return false;
    return m_game->stockLeft() > 0 || !m_game->pile().isEmpty();
}

QRect TableView::beckonArea() const {
    if (!m_game) return QRect();
    QRectF area;
    if (m_game->stockLeft() > 0) area = area.united(arrowBox(m_stockBox));
    if (!m_game->pile().isEmpty()) area = area.united(arrowBox(m_pileBox));
    if (area.isNull()) return QRect();
    return area.adjusted(-kBeckonSwing - 3.0, -3.0, kBeckonSwing + 3.0, 3.0).toAlignedRect();
}

void TableView::noteTakenBack(const QVector<int> &cardIds) {
    if (!m_game || cardIds.isEmpty()) return;

    // Les cartes sont déjà revenues en main : la disposition est donc calculée
    // d'abord, afin de connaître la position de chacune, puis chaque carte est
    // animée depuis la position qu'elle occupait sur le rail.
    relayout();

    for (int id : cardIds) {
        const QRectF leaves = m_poseWas.value(id);
        if (leaves.isNull()) continue;
        QRectF to;
        Card flying;
        for (const Spot &spot : m_handBoxes) {
            if (spot.id != id) continue;
            to = spot.box;
        }
        int me = -1;
        for (int at = 0; at < m_game->seatCount(); ++at) {
            if (m_game->seat(at).human()) me = at;
        }
        if (to.isNull() || me < 0) continue;
        const Card *held = findCard(m_game->seat(me).hand, id);
        if (!held) continue;
        flying = *held;

        Flight flight;
        flight.card = flying;
        flight.faceUp = true;
        flight.size = leaves.size();
        flight.from = leaves.topLeft();
        flight.to = to.topLeft();
        flight.grow = to.size();
        flight.speed = kFlightSpeed;
        m_flights.append(flight);
        m_glow[id] = 1.0;
    }
    wake();
}

bool TableView::inFlight(int cardId) const {
    for (const Flight &flight : m_flights) {
        if (flight.card.id == cardId) return true;
    }
    return false;
}

void TableView::wake() {
    if (m_ticker && !m_ticker->isActive()) m_ticker->start();
}

QPointF TableView::seatAnchor(int at) const {
    for (const Spot &spot : m_seatBoxes) {
        if (spot.id == at) return spot.box.center();
    }
    // Les cartes du joueur partent de sa main.
    return QPointF(m_handRow.center().x(), m_handRow.center().y());
}

void TableView::noteDiscard(int seat, const Card &card) {
    if (!card.valid() || m_pileBox.isNull()) return;
    Flight flight;
    flight.card = card;
    flight.faceUp = true;
    flight.to = m_pileBox.topLeft();
    flight.speed = kFlightSpeed;

    // La carte du joueur part de la position qu'elle occupait dans sa main.
    //
    // `seatAnchor()` ne connaît que les plaques d'adversaire ; pour le joueur,
    // elle retournait le centre de la rangée, d'où aucune carte ne part
    // jamais. La disposition de la main n'étant pas encore recalculée à cet
    // instant, la position de la carte y figure toujours.
    QRectF leaves;
    if (m_game && m_game->seat(seat).human()) leaves = m_handWas.value(card.id);
    if (leaves.isNull()) {
        flight.size = m_pileBox.size();
        flight.from =
            seatAnchor(seat) - QPointF(flight.size.width() / 2, flight.size.height() / 2);
    } else {
        // La carte est réduite en cours de déplacement, la main étant dessinée
        // à une taille supérieure à celle des deux tas : c'est l'inverse du
        // trajet effectué lors de la pioche.
        flight.size = leaves.size();
        flight.from = leaves.topLeft();
        flight.grow = m_pileBox.size();
    }

    m_flights.append(flight);
    m_glow[card.id] = 1.0;
    wake();
}

void TableView::noteDrawn(int seat, bool fromPile, const Card &card) {
    if (!card.valid() || !m_game) return;

    // La carte du joueur est animée du tas jusqu'à la position qu'elle vient
    // d'occuper dans sa main. C'est le seul moyen d'en suivre la destination :
    // la main est retriée automatiquement après la pioche, et une quinzième
    // carte triée par couleur s'insère au milieu des quatorze autres.
    if (m_game->seat(seat).human()) {
        // La disposition est calculée d'abord, la position de destination y
        // étant lue.
        relayout();
        QRectF landing;
        for (const Spot &spot : m_handBoxes) {
            if (spot.id == card.id) landing = spot.box;
        }
        if (!landing.isNull()) {
            Flight flight;
            flight.card = card;
            flight.faceUp = true;
            // Une carte venant de la pioche se retourne en cours de
            // déplacement : elle était face cachée sur le tas et n'est
            // identifiable qu'une fois en main. Une carte venant de la défausse
            // ne se retourne pas, étant déjà face visible.
            flight.flips = !fromPile;
            flight.size = (fromPile ? m_pileBox : m_stockBox).size();
            flight.from = (fromPile ? m_pileBox : m_stockBox).topLeft();
            flight.to = landing.topLeft();
            // Durée permettant de percevoir le retournement, d'autant plus
            // longue que le trajet l'est ; une carte venant de la défausse ne
            // se retournant pas, elle suit la vitesse ordinaire de la table.
            flight.speed = flight.flips
                               ? drawSpeedFor(QLineF(flight.from, flight.to).length(), width())
                               : kFlightSpeed;
            flight.grow = landing.size();
            m_flights.append(flight);
        }
        wake();
        return;
    }

    // La carte d'un adversaire est animée du tas vers sa plaque : face visible
    // si elle provient de la défausse, cette carte ayant été vue de tous ; face
    // cachée si elle provient de la pioche, auquel cas seule l'origine du
    // tirage est connue.
    Flight flight;
    flight.card = card;
    flight.faceUp = fromPile;
    flight.size = m_pileBox.size();
    flight.from = (fromPile ? m_pileBox : m_stockBox).topLeft();
    flight.to = seatAnchor(seat) - QPointF(flight.size.width() / 2, flight.size.height() / 2);
    flight.speed = kFlightSpeed;
    m_flights.append(flight);
    wake();
}

void TableView::noteLaid(const QVector<int> &cardIds, int by) {
    for (int id : cardIds) m_glow[id] = 1.0;
    if (!m_game) {
        wake();
        return;
    }

    // La disposition est calculée d'abord : la combinaison se trouvant sur le
    // tapis, c'est là que la position de chacune de ses cartes est lue.
    relayout();

    // Les combinaisons sont animées dans leur ordre d'arrivée, chacune
    // attendant le départ de la précédente.
    QVector<int> order;

    for (int id : cardIds) {
        QRectF to;
        Card flying;
        int owner = -1;
        int which = -1;
        for (const Spot &spot : m_meldBoxes) {
            const Meld *meld = m_game->meldById(spot.id);
            if (!meld) continue;
            for (int k = 0; k < meld->cards.size(); ++k) {
                if (meld->cards.at(k).id != id) continue;
                to = meldCardBox(spot.box, k);
                flying = meld->cards.at(k);
                owner = meld->owner;
                which = meld->id;
            }
        }
        // Une pose non encore déposée sur le tapis se trouve sur le rail,
        // d'où elle doit être animée. La pose s'effectue en deux étapes — de
        // la main vers le rail, puis du rail vers le tapis — qui doivent
        // toutes deux être perceptibles.
        if (to.isNull()) {
            for (const Spot &spot : m_poseBoxes) {
                const Meld *meld = m_game->meldById(spot.id);
                if (!meld) continue;
                for (int k = 0; k < meld->cards.size(); ++k) {
                    if (meld->cards.at(k).id != id) continue;
                    to = poseCardBox(spot.box, k);
                    flying = meld->cards.at(k);
                    owner = meld->owner;
                    which = meld->id;
                }
            }
        }
        if (to.isNull() || owner < 0 || owner >= m_game->seatCount()) continue;

        // L'origine de l'animation identifie l'auteur du coup : une carte
        // partant de la plaque d'un adversaire indique que c'est lui qui l'a
        // posée, information qu'un tapis se remplissant sans animation ne
        // donnait pas.
        if (which >= 0 && !order.contains(which)) order.append(which);

        Flight flight;
        flight.card = flying;
        flight.faceUp = true;
        flight.to = to.topLeft();
        flight.grow = to.size();
        flight.speed = kLaySpeed;
        // Une valeur négative indique que la carte est en cours de
        // déplacement : elle n'est donc dessinée nulle part, n'ayant pas
        // encore quitté sa plaque. Voir `paintFlights()`.
        const qsizetype rank = std::max(qsizetype(0), order.indexOf(which));
        flight.along = -kLayStagger * qreal(rank);

        // L'auteur du coup, qui n'est pas nécessairement le propriétaire de la
        // combinaison : une carte ajoutée à la combinaison d'un adversaire part
        // de la main de celui qui l'ajoute.
        const int player = (by >= 0 && by < m_game->seatCount()) ? by : owner;

        const QRectF leaves = m_handWas.value(id);
        if (m_game->seat(player).human() && !leaves.isNull()) {
            flight.size = leaves.size();
            flight.from = leaves.topLeft();
        } else {
            flight.size = m_pileBox.size();
            flight.from =
                seatAnchor(player) - QPointF(flight.size.width() / 2, flight.size.height() / 2);
        }
        m_flights.append(flight);
    }
    wake();
}

void TableView::tick() {
    bool busy = false;

    for (Flight &flight : m_flights) {
        flight.along += flight.speed;
        if (flight.along < 1.0) busy = true;
    }
    m_flights.erase(std::remove_if(m_flights.begin(), m_flights.end(),
                                   [](const Flight &flight) { return flight.along >= 1.0; }),
                    m_flights.end());

    for (auto it = m_glow.begin(); it != m_glow.end();) {
        it.value() -= 0.016;
        if (it.value() <= 0.0) {
            it = m_glow.erase(it);
        } else {
            busy = true;
            ++it;
        }
    }

    if (m_slideAlong < 1.0) {
        m_slideAlong += kFlightSpeed;
        if (m_slideAlong >= 1.0) {
            m_slideAlong = 1.0;
            m_slideFrom.clear();
        }
        busy = true;
    }

    const bool nudging = beckoning();
    if (nudging) {
        m_beckon += kBeckonSpeed;
        if (m_beckon > 2.0 * M_PI) m_beckon -= 2.0 * M_PI;
    }

    if (busy) {
        update();
    } else if (nudging) {
        // Rien d'autre ne change : seules les deux flèches sont redessinées,
        // afin qu'un tour d'attente ne provoque pas le rafraîchissement
        // complet du tapis soixante fois par seconde.
        update(beckonArea());
    } else {
        update();
        m_ticker->stop();
    }
}

// ---------------------------------------------------------------------------
// Calcul de la disposition.

void TableView::setZoom(int percent) {
    const int now = qBound(kZoomMin, percent, kZoomMax);
    if (now == m_zoom) return;
    m_zoom = now;
    // Les images mises à l'échelle sont mises en cache par taille : un
    // changement global de taille rend l'intégralité du cache inutilisable.
    if (const CardSet *set = CardDecks::instance().current()) set->forgetDrawings();
    forgetBackDrawings();
    relayout();
    update();
    Q_EMIT zoomChanged(now);
}

void TableView::wheelEvent(QWheelEvent *event) {
    // Le zoom s'effectue avec Ctrl et la molette, selon l'usage courant. La
    // molette seule n'est pas interceptée : elle est actionnée par inadvertance
    // en cours de partie, et un changement de taille non sollicité serait
    // indésirable.
    if (!event->modifiers().testFlag(Qt::ControlModifier)) {
        QWidget::wheelEvent(event);
        return;
    }
    const int turn = event->angleDelta().y();
    if (turn != 0) setZoom(m_zoom + (turn > 0 ? kZoomStep : -kZoomStep));
    event->accept();
}

void TableView::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    relayout();
}

void TableView::relayout() {
    // Les positions précédentes deviennent caduques si les dimensions de la
    // table changent.
    if (m_slideAlong >= 1.0) m_slideFrom.clear();
    const QRectF all = QRectF(rect()).adjusted(kMargin, kMargin, -kMargin, -kMargin);
    if (all.width() < 80 || all.height() < 80) return;

    // Les deux bornes de chaque zone sont affectées par le zoom, et non
    // seulement la borne inférieure : dans le cas contraire, rien ne change à
    // cent pour cent — comportement voulu — mais au-delà la zone conserve sa
    // taille et la carte déborde.
    const qreal z = m_zoom / 100.0;
    const qreal seats = std::min(kSeatsHeight, all.height() * 0.22);
    const qreal hand = std::min(kHandHeight * z, all.height() * 0.26 * z);
    const qreal rail = std::min(kRailHeight * z, all.height() * 0.12 * z);

    m_seatsRow = QRectF(all.left(), all.top(), all.width(), seats);
    m_handRow = QRectF(all.left(), all.bottom() - hand, all.width(), hand);
    m_rail = QRectF(all.left(), m_handRow.top() - rail - kGap * 0.4, all.width(), rail);

    const qreal middleTop = m_seatsRow.bottom() + kGap;
    const qreal middleHeight = m_rail.top() - kGap - middleTop;
    const qreal stacks = std::min(kStacksWidth * z, all.width() * 0.22 * z);
    m_tapis = QRectF(all.left(), middleTop, all.width() - stacks - kGap,
                     std::max(60.0, middleHeight));
    m_stacks = QRectF(m_tapis.right() + kGap, middleTop, stacks, std::max(60.0, middleHeight));

    layoutSeats();
    layoutMelds();
    layoutHand();

    // La pioche et la défausse, superposées dans la colonne de droite, avec
    // l'espace nécessaire en dessous pour la plaque du joueur.
    const qreal cardHeight = std::min(104.0 * z, std::max(56.0 * z, m_stacks.height() * 0.30));
    const QSizeF card = cardWithin(cardHeight, m_stacks.width() - 10.0);
    const qreal left = m_stacks.left() + (m_stacks.width() - card.width()) / 2.0;
    const qreal labels = kStackLabel;
    m_stockBox = QRectF(left, m_stacks.top() + labels, card.width(), card.height());
    // L'écartement est réduit lorsque la colonne est courte, plutôt que de
    // faire sortir la plaque du joueur : dans une fenêtre de petite taille,
    // deux tas rapprochés sont préférables à un score non visible. L'espace
    // requis comprend la hauteur des deux cartes, les deux lignes de libellé,
    // l'espacement sous la défausse et la hauteur de la plaque ; celle-ci
    // comporte un médaillon et deux lignes depuis qu'elle a été alignée sur
    // les autres, et l'espace réservé auparavant la tronquait.
    const qreal spare = m_stacks.height() -
                        (labels * 2.0 + card.height() * 2.0 + 20.0 + kMinePlaqueHeight);
    // La valeur plancher tient compte du contenu de cet écartement : le
    // nombre de cartes inscrit sous la pioche, puis le libellé de la défausse.
    // À dix-huit pixels, les deux se touchaient, le libellé ayant été
    // agrandi.
    const qreal stackGap = std::clamp(spare, 24.0, kStackGap);
    m_pileBox =
        QRectF(left, m_stockBox.bottom() + labels + stackGap, card.width(), card.height());
}

void TableView::layoutSeats() {
    m_seatBoxes.clear();
    if (!m_game || m_game->seatCount() < 2) return;

    // Tous les joueurs sauf le joueur humain : ses cartes sont affichées
    // devant lui et sa plaque sous la défausse, où l'espace est disponible.
    QVector<int> others;
    for (int at = 0; at < m_game->seatCount(); ++at) {
        if (!m_game->seat(at).human()) others.append(at);
    }
    if (others.isEmpty()) return;

    const qreal gap = 10.0;
    const qreal widest = 250.0;
    qreal width = (m_seatsRow.width() - gap * (others.size() - 1)) / others.size();
    width = std::min(width, widest);
    const qreal span = width * others.size() + gap * (others.size() - 1);
    qreal at = m_seatsRow.left() + (m_seatsRow.width() - span) / 2.0;
    for (int seat : others) {
        m_seatBoxes.append({seat, QRectF(at, m_seatsRow.top(), width, m_seatsRow.height())});
        at += width + gap;
    }
}

void TableView::layoutMelds() {
    m_meldBoxes.clear();
    if (!m_game) return;

    // Les combinaisons sont regroupées par auteur afin de rendre le tapis
    // lisible : vingt combinaisons disposées sans ordre sont difficiles à
    // parcourir, alors que les mêmes réparties en six groupes le sont. Le
    // groupe n'identifie plus son auteur — une combinaison posée n'appartient
    // à personne — mais indique seulement que ces combinaisons ont été posées
    // ensemble, ce qui suffit à les distinguer les unes des autres.
    QVector<Meld> order = m_game->table();
    std::stable_sort(order.begin(), order.end(),
                     [](const Meld &a, const Meld &b) { return a.owner < b.owner; });
    if (order.isEmpty()) return;

    // La taille des cartes est réduite jusqu'à ce que l'ensemble du tapis
    // tienne dans l'espace disponible. Deux itérations suffisent en pratique,
    // mais la boucle est peu coûteuse et l'absence de réduction ferait déborder
    // le tapis.
    const qreal z = m_zoom / 100.0;
    for (qreal height = kMeldTall * z; height >= kMeldShort * z; height -= 4.0 * z) {
        const QSizeF card = cardSizeForHeight(height);
        const qreal pad = 5.0;
        const qreal rowHeight = card.height() + pad * 2;
        QVector<Spot> laid;
        qreal x = m_tapis.left() + 6.0;
        // Sous le libellé, comme la pioche est placée sous le sien.
        qreal y = m_tapis.top() + kStackLabel;
        bool fits = true;
        int lastOwner = -2;
        for (const Meld &meld : order) {
            const qreal width =
                pad * 2 + card.width() + card.width() * kMeldStep * (meld.cards.size() - 1);
            // Le groupe d'un nouvel auteur commence sur une nouvelle ligne
            // lorsque la ligne courante est presque remplie, afin qu'un groupe
            // ne soit pas scindé pour une seule carte.
            const bool fresh = meld.owner != lastOwner;
            if (x + width > m_tapis.right() - 6.0 ||
                (fresh && lastOwner != -2 && x + width > m_tapis.right() - 60.0)) {
                x = m_tapis.left() + 6.0;
                y += rowHeight + 8.0;
            }
            if (y + rowHeight > m_tapis.bottom() - 4.0) {
                fits = false;
                break;
            }
            laid.append({meld.id, QRectF(x, y, width, rowHeight)});
            x += width + 9.0;
            lastOwner = meld.owner;
        }
        if (fits) {
            m_meldBoxes = laid;
            m_meldHeight = height;
            return;
        }
    }
    m_meldHeight = kMeldShort * z;
}

QRectF TableView::meldCardBox(const QRectF &spot, int index) const {
    const QSizeF card = cardSizeForHeight(m_meldHeight);
    // Six pixels de marge de sécurité, contre onze auparavant, lorsqu'un trait
    // identifiait l'auteur de la pose.
    return QRectF(spot.left() + 6.0 + card.width() * kMeldStep * index,
                  spot.center().y() - card.height() / 2.0, card.width(), card.height());
}

QRectF TableView::handBoxNow(int id, const QRectF &box) const {
    const auto slid = m_slideFrom.constFind(id);
    if (slid == m_slideFrom.constEnd()) return box;
    QRectF now = box;
    now.moveLeft(slid->left() + (box.left() - slid->left()) * plateau(m_slideAlong));
    return now;
}

QRectF TableView::poseCardBox(const QRectF &spot, int index) const {
    const QSizeF card = cardSizeForHeight(spot.height());
    return QRectF(spot.left() + card.width() * kMeldStep * index, spot.top(),
                  card.width(), card.height());
}

QRectF TableView::handSlot(int index, int count) const {
    if (count <= 0 || index < 0 || index >= count) return QRectF();
    const qreal z = m_zoom / 100.0;
    const qreal height = std::min(m_handRow.height() - kLift - 6.0, 118.0 * z);
    const QSizeF card = cardSizeForHeight(std::max(52.0, height));
    const qreal step = count > 1 ? std::min(card.width() + 7.0,
                                            (m_handRow.width() - card.width()) / (count - 1))
                                 : 0.0;
    const qreal span = card.width() + step * (count - 1);
    const qreal left = m_handRow.left() + std::max(0.0, (m_handRow.width() - span) / 2.0);
    const qreal top = m_handRow.bottom() - card.height();
    return QRectF(left + step * index, top, card.width(), card.height());
}

void TableView::layoutHand() {
    m_handBoxes.clear();
    m_poseBoxes.clear();
    m_poseCardBoxes.clear();
    if (!m_game) return;

    int me = -1;
    for (int at = 0; at < m_game->seatCount(); ++at) {
        if (m_game->seat(at).human()) me = at;
    }
    if (me < 0) return;

    const Seat &mine = m_game->seat(me);
    const int count = mine.hand.size();

    // Position de départ de chaque carte, lorsqu'elle se déplace.
    //
    // Plutôt que de traiter chaque coup séparément — insertion d'une carte
    // piochée, retrait d'une carte défaussée, retrait de plusieurs cartes lors
    // d'une pose — la main est simplement comparée à son état précédent :
    // toute carte ayant changé de position est animée. Un coup modifiant la
    // main est ainsi animé sans que cette fonction ait à en connaître la
    // nature.
    //
    // Ce traitement est désactivé lors d'un redimensionnement de la fenêtre :
    // les cartes n'ont alors pas changé de position dans la main, c'est la
    // disposition qui a changé, et les animer à chaque pixel de
    // redimensionnement produirait un retard à l'affichage.
    const bool sameRow = m_handRow == m_handRowWas;
    QHash<int, QRectF> moving;
    for (int i = 0; i < count; ++i) {
        const int id = mine.hand.at(i).id;
        const QRectF box = handSlot(i, count);
        m_handBoxes.append({id, box});
        const auto before = m_handWas.constFind(id);
        if (sameRow && before != m_handWas.constEnd() &&
            std::abs(before->left() - box.left()) > 0.5) {
            moving.insert(id, *before);
        }
        m_handWas.insert(id, box);
    }
    m_handRowWas = m_handRow;
    if (!moving.isEmpty()) {
        m_slideFrom = moving;
        m_slideAlong = 0.0;
        // La minuterie doit également être relancée : le tri de la main après
        // une pioche ne déclenche aucune animation de carte, et rien d'autre ne
        // la relancerait. La main restait alors dessinée à son ancienne
        // position alors que la détection des clics portait déjà sur les
        // nouvelles.
        wake();
    }

    const qreal z = m_zoom / 100.0;

    // La pose en cours, sur le rail : à une taille réduite et à gauche de la
    // ligne d'état, s'agissant d'une pose du joueur et non encore achevée.
    if (mine.waiting.isEmpty()) return;
    const QSizeF small = cardSizeForHeight(std::min(kPoseTall * z, m_rail.height() - 10.0));
    qreal x = m_rail.left() + 8.0;
    for (const Meld &meld : mine.waiting) {
        const qreal width = small.width() + small.width() * kMeldStep * (meld.cards.size() - 1);
        const QRectF spot(x, m_rail.center().y() - small.height() / 2.0, width,
                          small.height());
        m_poseBoxes.append({meld.id, spot});
        for (int k = 0; k < meld.cards.size(); ++k) {
            const QRectF box = poseCardBox(spot, k);
            m_poseCardBoxes.append({meld.cards.at(k).id, box});
            m_poseWas.insert(meld.cards.at(k).id, box);
        }
        x += width + 12.0;
    }
}

// ---------------------------------------------------------------------------
// Rendu.

void TableView::paintEvent(QPaintEvent *) {
    const Palette &scheme = currentPalette();
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    drawFelt(&painter, QRectF(rect()), scheme);
    if (!m_game || m_game->seatCount() < 2) return;

    paintSeats(&painter);
    paintMelds(&painter);
    paintStacks(&painter);
    paintRail(&painter);
    paintHand(&painter);
    paintFlights(&painter);
}

void TableView::paintSeatPlaque(QPainter *painter, const QRectF &box, int at) {
    const Palette &scheme = currentPalette();
    const Seat &seat = m_game->seat(at);
    const QColor mine = rivalColour(seat.hue);
    const bool active = m_game->turn() == at && m_game->phase() != Phase::HandOver &&
                        m_game->phase() != Phase::MatchOver;

    paintPlaqueBody(painter, box, scheme, mine, active);

    const QRectF inner = box.adjusted(9, 7, -9, -7);
    const qreal disc = std::min(inner.height() * 0.62, 44.0);
    const QRectF medallion(inner.left(), inner.top(), disc, disc);
    QString initial = seat.name.left(1);
    int emblem = 0;
    if (!seat.rivalId.isEmpty()) emblem = rivalById(seat.rivalId).emblem;
    drawRivalEmblem(painter, medallion, emblem, initial, mine,
                    blendColours(feltPlaqueFill(scheme), mine, 0.14));

    const qreal textLeft = medallion.right() + 9.0;
    const qreal textWidth = inner.right() - textLeft;

    QFont name = plainFont(painter);
    name.setBold(true);
    name.setPixelSize(plaqueTextSize(inner.height(), kPlaqueNameSize, 11));
    painter->setFont(name);
    painter->setPen(plaqueInk(scheme));
    const QFontMetricsF nameMetrics(name);
    painter->drawText(QRectF(textLeft, inner.top(), textWidth * 0.62, nameMetrics.height()),
                      Qt::AlignLeft | Qt::AlignVCenter, seat.name);

    // Le score, à droite et en grand corps : c'est la seule valeur
    // déterminante, et le résultat de la manche écoulée est inscrit en
    // dessous.
    QFont score = plainFont(painter);
    score.setBold(true);
    score.setPixelSize(plaqueTextSize(inner.height(), kPlaqueScoreSize, 12));
    painter->setFont(score);
    painter->setPen(active ? mine : scheme.gold);
    painter->drawText(QRectF(inner.left(), inner.top(), inner.width(), nameMetrics.height()),
                      Qt::AlignRight | Qt::AlignVCenter,
                      tr("%1 pts").arg(seat.score));

    // Le niveau et les étoiles.
    qreal y = inner.top() + nameMetrics.height() + 2.0;
    if (!seat.rivalId.isEmpty()) {
        const Learning learning = learningFor(seat.rivalId);
        const Level level = levelOf(learning.xp);
        QFont small = plainFont(painter);
        small.setBold(false);
        small.setPixelSize(plaqueTextSize(inner.height(), kPlaqueAsideSize, 9));
        painter->setFont(small);
        const QFontMetricsF smallMetrics(small);

        const qreal starSpan = std::min(44.0, textWidth * 0.34);
        const QRectF starBox(textLeft, y + 1.0, starSpan, smallMetrics.height() - 2.0);
        drawLevelStars(painter, starBox, levelStars(level), 3, scheme.gold,
                       blendColours(feltPlaqueFill(scheme), plaqueInk(scheme), 0.16));
        painter->setPen(plaqueAside(scheme));
        // Le libellé est centré sur les étoiles et non sur sa propre ligne de
        // base : les étoiles constituent un tracé dont la hauteur est imposée,
        // et le texte apparaissait plus bas qu'elles.
        QRectF levelBox(textLeft + starSpan + 6.0, 0.0, textWidth - starSpan - 6.0,
                        smallMetrics.height());
        levelBox.moveCenter(QPointF(levelBox.center().x(), starBox.center().y()));
        painter->drawText(capCentred(levelBox, smallMetrics), Qt::AlignLeft | Qt::AlignVCenter,
                          levelName(level));
        y += smallMetrics.height() + 2.0;

        drawGauge(painter, QRectF(textLeft, y + 2.0, std::min(textWidth, 96.0), 4.0),
                  progressWithin(learning.xp), mine,
                  blendColours(feltPlaqueFill(scheme), plaqueInk(scheme), 0.14));
        y += 9.0;
    }

    // Les cartes en main, face cachée, surmontées de leur nombre.
    const qreal pileHeight = std::max(20.0, inner.bottom() - y - 1.0);
    drawHandPile(painter, QRectF(textLeft, inner.bottom() - pileHeight,
                                 std::max(50.0, textWidth * 0.64), pileHeight),
                 seat.hand.size(), scheme, mine);

    // État d'ouverture : c'est la seule information sur un adversaire qui
    // modifie la façon de jouer contre lui, un joueur ouvert pouvant exploiter
    // toute carte défaussée.
    QFont flagFont = plainFont(painter);
    flagFont.setPixelSize(plaqueTextSize(inner.height(), kPlaqueBadgeSize, 8));
    flagFont.setBold(true);
    flagFont.setCapitalization(QFont::AllUppercase);
    flagFont.setLetterSpacing(QFont::PercentageSpacing, 110);
    painter->setFont(flagFont);
    const QFontMetricsF flagMetrics(flagFont);
    const QString flag = seat.opened ? tr("ouvert") : tr("fermé");
    // La pastille est dimensionnée sur la hauteur de capitale et non sur la
    // hauteur de ligne : celle d'une police japonaise doit loger un idéogramme
    // et ses signes, ce qui agrandissait l'ovale d'autant.
    QRectF badge(0, 0, flagMetrics.horizontalAdvance(flag) + 12.0,
                 flagMetrics.capHeight() + 9.0);
    badge.moveBottomRight(QPointF(inner.right(), inner.bottom()));
    painter->setPen(Qt::NoPen);
    painter->setBrush(seat.opened ? mine : feltGold(scheme, 0.16));
    painter->drawRoundedRect(badge, badge.height() / 2.0, badge.height() / 2.0);
    painter->setPen(seat.opened ? readableTextOn(mine) : plaqueAside(scheme));
    painter->drawText(capCentred(badge, flagMetrics), Qt::AlignCenter, flag);
}

void TableView::paintSeats(QPainter *painter) {
    for (const Spot &spot : m_seatBoxes) paintSeatPlaque(painter, spot.box, spot.id);
}

void TableView::paintStacks(QPainter *painter) {
    const Palette &scheme = currentPalette();
    const bool yours = m_game->seat(m_game->turn()).human();
    const bool drawing = yours && m_game->phase() == Phase::Draw;

    // Libellé d'un tas, composé dans la police de la ligne supérieure du
    // logo : il s'agit d'un intitulé posé sur le tapis, comme les titres de
    // section du menu. Cette police ne comporte pas de bas de casse, ses
    // minuscules étant dessinées en capitales ; `AllUppercase` ne sert donc
    // qu'à la police de repli.
    QFont label = familyFont(plainFont(painter), capsFamily(), kStackLabelSize);
    label.setCapitalization(QFont::AllUppercase);
    label.setLetterSpacing(QFont::PercentageSpacing, 120);
    painter->setFont(label);
    const qreal labelBand = kStackLabel - kStackLabelAir;

    // La pioche : trois dos décalés, afin de figurer un empilement.
    painter->setPen(scheme.feltText);
    painter->drawText(QRectF(m_stacks.left(), m_stockBox.top() - kStackLabel, m_stacks.width(),
                             labelBand),
                      Qt::AlignCenter, tr("Pioche"));
    const int left = m_game->stockLeft();
    if (left <= 0) {
        drawCardSlot(painter, m_stockBox, scheme, tr("vide"));
    } else {
        const int layers = stockLayersFor(left);
        for (int i = layers - 1; i >= 0; --i) {
            CardLook look;
            look.lit = i == 0 && m_hoverStock && drawing;
            drawCardBack(painter,
                         m_stockBox.translated(-kStockLayerStep * i, -kStockLayerStep * i),
                         scheme, look);
        }
    }
    QFont count = plainFont(painter);
    count.setCapitalization(QFont::MixedCase);
    count.setLetterSpacing(QFont::PercentageSpacing, 100);
    painter->setFont(count);
    painter->setPen(scheme.feltText);
    painter->drawText(QRectF(m_stacks.left(), m_stockBox.bottom() + 2.0, m_stacks.width(), 15.0),
                      Qt::AlignCenter, tr("%1 cartes").arg(left));

    // La défausse : la carte supérieure, et un bord de celle qui la précède.
    painter->setFont(label);
    painter->setPen(scheme.feltText);
    painter->drawText(QRectF(m_stacks.left(), m_pileBox.top() - kStackLabel, m_stacks.width(),
                             labelBand),
                      Qt::AlignCenter, tr("Défausse"));
    const QVector<Card> &pile = m_game->pile();
    // Même principe ici : la carte qui vient d'être défaussée est placée sur
    // le tas avant la fin de son animation. Pendant celle-ci, le tas est
    // affiché dans son état précédent, sans elle.
    int shown = pile.size() - 1;
    if (shown >= 0 && inFlight(pile.at(shown).id)) --shown;
    if (shown < 0) {
        drawCardSlot(painter, m_pileBox, scheme, tr("vide"));
    } else {
        if (shown >= 1) {
            CardLook under;
            under.dimmed = true;
            drawCardFace(painter, m_pileBox.translated(kPileUnderStep, -kPileUnderStep),
                         pile.at(shown - 1), scheme, under);
        }
        CardLook look;
        look.lit = m_hoverPile && drawing;
        look.glow = m_glow.value(pile.at(shown).id, 0.0);
        drawCardFace(painter, m_pileBox, pile.at(shown), scheme, look);
    }

    // Les emplacements où piocher sont signalés par une flèche, logée dans la
    // marge que la carte laisse à sa gauche dans la colonne.
    if (drawing) {
        QColor ink = scheme.gold;
        ink.setAlpha(235);
        const qreal nudge = std::sin(m_beckon) * kBeckonSwing;
        if (left > 0) {
            drawTurnArrow(painter, arrowBox(m_stockBox).translated(nudge, 0.0), ink);
        }
        if (!pile.isEmpty()) {
            drawTurnArrow(painter, arrowBox(m_pileBox).translated(nudge, 0.0), ink);
        }
    }

    // La plaque du joueur, sous les deux tas. Elle adopte la même présentation
    // que celles des adversaires : médaillon à gauche, nom puis score sur la
    // première ligne, indicateur d'ouverture en bas à droite. Elle porte la
    // même information — qui occupe cette place, son score, et s'il peut
    // exploiter les cartes défaussées — et deux présentations différentes pour
    // une même information les feraient percevoir comme distinctes.
    //
    // Elle ne comporte ni étoiles, ni jauge, ni représentation de la main : ces
    // éléments indiquent l'expérience d'un adversaire et le nombre de cartes
    // qu'il détient, informations dont le joueur dispose déjà le concernant.
    int me = -1;
    for (int at = 0; at < m_game->seatCount(); ++at) {
        if (m_game->seat(at).human()) me = at;
    }
    if (me < 0) return;
    const qreal top = m_pileBox.bottom() + 20.0;
    if (m_stacks.bottom() - top < kMinePlaqueHeight) return;
    const QRectF plaque(m_stacks.left() + kPlaqueInset, top,
                        m_stacks.width() - kPlaqueInset * 2.0,
                        std::min(kMinePlaqueMax, m_stacks.bottom() - top));
    const Seat &mine = m_game->seat(me);
    const QColor colour = blendColours(rivalColour(currentPalette().hue), QColor(Qt::white), 0.18);
    paintPlaqueBody(painter, plaque, scheme, colour,
                    m_game->turn() == me && m_game->phase() != Phase::HandOver);

    const QRectF inner = plaque.adjusted(9, 6, -9, -6);
    const qreal mineScale = inner.height() / kMinePlaqueInnerFull;

    // Le médaillon, placé à gauche comme sur les autres plaques, mais sans
    // symbole : les six symboles servent à distinguer les adversaires entre
    // eux, et le joueur n'en fait pas partie. L'initiale est celle du profil
    // courant, ce qui indique lequel des trois profils est en cours, le libellé
    // restant « Vous ».
    //
    // Il est aligné en haut et non centré verticalement, comme sur les plaques
    // d'adversaire : son bord supérieur et celui du nom sont sur la même ligne,
    // alignement qui rend les deux types de plaque visuellement semblables.
    const qreal disc = std::min(inner.height(), 36.0 * std::min(1.0, mineScale));
    const QRectF medallion(inner.left(), inner.top(), disc, disc);
    drawRivalEmblem(painter, medallion, -1, profileName(currentProfile()), colour,
                    blendColours(feltPlaqueFill(scheme), colour, 0.14));

    const qreal textLeft = medallion.right() + 9.0;
    const qreal textWidth = inner.right() - textLeft;

    QFont name = plainFont(painter);
    name.setBold(true);
    name.setPixelSize(plaqueTextSize(inner.height(), kPlaqueNameSize, 11, kMinePlaqueInnerFull));
    painter->setFont(name);
    painter->setPen(plaqueInk(scheme));
    const QFontMetricsF nameMetrics(name);
    painter->drawText(QRectF(textLeft, inner.top(), textWidth, nameMetrics.height()),
                      Qt::AlignLeft | Qt::AlignVCenter, mine.name);

    QFont score = plainFont(painter);
    score.setBold(true);
    score.setPixelSize(plaqueTextSize(inner.height(), kPlaqueScoreSize, 12, kMinePlaqueInnerFull));
    painter->setFont(score);
    painter->setPen(scheme.gold);
    painter->drawText(QRectF(textLeft, inner.top(), textWidth, nameMetrics.height()),
                      Qt::AlignRight | Qt::AlignVCenter, tr("%1 pts").arg(mine.score));

    // La pastille est dimensionnée sur la hauteur de capitale et non sur la
    // hauteur de ligne, pour la raison exposée dans `paintSeatPlaque()`.
    QFont flagFont = plainFont(painter);
    flagFont.setPixelSize(plaqueTextSize(inner.height(), kPlaqueBadgeSize, 8, kMinePlaqueInnerFull));
    flagFont.setBold(true);
    flagFont.setCapitalization(QFont::AllUppercase);
    flagFont.setLetterSpacing(QFont::PercentageSpacing, 110);
    painter->setFont(flagFont);
    const QFontMetricsF flagMetrics(flagFont);
    const QString flag = mine.opened ? tr("ouvert") : tr("fermé");
    QRectF badge(0, 0, flagMetrics.horizontalAdvance(flag) + 12.0,
                 flagMetrics.capHeight() + 9.0);
    badge.moveBottomRight(QPointF(inner.right(), inner.bottom()));
    painter->setPen(Qt::NoPen);
    painter->setBrush(mine.opened ? colour : feltGold(scheme, 0.16));
    painter->drawRoundedRect(badge, badge.height() / 2.0, badge.height() / 2.0);
    painter->setPen(mine.opened ? readableTextOn(colour) : plaqueAside(scheme));
    painter->drawText(capCentred(badge, flagMetrics), Qt::AlignCenter, flag);
}

void TableView::paintMelds(QPainter *painter) {
    const Palette &scheme = currentPalette();

    QFont label = familyFont(plainFont(painter), capsFamily(), kStackLabelSize);
    label.setCapitalization(QFont::AllUppercase);
    label.setLetterSpacing(QFont::PercentageSpacing, 120);
    painter->setFont(label);
    painter->setPen(scheme.feltText);
    // Le libellé est placé en haut du tapis et non au-dessus de celui-ci : la
    // bande qui le portait ne disposait que de dix pixels libres pour un
    // libellé de vingt-sept, et il passait sous la plaque de gauche. Le libellé
    // « PIOCHE » est disposé de cette façon depuis l'origine — en haut de sa
    // colonne, l'espace étant réservé en dessous — et le tapis et la colonne
    // commençant à la même hauteur, les deux libellés s'alignent.
    // Quatorze pixels et non six comme pour le cadre des combinaisons : ce qui
    // doit être aligné sous le libellé, ce sont les faces des cartes, qui
    // commencent cinq pixels au-delà du cadre les contenant.
    painter->drawText(QRectF(m_tapis.left() + kTapisLabelPad, m_tapis.top(), m_tapis.width(),
                             kStackLabel - kStackLabelAir),
                      Qt::AlignLeft | Qt::AlignVCenter, tr("Le tapis"));

    if (m_game->table().isEmpty()) {
        QFont empty = plainFont(painter);
        empty.setBold(false);
        empty.setCapitalization(QFont::MixedCase);
        empty.setLetterSpacing(QFont::PercentageSpacing, 100);
        empty.setPixelSize(13);
        painter->setFont(empty);
        QColor faint = scheme.feltText;
        faint.setAlpha(150);
        painter->setPen(faint);
        painter->drawText(m_tapis.adjusted(0, kStackLabel, 0, 0), Qt::AlignCenter,
                          tr("Rien de posé : il faut 51 points pour ouvrir."));
        return;
    }

    const QSizeF card = cardSizeForHeight(m_meldHeight);
    for (const Spot &spot : m_meldBoxes) {
        const Meld *meld = m_game->meldById(spot.id);
        if (!meld) continue;
        const bool lit = m_hoveredMeld == spot.id;

        QPainterPath shape;
        shape.addRoundedRect(spot.box, 8, 8);
        QColor bed = scheme.feltEdge;
        bed.setAlpha(lit ? 170 : 110);
        painter->setPen(Qt::NoPen);
        painter->fillPath(shape, bed);
        painter->setBrush(Qt::NoBrush);
        // Le liseré reprend la couleur de la table et non celle de l'auteur de
        // la pose : une combinaison déposée sur le tapis n'appartient plus à
        // personne. Ses cartes ne sont comptées à aucun joueur en fin de
        // manche, et tout joueur peut la compléter ; la colorer selon son
        // premier auteur suggérait un propriétaire inexistant.
        painter->setPen(QPen(lit ? scheme.gold : blendColours(scheme.trim, scheme.feltEdge, 0.3),
                             lit ? 2.0 : 1.0));
        painter->drawPath(shape);

        for (int k = 0; k < meld->cards.size(); ++k) {
            const Card &held = meld->cards.at(k);
            // Sa position reste vide pendant son déplacement, ce qui rend
            // perceptible le remplissage progressif de la combinaison.
            if (inFlight(held.id)) continue;
            CardLook look;
            look.glow = m_glow.value(held.id, 0.0);
            drawCardFace(painter, meldCardBox(spot.box, k), held, scheme, look);
        }
    }
}

void TableView::paintRail(QPainter *painter) {
    const Palette &scheme = currentPalette();
    int me = -1;
    for (int at = 0; at < m_game->seatCount(); ++at) {
        if (m_game->seat(at).human()) me = at;
    }

    qreal textLeft = m_rail.left() + 6.0;

    if (!m_poseBoxes.isEmpty() && me >= 0) {
        for (const Spot &spot : m_poseBoxes) {
            const Meld *meld = m_game->meldById(spot.id);
            if (!meld) continue;
            for (int k = 0; k < meld->cards.size(); ++k) {
                const Card &held = meld->cards.at(k);
                if (inFlight(held.id)) continue;
                CardLook look;
                look.glow = m_glow.value(held.id, 0.0);
                look.lit = held.id == m_hoveredPose;
                drawCardFace(painter, poseCardBox(spot.box, k), held, scheme, look);
            }
        }
        // Le nombre de points manquants, seule information utile à ce stade.
        const int value = m_game->poseValue(me);
        QFont tally = plainFont(painter);
        tally.setBold(true);
        tally.setPixelSize(14);
        painter->setFont(tally);
        const QString text = tr("%1 / %2").arg(value).arg(kOpeningPoints);
        const QFontMetricsF metrics(tally);
        QRectF badge(m_poseBoxes.last().box.right() + 12.0, 0,
                     metrics.horizontalAdvance(text) + 18.0, 24.0);
        badge.moveCenter(QPointF(badge.center().x(), m_rail.center().y()));
        painter->setPen(Qt::NoPen);
        painter->setBrush(value >= kOpeningPoints ? scheme.accent : scheme.danger);
        painter->drawRoundedRect(badge, 12, 12);
        painter->setPen(readableTextOn(value >= kOpeningPoints ? scheme.accent : scheme.danger));
        painter->drawText(capCentred(badge, metrics), Qt::AlignCenter, text);
        textLeft = badge.right() + 14.0;
    }

    if (m_line.isEmpty()) return;
    QFont line = plainFont(painter);
    line.setPixelSize(14);
    line.setBold(m_trouble);
    painter->setFont(line);
    painter->setPen(m_trouble ? blendColours(scheme.danger, QColor(Qt::white), 0.35)
                              : scheme.feltText);
    painter->drawText(
        QRectF(textLeft, m_rail.top(), std::max(60.0, m_rail.right() - textLeft - 6.0),
               m_rail.height()),
        Qt::AlignRight | Qt::AlignVCenter | Qt::TextWordWrap, m_line);
}

void TableView::paintHand(QPainter *painter) {
    const Palette &scheme = currentPalette();
    int me = -1;
    for (int at = 0; at < m_game->seatCount(); ++at) {
        if (m_game->seat(at).human()) me = at;
    }
    if (me < 0) return;
    const Seat &mine = m_game->seat(me);

    for (const Spot &spot : m_handBoxes) {
        const Card *card = findCard(mine.hand, spot.id);
        if (!card) continue;
        // La carte en cours de déplacement figure déjà dans la main : sa
        // position y est laissée vide pendant l'animation, plutôt que de
        // l'afficher simultanément à deux endroits.
        if (inFlight(spot.id)) continue;
        CardLook look;
        look.chosen = m_chosen.contains(spot.id);
        look.lit = m_hovered == spot.id;
        look.glow = m_glow.value(spot.id, 0.0);
        QRectF box = handBoxNow(spot.id, spot.box);
        if (look.chosen) box.translate(0, -kLift);
        else if (look.lit) box.translate(0, -6.0);
        drawCardFace(painter, box, *card, scheme, look);
    }

    if (mine.hand.isEmpty()) {
        QFont empty = plainFont(painter);
        empty.setPixelSize(15);
        painter->setFont(empty);
        painter->setPen(scheme.feltText);
        painter->drawText(m_handRow, Qt::AlignCenter, tr("Plus une carte en main."));
    }
}

void TableView::paintFlights(QPainter *painter) {
    const Palette &scheme = currentPalette();
    for (const Flight &flight : m_flights) {
        // La carte attend le départ de sa combinaison : elle se trouve encore
        // dans la main ou sur la plaque de son auteur, et n'a pas à être
        // affichée en cours de trajet.
        if (flight.along < 0.0) continue;
        const qreal along = plateau(flight.along);
        const QPointF at = flight.from + (flight.to - flight.from) * along;
        // Les deux tas et la main n'étant pas à la même échelle, une carte
        // passant de l'un à l'autre est redimensionnée pendant son
        // déplacement.
        QSizeF size = flight.size;
        if (flight.grow.isValid()) {
            size = QSizeF(flight.size.width() + (flight.grow.width() - flight.size.width()) * along,
                          flight.size.height() +
                              (flight.grow.height() - flight.size.height()) * along);
        }
        const QRectF box(at, size);
        painter->save();
        // Une carte en déplacement subit une légère rotation, annulée à
        // l'arrivée.
        painter->translate(box.center());
        painter->rotate((1.0 - along) * 14.0);

        if (flight.flips) {
            // Retournement. La largeur apparente suit le cosinus de l'angle,
            // ce qui reproduit le comportement d'une carte que l'on retourne :
            // largeur maximale de dos, nulle de profil, maximale de face. Elle
            // n'est jamais tout à fait nulle, afin que la tranche reste visible
            // au passage.
            const qreal turned = std::cos(along * M_PI);
            painter->scale(std::max(0.03, std::abs(turned)), 1.0);
            painter->translate(-box.center());
            if (turned > 0.0) {
                drawCardBack(painter, box, scheme);
            } else {
                drawCardFace(painter, box, flight.card, scheme);
            }
            painter->restore();
            continue;
        }

        painter->translate(-box.center());
        if (flight.faceUp) {
            drawCardFace(painter, box, flight.card, scheme);
        } else {
            drawCardBack(painter, box, scheme);
        }
        painter->restore();
    }
}

// ---------------------------------------------------------------------------
// Gestion de la souris.

int TableView::cardAt(const QPointF &where) const {
    // Parcours en sens inverse : la carte dessinée en dernier est celle du
    // dessus, et c'est celle que le clic désigne.
    for (int i = m_handBoxes.size() - 1; i >= 0; --i) {
        const int id = m_handBoxes.at(i).id;
        QRectF box = handBoxNow(id, m_handBoxes.at(i).box);
        // Une carte sélectionnée, donc surélevée, est cliquée à sa position
        // courante et non à sa position de repos.
        if (m_chosen.contains(id)) box.translate(0, -kLift);
        if (box.contains(where)) return id;
    }
    return -1;
}

int TableView::meldAt(const QPointF &where) const {
    for (const Spot &spot : m_meldBoxes) {
        if (spot.box.contains(where)) return spot.id;
    }
    return -1;
}

int TableView::seatAt(const QPointF &where) const {
    for (const Spot &spot : m_seatBoxes) {
        if (spot.box.contains(where)) return spot.id;
    }
    return -1;
}

int TableView::poseCardAt(const QPointF &where) const {
    // Parcours en sens inverse, comme pour la main : dans une combinaison dont
    // les cartes se chevauchent, celle du dessus est celle que le clic
    // désigne.
    for (int i = m_poseCardBoxes.size() - 1; i >= 0; --i) {
        const Spot &spot = m_poseCardBoxes.at(i);
        // Une carte encore en déplacement vers le rail n'y figure pas : seuls
        // les éléments effectivement dessinés sont cliquables.
        if (inFlight(spot.id)) continue;
        if (spot.box.contains(where)) return spot.id;
    }
    return -1;
}

void TableView::mousePressEvent(QMouseEvent *event) {
    const QPointF where = event->position();
    if (event->button() != Qt::LeftButton) {
        const int meld = meldAt(where);
        if (meld >= 0) {
            Q_EMIT meldRightPressed(meld);
            return;
        }
        clearChosen();
        return;
    }
    if (m_stockBox.adjusted(-4, -4, 4, 4).contains(where)) {
        Q_EMIT stockPressed();
        return;
    }
    if (m_pileBox.adjusted(-4, -4, 4, 4).contains(where)) {
        Q_EMIT pilePressed();
        return;
    }
    const int onRail = poseCardAt(where);
    if (onRail >= 0) {
        Q_EMIT poseCardPressed(onRail);
        return;
    }
    const int card = cardAt(where);
    if (card >= 0) {
        if (m_chosen.contains(card)) {
            m_chosen.removeAll(card);
        } else {
            m_chosen.append(card);
        }
        Q_EMIT chosenChanged();
        Q_EMIT cardPressed(card);
        update();
        return;
    }
    const int meld = meldAt(where);
    if (meld >= 0) {
        Q_EMIT meldPressed(meld);
        return;
    }
    clearChosen();
}

void TableView::mouseDoubleClickEvent(QMouseEvent *event) {
    const int card = cardAt(event->position());
    if (card < 0) return;
    // Un double-clic sur une carte l'a sélectionnée puis désélectionnée au
    // passage ; il doit être interprété comme une validation directe de cette
    // carte.
    if (!m_chosen.contains(card)) {
        m_chosen.append(card);
        Q_EMIT chosenChanged();
    }
    Q_EMIT cardDoubleClicked(card);
}

void TableView::mouseMoveEvent(QMouseEvent *event) {
    const QPointF where = event->position();
    const int pose = poseCardAt(where);
    const int card = pose >= 0 ? -1 : cardAt(where);
    const int meld = pose >= 0 || card >= 0 ? -1 : meldAt(where);
    const bool stock = m_stockBox.adjusted(-4, -4, 4, 4).contains(where);
    const bool pile = m_pileBox.adjusted(-4, -4, 4, 4).contains(where);
    const int seat = seatAt(where);
    if (card == m_hovered && meld == m_hoveredMeld && pose == m_hoveredPose &&
        stock == m_hoverStock && pile == m_hoverPile && seat == m_hoveredSeat) {
        return;
    }
    if (seat != m_hoveredSeat) {
        // Changement de plaque survolée, ou sortie de la plaque courante :
        // l'aperçu est libéré — il se referme si le pointeur l'a quitté, il
        // subsiste si le pointeur est passé sur lui — et la minuterie est
        // relancée pour la plaque courante.
        m_hoveredSeat = seat;
        m_glanceBeat->stop();
        releaseRivalGlance(this);
        if (seat >= 0) m_glanceBeat->start();
    }
    m_hovered = card;
    m_hoveredMeld = meld;
    m_hoveredPose = pose;
    m_hoverStock = stock;
    m_hoverPile = pile;
    setCursor(card >= 0 || meld >= 0 || pose >= 0 || stock || pile ? Qt::PointingHandCursor
                                                                  : Qt::ArrowCursor);
    update();
}

void TableView::leaveEvent(QEvent *) {
    m_hovered = -1;
    m_hoveredMeld = -1;
    m_hoveredPose = -1;
    m_hoveredSeat = -1;
    m_hoverStock = false;
    m_hoverPile = false;
    m_glanceBeat->stop();
    releaseRivalGlance(this);
    update();
}

void TableView::hideEvent(QHideEvent *event) {
    // Retour au menu ou fermeture de la fenêtre : l'aperçu doit être fermé,
    // faute de quoi il subsisterait à l'écran sans la table à laquelle il se
    // rapporte.
    m_hoveredSeat = -1;
    m_glanceBeat->stop();
    hideRivalGlance(this);
    QWidget::hideEvent(event);
}
