#include "SettingsDialog.h"

#include "CardArt.h"
#include "CardDeck.h"
#include "DialogChrome.h"
#include "Palette.h"
#include "Tables.h"

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
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace {

constexpr int kHeadingGap = 8;
constexpr int kSectionGap = 20;

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

    // --- Pendant la partie.
    chrome.fields->addWidget(dialogSectionHeading(this, tr("Pendant la partie")));
    chrome.fields->addSpacing(kHeadingGap);
    m_autoSort = switchRow(this, tr("Ranger la main toute seule après chaque pioche"),
                           m_settings.autoSort);
    m_offerHints = switchRow(this, tr("Proposer le bouton Conseil"), m_settings.offerHints);
    m_celebrate = switchRow(this, tr("Feu d'artifice quand la partie est gagnée"),
                            m_settings.celebrate);
    chrome.fields->addWidget(m_autoSort);
    chrome.fields->addWidget(m_offerHints);
    chrome.fields->addWidget(m_celebrate);
    chrome.fields->addSpacing(10);

    auto *paceRow = new QHBoxLayout;
    paceRow->setSpacing(18);
    auto *paceLabel = new QLabel(tr("Les adversaires jouent :"), this);
    QPalette ink = paceLabel->palette();
    ink.setColor(QPalette::WindowText, currentPalette().surfaceText);
    paceLabel->setPalette(ink);
    paceRow->addWidget(paceLabel);
    m_paces = new QButtonGroup(this);
    const QStringList paces = {tr("posément"), tr("normalement"), tr("vivement")};
    for (int i = 0; i < paces.size(); ++i) {
        auto *button = choiceRow(this, paces.at(i));
        button->setChecked(i == qBound(0, m_settings.pace, 2));
        m_paces->addButton(button, i);
        paceRow->addWidget(button);
    }
    paceRow->addStretch();
    chrome.fields->addLayout(paceRow);
    chrome.fields->addSpacing(kSectionGap);

    // --- Les cartes: le jeu, les deux boutons qui vont avec le dossier, et
    // les dos en aperçu.
    chrome.fields->addWidget(dialogSectionHeading(this, tr("Les cartes")));
    chrome.fields->addSpacing(kHeadingGap);

    auto *deckRow = new QHBoxLayout;
    deckRow->setSpacing(8);
    auto *deckLabel = new QLabel(tr("Jeu de cartes :"), this);
    deckLabel->setPalette(ink);
    m_deck = new QComboBox(this);
    deckRow->addWidget(deckLabel);
    deckRow->addWidget(m_deck, 1);

    auto *openFolder = iconButton(this,
                                  {QStringLiteral("folder-open-symbolic"),
                                   QStringLiteral("folder-symbolic"),
                                   QStringLiteral("document-open-symbolic"),
                                   QStringLiteral("folder")},
                                  tr("Dossier"), tr("Ouvrir le dossier des cartes"));
    connect(openFolder, &QToolButton::clicked, this, [] {
        CardDecks::ensureFolder();
        QDesktopServices::openUrl(QUrl::fromLocalFile(CardDecks::folder()));
    });
    auto *rescan = iconButton(this,
                              {QStringLiteral("view-refresh-symbolic"),
                               QStringLiteral("view-refresh")},
                              tr("Relire"), tr("Relire le dossier des cartes"));
    connect(rescan, &QToolButton::clicked, this, [this] {
        CardDecks::instance().refresh();
        fillDecks();
    });
    deckRow->addWidget(openFolder);
    deckRow->addWidget(rescan);
    chrome.fields->addLayout(deckRow);
    chrome.fields->addSpacing(4);

    m_deckNote = new QLabel(this);
    m_deckNote->setWordWrap(true);
    markHintLabel(m_deckNote);
    chrome.fields->addWidget(m_deckNote);
    chrome.fields->addSpacing(10);

    auto *zoomRow = new QHBoxLayout;
    zoomRow->setSpacing(10);
    auto *zoomLabel = new QLabel(tr("Taille des cartes :"), this);
    zoomLabel->setPalette(ink);
    m_zoom = new QSlider(Qt::Horizontal, this);
    m_zoom->setRange(80, 200);
    m_zoom->setSingleStep(10);
    m_zoom->setPageStep(10);
    m_zoom->setTickInterval(10);
    m_zoom->setValue(qBound(80, m_settings.zoom, 200));
    m_zoom->setFixedWidth(220);
    m_zoomNote = new QLabel(this);
    markHintLabel(m_zoomNote);
    auto sayZoom = [this] {
        // Arrondi au cran: la poignée se pose entre deux sans cela.
        const int at = int(std::round(m_zoom->value() / 10.0)) * 10;
        m_zoomNote->setText(at == 100 ? tr("%1 % — la taille d'origine").arg(at)
                                      : tr("%1 %").arg(at));
    };
    connect(m_zoom, &QSlider::valueChanged, this, sayZoom);
    sayZoom();
    zoomRow->addWidget(zoomLabel);
    zoomRow->addWidget(m_zoom);
    zoomRow->addWidget(m_zoomNote);
    zoomRow->addStretch();
    chrome.fields->addLayout(zoomRow);
    chrome.fields->addSpacing(4);
    auto *zoomWhy = new QLabel(
        tr("Un jeu installé apporte des images d'une finesse donnée : agrandies, "
           "les figures posées sur le tapis se lisent. Ctrl et la molette font la "
           "même chose pendant la partie."),
        this);
    zoomWhy->setWordWrap(true);
    markHintLabel(zoomWhy);
    chrome.fields->addWidget(zoomWhy);
    chrome.fields->addSpacing(12);

    auto *backLabel = new QLabel(tr("Dos :"), this);
    backLabel->setPalette(ink);
    chrome.fields->addWidget(backLabel);
    chrome.fields->addSpacing(5);

    // Les dos se choisissent sur l'image plutôt que sur leur nom: « Treillis »
    // ne dit pas à quoi un dos ressemble, et c'est la seule chose qu'on veut
    // savoir d'un dos.
    const Palette &scheme = currentPalette();
    const int backWide = int(std::ceil(kBackHeight * kCardRatio));
    m_backs = new QListWidget(this);
    m_backs->setViewMode(QListView::IconMode);
    m_backs->setFlow(QListView::LeftToRight);
    m_backs->setWrapping(false);
    m_backs->setMovement(QListView::Static);
    m_backs->setResizeMode(QListView::Adjust);
    m_backs->setSelectionMode(QAbstractItemView::SingleSelection);
    m_backs->setIconSize(QSize(backWide, kBackHeight));
    m_backs->setGridSize(QSize(std::max(backWide + 22, 86), kBackHeight + 30));
    m_backs->setSpacing(6);
    m_backs->setFrameShape(QFrame::NoFrame);
    m_backs->setWordWrap(true);
    m_backs->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_backs->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    // Juste la place d'une rangée de vignettes et de la barre qui glisse
    // dessous quand un jeu en apporte plus qu'il n'en tient.
    m_backs->setFixedHeight(kBackHeight + 30 + 22);
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
    chrome.fields->addWidget(m_backs);
    chrome.fields->addStretch(1);

    fillDecks();
    connect(m_deck, &QComboBox::currentIndexChanged, this, [this] {
        CardDecks::instance().choose(m_deck->currentData().toString());
        fillBacks();
        describeDeck();
    });
    connect(m_backs, &QListWidget::currentItemChanged, this,
            [](QListWidgetItem *now, QListWidgetItem *) {
                if (!now) return;
                const int which = now->data(Qt::UserRole).toInt();
                if (which >= 0) CardDecks::instance().chooseBack(which);
            });

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

    // Raccourcie d'autant que la longueur de la partie en occupait: elle se
    // choisit au menu d'ouverture, au-dessus des tables dont elle change le
    // compte.
    resize(700, 660);
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
    CardDecks &decks = CardDecks::instance();
    const CardSet *set = decks.current();

    // Muet pendant qu'on le remplit: sans cela, la première vignette posée
    // se ferait choisir et écrirait son numéro par-dessus celui qui est gardé.
    QSignalBlocker quiet(m_backs);
    m_backs->clear();

    // La même règle que le tapis: la vignette est exactement la carte que la
    // table dessinera, proportion et largeur comprises.
    const QSizeF box = cardSizeForHeight(kBackHeight);

    if (!set || set->backCount() == 0) {
        // Un seul aperçu, celui que le programme dessine -- et rien à choisir.
        auto *item = new QListWidgetItem(QIcon(backPreview(set, -1, box)),
                                         tr("celui du jeu"), m_backs);
        item->setData(Qt::UserRole, -1);
        item->setTextAlignment(Qt::AlignHCenter | Qt::AlignTop);
        item->setFlags(Qt::ItemIsEnabled);
        return;
    }

    const int chosen = decks.chosenBack();
    for (int at = 0; at < set->backCount(); ++at) {
        auto *item = new QListWidgetItem(QIcon(backPreview(set, at, box)),
                                         set->backName(at), m_backs);
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
    chosen.pace = qBound(0, m_paces->checkedId(), 2);
    chosen.autoSort = m_autoSort->isChecked();
    chosen.offerHints = m_offerHints->isChecked();
    chosen.celebrate = m_celebrate->isChecked();
    chosen.zoom = int(std::round(m_zoom->value() / 10.0)) * 10;
    return chosen;
}
