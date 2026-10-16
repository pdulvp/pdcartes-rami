#include "SettingsDialog.h"

#include "CardArt.h"
#include "CardDeck.h"
#include "DialogChrome.h"
#include "Palette.h"
#include "TableLook.h"

#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QRadioButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QStackedWidget>
#include <QToolButton>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace {

constexpr int kHeadingGap = 8;

// La liste des pages, à gauche. Assez large pour le plus long des quatre noms,
// et pas plus: ce qui reste est pour la page.
constexpr int kSectionsWidth = 172;
constexpr int kSectionRowHeight = 30;

// La hauteur d'un aperçu de dos. Assez grand pour qu'un motif se reconnaisse,
// assez petit pour que les dos d'un jeu tiennent sur une ligne.
constexpr int kBackHeight = 92;

QCheckBox *switchRow(QWidget *parent, const QString &text, bool on) {
    auto *box = new QCheckBox(text, parent);
    box->setChecked(on);
    // Told the table's ink rather than left to inherit it: a platform style
    // hands a checkbox a palette of its own, which outranks the one the
    // dialog's body is wearing.
    QPalette ink = box->palette();
    ink.setColor(QPalette::WindowText, currentPalette().surfaceText);
    ink.setColor(QPalette::Text, currentPalette().surfaceText);
    box->setPalette(ink);
    return box;
}

QRadioButton *choiceRow(QWidget *parent, const QString &text) {
    auto *button = new QRadioButton(text, parent);
    QPalette ink = button->palette();
    ink.setColor(QPalette::WindowText, currentPalette().surfaceText);
    ink.setColor(QPalette::Text, currentPalette().surfaceText);
    button->setPalette(ink);
    return button;
}

// Une étiquette posée devant un champ, dans l'encre du corps de la boîte.
QLabel *fieldLabel(QWidget *parent, const QString &text) {
    auto *label = new QLabel(text, parent);
    QPalette ink = label->palette();
    ink.setColor(QPalette::WindowText, currentPalette().surfaceText);
    label->setPalette(ink);
    return label;
}

// Une ligne d'explication sous le champ qu'elle explique.
QLabel *asideLabel(QWidget *parent, const QString &text) {
    auto *label = new QLabel(text, parent);
    label->setWordWrap(true);
    markHintLabel(label);
    return label;
}

// Un bouton d'icône, à droite du champ sur lequel il agit.
//
// Deux grands boutons en bas de la section disaient la même chose et la
// disaient loin de la liste qu'ils relisaient. Ici ils sont contre elle, et
// ce qu'ils font tient dans leur infobulle -- sauf sur un bureau dont le
// thème ne porte pas ces icônes, où chacun reprend son mot.
QToolButton *iconButton(QWidget *parent, const QStringList &iconNames, const QString &fallback,
                        const QString &tip) {
    const Palette &scheme = currentPalette();
    auto *button = new QToolButton(parent);
    button->setToolTip(tip);
    button->setCursor(Qt::PointingHandCursor);
    button->setFocusPolicy(Qt::NoFocus);
    button->setIconSize(QSize(17, 17));

    const QIcon icon = tintedIcon(iconNames, scheme.surfaceText);
    if (icon.isNull()) {
        button->setText(fallback);
        button->setToolButtonStyle(Qt::ToolButtonTextOnly);
    } else {
        button->setIcon(icon);
        button->setToolButtonStyle(Qt::ToolButtonIconOnly);
        button->setFixedSize(32, 30);
    }
    button->setStyleSheet(QStringLiteral("QToolButton { background: %1; color: %2;"
                                         " border: 1px solid %3; border-radius: 6px;"
                                         " padding: 4px 7px; }"
                                         "QToolButton:hover { border: 1px solid %4; }"
                                         "QToolButton:pressed { background: %5; }")
                              .arg(scheme.field.name(), scheme.surfaceText.name(),
                                   scheme.edge.name(), scheme.accent.name(),
                                   blendColours(scheme.field, scheme.accent, 0.18).name()));
    return button;
}

// L'aperçu d'un dos, dessiné comme la table le dessinera.
//
// `set` nul -- ou un dos que le jeu choisi n'a pas -- rend le treillis que le
// programme dessine lui-même, qui est bien ce qui serait sur la table dans ce
// cas-là. L'aperçu porte donc toujours la vérité.
QPixmap backPreview(const CardSet *set, int at, const QSizeF &box) {
    const qreal dpr = qApp->devicePixelRatio();
    QPixmap tile(QSize(int(std::ceil(box.width() * dpr)), int(std::ceil(box.height() * dpr))));
    tile.setDevicePixelRatio(dpr);
    tile.fill(Qt::transparent);

    QPainter painter(&tile);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    const QRectF where(0.5, 0.5, box.width() - 1.0, box.height() - 1.0);
    const QPixmap art = set ? set->backImage(at, tile.size()) : QPixmap();
    if (art.isNull()) {
        CardLook look;
        look.noShadow = true;
        drawCardBack(&painter, where, currentPalette(), look);
        return tile;
    }

    const qreal corner = cardCornerFor(where);
    QPainterPath shape;
    shape.addRoundedRect(where, corner, corner);
    painter.setPen(Qt::NoPen);
    painter.fillPath(shape, currentPalette().cardBack);
    painter.setClipPath(shape);
    QRectF onto(0, 0, art.width() / dpr, art.height() / dpr);
    onto.moveCenter(where.center());
    painter.drawPixmap(onto, art, QRectF(0, 0, art.width(), art.height()));
    return tile;
}

} // namespace

SettingsDialog::SettingsDialog(const GameSettings &settings, QWidget *parent)
    : QDialog(parent), m_settings(settings) {
    setWindowTitle(tr("Réglages"));

    const DialogChrome chrome = applyDialogChrome(this, tr("Réglages"));

    // La liste des pages, et les pages. Une liste plate: les quatre groupes
    // n'ont rien sous eux.
    m_sections = new QTreeWidget(this);
    m_sections->setFixedWidth(kSectionsWidth);
    m_sections->setHeaderHidden(true);
    m_sections->setRootIsDecorated(false);
    m_sections->setFrameShape(QFrame::NoFrame);   // la feuille de style dessine le bord
    m_sections->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_sections->setFocusPolicy(Qt::NoFocus);

    m_pages = new QStackedWidget(this);

    auto *columns = new QHBoxLayout;
    columns->setSpacing(kDialogMargin);
    columns->addWidget(m_sections);
    columns->addWidget(m_pages, 1);
    chrome.fields->addLayout(columns, 1);

    // Dans l'ordre où l'on y pense: ce qui touche au jeu d'abord, le décor
    // après, puisqu'il ne change rien à la partie.
    buildDuringPage();
    buildCardsPage();
    buildBacksPage();
    buildFeltPage();

    // Après les pages, et non au bout de celle des cartes: remplir la liste
    // des jeux remplit aussi celle des dos, qui est sur la page d'après.
    fillDecks();
    dressLists();

    connect(m_sections, &QTreeWidget::currentItemChanged, this,
            [this](QTreeWidgetItem *now, QTreeWidgetItem *) {
                if (now) m_pages->setCurrentIndex(now->data(0, Qt::UserRole).toInt());
            });
    m_sections->setCurrentItem(m_sections->topLevelItem(0));

    // --- Le pied.
    auto *buttons = new QDialogButtonBox(this);
    buttons->addButton(tr("Annuler"), QDialogButtonBox::RejectRole);
    QPushButton *ok = buttons->addButton(tr("Valider"), QDialogButtonBox::AcceptRole);
    markDefaultButton(ok);
    ok->setDefault(true);
    chrome.footer->addStretch();
    chrome.footer->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    // Haute de ce qu'il faut à la plus remplie des quatre pages -- les cartes,
    // à cause de la rangée de dos -- pour que passer de l'une à l'autre ne
    // redimensionne jamais la fenêtre: le mou reste en bas des pages qui en
    // demandent moins. Et assez courte pour tenir sur un écran d'ordinateur
    // portable, ce que la colonne unique de huit cents pixels ne faisait pas.
    resize(780, 490);
}

QVBoxLayout *SettingsDialog::addPage(const QString &name, const QString &heading) {
    auto *page = new QWidget(m_pages);
    auto *rows = new QVBoxLayout(page);
    rows->setContentsMargins(0, 0, 0, 0);
    rows->setSpacing(0);
    rows->addWidget(dialogSectionHeading(page, heading));
    rows->addSpacing(kHeadingGap);

    const int at = m_pages->addWidget(page);
    auto *row = new QTreeWidgetItem(m_sections, {name});
    row->setData(0, Qt::UserRole, at);
    row->setSizeHint(0, QSize(0, kSectionRowHeight));
    return rows;
}

void SettingsDialog::buildDuringPage() {
    QVBoxLayout *rows = addPage(tr("Pendant la partie"), tr("Pendant la partie"));
    QWidget *page = m_pages->widget(m_pages->count() - 1);

    m_autoSort = switchRow(page, tr("Ranger la main toute seule après chaque pioche"),
                           m_settings.autoSort);
    m_offerHints = switchRow(page, tr("Proposer le bouton Conseil"), m_settings.offerHints);
    m_celebrate = switchRow(page, tr("Feu d'artifice quand la partie est gagnée"),
                            m_settings.celebrate);
    rows->addWidget(m_autoSort);
    rows->addWidget(m_offerHints);
    rows->addWidget(m_celebrate);
    rows->addSpacing(14);

    auto *paceRow = new QHBoxLayout;
    paceRow->setSpacing(18);
    paceRow->addWidget(fieldLabel(page, tr("Les adversaires jouent :")));
    m_paces = new QButtonGroup(this);
    const QStringList paces = {tr("posément"), tr("normalement"), tr("vivement")};
    for (int i = 0; i < paces.size(); ++i) {
        auto *button = choiceRow(page, paces.at(i));
        button->setChecked(i == qBound(0, m_settings.pace, 2));
        m_paces->addButton(button, i);
        paceRow->addWidget(button);
    }
    paceRow->addStretch();
    rows->addLayout(paceRow);
    rows->addStretch(1);
}

void SettingsDialog::buildCardsPage() {
    QVBoxLayout *rows = addPage(tr("Les cartes"), tr("Les cartes"));
    QWidget *page = m_pages->widget(m_pages->count() - 1);

    auto *deckRow = new QHBoxLayout;
    deckRow->setSpacing(8);
    deckRow->addWidget(fieldLabel(page, tr("Jeu de cartes :")));
    m_deck = new QComboBox(page);
    deckRow->addWidget(m_deck, 1);

    auto *openFolder = iconButton(page,
                                  {QStringLiteral("folder-open-symbolic"),
                                   QStringLiteral("folder-symbolic"),
                                   QStringLiteral("document-open-symbolic"),
                                   QStringLiteral("folder")},
                                  tr("Dossier"), tr("Ouvrir le dossier des cartes"));
    connect(openFolder, &QToolButton::clicked, this, [] {
        CardDecks::ensureFolder();
        QDesktopServices::openUrl(QUrl::fromLocalFile(CardDecks::folder()));
    });
    auto *rescan = iconButton(page,
                              {QStringLiteral("view-refresh-symbolic"),
                               QStringLiteral("view-refresh")},
                              tr("Relire"), tr("Relire le dossier des cartes"));
    connect(rescan, &QToolButton::clicked, this, [this] {
        CardDecks::instance().refresh();
        fillDecks();
    });
    deckRow->addWidget(openFolder);
    deckRow->addWidget(rescan);
    rows->addLayout(deckRow);
    rows->addSpacing(4);

    m_deckNote = asideLabel(page, QString());
    rows->addWidget(m_deckNote);
    rows->addSpacing(12);

    auto *zoomRow = new QHBoxLayout;
    zoomRow->setSpacing(10);
    zoomRow->addWidget(fieldLabel(page, tr("Taille des cartes :")));
    m_zoom = new QSlider(Qt::Horizontal, page);
    m_zoom->setRange(80, 200);
    m_zoom->setSingleStep(10);
    m_zoom->setPageStep(10);
    m_zoom->setTickInterval(10);
    m_zoom->setValue(qBound(80, m_settings.zoom, 200));
    m_zoom->setFixedWidth(220);
    m_zoomNote = new QLabel(page);
    markHintLabel(m_zoomNote);
    auto sayZoom = [this] {
        // Arrondi au cran: la poignée se pose entre deux sans cela.
        const int at = int(std::round(m_zoom->value() / 10.0)) * 10;
        m_zoomNote->setText(at == 100 ? tr("%1 % — la taille d'origine").arg(at)
                                      : tr("%1 %").arg(at));
    };
    connect(m_zoom, &QSlider::valueChanged, this, sayZoom);
    sayZoom();
    zoomRow->addWidget(m_zoom);
    zoomRow->addWidget(m_zoomNote);
    zoomRow->addStretch();
    rows->addLayout(zoomRow);
    rows->addSpacing(4);
    rows->addWidget(asideLabel(
        page, tr("Un jeu installé apporte des images d'une finesse donnée : agrandies, "
                 "les figures posées sur le tapis se lisent. Ctrl et la molette font la "
                 "même chose pendant la partie.")));
    rows->addStretch(1);

    connect(m_deck, &QComboBox::currentIndexChanged, this, [this] {
        rememberDeck(m_deck->currentData().toString());
        fillBacks();
        describeDeck();
    });
}

void SettingsDialog::buildBacksPage() {
    QVBoxLayout *rows = addPage(tr("Les dos"), tr("Les dos"));
    QWidget *page = m_pages->widget(m_pages->count() - 1);

    // Les dos se choisissent sur l'image plutôt que sur leur nom: « Treillis »
    // ne dit pas à quoi un dos ressemble, et c'est la seule chose qu'on veut
    // savoir d'un dos.
    const int backWide = int(std::ceil(kBackHeight * kCardRatio));
    m_backs = new QListWidget(page);
    m_backs->setViewMode(QListView::IconMode);
    m_backs->setFlow(QListView::LeftToRight);
    m_backs->setWrapping(false);
    m_backs->setMovement(QListView::Static);
    m_backs->setSelectionMode(QAbstractItemView::SingleSelection);
    m_backs->setIconSize(QSize(backWide, kBackHeight));
    // Large de la vignette et d'un peu plus, et jamais moins que de quoi
    // écrire « celui du jeu » sur deux lignes: un nom de dos coupé net ne dit
    // plus lequel c'est.
    m_backs->setGridSize(QSize(std::max(backWide + 26, 104), kBackHeight + 32));
    m_backs->setSpacing(6);
    m_backs->setFrameShape(QFrame::NoFrame);
    m_backs->setWordWrap(true);
    m_backs->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_backs->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    // Juste la place d'une rangée de vignettes et de la barre qui glisse
    // dessous quand un jeu en apporte plus qu'il n'en tient.
    m_backs->setFixedHeight(kBackHeight + 32 + 22);
    rows->addWidget(m_backs);
    rows->addSpacing(6);
    rows->addWidget(asideLabel(
        page, tr("Le dos est ce qu'on voit de la pioche et des mains d'en face pendant "
                 "toute la manche. Un jeu installé apporte les siens ; sans eux, c'est le "
                 "treillis que le programme dessine, dans la couleur du tapis. Le dos "
                 "appartient au profil, comme le jeu de cartes.")));
    rows->addStretch(1);

    connect(m_backs, &QListWidget::currentItemChanged, this,
            [](QListWidgetItem *now, QListWidgetItem *) {
                if (!now) return;
                const int which = now->data(Qt::UserRole).toInt();
                if (which >= 0) rememberBack(which);
            });
}

void SettingsDialog::buildFeltPage() {
    QVBoxLayout *rows = addPage(tr("Le tapis"), tr("Le tapis"));
    QWidget *page = m_pages->widget(m_pages->count() - 1);

    auto *feltRow = new QHBoxLayout;
    feltRow->setSpacing(8);
    for (const FeltColour &felt : feltColours()) {
        auto *swatch = new QToolButton(page);
        swatch->setCursor(Qt::PointingHandCursor);
        swatch->setFocusPolicy(Qt::NoFocus);
        swatch->setToolTip(felt.name);
        swatch->setFixedSize(58, 40);
        const int hue = felt.hue;
        connect(swatch, &QToolButton::clicked, this, [this, hue] {
            if (m_settings.hue == hue) return;
            m_settings.hue = hue;
            // Essayée en vrai: la boîte est elle-même peinte aux couleurs du
            // tapis, alors changer la teinte se voit tout de suite plutôt
            // qu'une fois la boîte refermée.
            setCurrentPalette(hue);
            refreshDialogChrome(this);
            // Les titres des quatre pages portent la couleur du tapis, et ils
            // la prennent au moment où ils sont faits: la feuille de style de
            // la boîte ne les atteint pas, il faut le leur dire.
            recolourSectionHeadings(this);
            dressFelts();
            dressLists();
            // Les vignettes des dos sont dessinées aux couleurs du tapis: le
            // treillis que le programme dessine en est fait, et le fond sur
            // lequel une carte transparente est posée aussi.
            fillBacks();
        });
        m_felts.append(swatch);
        feltRow->addWidget(swatch);
    }
    feltRow->addStretch();
    rows->addLayout(feltRow);
    rows->addSpacing(6);
    rows->addWidget(asideLabel(
        page, tr("La couleur du drap, et de tout ce qui va avec : la bande du haut, les "
                 "plaques, et les boîtes comme celle-ci. Elle ne dépend pas du nombre de "
                 "joueurs -- une table de six n'est pas d'une autre couleur qu'une table "
                 "de deux, elle est seulement plus pleine -- et chaque profil garde la "
                 "sienne.")));
    rows->addStretch(1);

    dressFelts();
}

void SettingsDialog::dressFelts() {
    for (int i = 0; i < m_felts.size() && i < feltColours().size(); ++i) {
        const FeltColour &felt = feltColours().at(i);
        // Chaque pastille porte le drap qu'elle propose, pas une case de
        // couleur: c'est le tapis qu'on choisit, avec sa bordure et son ombre,
        // et c'est lui qu'il faut montrer.
        const Palette scheme = paletteForHue(felt.hue, darkMode());
        const bool mine = felt.hue == m_settings.hue;
        m_felts.at(i)->setStyleSheet(
            QStringLiteral("QToolButton { border-radius: 8px; background: %1;"
                           " border: %2px solid %3; }"
                           "QToolButton:hover { border: 2px solid %4; }")
                .arg(scheme.felt.name())
                .arg(mine ? 3 : 1)
                .arg(mine ? currentPalette().accent.name() : scheme.feltEdge.name(),
                     currentPalette().accent.name()));
    }
}

void SettingsDialog::dressLists() {
    const Palette &scheme = currentPalette();

    // La liste des pages: la page en cours dans la couleur du tapis, les
    // autres à plat sur le corps de la boîte. Pas de cadre autour -- elle est
    // une colonne de la boîte et non un champ posé dedans -- mais un filet sur
    // sa droite, qui est ce qui sépare la liste de la page.
    if (m_sections) {
        m_sections->setStyleSheet(
            QStringLiteral("QTreeWidget { background: transparent; color: %1;"
                           " border: none; border-right: 1px solid %2; padding: 2px 6px 2px 0; }"
                           "QTreeView::item { border: 1px solid transparent;"
                           " border-radius: 6px; padding: 4px 8px; }"
                           "QTreeView::item:hover { background: %3; }"
                           "QTreeView::item:selected { background: %4; color: %5;"
                           " font-weight: bold; }")
                .arg(scheme.surfaceText.name(), scheme.edge.name(),
                     blendColours(scheme.surface, scheme.accent, 0.12).name(),
                     scheme.accent.name(), scheme.accentText.name()));
    }

    if (m_backs) {
        m_backs->setStyleSheet(
            QStringLiteral("QListWidget { background: %1; color: %2; border: 1px solid %3;"
                           " border-radius: 8px; padding: 3px; }"
                           "QListWidget::item { border: 1px solid transparent;"
                           " border-radius: 8px; padding: 4px; }"
                           "QListWidget::item:hover { border: 1px solid %4; }"
                           "QListWidget::item:selected { background: %5;"
                           " border: 1px solid %6; color: %2; }")
                .arg(scheme.field.name(), scheme.surfaceText.name(), scheme.edge.name(),
                     blendColours(scheme.edge, scheme.accent, 0.5).name(),
                     blendColours(scheme.field, scheme.accent, 0.18).name(),
                     scheme.accent.name()));
    }
}

void SettingsDialog::fillDecks() {
    CardDecks &decks = CardDecks::instance();
    const QString chosen = decks.chosenId();

    QSignalBlocker quiet(m_deck);
    m_deck->clear();
    for (const DeckEntry &entry : decks.entries()) {
        QString label = entry.name;
        if (!entry.builtIn && !entry.complete) {
            label += tr(" — %1 cartes sur %2").arg(entry.faces).arg(CardSet::kSlots);
        }
        m_deck->addItem(label, entry.id);
        if (entry.id == chosen) m_deck->setCurrentIndex(m_deck->count() - 1);
    }
    fillBacks();
    describeDeck();
}

void SettingsDialog::fillBacks() {
    if (!m_backs) return;
    CardDecks &decks = CardDecks::instance();
    const CardSet *set = decks.current();

    // Muet pendant qu'on le remplit: sans cela, la première vignette posée se
    // ferait choisir et écrirait son numéro par-dessus celui qui est gardé.
    QSignalBlocker quiet(m_backs);
    m_backs->clear();

    // La même règle que le tapis: la vignette est exactement la carte que la
    // table dessinera, proportion et largeur comprises.
    const QSizeF box = cardSizeForHeight(kBackHeight);

    if (!set || set->backCount() == 0) {
        // Un seul aperçu, celui que le programme dessine -- et rien à choisir.
        auto *item =
            new QListWidgetItem(QIcon(backPreview(set, -1, box)), tr("celui du jeu"), m_backs);
        item->setData(Qt::UserRole, -1);
        item->setTextAlignment(Qt::AlignHCenter | Qt::AlignTop);
        item->setFlags(Qt::ItemIsEnabled);
        return;
    }

    const int chosen = decks.chosenBack();
    for (int at = 0; at < set->backCount(); ++at) {
        auto *item =
            new QListWidgetItem(QIcon(backPreview(set, at, box)), set->backName(at), m_backs);
        item->setData(Qt::UserRole, at);
        item->setTextAlignment(Qt::AlignHCenter | Qt::AlignTop);
        item->setToolTip(set->backName(at));
        if (at == chosen) m_backs->setCurrentItem(item);
    }
}

void SettingsDialog::describeDeck() {
    CardDecks &decks = CardDecks::instance();
    const DeckEntry *entry = decks.entry(decks.chosenId());
    if (!entry) {
        m_deckNote->setText(QString());
        return;
    }
    QString says;
    if (entry->builtIn) {
        says = tr("Les cartes que le jeu dessine lui-même.");
    } else {
        says = entry->note.trimmed();
        if (!entry->author.trimmed().isEmpty()) {
            says = tr("De %1. %2").arg(entry->author.trimmed(), says);
        }
        if (!entry->complete) {
            says += tr(" %1 cartes manquantes, dessinées par le jeu.")
                        .arg(CardSet::kSlots - entry->faces);
        }
    }
    m_deckNote->setText(says.trimmed());
}

GameSettings SettingsDialog::settings() const {
    // La longueur de la partie se choisit au menu d'ouverture, au-dessus des
    // tables dont elle change le compte: ce qui est ici vaut pour toutes.
    GameSettings chosen = m_settings;
    chosen.hue = m_settings.hue;
    chosen.pace = qBound(0, m_paces->checkedId(), 2);
    chosen.autoSort = m_autoSort->isChecked();
    chosen.offerHints = m_offerHints->isChecked();
    chosen.celebrate = m_celebrate->isChecked();
    chosen.zoom = int(std::round(m_zoom->value() / 10.0)) * 10;
    return chosen;
}
