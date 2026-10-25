#include "TablePicker.h"

#include "AskDialog.h"
#include "CardArt.h"
#include "Fonts.h"
#include "MenuStyle.h"
#include "NameDialog.h"
#include "Palette.h"
#include "Profiles.h"
#include "RivalCard.h"
#include "RivalSheet.h"
#include "Rivals.h"
#include "Tables.h"
#include "TileButton.h"
#include "TitleMark.h"

#include <QAction>
#include <QCoreApplication>
#include <QFontInfo>
#include <QFontMetricsF>
#include <QEasingCurve>
#include <QFrame>
#include <QGraphicsOpacityEffect>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QPainter>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace {

constexpr QSize kTableTile(188, 170);

// Espacement entre deux éléments voisins sur le tapis, tuiles et cartes
// comprises. Cette valeur est nommée plutôt que répétée, l'accord des deux
// largeurs calculées ci-dessous en dépendant.
constexpr int kFeltGap = 10;

// La rangée des cinq tables détermine la largeur du bloc, et la grille des
// six adversaires s'y aligne : trois colonnes se partagent exactement la
// largeur des cinq tuiles, afin que les deux sections aient le même bord
// droit. La valeur est calculée et non écrite en dur, pour qu'un changement de
// taille des tuiles se répercute sur les cartes.
constexpr int kBlockWidth = 5 * kTableTile.width() + 4 * kFeltGap;
constexpr int kRivalCardWidth = (kBlockWidth - 2 * kFeltGap) / 3;

// Marge de la page, hauteur de la bande du logo, et hauteur du logo dans
// celle-ci.
constexpr int kMargin = 34;
constexpr int kHeaderHeight = 114;
// La ligne supérieure du logo ne représentant plus que le quart de la
// grande, le logo en tête de page est augmenté d'autant : à défaut,
// « PDCartes » y serait rendu en deçà de la taille où cette police reste
// lisible.
constexpr int kMarkHeight = 82;

// Espacement entre deux sections, et entre un titre de section et son
// contenu. Le premier est large et le second réduit : c'est l'écart qui
// rattache un titre à son contenu, et des sections espacées de façon uniforme
// seraient perçues comme un bloc unique.
constexpr int kSectionGap = 44;
constexpr int kHeadingGap = 10;
// Durée du fondu d'apparition de la page pendant le déplacement du logo.
// Elle est légèrement inférieure à celle de l'animation, afin que le menu soit
// affiché à l'arrivée du logo et non après.
constexpr int kEntranceMs = 430;

// Titre de section : quelques mots en capitales dorées, suivis d'un filet
// courant jusqu'au bord de la page. Il est composé d'un libellé et d'un trait
// plutôt que par l'habillage des boîtes de dialogue, lequel est prévu pour un
// fond clair.
//
// La police employée est Zen Dots, celle de la ligne supérieure du logo : les
// titres de section étant les éléments de la page les plus proches d'un
// intitulé, cette police les rattache visuellement au logo. Cette police ne
// comporte pas de bas de casse, ses minuscules étant dessinées en capitales ;
// le texte source conserve donc sa casse ordinaire, et `AllUppercase` ne sert
// qu'à la police de repli, pour que le titre soit en capitales dans tous les
// cas.
//
// Un interlettrage est appliqué et le corps légèrement réduit : Zen Dots étant
// large, des capitales trop resserrées formeraient un bloc compact plutôt que
// des mots distincts.
QWidget *feltSection(QWidget *parent, const QString &text) {
    auto *heading = new QWidget(parent);
    heading->setObjectName(QStringLiteral("feltSection"));
    auto *across = new QHBoxLayout(heading);
    across->setContentsMargins(0, 0, 0, 0);
    across->setSpacing(12);

    auto *label = new QLabel(text, heading);
    label->setObjectName(QStringLiteral("feltSectionLabel"));
    const qreal body = QFontInfo(label->font()).pixelSize();
    QFont caps = familyFont(label->font(), capsFamily(), body * 0.88);
    caps.setCapitalization(QFont::AllUppercase);
    caps.setLetterSpacing(QFont::PercentageSpacing, 118);
    label->setFont(caps);

    auto *rule = new QFrame(heading);
    rule->setObjectName(QStringLiteral("feltSectionRule"));
    rule->setFrameShape(QFrame::HLine);
    rule->setFixedHeight(1);

    across->addWidget(label);
    across->addWidget(rule, 1);
    return heading;
}

// Durée d'une partie, exprimée de façon abrégée. Elle est indiquée par le
// nombre de manches plutôt que par la phrase associée dans `Tables` : une
// mention telle que « Une douzaine de manches » sous « Partie longue »
// répéterait l'information du libellé.
QString lengthCost(const Target &target) {
    if (target.manches <= 1) return QCoreApplication::translate("TablePicker", "1 donne");
    return QCoreApplication::translate("TablePicker", "≈ %1 manches").arg(target.manches);
}

} // namespace

TablePicker::TablePicker(QWidget *parent) : QWidget(parent) {
    // Le tapis couvre toute la surface et est peint par cette classe.
    setAttribute(Qt::WA_OpaquePaintEvent);

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    // La bande du logo ne défile pas : le logo identifie la page et doit
    // rester visible. Elle ne peint rien elle-même, le logo y étant dessiné
    // par la page, afin que l'animation venue de l'écran titre aboutisse sur
    // le tapis et non sur un widget.
    m_header = new QWidget(this);
    m_header->setFixedHeight(kHeaderHeight);
    m_header->setAttribute(Qt::WA_TransparentForMouseEvents);
    outer->addWidget(m_header);

    m_content = new QWidget(this);
    auto *down = new QVBoxLayout(m_content);
    down->setContentsMargins(0, 0, 0, 0);
    down->setSpacing(0);
    outer->addWidget(m_content, 1);

    auto *scroller = new QScrollArea(m_content);
    scroller->setWidgetResizable(true);
    scroller->setFrameShape(QFrame::NoFrame);
    // Une barre de défilement horizontale apparaît lorsque la fenêtre est trop
    // étroite pour les cinq tables ; sans elle, la sixième table et le
    // troisième adversaire seraient tronqués et inaccessibles. Elle n'apparaît
    // que dans ce cas.
    scroller->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    // Le tapis reste visible au travers : ni la zone de défilement, ni son
    // conteneur, ni la page qu'elle contient ne peignent de fond.
    scroller->setStyleSheet(QStringLiteral("QScrollArea { background: transparent; border: 0; }"));
    scroller->viewport()->setAutoFillBackground(false);
    scroller->viewport()->setStyleSheet(QStringLiteral("background: transparent;"));
    down->addWidget(scroller, 1);

    auto *page = new QWidget(scroller);
    page->setAutoFillBackground(false);
    page->setStyleSheet(QStringLiteral("background: transparent;"));
    scroller->setWidget(page);

    auto *rows = new QVBoxLayout(page);
    rows->setContentsMargins(kMargin, 0, kMargin, 22);
    rows->setSpacing(0);
    // Deux ressorts, un de chaque côté : la page est centrée dans l'espace
    // disponible et se cale en haut dès que celui-ci devient insuffisant, un
    // ressort réduit à zéro n'ayant plus d'effet ; c'est alors la barre de
    // défilement qui prend le relais.
    rows->addStretch(1);

    // La durée de la partie, placée en tête : elle modifie la valeur inscrite
    // dans le coin des tuiles, et l'activation d'une table lance la partie ;
    // tout choix préalable doit donc être présenté avant les tables.
    QWidget *lengthHeading = feltSection(page, tr("La partie"));
    m_headings.append(lengthHeading);
    rows->addWidget(lengthHeading);
    rows->addSpacing(kHeadingGap);
    auto *lengths = new QHBoxLayout;
    lengths->setSpacing(9);
    for (const Target &target : targets()) {
        auto *button = new QPushButton(page);
        button->setCursor(Qt::PointingHandCursor);
        button->setFocusPolicy(Qt::NoFocus);
        button->setMinimumHeight(50);

        auto *stack = new QVBoxLayout(button);
        // Un pixel de moins en haut qu'en bas, pour la raison exposée dans
        // `capCentred` (Fonts.h).
        stack->setContentsMargins(15, 5, 15, 7);
        stack->setSpacing(0);
        auto *name = new QLabel(target.name, button);
        auto *blurb = new QLabel(lengthCost(target), button);
        QFont smaller = blurb->font();
        if (smaller.pointSizeF() > 0) {
            smaller.setPointSizeF(std::max(7.0, smaller.pointSizeF() - 1.4));
        }
        blurb->setFont(smaller);
        for (QLabel *line : {name, blurb}) {
            // Le clic est traité par le bouton : les libellés le laissent
            // passer.
            line->setAttribute(Qt::WA_TransparentForMouseEvents);
            stack->addWidget(line);
        }

        const QString id = target.id;
        connect(button, &QPushButton::clicked, this, [this, id] {
            if (m_targetId == id) return;
            m_targetId = id;
            // Les tuiles affichent l'objectif dans leur coin, lequel est mis à
            // jour en conséquence.
            reread();
            Q_EMIT targetChosen(id);
        });
        m_lengths.append(LengthPlaque{button, name, blurb});
        lengths->addWidget(button);
    }
    lengths->addStretch();
    rows->addLayout(lengths);
    rows->addSpacing(kSectionGap);

    // Les six adversaires, présentés avant les tables bien qu'aucune
    // invitation ne soit requise : il s'agit d'un choix, et un choix placé
    // après l'élément qui lance la partie ne pourrait plus être exercé.
    QWidget *rivalHeading = feltSection(page, tr("Les adversaires"));
    m_headings.append(rivalHeading);
    rows->addWidget(rivalHeading);
    rows->addSpacing(kHeadingGap);
    auto *grid = new QGridLayout;
    grid->setHorizontalSpacing(kFeltGap);
    grid->setVerticalSpacing(kFeltGap);
    // Une quatrième colonne vide absorbe l'espace restant : sans elle, les
    // trois autres s'étireraient jusqu'au bord de la fenêtre et la grille
    // cesserait de s'aligner sur les tuiles lors d'un élargissement.
    grid->setColumnStretch(3, 1);
    int at = 0;
    for (const Rival &rival : rivals()) {
        auto *card = new RivalCard(rival.id, page);
        card->setPickable(true);
        card->setOnFelt(true);
        // Largeur fixe, comme celle d'une tuile : dans une fenêtre étroite,
        // les deux sections débordent conjointement et la même barre de
        // défilement les couvre, plutôt que l'une se comprime pendant que
        // l'autre conserve sa taille.
        card->setFixedWidth(kRivalCardWidth);
        // Aucune infobulle : c'est l'aperçu des compétences qui s'affiche au
        // maintien du pointeur sur la carte, et deux panneaux déclenchés par le
        // même survol se recouvriraient. L'effet des clics est indiqué en bas
        // de page.
        connect(card, &RivalCard::invitedChanged, this, [this] { reread(); });
        connect(card, &RivalCard::menuWanted, this,
                [this, card](const QPoint &where) { showRivalMenu(card, where); });
        connect(card, &RivalCard::sheetWanted, this,
                [this, card] { showRivalSheet(card->rivalId()); });
        grid->addWidget(card, at / 3, at % 3);
        m_cards.append(card);
        ++at;
    }
    rows->addLayout(grid);
    rows->addSpacing(kSectionGap);

    // Les tables en dernier : leur activation ne constitue pas un choix mais
    // lance la partie.
    QWidget *tableHeading = feltSection(page, tr("La table"));
    m_headings.append(tableHeading);
    rows->addWidget(tableHeading);
    rows->addSpacing(kHeadingGap);
    auto *tiles = new QHBoxLayout;
    tiles->setSpacing(kFeltGap);
    for (const TableKind &kind : tableKinds()) {
        auto *tile = new TileButton(page);
        tile->setText(kind.name);
        tile->setSeats(kind.seats);
        tile->setTileSize(kTableTile);
        tile->setOnFelt(true);
        const QString id = kind.id;
        connect(tile, &TileButton::clicked, this, [this, id] { Q_EMIT tableChosen(id); });
        m_tiles.append(tile);
        tiles->addWidget(tile);
    }
    tiles->addStretch();
    rows->addLayout(tiles);
    rows->addSpacing(12);

    // Unique ligne d'aide de la page. Elle décrit les trois actions non
    // déductibles de l'affichage, le reste étant indiqué sur les éléments
    // eux-mêmes. Elle est placée au pied des tuiles et non au pied de la
    // fenêtre, afin de rester rattachée à ce qu'elle décrit ; ses mentions
    // suivent l'ordre de la page, celle relative aux tables figurant en
    // dernier, immédiatement sous celles-ci.
    m_footer = new QLabel(page);
    m_footer->setText(tr("Un clic invite  ·  deux clics ouvrent sa fiche  ·  "
                         "une table lance la partie"));
    rows->addWidget(m_footer);
    rows->addStretch(1);

    reread();
}

QRectF TablePicker::markRect() const {
    const qreal height = kMarkHeight;
    const qreal width = height * titleMarkAspect(font());
    const qreal top = m_header ? m_header->y() : 0.0;
    return QRectF(kMargin, top + (kHeaderHeight - height) / 2.0 + 4.0, width, height);
}

void TablePicker::setMarkHidden(bool hidden) {
    if (m_markHidden == hidden) return;
    m_markHidden = hidden;
    update();
}

void TablePicker::beginEntrance() {
    if (!m_content) return;
    // L'effet n'est appliqué que pendant le fondu : il fait transiter toute la
    // page par un pixmap, ce qui n'a pas lieu d'être au-delà.
    auto *fade = new QGraphicsOpacityEffect(m_content);
    fade->setOpacity(0.0);
    m_content->setGraphicsEffect(fade);

    auto *rise = new QPropertyAnimation(fade, "opacity", this);
    rise->setDuration(kEntranceMs);
    rise->setStartValue(0.0);
    rise->setEndValue(1.0);
    rise->setEasingCurve(QEasingCurve::OutCubic);
    connect(rise, &QPropertyAnimation::finished, m_content,
            [content = m_content] { content->setGraphicsEffect(nullptr); });
    rise->start(QAbstractAnimation::DeleteWhenStopped);
}

void TablePicker::rebuildFelt() {
    if (width() <= 0 || height() <= 0) {
        m_felt = QPixmap();
        return;
    }
    const qreal ratio = devicePixelRatioF();
    m_felt = QPixmap(QSize(int(std::ceil(width() * ratio)), int(std::ceil(height() * ratio))));
    m_felt.setDevicePixelRatio(ratio);

    QPainter painter(&m_felt);
    painter.setRenderHint(QPainter::Antialiasing, true);
    drawFelt(&painter, QRectF(0, 0, width(), height()), currentPalette(), false);
}

void TablePicker::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);
    if (m_felt.isNull()) rebuildFelt();

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.drawPixmap(0, 0, m_felt);
    if (!m_markHidden) drawTitleMark(&painter, markRect(), font(), currentPalette());
}

void TablePicker::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    rebuildFelt();
}

void TablePicker::setTarget(const QString &targetId) {
    m_targetId = targetId;
    reread();
}

QVector<QString> TablePicker::invited() const {
    QVector<QString> who;
    for (RivalCard *card : m_cards) {
        if (card->invited()) who.append(card->rivalId());
    }
    return who;
}

void TablePicker::showRivalMenu(RivalCard *card, const QPoint &where) {
    const Rival &who = rivalById(card->rivalId());
    const Learning learning = learningFor(who.id);

    QMenu menu(this);
    dressMenu(&menu);
    const QString called = rivalName(who.id);
    QAction *invite = menu.addAction(card->invited() ? tr("Ne pas inviter %1").arg(called)
                                                     : tr("Inviter %1 à la table").arg(called));
    menu.addSeparator();
    QAction *sheet = menu.addAction(tr("Sa fiche…"));
    QAction *rename = menu.addAction(tr("Renommer…"));
    menu.addSeparator();
    QAction *reset = menu.addAction(tr("Remettre son expérience à zéro"));
    // Aucune réinitialisation n'est possible si l'adversaire n'a acquis aucune
    // expérience : l'entrée est alors désactivée, plutôt que d'ouvrir une
    // confirmation sans effet.
    reset->setEnabled(learning.xp > 0 || learning.handsPlayed > 0);

    const QAction *chosen = menu.exec(where);
    if (chosen == invite) {
        card->setInvited(!card->invited());
    } else if (chosen == sheet) {
        showRivalSheet(who.id);
    } else if (chosen == rename) {
        renameRival(who.id);
    } else if (chosen == reset) {
        askResetRival(who.id);
    }
}

void TablePicker::renameRival(const QString &rivalId) {
    NameDialog dialog(tr("Renommer"), tr("Comment s'appelle ce joueur ?"),
                      tr("Son médaillon en prend l'initiale. Il garde sa couleur, son "
                         "expérience et sa façon de jouer. Laissez vide pour lui rendre "
                         "son nom."),
                      rivalName(rivalId), kRivalNameLength, this);
    if (dialog.exec() != QDialog::Accepted) return;
    // Un nom vide rétablit le nom d'origine ; cette opération est effectuée
    // par `setRivalName`.
    setRivalName(rivalId, dialog.name());
    reread();
}

void TablePicker::showRivalSheet(const QString &rivalId) {
    RivalSheet sheet(rivalId, this);
    connect(&sheet, &RivalSheet::changed, this, [this] { reread(); });
    sheet.exec();
    reread();
}

void TablePicker::askResetRival(const QString &rivalId) {
    const Rival &who = rivalById(rivalId);
    if (!askYesNo(this, tr("Remettre à zéro"),
                  tr("Remettre %1 à son niveau de départ ?").arg(rivalName(rivalId)),
                  tr("Remettre à zéro"),
                  tr("Sur « %1 » : %2 points d'expérience, soit %3.")
                      .arg(profileName(currentProfile()))
                      .arg(who.startXp)
                      .arg(levelName(levelOf(who.startXp)).toLower()))) {
        return;
    }
    resetRivalExperience(rivalId);
    reread();
}

void TablePicker::dressLengths() {
    const Palette &scheme = currentPalette();
    const QString here = m_targetId.isEmpty() ? defaultTargetId() : m_targetId;
    for (int i = 0; i < m_lengths.size() && i < targets().size(); ++i) {
        const Target &target = targets().at(i);
        const LengthPlaque &plaque = m_lengths.at(i);
        const bool mine = target.id == here;

        QFont name = plaque.name->font();
        name.setBold(mine);
        plaque.name->setFont(name);

        // La largeur est imposée et non déduite : un `QPushButton` calcule sa
        // taille d'après son propre texte, vide ici, et non d'après les widgets
        // qu'il contient ; sans cela, les plaques se réduiraient à la largeur
        // d'un caractère. Elle est calculée sur le nom en gras dans tous les
        // cas, afin que la plaque ne change pas de largeur lors de sa
        // sélection.
        QFont widest = name;
        widest.setBold(true);
        const qreal room =
            std::max(QFontMetricsF(widest).horizontalAdvance(plaque.name->text()),
                     QFontMetricsF(plaque.blurb->font())
                         .horizontalAdvance(plaque.blurb->text()));
        plaque.button->setMinimumWidth(int(std::ceil(room)) + 32);
        plaque.name->setStyleSheet(
            QStringLiteral("color: %1; background: transparent;")
                .arg((mine ? scheme.gold : scheme.feltText).name()));
        // La durée est affichée dans un doré atténué : il s'agit d'une valeur
        // secondaire et non de l'information principale.
        plaque.blurb->setStyleSheet(
            QStringLiteral("color: rgba(%1,%2,%3,%4); background: transparent;")
                .arg(scheme.gold.red())
                .arg(scheme.gold.green())
                .arg(scheme.gold.blue())
                .arg(mine ? 230 : 150));

        const QColor fill = feltPlaqueFill(scheme, mine ? 0.14 : 0.0);
        plaque.button->setStyleSheet(
            QStringLiteral("QPushButton { border-radius: 9px;"
                           " background: rgba(%1,%2,%3,%4);"
                           " border: %5px solid %6; }"
                           "QPushButton:hover { border-color: %7; }")
                .arg(fill.red())
                .arg(fill.green())
                .arg(fill.blue())
                .arg(fill.alpha())
                .arg(mine ? 2 : 1)
                .arg(feltGold(scheme, mine ? 0.95 : 0.26).name(QColor::HexArgb),
                     feltGold(scheme, 0.75).name(QColor::HexArgb)));
    }
}

void TablePicker::reread() {
    dressLengths();
    const Target &target = targetById(m_targetId.isEmpty() ? defaultTargetId() : m_targetId);
    for (int i = 0; i < m_tiles.size() && i < tableKinds().size(); ++i) {
        const TableKind &kind = tableKinds().at(i);
        const int points = targetPointsFor(target, kind.seats);
        m_tiles.at(i)->setCorner(points > 0 ? tr("%1 pts").arg(points) : tr("1 manche"));
    }

    for (RivalCard *card : m_cards) card->reread();
}

void TablePicker::refresh() {
    const Palette &scheme = currentPalette();
    rebuildFelt();

    for (QWidget *heading : m_headings) {
        if (auto *label = heading->findChild<QLabel *>(QStringLiteral("feltSectionLabel"))) {
            label->setStyleSheet(
                QStringLiteral("color: %1; background: transparent;").arg(scheme.gold.name()));
        }
        if (auto *rule = heading->findChild<QFrame *>(QStringLiteral("feltSectionRule"))) {
            rule->setStyleSheet(QStringLiteral("background: %1; border: 0;")
                                    .arg(feltGold(scheme, 0.28).name(QColor::HexArgb)));
        }
    }

    QColor aside = scheme.feltText;
    aside.setAlphaF(0.62);
    m_footer->setStyleSheet(QStringLiteral("color: %1; background: transparent;")
                                .arg(aside.name(QColor::HexArgb)));

    reread();
    update();
}
