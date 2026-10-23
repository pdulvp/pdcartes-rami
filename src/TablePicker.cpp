#include "TablePicker.h"

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
#include <QMessageBox>
#include <QPainter>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace {

const QSize kTableTile(188, 170);

// La marge de la page, la bande du logo, et la hauteur du logo dedans.
constexpr int kMargin = 34;
constexpr int kHeaderHeight = 114;
// La ligne du dessus ne fait plus que le quart de la grande: le logo en tête
// de page est d'autant plus haut, sans quoi `PDCartes` y tomberait sous la
// taille où ce trait de fonte se tient encore.
constexpr int kMarkHeight = 82;

// Ce qui sépare une section de la suivante, et ce qui sépare un titre de
// section de ce qu'il annonce. Le premier est large et le second serré: c'est
// l'écart qui dit à quoi un titre appartient, et trois sections également
// espacées de leur titre et les unes des autres n'en font qu'une longue.
constexpr int kSectionGap = 44;
constexpr int kHeadingGap = 10;
// Le temps que la page met à paraître derrière le logo qui se pose. Un peu
// plus court que le vol: le menu est là quand le logo arrive, et non après.
constexpr int kEntranceMs = 430;

// Le titre d'une section: trois mots d'or en capitales, et un filet qui court
// jusqu'au bout de la page. Dessiné par une étiquette et une règle plutôt que
// par la chrome des dialogues, qui est faite pour un fond pâle.
//
// En Zen Dots, la fonte de la ligne du dessus du logo: les titres de section
// sont ce que la page a de plus proche d'une enseigne, et les voir dans la
// fonte du logo les rattache à lui. Elle n'a pas de bas-de-casse -- ses
// minuscules sont ses capitales -- si bien que le texte source garde sa
// casse ordinaire et que `AllUppercase` ne sert qu'au repli: sans la fonte,
// le titre est en capitales quand même.
//
// De l'interlettrage, et un corps un peu réduit: Zen Dots est déjà large, et
// des capitales serrées font un bloc plutôt que des mots.
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

// Ce qu'une longueur coûte, en une poignée de caractères. Dit par le nombre
// de manches plutôt que par la phrase qui l'accompagne dans `Tables` -- « Une
// douzaine de manches » sous « Partie longue » répète ce qu'on vient de lire.
QString lengthCost(const Target &target) {
    if (target.manches <= 1) return QCoreApplication::translate("TablePicker", "1 donne");
    return QCoreApplication::translate("TablePicker", "≈ %1 manches").arg(target.manches);
}

} // namespace

TablePicker::TablePicker(QWidget *parent) : QWidget(parent) {
    // Le tapis couvre tout, et c'est nous qui le peignons.
    setAttribute(Qt::WA_OpaquePaintEvent);

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    // La bande du logo ne défile pas: le logo est l'enseigne de la page, et
    // une enseigne qui s'en va au premier cran de molette n'en est pas une.
    // Elle ne peint rien elle-même -- c'est la page qui dessine le logo
    // dedans, pour que le vol depuis l'écran titre arrive sur le tapis et non
    // sur un widget.
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
    // Une barre horizontale quand la fenêtre est trop étroite pour les cinq
    // tables: sans elle, la sixième place et le troisième adversaire sont
    // coupés et hors d'atteinte. Elle ne paraît que là.
    scroller->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    // Le drap se voit à travers: ni la zone, ni sa vitre, ni la page dedans
    // ne peignent de fond.
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
    // Deux ressorts, un de chaque côté: la page se tient au milieu de la
    // place qu'elle a, et se range en haut dès qu'elle n'en a plus -- un
    // ressort rendu à zéro est un ressort qui ne fait rien, et c'est alors
    // la barre de défilement qui prend la suite.
    rows->addStretch(1);

    // La longueur de la partie. Au-dessus des tables, parce qu'elle change ce
    // qui est écrit dans leur coin -- et parce que presser une table lance la
    // partie: ce qui se choisit avant doit se lire avant.
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
        // Un pixel de moins en haut qu'en bas, pour la même raison que
        // partout ailleurs: voir `capCentred` dans Fonts.h.
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
            // Le clic appartient au bouton: les étiquettes le laissent passer.
            line->setAttribute(Qt::WA_TransparentForMouseEvents);
            stack->addWidget(line);
        }

        const QString id = target.id;
        connect(button, &QPushButton::clicked, this, [this, id] {
            if (m_targetId == id) return;
            m_targetId = id;
            // Les tuiles portent le but dans leur coin: il bouge avec lui.
            reread();
            Q_EMIT targetChosen(id);
        });
        m_lengths.append(LengthPlaque{button, name, blurb});
        lengths->addWidget(button);
    }
    lengths->addStretch();
    rows->addLayout(lengths);
    rows->addSpacing(kSectionGap);

    // Les tables.
    QWidget *tableHeading = feltSection(page, tr("La table"));
    m_headings.append(tableHeading);
    rows->addWidget(tableHeading);
    rows->addSpacing(kHeadingGap);
    auto *tiles = new QHBoxLayout;
    tiles->setSpacing(10);
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
    rows->addSpacing(kSectionGap);

    // Les six.
    QWidget *rivalHeading = feltSection(page, tr("Les adversaires"));
    m_headings.append(rivalHeading);
    rows->addWidget(rivalHeading);
    rows->addSpacing(kHeadingGap);
    auto *grid = new QGridLayout;
    grid->setHorizontalSpacing(10);
    grid->setVerticalSpacing(10);
    int at = 0;
    for (const Rival &rival : rivals()) {
        auto *card = new RivalCard(rival.id, page);
        card->setPickable(true);
        card->setOnFelt(true);
        // Pas d'infobulle: c'est l'aperçu de ses compétences qui paraît quand
        // on reste sur sa carte, et deux panneaux au même survol se marchent
        // dessus. Ce que les clics font est écrit en bas de page.
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
    rows->addSpacing(12);

    // La seule ligne de la page. Les trois gestes qu'on ne devine pas, et
    // rien d'autre: tout le reste est écrit sur la chose elle-même. Sous la
    // grille plutôt qu'au pied de la fenêtre, pour qu'elle appartienne à ce
    // qu'elle explique.
    m_footer = new QLabel(page);
    m_footer->setText(tr("Une table lance la partie  ·  un clic invite  ·  "
                         "deux clics ouvrent sa fiche"));
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
    // L'effet n'est posé que le temps de la montée: il fait passer toute la
    // page par un pixmap, ce qui n'a pas à durer au-delà.
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
    // Rien à remettre quand il n'a rien appris: l'entrée le dit en étant
    // éteinte, plutôt qu'en posant une question dont la réponse ne fait rien.
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
    // Un nom vide le fait répondre au sien: c'est setRivalName qui s'en
    // charge, et c'est la façon de le débaptiser.
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
    const auto answer = QMessageBox::question(
        this, tr("pdcartes-rami"),
        tr("Remettre %1 à %2 points d'expérience (%3), sur « %4 » ?")
            .arg(rivalName(rivalId))
            .arg(who.startXp)
            .arg(levelName(levelOf(who.startXp)).toLower(), profileName(currentProfile())),
        QMessageBox::Cancel | QMessageBox::Yes);
    if (answer != QMessageBox::Yes) return;
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

        // La largeur, dite plutôt que déduite: un QPushButton tire sa taille
        // de son propre texte, qui est vide ici, et pas de ce qu'on a mis
        // dedans -- sans cela les plaques se referment sur une lettre. Prise
        // sur le nom en gras quoi qu'il arrive, pour que la plaque ne change
        // pas de largeur en étant choisie.
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
        // Ce que la longueur coûte, dans l'or éteint: c'est un chiffre, pas
        // ce qu'on vient lire.
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
