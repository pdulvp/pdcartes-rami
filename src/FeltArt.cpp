#include "FeltArt.h"

#include "Fonts.h"

#include "CardDeck.h"
#include "TableLook.h"

#include <QFontMetricsF>
#include <QHash>
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>

#include <algorithm>
#include <cmath>

namespace {

// Motif de trame du tapis.
//
// Une trame est formée de fils se croisant. Elle est rendue au moyen de deux
// familles de lignes, l'une légèrement plus claire que le tissu et l'autre
// légèrement plus sombre, décalées d'un demi-pas. La tuile mesure huit pixels
// de côté, de sorte que le motif se répète sans raccord visible et que les
// lignes tombent sur des pixels entiers.
struct Weave {
    QPixmap tile;
    int hue = -1;
    bool dark = false;
    bool made = false;
};

// La tuile est fournie par une fonction plutôt que conservée dans une
// variable de portée fichier : la construction d'un `QPixmap` suppose qu'une
// `QGuiApplication` existe déjà, alors qu'une variable de portée fichier est
// construite avant que `main()` n'en ait créé une. Une variable statique
// locale à une fonction est construite au premier appel, bien plus tard.
Weave &weave() {
    static Weave kept;
    return kept;
}

// Les couleurs d'une carte de cette table, dans l'état où elle se trouve.
//
// La cartothèque dessine une carte d'après un `CardStyle` et ignore les
// palettes : c'est ici que la palette de la table en produit un, et que
// l'extinction d'une carte injouable ou l'éclaircissement de la carte
// survolée s'y appliquent.
CardStyle lookStyle(const Palette &scheme, const CardLook &look) {
    CardStyle style = CardStyle::fromPalette(scheme);
    if (look.dimmed) style.paper = blendColours(scheme.cardFace, scheme.felt, 0.34);
    if (look.lit) {
        style.paper = blendColours(style.paper, QColor(Qt::white), darkMode() ? 0.22 : 0.55);
    }
    // Le beige des figures est tiré du papier : il suit donc la carte éteinte
    // comme la carte éclairée.
    style.panel = blendColours(style.paper, scheme.gold, 0.30);
    if (look.dimmed) {
        style.ink = blendColours(style.ink, style.paper, 0.45);
        style.red = blendColours(style.red, style.paper, 0.45);
    }
    return style;
}

// Cache des dos de cartes dessinés.
//
// Le treillis se compose d'une cinquantaine de diagonales découpées selon un
// coin arrondi, et un dos n'apparaît jamais seul à l'écran : il y en a un sur
// la pioche, un sur la défausse et un par tas de cartes adverse. Redessiné à
// chaque rafraîchissement, il coûterait deux fois plus cher qu'une figure
// — valeur mesurée. Comme la trame du tapis, il est donc produit une fois par
// taille puis recopié.
//
// Les images sont indexées par taille et par état, et l'ensemble du cache est
// vidé lorsque la palette change, celle-ci déterminant leur coloration.
struct Backs {
    QHash<quint64, QPixmap> tiles;
    int hue = -1;
    bool dark = false;
};

Backs &backs() {
    static Backs kept;
    return kept;
}

// Corps d'un dos dessiné : tout sauf l'ombre portée et le halo, qui dépendent
// du coup en cours et non de la carte elle-même.
const QPixmap &drawnBack(const QSizeF &wanted, const Palette &scheme, bool lit) {
    static const QPixmap nothing;
    const QSize size = wanted.toSize();
    if (size.width() < 3 || size.height() < 5) return nothing;

    Backs &kept = backs();
    if (kept.hue != scheme.hue || kept.dark != darkMode()) {
        kept.tiles.clear();
        kept.hue = scheme.hue;
        kept.dark = darkMode();
    }

    const quint64 key = (quint64(size.width()) << 32) | (quint64(size.height()) << 1) |
                        quint64(lit ? 1 : 0);
    const auto found = kept.tiles.constFind(key);
    if (found != kept.tiles.constEnd()) return *found;

    // Un rafraîchissement emploie cinq ou six tailles, et chaque cran de la
    // molette de zoom en introduit une nouvelle. Au-delà d'un seuil largement
    // suffisant pour une table, le cache est vidé plutôt que de conserver
    // toutes les tailles traversées.
    if (kept.tiles.size() > 64) kept.tiles.clear();

    QPixmap tile(size);
    tile.fill(Qt::transparent);
    QPainter painter(&tile);

    CardStyle style = CardStyle::fromPalette(scheme);
    if (lit) style.back = blendColours(style.back, QColor(Qt::white), 0.18);
    drawDefaultBack(&painter, QRectF(0.0, 0.0, size.width(), size.height()), style);
    painter.end();

    return kept.tiles.insert(key, tile).value();
}

} // namespace

qreal cardAspect() {
    if (const CardSet *set = CardDecks::instance().current()) {
        const qreal theirs = set->aspect();
        // Bornée : une proportion aberrante romprait la disposition.
        if (theirs > 0.35 && theirs < 1.60) return theirs;
    }
    return kCardRatio;
}

QSizeF cardSizeForHeight(qreal height) {
    const qreal aspect = cardAspect();
    // La hauteur demandée fixe l'encombrement disponible, que la largeur
    // consomme.
    //
    // Toute la disposition du tapis — la main, les combinaisons, la pioche —
    // est calculée en hauteur, la largeur découlant de la proportion du jeu
    // choisi. Tant qu'il s'agit du jeu dessiné par le programme, les deux
    // reviennent au même. Un jeu installé impose en revanche sa propre
    // proportion, et un jeu plus large que le jeu dessiné gagnerait de la
    // largeur à hauteur égale : la main de quatorze cartes passerait d'une
    // rangée aérée à une rangée touchant les deux bords de la fenêtre.
    //
    // La carte est donc contenue dans la largeur qu'aurait la carte dessinée à
    // cette hauteur, et c'est sa hauteur qui est réduite lorsque le jeu est
    // plus large. Un jeu plus étroit que le jeu dessiné conserve la hauteur
    // demandée, ce cas ne posant pas de difficulté.
    //
    // Le jeu dessiné ayant désormais la proportion de `CARDS.DLL`, les jeux
    // tirés de ces fichiers ne perdent plus rien : ils tombent exactement sur
    // la borne.
    if (aspect <= kCardRatio) return QSizeF(height * aspect, height);
    const qreal wide = height * kCardRatio;
    return QSizeF(wide, wide / aspect);
}
QSizeF cardSizeForWidth(qreal width) { return QSizeF(width, width / cardAspect()); }

const QPixmap &weaveTile(const Palette &scheme) {
    Weave &kept = weave();
    if (kept.made && kept.hue == scheme.hue && kept.dark == darkMode()) return kept.tile;

    constexpr int kPitch = 8;
    QPixmap tile(kPitch, kPitch);
    tile.fill(Qt::transparent);

    QPainter painter(&tile);
    QColor thread = scheme.feltWeave;
    thread.setAlpha(58);
    painter.setPen(QPen(thread, 1.0));
    // Une famille de fils horizontaux et une famille de fils verticaux,
    // décalées d'un demi-pas : c'est l'aspect d'un tissu vu de près, qui à
    // distance normale se lit comme une texture et non comme une grille.
    painter.drawLine(0, 0, kPitch, 0);
    painter.drawLine(0, 4, 0, kPitch);
    QColor shadow = scheme.feltEdge;
    shadow.setAlpha(44);
    painter.setPen(QPen(shadow, 1.0));
    painter.drawLine(0, 1, kPitch, 1);
    painter.drawLine(1, 4, 1, kPitch);
    painter.end();

    kept.tile = tile;
    kept.hue = scheme.hue;
    kept.dark = darkMode();
    kept.made = true;
    return kept.tile;
}

QColor feltPlaqueFill(const Palette &scheme, qreal lift) {
    QColor fill = blendColours(scheme.feltEdge, QColor(0, 0, 0), 0.52);
    if (lift > 0.0) fill = blendColours(fill, scheme.gold, lift);
    fill.setAlpha(205);
    return fill;
}

QColor feltGold(const Palette &scheme, qreal strength) {
    QColor line = scheme.gold;
    line.setAlphaF(qBound(0.0, strength, 1.0));
    return line;
}

QColor feltBarFace(const Palette &scheme) {
    return blendColours(scheme.feltEdge, QColor(0, 0, 0), 0.46);
}

void drawFelt(QPainter *painter, const QRectF &box, const Palette &scheme, bool withBraid) {
    painter->save();
    painter->setPen(Qt::NoPen);

    // Éclairage de la table : plus clair à l'aplomb de la source lumineuse,
    // située au-dessus du centre. C'est ce dégradé qui distingue un aplat vert
    // uniforme d'un tissu éclairé dans une pièce.
    QRadialGradient light(box.center() - QPointF(0, box.height() * 0.10),
                          std::max(box.width(), box.height()) * 0.78);
    light.setColorAt(0.0, scheme.felt);
    light.setColorAt(0.62, blendColours(scheme.felt, scheme.feltEdge, 0.45));
    light.setColorAt(1.0, scheme.feltEdge);
    painter->setBrush(light);
    painter->drawRect(box);

    painter->setBrush(QBrush(weaveTile(scheme)));
    painter->drawRect(box);

    if (!withBraid) {
        painter->restore();
        return;
    }

    // Bordure : un galon légèrement en retrait du bord, doublé d'une ligne
    // plus sombre à l'extérieur, afin que le tissu se lise comme tendu sur un
    // cadre.
    const qreal inset = std::max(5.0, std::min(box.width(), box.height()) * 0.012);
    QRectF braid = box.adjusted(inset, inset, -inset, -inset);
    painter->setBrush(Qt::NoBrush);
    QColor trim = scheme.trim;
    trim.setAlpha(150);
    painter->setPen(QPen(trim, 2.0));
    painter->drawRoundedRect(braid, 10, 10);
    trim.setAlpha(60);
    painter->setPen(QPen(trim, 1.0));
    painter->drawRoundedRect(braid.adjusted(-3, -3, 3, 3), 12, 12);
    painter->restore();
}

void drawCardFace(QPainter *painter, const QRectF &box, const Card &card, const Palette &scheme,
                  const CardLook &look) {
    if (box.width() < 3 || box.height() < 5) return;
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    const qreal corner = cardCornerFor(box);
    QPainterPath shape;
    shape.addRoundedRect(box, corner, corner);

    // Ombre portée : la carte repose sur un tissu, qui reçoit son ombre.
    painter->setPen(Qt::NoPen);
    if (!look.noShadow) {
        QColor shade = scheme.feltEdge;
        shade.setAlpha(look.chosen ? 130 : 96);
        QPainterPath cast;
        cast.addRoundedRect(box.translated(0.0, look.chosen ? 3.5 : 2.0), corner, corner);
        painter->fillPath(cast, shade);
    }

    const CardStyle style = lookStyle(scheme, look);

    // Une carte issue d'un jeu installé se substitue à celle que la
    // cartothèque dessinerait.
    QPixmap art;
    if (const CardSet *set = CardDecks::instance().current()) {
        const int slot = CardSet::slotFor(card.joker() ? 0 : card.rank, int(card.suit));
        art = set->faceImage(slot, box.size().toSize());
    }

    if (art.isNull()) {
        drawDefaultFace(painter, box, card.joker() ? 0 : card.rank, int(card.suit), style);
    } else {
        // L'image d'un jeu installé peut comporter des zones transparentes :
        // le papier est posé d'abord, et le filet du bord avec lui.
        painter->setPen(Qt::NoPen);
        painter->fillPath(shape, style.paper);
        painter->setBrush(Qt::NoBrush);
        painter->setPen(QPen(blendColours(style.paper, scheme.cardInk, 0.22), 1.0));
        painter->drawPath(shape);

        painter->save();
        // Découpée selon les coins arrondis.
        painter->setClipPath(shape);
        QRectF where(0, 0, art.width(), art.height());
        where.moveCenter(box.center());
        painter->drawPixmap(where.topLeft(), art);
        painter->restore();
    }

    // Liseré : la carte sélectionnée est bordée de la couleur de la table,
    // seul moyen de signaler une sélection sur une carte par ailleurs
    // entièrement blanche.
    painter->setBrush(Qt::NoBrush);
    if (look.chosen) {
        painter->setPen(QPen(scheme.accent, std::max(1.6, box.width() * 0.035)));
        painter->drawPath(shape);
    }

    if (look.glow > 0.005) {
        // Carte qui vient d'être jouée : un anneau qui s'estompe, afin qu'un
        // coup joué par un adversaire soit perçu au moment où il survient.
        QColor halo = scheme.gold;
        halo.setAlphaF(std::clamp(look.glow, 0.0, 1.0) * 0.95);
        painter->setPen(QPen(halo, std::max(2.0, box.width() * 0.055)));
        painter->drawPath(shape);
    }
    painter->restore();
}

void drawCardBack(QPainter *painter, const QRectF &box, const Palette &scheme,
                  const CardLook &look) {
    if (box.width() < 3 || box.height() < 5) return;
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    const qreal corner = cardCornerFor(box);
    QPainterPath shape;
    shape.addRoundedRect(box, corner, corner);

    painter->setPen(Qt::NoPen);
    if (!look.noShadow) {
        QColor shade = scheme.feltEdge;
        shade.setAlpha(96);
        QPainterPath cast;
        cast.addRoundedRect(box.translated(0.0, 2.0), corner, corner);
        painter->fillPath(cast, shade);
    }

    // Le dos sélectionné dans les réglages, s'il en existe un, remplace le
    // treillis.
    //
    // Il est demandé à `TableLook` et non à la cartothèque : le dos ne
    // provient pas nécessairement du jeu dont sont tirées les faces, et seul
    // `TableLook` connaît son origine.
    QPixmap art;
    if (const CardSet *set = backCards()) {
        art = set->backImage(backIndex(), box.size().toSize());
    }

    if (art.isNull()) {
        // Le treillis est recopié depuis le cache plutôt que redessiné.
        const QPixmap &tile = drawnBack(box.size(), scheme, look.lit);
        if (!tile.isNull()) painter->drawPixmap(box.topLeft(), tile);
    } else {
        // La carte d'un jeu installé peut comporter des zones transparentes :
        // le corps est dessiné d'abord, l'image par-dessus.
        QColor back = scheme.cardBack;
        if (look.lit) back = blendColours(back, QColor(Qt::white), 0.18);
        painter->fillPath(shape, back);

        painter->save();
        painter->setClipPath(shape);
        QRectF where(0, 0, art.width(), art.height());
        where.moveCenter(box.center());
        painter->drawPixmap(where.topLeft(), art);
        painter->restore();
    }

    if (look.glow > 0.005) {
        QColor halo = scheme.gold;
        halo.setAlphaF(std::clamp(look.glow, 0.0, 1.0) * 0.9);
        painter->setPen(QPen(halo, std::max(2.0, box.width() * 0.055)));
        painter->setBrush(Qt::NoBrush);
        painter->drawPath(shape);
    }
    painter->restore();
}

void drawCardSlot(QPainter *painter, const QRectF &box, const Palette &scheme,
                  const QString &label) {
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    const qreal corner = cardCornerFor(box);

    QColor line = scheme.feltText;
    line.setAlpha(110);
    QPen dashes(line, 1.6, Qt::DashLine);
    dashes.setDashPattern({4.0, 3.5});
    painter->setPen(dashes);
    QColor hollow = scheme.feltEdge;
    hollow.setAlpha(70);
    painter->setBrush(hollow);
    painter->drawRoundedRect(box, corner, corner);

    if (!label.isEmpty() && box.width() > 36) {
        QFont word = plainFont(painter);
        word.setPixelSize(std::max(8, int(box.height() * 0.11)));
        word.setCapitalization(QFont::AllUppercase);
        word.setLetterSpacing(QFont::PercentageSpacing, 120);
        painter->setFont(word);
        painter->setPen(line);
        painter->drawText(box, Qt::AlignCenter | Qt::TextWordWrap, label);
    }
    painter->restore();
}

void drawRivalEmblem(QPainter *painter, const QRectF &box, int emblem, const QString &initial,
                     const QColor &ink, const QColor &back) {
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    const qreal span = std::min(box.width(), box.height());
    QRectF disc(0, 0, span, span);
    disc.moveCenter(box.center());

    painter->setPen(Qt::NoPen);
    painter->setBrush(back);
    painter->drawEllipse(disc);
    painter->setPen(QPen(ink, std::max(1.6, span * 0.075)));
    painter->setBrush(Qt::NoBrush);
    painter->drawEllipse(disc.adjusted(span * 0.045, span * 0.045, -span * 0.045, -span * 0.045));

    // Une valeur négative supprime le symbole. C'est le cas de la plaque du
    // joueur : le symbole sert à distinguer les six adversaires entre eux, et
    // le joueur n'en fait pas partie ; le cercle et l'initiale suffisent.
    const bool marked = emblem >= 0;

    // L'initiale est centrée et le symbole placé au-dessus : parmi six
    // médaillons, la couleur permet une identification immédiate, le symbole
    // une identification rapide, et la lettre une identification certaine.
    //
    // L'initiale est légèrement abaissée pour laisser la place au symbole. Ce
    // décalage est lié à la présence du symbole : un médaillon qui n'en porte
    // pas conserve sa lettre au centre du cercle, là où le regard la
    // cherche.
    const qreal drop = marked ? span * 0.06 : 0.0;
    QFont letter = plainFont(painter);
    letter.setBold(true);
    letter.setPixelSize(std::max(8, int(span * 0.44)));
    painter->setFont(letter);
    painter->setPen(ink);
    painter->drawText(capCentred(disc.adjusted(0, drop, 0, drop), QFontMetricsF(letter)),
                      Qt::AlignCenter, initial.left(1).toUpper());

    if (!marked) {
        painter->restore();
        return;
    }

    const qreal mark = span * 0.22;
    const QPointF at(disc.center().x(), disc.top() + span * 0.21);
    painter->setPen(Qt::NoPen);
    painter->setBrush(ink);
    switch (qBound(0, emblem, 5)) {
    case 0: drawPip(painter, at, mark, int(Suit::Pique), ink); break;
    case 1: drawPip(painter, at, mark, int(Suit::Coeur), ink); break;
    case 2: drawPip(painter, at, mark, int(Suit::Carreau), ink); break;
    case 3: drawPip(painter, at, mark, int(Suit::Trefle), ink); break;
    case 4: {
        QPainterPath star;
        for (int i = 0; i < 10; ++i) {
            const qreal angle = -M_PI / 2.0 + i * M_PI / 5.0;
            const qreal reach = (i % 2 == 0) ? mark * 0.58 : mark * 0.25;
            const QPointF spoke(at.x() + std::cos(angle) * reach, at.y() + std::sin(angle) * reach);
            if (i == 0) {
                star.moveTo(spoke);
            } else {
                star.lineTo(spoke);
            }
        }
        star.closeSubpath();
        painter->drawPath(star);
        break;
    }
    default: {
        // Un croissant.
        QPainterPath moon;
        moon.addEllipse(QRectF(at.x() - mark * 0.55, at.y() - mark * 0.55, mark * 1.1, mark * 1.1));
        QPainterPath bite;
        bite.addEllipse(QRectF(at.x() - mark * 0.22, at.y() - mark * 0.62, mark * 1.1, mark * 1.1));
        painter->drawPath(moon.subtracted(bite));
        break;
    }
    }
    painter->restore();
}

void drawLevelStars(QPainter *painter, const QRectF &box, int stars, int outOf, const QColor &ink,
                    const QColor &empty) {
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(Qt::NoPen);

    const int count = std::max(1, outOf);
    const qreal step = box.width() / count;
    const qreal span = std::min(step * 0.78, box.height());
    for (int i = 0; i < count; ++i) {
        const QPointF at(box.left() + step * (i + 0.5), box.center().y());
        QPainterPath star;
        for (int corner = 0; corner < 10; ++corner) {
            const qreal angle = -M_PI / 2.0 + corner * M_PI / 5.0;
            const qreal reach = (corner % 2 == 0) ? span * 0.5 : span * 0.21;
            const QPointF spoke(at.x() + std::cos(angle) * reach, at.y() + std::sin(angle) * reach);
            if (corner == 0) {
                star.moveTo(spoke);
            } else {
                star.lineTo(spoke);
            }
        }
        star.closeSubpath();
        painter->setBrush(i < stars ? ink : empty);
        painter->drawPath(star);
    }
    painter->restore();
}

void drawGauge(QPainter *painter, const QRectF &box, qreal filled, const QColor &ink,
               const QColor &back) {
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(Qt::NoPen);
    const qreal round = box.height() / 2.0;
    painter->setBrush(back);
    painter->drawRoundedRect(box, round, round);
    const qreal part = std::clamp(filled, 0.0, 1.0);
    if (part > 0.001) {
        QRectF run = box;
        run.setWidth(std::max(box.height(), box.width() * part));
        painter->setBrush(ink);
        painter->drawRoundedRect(run, round, round);
    }
    painter->restore();
}

void drawHandPile(QPainter *painter, const QRectF &box, int cards, const Palette &scheme,
                  const QColor &tint) {
    if (cards <= 0) return;
    // Le tas ne comporte pas un dos par carte — quinze dos sur quatre-vingts
    // pixels seraient indistincts — mais un nombre suffisant pour qu'une main
    // nombreuse paraisse telle.
    const int shown = std::min(cards, 7);
    const qreal height = box.height();
    const QSizeF card = cardSizeForHeight(height);
    const qreal step = shown > 1 ? std::min(card.width() * 0.42,
                                            (box.width() - card.width()) / (shown - 1))
                                 : 0.0;
    for (int i = 0; i < shown; ++i) {
        QRectF at(box.left() + step * i, box.top(), card.width(), card.height());
        drawCardBack(painter, at, scheme);
    }

    // Le nombre exact de cartes, affiché en chiffres.
    QFont count = plainFont(painter);
    count.setBold(true);
    count.setPixelSize(std::max(9, int(height * 0.42)));
    painter->save();
    painter->setFont(count);
    const QString text = QString::number(cards);
    const QFontMetricsF metrics(count);
    QRectF badge(0, 0, std::max(metrics.horizontalAdvance(text) + height * 0.34, height * 0.62),
                 height * 0.52);
    badge.moveCenter(QPointF(box.left() + step * (shown - 1) + card.width() * 0.5,
                             box.center().y()));
    painter->setPen(Qt::NoPen);
    painter->setBrush(tint);
    painter->drawRoundedRect(badge, badge.height() / 2.0, badge.height() / 2.0);
    painter->setPen(readableTextOn(tint));
    painter->drawText(capCentred(badge, metrics), Qt::AlignCenter, text);
    painter->restore();
}
