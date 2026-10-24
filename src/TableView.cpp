#include "TableView.h"

#include "CardArt.h"
#include "Fonts.h"
#include "Palette.h"
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

// What each band of the cloth is given, before anything is squeezed.
constexpr qreal kSeatsHeight = 104.0;
constexpr qreal kStacksWidth = 178.0;
// Le rail, au-dessus de la main: ce qu'on a commencé à poser y attend.
//
// Il tenait des cartes de 44 quand le tapis en donnait 56 -- déjà petites, et
// franchement perdues depuis que le tapis est à 96. Or ces cartes-là sont
// exactement celles qui vont partir sur le tapis: les voir en miniature puis
// les retrouver au double un instant plus tard est un à-coup pour rien.
constexpr qreal kRailHeight = 82.0;
constexpr qreal kPoseTall = 62.0;

// Le va-et-vient des flèches: de combien elles glissent, et à quelle vitesse.
//
// Trois pixels et un aller-retour en une seconde et demie. Assez pour que
// l'œil l'attrape au bord du regard, trop peu pour qu'on ait envie qu'elles
// s'arrêtent -- elles restent là tant qu'on n'a pas pioché.
// L'allure des cartes qui voyagent, en part du trajet par image.
//
// Une seule pour tout le monde. La carte que vous piochiez allait à 0,048
// quand celle d'en face allait à 0,072 -- l'idée étant qu'on veut suivre la
// sienne -- et la main s'écartait à 0,048 elle aussi. Mais deux allures sur
// la même table se remarquent, et de deux c'est la lente qu'on remarque:
// tout ce qui touchait votre main paraissait traîner à côté du reste.
//
// Poser reste un peu plus posé, et l'est pour tout le monde pareil.
constexpr qreal kFlightSpeed = 0.072;
constexpr qreal kLaySpeed = 0.055;

// Ce qu'une combinaison attend que la précédente ait pris d'avance, en part
// du trajet.
//
// Ouvrir met toute la pose sur le tapis d'un coup -- souvent trois
// combinaisons -- et trois volées parties ensemble ne se lisent pas: on voit
// le tapis se remplir sans voir ce qui s'y pose. L'une après l'autre, on
// suit. À 0,055 par image, 0,45 fait une demi-douzaine d'images entre deux.
constexpr qreal kLayStagger = 0.45;

// Votre pioche, seule, prend le temps de se retourner -- et ce temps-là suit
// la distance qu'elle a à couvrir.
//
// Une allure donnée en part du trajet par image fait arriver tous les vols
// en autant d'images, quelle que soit leur longueur: la carte qui se range
// à côté de la pioche et celle qui traverse toute la main mettent le même
// temps, donc la seconde va trois ou quatre fois plus vite. C'est justement
// celle-là qu'on rate -- elle file trop pour qu'on voie le demi-tour, alors
// que la courte, elle, allait bien.
//
// La durée est donc un fond plus tant de millisecondes par largeur de table
// parcourue -- rapportée à la largeur, pour que le réglage tienne quelle que
// soit la taille de la fenêtre.
//
// Les deux nombres sont pris sur la table elle-même. La pioche est en haut à
// droite et la main en bas, si bien que la descente compte autant que
// l'écart: à 1220 sur 850, la case la plus proche est déjà à 582 pixels et
// la plus lointaine à 1202, soit le double et non le quintuple.
//
//     à côté de la pioche    582 px    242 ms    38 px par image
//     au milieu              772 px    269 ms    46
//     à l'autre bout       1 202 px    330 ms    58
//
// Celle d'à côté garde les 242 ms qui allaient bien. Celle du bout prend le
// tiers de plus qu'il lui faut pour qu'on voie le demi-tour -- pas davantage:
// à une demi-seconde elle traînait, et une pioche qu'on attend est pire
// qu'une pioche qu'on voit mal. L'écart de vitesse apparente passe ainsi du
// simple au double à une fois et demie, ce qui suffit à ne plus se voir.
//
// C'est le seul vol réglé ainsi: les autres gardent l'allure unique de la
// table, qui est ce qui l'empêche de se remarquer.
constexpr qreal kDrawMsBase = 159.0;
constexpr qreal kDrawMsPerWidth = 173.0;
constexpr qreal kDrawMsLeast = 200.0;
constexpr qreal kDrawMsMost = 380.0;

constexpr qreal kBeckonSwing = 3.0;
constexpr qreal kBeckonSpeed = 0.07;

// De combien les tas débordent de la boîte de leur carte: la pioche est un
// empilement dont chaque épaisseur dépasse en haut à gauche, la défausse
// laisse voir un bord de la carte d'en dessous, en haut à droite.
constexpr qreal kStockLayerStep = 1.6;
constexpr qreal kPileUnderStep = 3.0;

// L'air entre les deux tas, sous le mot qui annonce chacun.
//
// Il en faut plus qu'il n'y paraît: le compte de la pioche s'écrit sous elle
// et le mot « défausse » au-dessus de l'autre, si bien que l'écart porte deux
// lignes de texte et non pas une. À dix-huit, la seconde commençait au pixel
// où la première finissait et les deux tas n'en faisaient qu'un.
constexpr qreal kStackGap = 36.0;
// La bande au-dessus d'un tas, où s'écrit son nom, et l'air qu'elle garde
// sous le texte: sans lui le titre touchait sa carte. Partagée entre la mise
// en place et le dessin, qui doivent s'accorder.
constexpr qreal kStackLabel = 27.0;
constexpr qreal kStackLabelAir = 8.0;
// Le corps des trois enseignes du tapis. Zen Dots est large: ce qui est petit
// dans une fonte de texte y est encore lisible, mais ces trois-là nomment des
// endroits et non des détails, et se lisent de loin.
constexpr qreal kStackLabelSize = 13.0;

// Ce dont votre plaque se retire des bords de sa colonne.
//
// Les deux cartes sont centrées et laissent déjà large de chaque côté; la
// plaque, elle, prenait toute la largeur et venait buter contre le bord de
// la fenêtre, à la marge près.
constexpr qreal kPlaqueInset = 13.0;

// Les corps de texte d'une plaque d'adversaire, et la hauteur utile où ils
// valent. Ce sont ceux de la carte du même adversaire dans le menu
// d'ouverture (`RivalCard`): c'est la même personne décrite de la même
// manière, et la lire plus gros à la table qu'au menu la fait passer pour
// deux objets. Le score n'a pas d'équivalent au menu -- il prend celui de la
// plaque `Vous`, juste en dessous, qui est le seul autre endroit où un score
// s'écrit.
constexpr int kPlaqueNameSize = 15;
constexpr int kPlaqueScoreSize = 17;
constexpr int kPlaqueAsideSize = 11;
constexpr int kPlaqueBadgeSize = 10;
// Ce qui reste d'une plaque quand on lui a retiré ses marges du haut et du
// bas, la fenêtre lui laissant toute sa place.
constexpr qreal kPlaqueInnerFull = kSeatsHeight - 14.0;

// Le corps d'un texte de plaque, à la hauteur qu'elle a. La rangée des
// plaques se réduit avec la fenêtre (`min(kSeatsHeight, hauteur * 0.22)`), et
// un corps fixe y déborderait: le texte garde donc sa réduction. Ce qu'il
// n'a plus, c'est le droit de grandir -- la taille du menu est un plafond, et
// non une valeur de départ qu'une plaque entière multipliait par un tiers.
int plaqueTextSize(qreal innerHeight, int atFullHeight, int floor) {
    const qreal scale = std::min(1.0, innerHeight / kPlaqueInnerFull);
    return std::max(floor, int(std::lround(atFullHeight * scale)));
}

int stockLayersFor(int left) { return std::min(3, std::max(1, left / 6 + 1)); }
constexpr qreal kHandHeight = 134.0;
constexpr qreal kMargin = 14.0;
constexpr qreal kGap = 10.0;

// How far apart the cards of a combination sit. Enough of each one shows for
// its corner to be read, which is the whole trick of a fanned hand.
constexpr qreal kMeldStep = 0.60;

// Ce que le tapis accorde à une carte posée, au mieux et au pire.
//
// Le plafond était à 56, et la boucle qui descend de quatre en quatre n'avait
// jamais à descendre: même trente combinaisons tiennent à cette taille-là sur
// un tapis de fenêtre ordinaire. Les cartes posées étaient donc petites tout
// le temps, et pour rien -- la moitié de celles de la main, sur un tapis aux
// neuf dixièmes vide.
//
// À 96 elles se lisent tant que le tapis est clair, et la boucle retrouve son
// office quand il se remplit: 92 à dix-huit combinaisons, 80 à vingt-quatre,
// 72 à trente -- qui reste au-dessus de ce qu'on avait en permanence avant.
constexpr qreal kMeldTall = 96.0;
constexpr qreal kMeldShort = 26.0;

// La plus grande carte qui tienne dans la boîte, dans les deux sens.
//
// Tout le tapis se dispose en hauteur, et la colonne de droite est la seule
// place où la largeur est, elle, arrêtée d'avance: une carte plus large que
// sa colonne y débordait sur le tapis, et c'est exactement ce que le zoom et
// un jeu large font arriver.
// La flèche qui dit où l'on peut prendre: un trait et une pointe, couchée
// vers la droite, dans la place qu'on lui laisse.
//
// Elle remplace le pointillé qui faisait le tour des deux tas. Un cadre en
// pointillé autour d'une carte ressemble à une carte sélectionnée, et il en
// entourait deux à la fois sans dire laquelle: la flèche, elle, montre, et
// deux flèches côte à côte se lisent comme « l'un ou l'autre ».
void drawTurnArrow(QPainter *painter, const QRectF &box, const QColor &ink) {
    // Courte et grasse plutôt que longue et fine: ce qu'elle a à dire tient
    // dans sa pointe, et un long trait traversant la marge ne faisait que
    // mener l'œil loin de la carte qu'il désigne. Elle n'occupe donc que la
    // moitié de la place offerte, serrée contre la carte.
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

// How much a chosen card rises out of the hand.
constexpr qreal kLift = 18.0;

// Les bornes du zoom, et le cran de la molette. En deçà les figures ne se
// lisent plus, au-delà la main de quatorze cartes ne tient plus en largeur.
constexpr int kZoomMin = 80;
constexpr int kZoomMax = 200;
constexpr int kZoomStep = 10;

// L'allure d'une pioche qui a `reach` pixels à couvrir sur une table large
// de `across`.
qreal drawSpeedFor(qreal reach, qreal across) {
    const qreal part = across > 1.0 ? std::clamp(reach / across, 0.0, 2.0) : 0.5;
    const qreal ms = std::clamp(kDrawMsBase + kDrawMsPerWidth * part, kDrawMsLeast, kDrawMsMost);
    return qreal(kFrameMs) / ms;
}

qreal plateau(qreal along) {
    // Starts quickly and lands softly: a card thrown onto a table does not
    // arrive at a constant speed.
    const qreal at = std::clamp(along, 0.0, 1.0);
    return 1.0 - (1.0 - at) * (1.0 - at);
}

// Le corps d'une plaque -- celle d'un adversaire, et la vôtre.
//
// Sombre et translucide à filet d'or, comme les cartes du menu d'ouverture:
// un panneau blanc sur un drap vert est un trou dans le drap, et les deux
// pages doivent se ressembler. Celui dont c'est le tour s'éclaire un peu et
// prend sa propre couleur dans le filet -- c'est la seule chose qui distingue
// encore les six d'un coup d'oeil.
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

// L'encre d'une plaque, et son aparté: claires, maintenant qu'elle est
// sombre.
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

    // L'aperçu d'un adversaire, le temps qu'on reste sur sa plaque: ce qu'il
    // est et ce qu'il a appris, sans ouvrir sa fiche au milieu d'une manche.
    m_glanceBeat = new QTimer(this);
    m_glanceBeat->setSingleShot(true);
    m_glanceBeat->setInterval(kGlanceDelayMs);
    connect(m_glanceBeat, &QTimer::timeout, this, [this] {
        if (!m_game || m_hoveredSeat < 0 || m_hoveredSeat >= m_game->seatCount()) return;
        const QString who = m_game->seat(m_hoveredSeat).rivalId;
        for (const Spot &spot : m_seatBoxes) {
            if (spot.id != m_hoveredSeat) continue;
            // La plaque, en coordonnées d'écran: l'aperçu se pose dessous,
            // sur le tapis, où il ne cache ni la plaque ni votre main.
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
    // La ligne change au moment précis où le tour passe: c'est l'occasion de
    // remettre l'horloge en marche pour que les flèches se mettent à battre.
    // Elle s'arrête d'elle-même s'il n'y a rien à animer.
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
    // Treize pixels entre la pointe et la carte, et la flèche à sa hauteur.
    //
    // Réglée sur la boîte de la carte, qui est la même pour les deux tas, et
    // non sur ce qui dépasse autour -- l'empilement de la pioche, le bord de
    // la carte sous la défausse. Les régler là-dessus a été essayé: comme un
    // dessin de dos et un dessin de face ne laissent pas la même marge
    // blanche au bord de leur image, cela ne rapproche rien et écarte les
    // deux flèches de trois pixels pour de bon.
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

    // Les cartes sont déjà rentrées en main: la disposition d'abord, pour
    // savoir où chacune s'est rangée, puis chacune repart de la place
    // qu'elle tenait sur le rail.
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
    // Your own cards come up out of your hand.
    return QPointF(m_handRow.center().x(), m_handRow.center().y());
}

void TableView::noteDiscard(int seat, const Card &card) {
    if (!card.valid() || m_pileBox.isNull()) return;
    Flight flight;
    flight.card = card;
    flight.faceUp = true;
    flight.to = m_pileBox.topLeft();
    flight.speed = kFlightSpeed;

    // La vôtre part de la place qu'elle tenait dans votre main.
    //
    // seatAnchor() ne connaît que les plaques d'en face, et la vôtre n'en est
    // pas une: pour vous elle rendait le milieu de la rangée, d'où aucune
    // carte n'était jamais partie. La disposition de la main n'est pas encore
    // refaite à cet instant, la place de la carte y est donc toujours.
    QRectF leaves;
    if (m_game && m_game->seat(seat).human()) leaves = m_handWas.value(card.id);
    if (leaves.isNull()) {
        flight.size = m_pileBox.size();
        flight.from =
            seatAnchor(seat) - QPointF(flight.size.width() / 2, flight.size.height() / 2);
    } else {
        // Et elle rétrécit en chemin, la main étant dessinée plus grand que
        // les deux tas -- l'inverse exact du voyage qu'elle a fait en venant.
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

    // La vôtre vole du tas jusqu'à la place qu'elle vient de prendre dans
    // votre main. C'est la seule façon de voir où elle est allée: la main se
    // range toute seule après la pioche, et une quinzième carte rangée par
    // couleur atterrit au milieu des quatorze autres.
    if (m_game->seat(seat).human()) {
        // La disposition d'abord, parce que c'est là qu'on cherche la place.
        relayout();
        QRectF landing;
        for (const Spot &spot : m_handBoxes) {
            if (spot.id == card.id) landing = spot.box;
        }
        if (!landing.isNull()) {
            Flight flight;
            flight.card = card;
            flight.faceUp = true;
            // De la pioche, elle se retourne en chemin: elle était de dos sur
            // le tas et vous ne savez ce que c'est qu'une fois en main. De la
            // défausse, il n'y a rien à retourner -- elle était face en haut,
            // et c'est pour ça que vous l'avez prise.
            flight.flips = !fromPile;
            flight.size = (fromPile ? m_pileBox : m_stockBox).size();
            flight.from = (fromPile ? m_pileBox : m_stockBox).topLeft();
            flight.to = landing.topLeft();
            // Le temps de voir la carte se retourner, d'autant plus qu'elle
            // va loin; de la défausse il n'y a rien à retourner, et elle va
            // à l'allure de la table.
            flight.speed = flight.flips
                               ? drawSpeedFor(QLineF(flight.from, flight.to).length(), width())
                               : kFlightSpeed;
            flight.grow = landing.size();
            m_flights.append(flight);
        }
        wake();
        return;
    }

    // Celle de quelqu'un d'autre vole du tas vers sa plaque, face en haut si
    // elle vient de la défausse -- une carte ramassée là est une carte que
    // tout le monde a vue -- et face en bas si elle vient de la pioche, où
    // tout ce qu'on apprend est de quel tas il a pris.
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

    // La disposition d'abord: la combinaison est sur le tapis, c'est là
    // qu'on va chercher la place de chacune de ses cartes.
    relayout();

    // Les combinaisons dans l'ordre où elles arrivent: chacune attend que la
    // précédente soit partie.
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
        // Une pose qui n'est pas encore descendue sur le tapis attend sur le
        // rail: c'est là qu'il faut aller la chercher. Poser se fait en deux
        // temps -- la main vers le rail, puis le rail vers le tapis -- et les
        // deux méritent d'être vus.
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

        // D'où elle vient, et c'est tout l'intérêt: une carte qui part de la
        // plaque de Zéphyr dit que c'est Zéphyr qui l'a posée, ce qu'un tapis
        // qui se remplit tout seul ne disait pas.
        if (which >= 0 && !order.contains(which)) order.append(which);

        Flight flight;
        flight.card = flying;
        flight.faceUp = true;
        flight.to = to.topLeft();
        flight.grow = to.size();
        flight.speed = kLaySpeed;
        // Négatif: la carte est en vol et n'est donc dessinée nulle part,
        // mais elle n'a pas encore quitté sa plaque. Voir paintFlights().
        const qsizetype rank = std::max(qsizetype(0), order.indexOf(which));
        flight.along = -kLayStagger * qreal(rank);

        // Celui qui la joue, qui n'est pas forcément celui à qui la
        // combinaison appartient: une carte mise sur la combinaison d'un
        // adversaire part de la main de qui la met.
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
        // Rien d'autre ne bouge: on ne redemande que les deux flèches, pour
        // qu'un tour passé à réfléchir ne repeigne pas tout le tapis soixante
        // fois par seconde.
        update(beckonArea());
    } else {
        update();
        m_ticker->stop();
    }
}

// ---------------------------------------------------------------------------
// Où tout se place.

void TableView::setZoom(int percent) {
    const int now = qBound(kZoomMin, percent, kZoomMax);
    if (now == m_zoom) return;
    m_zoom = now;
    // Les images mises à l'échelle sont gardées par taille: en changer toutes
    // les tailles d'un coup laisse derrière soi un cache qui ne resservira
    // plus.
    if (const CardSet *set = CardDecks::instance().current()) set->forgetDrawings();
    forgetBackDrawings();
    relayout();
    update();
    Q_EMIT zoomChanged(now);
}

void TableView::wheelEvent(QWheelEvent *event) {
    // Ctrl et la molette, comme partout ailleurs. La molette seule est
    // laissée tranquille: on la pousse sans y penser en jouant, et une table
    // qui change de taille toute seule serait une mauvaise surprise.
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
    // Les places d'avant ne valent plus rien si la table change de forme.
    if (m_slideAlong >= 1.0) m_slideFrom.clear();
    const QRectF all = QRectF(rect()).adjusted(kMargin, kMargin, -kMargin, -kMargin);
    if (all.width() < 80 || all.height() < 80) return;

    // Les deux bornes de chaque bande sont tirées par le zoom, pas seulement
    // la première: sans cela, à cent pour cent rien ne bouge -- c'est voulu --
    // mais au-delà la bande reste et la carte déborde.
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

    // La pioche et la défausse, l'une sous l'autre dans la colonne de droite,
    // avec de la place sous elles pour votre propre plaque.
    const qreal cardHeight = std::min(104.0 * z, std::max(56.0 * z, m_stacks.height() * 0.30));
    const QSizeF card = cardWithin(cardHeight, m_stacks.width() - 10.0);
    const qreal left = m_stacks.left() + (m_stacks.width() - card.width()) / 2.0;
    const qreal labels = kStackLabel;
    m_stockBox = QRectF(left, m_stacks.top() + labels, card.width(), card.height());
    // L'écart se resserre quand la colonne est courte, au lieu de pousser
    // votre plaque dehors: sur une petite fenêtre, deux tas un peu proches
    // valent mieux qu'un score qu'on ne voit plus. Ce qu'il faut sous eux,
    // c'est la hauteur des deux cartes, les deux lignes de titre, l'air sous
    // la défausse et de quoi poser la plaque.
    const qreal spare =
        m_stacks.height() - (labels * 2.0 + card.height() * 2.0 + 20.0 + 44.0);
    // Le plancher tient compte de ce qui vit dans cet écart: le compte de
    // cartes sous la pioche, puis l'enseigne de la défausse. Serrées à
    // dix-huit pixels elles se touchaient, l'enseigne ayant grandi.
    const qreal stackGap = std::clamp(spare, 24.0, kStackGap);
    m_pileBox =
        QRectF(left, m_stockBox.bottom() + labels + stackGap, card.width(), card.height());
}

void TableView::layoutSeats() {
    m_seatBoxes.clear();
    if (!m_game || m_game->seatCount() < 2) return;

    // Everybody but you. Your own cards are in front of you and your own
    // plaque is under the défausse, where there is room for it.
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

    // Groupées par poseur, pour que le tapis se lise: vingt combinaisons en
    // vrac ne disent rien, les mêmes vingt en six blocs se parcourent. Le
    // bloc ne dit plus de qui il est -- une combinaison posée n'appartient à
    // personne -- il dit seulement qu'elles sont arrivées ensemble, ce qui
    // suffit à ne pas les lire comme un tas.
    QVector<Meld> order = m_game->table();
    std::stable_sort(order.begin(), order.end(),
                     [](const Meld &a, const Meld &b) { return a.owner < b.owner; });
    if (order.isEmpty()) return;

    // The cards come down in size until the whole tapis fits. Two passes at
    // most in practice, but the loop is cheap and the alternative is a tapis
    // that runs off the bottom of the cloth.
    const qreal z = m_zoom / 100.0;
    for (qreal height = kMeldTall * z; height >= kMeldShort * z; height -= 4.0 * z) {
        const QSizeF card = cardSizeForHeight(height);
        const qreal pad = 5.0;
        const qreal rowHeight = card.height() + pad * 2;
        QVector<Spot> laid;
        qreal x = m_tapis.left() + 6.0;
        qreal y = m_tapis.top() + 6.0;
        bool fits = true;
        int lastOwner = -2;
        for (const Meld &meld : order) {
            const qreal width =
                pad * 2 + card.width() + card.width() * kMeldStep * (meld.cards.size() - 1);
            // A new player starts on a new row when there is not much left of
            // this one, so that a group is not split for the sake of a card.
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
    // Six: le petit pas de garde, là où il y en avait onze pour laisser
    // passer le trait du poseur.
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

    // D'où chaque carte part, si elle bouge.
    //
    // Plutôt qu'un cas par coup -- une place qui s'ouvre pour la pioche, une
    // qui se referme sur la défausse, plusieurs d'un coup sur une pose -- la
    // main se compare simplement à ce qu'elle était: toute carte qui change
    // de place y glisse. Un coup qui la remanie est alors animé sans que rien
    // ici ait à savoir lequel c'était.
    //
    // Sauf quand c'est la fenêtre qui a changé de taille: là, les cartes sont
    // à leur place, c'est la place qui a bougé, et la faire glisser à chaque
    // pixel de la poignée ne ferait que traîner derrière la souris.
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
        // Et l'horloge, sans quoi rien n'avance: ranger sa main après avoir
        // pioché ne fait voler aucune carte, donc personne d'autre ne la
        // remet en marche -- et la main restait dessinée là où elle était
        // pendant que le pointage, lui, visait déjà les nouvelles places.
        wake();
    }

    const qreal z = m_zoom / 100.0;

    // La pose en cours, sur le rail: smaller, and to the left of the line of
    // commentary, because it is yours and it is not finished.
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
// Le dessin.

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

    // Le score, à droite, en gros: it is the only number that decides
    // anything, and the manche just gone is written under it.
    QFont score = plainFont(painter);
    score.setBold(true);
    score.setPixelSize(plaqueTextSize(inner.height(), kPlaqueScoreSize, 12));
    painter->setFont(score);
    painter->setPen(active ? mine : scheme.gold);
    painter->drawText(QRectF(inner.left(), inner.top(), inner.width(), nameMetrics.height()),
                      Qt::AlignRight | Qt::AlignVCenter,
                      tr("%1 pts").arg(seat.score));

    // Le niveau, et les étoiles.
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
        // Le mot se centre sur les étoiles et non sur sa propre ligne: les
        // étoiles sont un dessin, haut de ce qu'on lui donne, et le texte
        // était posé plus bas qu'elles.
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

    // Les cartes en main, de dos, et le compte dessus.
    const qreal pileHeight = std::max(20.0, inner.bottom() - y - 1.0);
    drawHandPile(painter, QRectF(textLeft, inner.bottom() - pileHeight,
                                 std::max(50.0, textWidth * 0.64), pileHeight),
                 seat.hand.size(), scheme, mine);

    // Ouvert ou pas: the one fact about an opponent that changes how you play
    // against them, because an opened player can use anything you throw.
    QFont flagFont = plainFont(painter);
    flagFont.setPixelSize(plaqueTextSize(inner.height(), kPlaqueBadgeSize, 8));
    flagFont.setBold(true);
    flagFont.setCapitalization(QFont::AllUppercase);
    flagFont.setLetterSpacing(QFont::PercentageSpacing, 110);
    painter->setFont(flagFont);
    const QFontMetricsF flagMetrics(flagFont);
    const QString flag = seat.opened ? tr("ouvert") : tr("fermé");
    // La pastille se mesure sur les capitales et non sur la hauteur de ligne:
    // celle d'une fonte japonaise loge un idéogramme et ses signes, et
    // l'ovale enflait de tout ce vide.
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

    // Le nom d'une pile, dans la fonte du haut du logo: c'est une enseigne
    // posée sur le drap, comme les titres de section du menu. Elle n'a pas de
    // bas-de-casse, ses minuscules étant ses capitales -- `AllUppercase` ne
    // sert donc qu'au repli.
    QFont label = familyFont(plainFont(painter), capsFamily(), kStackLabelSize);
    label.setCapitalization(QFont::AllUppercase);
    label.setLetterSpacing(QFont::PercentageSpacing, 120);
    painter->setFont(label);
    const qreal labelBand = kStackLabel - kStackLabelAir;

    // La pioche: trois dos décalés, pour que ce soit un tas.
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

    // La défausse: la carte du dessus, et un bord de celle d'en dessous.
    painter->setFont(label);
    painter->setPen(scheme.feltText);
    painter->drawText(QRectF(m_stacks.left(), m_pileBox.top() - kStackLabel, m_stacks.width(),
                             labelBand),
                      Qt::AlignCenter, tr("Défausse"));
    const QVector<Card> &pile = m_game->pile();
    // Même chose ici: la carte qu'on vient de jeter est posée sur le tas
    // avant d'avoir fini d'y voler. Tant qu'elle vole, le tas se montre tel
    // qu'il était -- sans elle.
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

    // Ce qu'on peut prendre, montré du doigt. La flèche tient dans la marge
    // que la carte laisse à sa gauche dans la colonne.
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

    // Et votre propre plaque, sous les deux tas.
    int me = -1;
    for (int at = 0; at < m_game->seatCount(); ++at) {
        if (m_game->seat(at).human()) me = at;
    }
    if (me < 0) return;
    const qreal top = m_pileBox.bottom() + 20.0;
    if (m_stacks.bottom() - top < 44.0) return;
    const QRectF plaque(m_stacks.left() + kPlaqueInset, top,
                        m_stacks.width() - kPlaqueInset * 2.0,
                        std::min(72.0, m_stacks.bottom() - top));
    const Seat &mine = m_game->seat(me);
    const QColor colour = rivalColour(currentPalette().hue);
    paintPlaqueBody(painter, plaque, scheme,
                    blendColours(colour, QColor(Qt::white), 0.18),
                    m_game->turn() == me && m_game->phase() != Phase::HandOver);

    const QRectF inner = plaque.adjusted(9, 6, -9, -6);
    QFont name = plainFont(painter);
    name.setBold(true);
    name.setPixelSize(13);
    painter->setFont(name);
    painter->setPen(plaqueInk(scheme));
    const QFontMetricsF nameMetrics(name);
    painter->drawText(QRectF(inner.left(), inner.top(), inner.width(), nameMetrics.height()),
                      Qt::AlignLeft | Qt::AlignVCenter, mine.name);
    QFont big = plainFont(painter);
    big.setPixelSize(17);
    painter->setFont(big);
    painter->setPen(scheme.gold);
    painter->drawText(QRectF(inner.left(), inner.top() + nameMetrics.height(), inner.width(),
                             inner.height() - nameMetrics.height()),
                      Qt::AlignLeft | Qt::AlignTop, tr("%1 pts").arg(mine.score));
    QFont small = plainFont(painter);
    small.setPixelSize(10);
    small.setBold(true);
    small.setCapitalization(QFont::AllUppercase);
    painter->setFont(small);
    painter->setPen(mine.opened ? scheme.gold : plaqueAside(scheme));
    painter->drawText(inner, Qt::AlignRight | Qt::AlignBottom,
                      mine.opened ? tr("ouvert") : tr("fermé"));
}

void TableView::paintMelds(QPainter *painter) {
    const Palette &scheme = currentPalette();

    QFont label = familyFont(plainFont(painter), capsFamily(), kStackLabelSize);
    label.setCapitalization(QFont::AllUppercase);
    label.setLetterSpacing(QFont::PercentageSpacing, 120);
    painter->setFont(label);
    painter->setPen(scheme.feltText);
    painter->drawText(QRectF(m_tapis.left() + 8.0, m_tapis.top() - kStackLabel,
                             m_tapis.width(), kStackLabel - kStackLabelAir),
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
        painter->drawText(m_tapis, Qt::AlignCenter,
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
        // Le liseré est celui de la table, et non celui de qui a posé: une
        // combinaison sur le tapis n'appartient plus à personne. Les cartes
        // qui y sont ne comptent pour personne à la fin de la manche, et
        // n'importe qui peut la compléter -- la teinter aux couleurs de son
        // premier poseur promettait un propriétaire qui n'existe pas.
        painter->setPen(QPen(lit ? scheme.gold : blendColours(scheme.trim, scheme.feltEdge, 0.3),
                             lit ? 2.0 : 1.0));
        painter->drawPath(shape);

        for (int k = 0; k < meld->cards.size(); ++k) {
            const Card &held = meld->cards.at(k);
            // Sa place l'attend, vide, tant qu'elle est en chemin: c'est ce
            // qui fait qu'on voit la combinaison se remplir.
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
        // Combien il manque, parce que c'est la seule question.
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
        // Celle qui vole est déjà rangée dans la main: on lui laisse sa place
        // vide le temps du vol, plutôt que de la montrer ici et en chemin.
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
        // Elle attend que sa combinaison parte: elle est encore dans la main
        // ou sur la plaque de qui la pose, et n'a rien à montrer en chemin.
        if (flight.along < 0.0) continue;
        const qreal along = plateau(flight.along);
        const QPointF at = flight.from + (flight.to - flight.from) * along;
        // Les deux tas et la main ne sont pas à la même échelle, alors une
        // carte qui va de l'un à l'autre grandit en chemin.
        QSizeF size = flight.size;
        if (flight.grow.isValid()) {
            size = QSizeF(flight.size.width() + (flight.grow.width() - flight.size.width()) * along,
                          flight.size.height() +
                              (flight.grow.height() - flight.size.height()) * along);
        }
        const QRectF box(at, size);
        painter->save();
        // Une carte lancée tourne un peu, et se redresse en arrivant.
        painter->translate(box.center());
        painter->rotate((1.0 - along) * 14.0);

        if (flight.flips) {
            // Le demi-tour. La largeur apparente est le cosinus de l'angle,
            // ce qui est exactement ce que fait une carte qu'on retourne
            // entre deux doigts: pleine de dos, nulle de profil, pleine de
            // face. Jamais tout à fait nulle, pour qu'il reste la tranche à
            // voir au moment où elle passe.
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
// La souris.

int TableView::cardAt(const QPointF &where) const {
    // À l'envers: the one drawn last is the one on top, and the one on top is
    // the one you meant.
    for (int i = m_handBoxes.size() - 1; i >= 0; --i) {
        const int id = m_handBoxes.at(i).id;
        QRectF box = handBoxNow(id, m_handBoxes.at(i).box);
        // Une carte soulevée se clique là où elle est, pas là où elle était.
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
    // À l'envers, comme pour la main: dans une combinaison qui se chevauche,
    // celle du dessus est celle qu'on voulait.
    for (int i = m_poseCardBoxes.size() - 1; i >= 0; --i) {
        const Spot &spot = m_poseCardBoxes.at(i);
        // Une carte qui vole encore vers le rail n'est pas encore là: on ne
        // clique que ce qui est dessiné.
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
    // A double click on a card chose it and then unchose it on the way
    // through; it is meant as "that one, go on".
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
        // On a changé de plaque, ou on est sorti de celle qu'on survolait:
        // l'aperçu est lâché -- il se referme si le pointeur l'a quitté, il
        // reste si c'est sur lui qu'on est passé -- et la minuterie repart de
        // la plaque où l'on est.
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
    // On est retourné au menu, ou la fenêtre s'est fermée: un aperçu qui
    // survit à la table qu'il surplombe est un panneau qui flotte tout seul.
    m_hoveredSeat = -1;
    m_glanceBeat->stop();
    hideRivalGlance(this);
    QWidget::hideEvent(event);
}
