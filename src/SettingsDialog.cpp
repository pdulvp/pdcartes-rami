#include "SettingsDialog.h"

#include "CardDeck.h"
#include "DialogChrome.h"
#include "Palette.h"
#include "Profiles.h"
#include "RivalCard.h"
#include "Rivals.h"
#include "Tables.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QUrl>
#include <QScrollArea>
#include <QVBoxLayout>

namespace {

constexpr int kHeadingGap = 8;
constexpr int kSectionGap = 20;

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

} // namespace

SettingsDialog::SettingsDialog(const GameSettings &settings, QWidget *parent)
    : QDialog(parent), m_settings(settings) {
    setWindowTitle(tr("Réglages"));

    const DialogChrome chrome = applyDialogChrome(this, tr("Réglages"));

    // --- La longueur de la partie.
    chrome.fields->addWidget(dialogSectionHeading(this, tr("La longueur de la partie")));
    chrome.fields->addSpacing(kHeadingGap);

    m_targets = new QButtonGroup(this);
    auto *lengths = new QHBoxLayout;
    lengths->setSpacing(18);
    int at = 0;
    for (const Target &target : targets()) {
        auto *button = choiceRow(this, target.name);
        button->setChecked(target.id == m_settings.targetId);
        m_targets->addButton(button, at++);
        lengths->addWidget(button);
        connect(button, &QRadioButton::toggled, this, [this] { describeTargets(); });
    }
    lengths->addStretch();
    chrome.fields->addLayout(lengths);
    chrome.fields->addSpacing(4);
    m_targetNote = new QLabel(this);
    m_targetNote->setWordWrap(true);
    markHintLabel(m_targetNote);
    chrome.fields->addWidget(m_targetNote);
    chrome.fields->addSpacing(kSectionGap);

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

    // --- Les cartes.
    chrome.fields->addWidget(dialogSectionHeading(this, tr("Les cartes")));
    chrome.fields->addSpacing(kHeadingGap);

    auto *deckRow = new QHBoxLayout;
    deckRow->setSpacing(10);
    auto *deckLabel = new QLabel(tr("Jeu de cartes :"), this);
    deckLabel->setPalette(ink);
    m_deck = new QComboBox(this);
    auto *backLabel = new QLabel(tr("Dos :"), this);
    backLabel->setPalette(ink);
    m_back = new QComboBox(this);
    deckRow->addWidget(deckLabel);
    deckRow->addWidget(m_deck, 2);
    deckRow->addWidget(backLabel);
    deckRow->addWidget(m_back, 1);
    chrome.fields->addLayout(deckRow);
    chrome.fields->addSpacing(4);

    m_deckNote = new QLabel(this);
    m_deckNote->setWordWrap(true);
    markHintLabel(m_deckNote);
    chrome.fields->addWidget(m_deckNote);
    chrome.fields->addSpacing(6);

    auto *deckButtons = new QHBoxLayout;
    deckButtons->setSpacing(8);
    auto *openFolder = new QPushButton(tr("Ouvrir le dossier des cartes"), this);
    openFolder->setCursor(Qt::PointingHandCursor);
    connect(openFolder, &QPushButton::clicked, this, [this] {
        CardDecks::ensureFolder();
        QDesktopServices::openUrl(QUrl::fromLocalFile(CardDecks::folder()));
    });
    auto *rescan = new QPushButton(tr("Relire le dossier"), this);
    rescan->setCursor(Qt::PointingHandCursor);
    connect(rescan, &QPushButton::clicked, this, [this] {
        CardDecks::instance().refresh();
        fillDecks();
    });
    deckButtons->addWidget(openFolder);
    deckButtons->addWidget(rescan);
    deckButtons->addStretch();
    chrome.fields->addLayout(deckButtons);
    chrome.fields->addSpacing(kSectionGap);

    fillDecks();
    connect(m_deck, &QComboBox::currentIndexChanged, this, [this] {
        CardDecks::instance().choose(m_deck->currentData().toString());
        fillBacks();
        describeDeck();
    });
    connect(m_back, &QComboBox::currentIndexChanged, this, [this] {
        if (m_back->currentIndex() >= 0) {
            CardDecks::instance().chooseBack(m_back->currentData().toInt());
        }
    });

    // --- Le profil.
    chrome.fields->addWidget(dialogSectionHeading(this, tr("Le profil")));
    chrome.fields->addSpacing(kHeadingGap);
    auto *nameRow = new QHBoxLayout;
    nameRow->setSpacing(10);
    auto *nameLabel = new QLabel(tr("Nom du profil %1 :").arg(currentProfile()), this);
    nameLabel->setPalette(ink);
    m_profileName = new QLineEdit(profileName(currentProfile()), this);
    m_profileName->setMaxLength(24);
    nameRow->addWidget(nameLabel);
    nameRow->addWidget(m_profileName, 1);

    auto *forget = new QPushButton(tr("Tout oublier sur ce profil"), this);
    forget->setCursor(Qt::PointingHandCursor);
    connect(forget, &QPushButton::clicked, this, [this] {
        const ProfileRecord record = profileRecord(currentProfile());
        const auto answer = QMessageBox::question(
            this, tr("Ramisol"),
            tr("Effacer les parties de « %1 » et remettre les six adversaires à leur "
               "niveau de départ ?")
                .arg(record.name),
            QMessageBox::Cancel | QMessageBox::Yes);
        if (answer != QMessageBox::Yes) return;
        forgetProfile(currentProfile());
        m_profileName->setText(profileName(currentProfile()));
        for (RivalCard *card : m_cards) card->reread();
    });
    nameRow->addWidget(forget);
    chrome.fields->addLayout(nameRow);
    chrome.fields->addSpacing(kSectionGap);

    // --- Les six.
    chrome.fields->addWidget(dialogSectionHeading(this, tr("Les adversaires")));
    chrome.fields->addSpacing(kHeadingGap);
    auto *note = new QLabel(
        tr("L'expérience monte à chaque manche jouée. Le bouton remet un adversaire à son "
           "niveau de départ, sur ce profil."),
        this);
    note->setWordWrap(true);
    markHintLabel(note);
    chrome.fields->addWidget(note);
    chrome.fields->addSpacing(8);

    auto *scroller = new QScrollArea(this);
    scroller->setWidgetResizable(true);
    scroller->setFrameShape(QFrame::NoFrame);
    scroller->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroller->setMinimumHeight(250);
    auto *pen = new QWidget(scroller);
    auto *grid = new QGridLayout(pen);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(10);
    grid->setVerticalSpacing(10);
    int which = 0;
    for (const Rival &rival : rivals()) {
        auto *card = new RivalCard(rival.id, pen);
        card->showReset(true);
        const QString id = rival.id;
        connect(card, &RivalCard::wantsReset, this, [this, card, id] {
            const Rival &who = rivalById(id);
            const auto answer = QMessageBox::question(
                this, tr("Ramisol"),
                tr("Remettre %1 à %2 points d'expérience (%3), sur « %4 » ?")
                    .arg(who.name)
                    .arg(who.startXp)
                    .arg(levelName(levelOf(who.startXp)).toLower(),
                         profileName(currentProfile())),
                QMessageBox::Cancel | QMessageBox::Yes);
            if (answer != QMessageBox::Yes) return;
            resetRival(id);
            card->reread();
        });
        grid->addWidget(card, which / 2, which % 2);
        m_cards.append(card);
        ++which;
    }
    grid->setRowStretch(which / 2 + 1, 1);
    scroller->setWidget(pen);
    chrome.fields->addWidget(scroller, 1);

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

    describeTargets();
    resize(772, 744);
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

    QSignalBlocker quiet(m_back);
    m_back->clear();
    if (!set || set->backCount() == 0) {
        m_back->addItem(tr("celui du jeu"), 0);
        m_back->setEnabled(false);
        return;
    }
    m_back->setEnabled(true);
    const int chosen = decks.chosenBack();
    for (int at = 0; at < set->backCount(); ++at) {
        m_back->addItem(set->backName(at), at);
        if (at == chosen) m_back->setCurrentIndex(at);
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

void SettingsDialog::describeTargets() {
    const int which = m_targets->checkedId();
    if (which < 0 || which >= targets().size()) return;
    const Target &target = targets().at(which);
    const TableKind &table = tableKindById(m_settings.tableId);
    const int points = targetPointsFor(target, table.seats);

    if (points <= 0) {
        m_targetNote->setText(tr("Une seule donne."));
        return;
    }
    m_targetNote->setText(tr("%1 à la table « %2 » : %3 points. Le but s'ajuste à la "
                             "table. Le moins de points l'emporte.")
                              .arg(target.name, table.name.toLower())
                              .arg(points));
}

GameSettings SettingsDialog::settings() const {
    GameSettings chosen = m_settings;
    const int target = m_targets->checkedId();
    if (target >= 0 && target < targets().size()) chosen.targetId = targets().at(target).id;
    chosen.pace = qBound(0, m_paces->checkedId(), 2);
    chosen.autoSort = m_autoSort->isChecked();
    chosen.offerHints = m_offerHints->isChecked();
    chosen.celebrate = m_celebrate->isChecked();

    const QString name = m_profileName->text().trimmed();
    if (!name.isEmpty()) setProfileName(currentProfile(), name);
    return chosen;
}
