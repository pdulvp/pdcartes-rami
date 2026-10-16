#include "TablePicker.h"

#include "DialogChrome.h"
#include "Palette.h"
#include "Profiles.h"
#include "RivalCard.h"
#include "Rivals.h"
#include "Tables.h"
#include "TileButton.h"

#include <QAction>
#include <QContextMenuEvent>
#include <QEvent>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

namespace {

const QSize kTableTile(200, 182);

// Quel profil une plaque porte. Rangé sur le bouton lui-même, pour que le
// filtre qui reçoit l'événement sache de qui il s'agit sans compter les
// boutons.
const char *kSlotProperty = "profileSlot";

// Habille un menu contextuel aux couleurs de la table. Un menu est une
// fenêtre à lui et n'hérite donc de rien: les couleurs viennent de
// currentPalette(), comme pour les boîtes de dialogue.
//
// Les menus sont montés sur la pile et leur action choisie comparée au
// retour d'exec(), plutôt que posés sur le tas avec une lambda par action:
// ce que fait une entrée se lit alors à côté de ce qu'elle dit.
void dressMenu(QMenu *menu) {
    const Palette &scheme = currentPalette();
    menu->setStyleSheet(
        QStringLiteral("QMenu { background: %1; color: %2; border: 1px solid %3;"
                       " border-radius: 8px; padding: 5px; }"
                       "QMenu::item { padding: 6px 22px 6px 14px; border-radius: 5px; }"
                       "QMenu::item:selected { background: %4; color: %5; }"
                       "QMenu::item:disabled { color: %6; }"
                       "QMenu::separator { height: 1px; background: %3; margin: 5px 8px; }")
            .arg(scheme.surface.name(), scheme.surfaceText.name(), scheme.edge.name(),
                 scheme.accent.name(), scheme.accentText.name(), scheme.hint.name()));
}

QLabel *heading(QWidget *parent, const QString &text, qreal scale, bool bold) {
    auto *label = new QLabel(text, parent);
    QFont font = label->font();
    font.setBold(bold);
    if (font.pointSizeF() > 0) font.setPointSizeF(font.pointSizeF() * scale);
    label->setFont(font);
    return label;
}

} // namespace

TablePicker::TablePicker(QWidget *parent) : QWidget(parent) {
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    auto *scroller = new QScrollArea(this);
    scroller->setWidgetResizable(true);
    scroller->setFrameShape(QFrame::NoFrame);
    scroller->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    outer->addWidget(scroller);

    auto *page = new QWidget(scroller);
    scroller->setWidget(page);

    auto *rows = new QVBoxLayout(page);
    rows->setContentsMargins(30, 24, 30, 26);
    rows->setSpacing(0);

    m_title = heading(page, tr("Ramisol"), 2.1, true);
    m_subtitle = heading(page, tr("Le rami, à deux ou à six, contre des adversaires qui "
                                  "apprennent d'une partie à l'autre."),
                         1.0, false);
    rows->addWidget(m_title);
    rows->addSpacing(2);
    rows->addWidget(m_subtitle);
    rows->addSpacing(22);

    // Le profil. Trois, et l'expérience des six est comptée séparément dans
    // chacun -- c'est ce qui permet de garder une table de débutants d'un
    // côté et une table d'experts de l'autre.
    rows->addWidget(dialogSectionHeading(page, tr("Le profil")));
    rows->addSpacing(8);
    auto *profiles = new QHBoxLayout;
    profiles->setSpacing(9);
    for (int slot = 1; slot <= kProfileCount; ++slot) {
        auto *button = new QPushButton(page);
        button->setCursor(Qt::PointingHandCursor);
        button->setFocusPolicy(Qt::NoFocus);
        button->setMinimumHeight(52);
        button->setProperty(kSlotProperty, slot);
        button->setToolTip(tr("Cliquer pour jouer sous ce profil. Double cliquer pour le "
                              "renommer ; clic droit pour le reste."));
        button->installEventFilter(this);
        connect(button, &QPushButton::clicked, this, [this, slot] {
            setCurrentProfile(slot);
            reread();
            Q_EMIT profileChosen(slot);
        });
        m_profiles.append(button);
        profiles->addWidget(button);
    }
    profiles->addStretch();
    rows->addLayout(profiles);
    rows->addSpacing(6);
    m_profileNote = new QLabel(page);
    m_profileNote->setWordWrap(true);
    markHintLabel(m_profileNote);
    rows->addWidget(m_profileNote);
    rows->addSpacing(24);

    // Les tables.
    rows->addWidget(dialogSectionHeading(page, tr("La table")));
    rows->addSpacing(8);
    auto *tiles = new QHBoxLayout;
    tiles->setSpacing(10);
    for (const TableKind &kind : tableKinds()) {
        auto *tile = new TileButton(page);
        tile->setText(kind.name);
        tile->setSeats(kind.seats);
        tile->setTileHue(kind.hue);
        tile->setTileSize(kTableTile);
        const QString id = kind.id;
        connect(tile, &TileButton::clicked, this, [this, id] { Q_EMIT tableChosen(id); });
        m_tiles.append(tile);
        tiles->addWidget(tile);
    }
    tiles->addStretch();
    rows->addLayout(tiles);
    rows->addSpacing(6);
    m_footer = new QLabel(page);
    m_footer->setWordWrap(true);
    markHintLabel(m_footer);
    rows->addWidget(m_footer);
    rows->addSpacing(24);

    // Les six.
    rows->addWidget(dialogSectionHeading(page, tr("Les adversaires")));
    rows->addSpacing(8);
    auto *grid = new QGridLayout;
    grid->setHorizontalSpacing(10);
    grid->setVerticalSpacing(10);
    int at = 0;
    for (const Rival &rival : rivals()) {
        auto *card = new RivalCard(rival.id, page);
        card->setPickable(true);
        card->setToolTip(tr("Cliquer pour inviter %1 à la table ; clic droit pour le "
                            "remettre à son niveau de départ.")
                             .arg(rival.name));
        connect(card, &RivalCard::invitedChanged, this, [this] { reread(); });
        connect(card, &RivalCard::menuWanted, this,
                [this, card](const QPoint &where) { showRivalMenu(card, where); });
        grid->addWidget(card, at / 3, at % 3);
        m_cards.append(card);
        ++at;
    }
    rows->addLayout(grid);
    rows->addSpacing(6);
    m_rivalNote = new QLabel(page);
    m_rivalNote->setWordWrap(true);
    markHintLabel(m_rivalNote);
    rows->addWidget(m_rivalNote);
    rows->addStretch(1);

    reread();
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

bool TablePicker::eventFilter(QObject *watched, QEvent *event) {
    const int slot = watched->property(kSlotProperty).toInt();
    if (slot >= 1 && slot <= kProfileCount) {
        // Rendus tous deux: sans quoi QPushButton fait du double clic un
        // second clic, et du clic droit rien du tout.
        if (event->type() == QEvent::MouseButtonDblClick) {
            renameProfile(slot);
            return true;
        }
        if (event->type() == QEvent::ContextMenu) {
            const auto *where = static_cast<QContextMenuEvent *>(event);
            showProfileMenu(slot, where->globalPos());
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void TablePicker::showProfileMenu(int slot, const QPoint &where) {
    QMenu menu(this);
    dressMenu(&menu);
    QAction *play = menu.addAction(tr("Jouer sous ce profil"));
    play->setEnabled(slot != currentProfile());
    QAction *rename = menu.addAction(tr("Renommer…"));
    menu.addSeparator();
    QAction *forget = menu.addAction(tr("Tout oublier sur ce profil"));

    const QAction *chosen = menu.exec(where);
    if (chosen == play) {
        setCurrentProfile(slot);
        reread();
        Q_EMIT profileChosen(slot);
    } else if (chosen == rename) {
        renameProfile(slot);
    } else if (chosen == forget) {
        askForgetProfile(slot);
    }
}

void TablePicker::renameProfile(int slot) {
    bool said = false;
    const QString was = profileName(slot);
    const QString now =
        QInputDialog::getText(this, tr("Ramisol"), tr("Le nom de ce profil :"),
                              QLineEdit::Normal, was, &said);
    // Un nom vide rendrait la plaque muette: on garde alors l'ancien.
    if (!said || now.trimmed().isEmpty()) return;
    setProfileName(slot, now.trimmed().left(kProfileNameLength));
    reread();
    Q_EMIT profileChanged();
}

void TablePicker::askForgetProfile(int slot) {
    const ProfileRecord record = profileRecord(slot);
    const auto answer = QMessageBox::question(
        this, tr("Ramisol"),
        tr("Effacer les parties de « %1 » et remettre les six adversaires à leur "
           "niveau de départ ?")
            .arg(record.name),
        QMessageBox::Cancel | QMessageBox::Yes);
    if (answer != QMessageBox::Yes) return;
    forgetProfile(slot);
    reread();
    Q_EMIT profileChanged();
}

void TablePicker::showRivalMenu(RivalCard *card, const QPoint &where) {
    const Rival &who = rivalById(card->rivalId());
    const Learning learning = learningFor(who.id);

    QMenu menu(this);
    dressMenu(&menu);
    QAction *invite = menu.addAction(card->invited() ? tr("Ne pas inviter %1").arg(who.name)
                                                     : tr("Inviter %1 à la table").arg(who.name));
    menu.addSeparator();
    QAction *reset = menu.addAction(tr("Niveau de départ"));
    // Rien à remettre quand il n'a rien appris: l'entrée le dit en étant
    // éteinte, plutôt qu'en posant une question dont la réponse ne fait rien.
    reset->setEnabled(learning.xp > who.startXp);
    reset->setToolTip(tr("Remettre %1 au niveau avec lequel il arrive, sur ce profil "
                         "seulement")
                          .arg(who.name));

    const QAction *chosen = menu.exec(where);
    if (chosen == invite) {
        card->setInvited(!card->invited());
    } else if (chosen == reset) {
        askResetRival(who.id);
    }
}

void TablePicker::askResetRival(const QString &rivalId) {
    const Rival &who = rivalById(rivalId);
    const auto answer = QMessageBox::question(
        this, tr("Ramisol"),
        tr("Remettre %1 à %2 points d'expérience (%3), sur « %4 » ?")
            .arg(who.name)
            .arg(who.startXp)
            .arg(levelName(levelOf(who.startXp)).toLower(), profileName(currentProfile())),
        QMessageBox::Cancel | QMessageBox::Yes);
    if (answer != QMessageBox::Yes) return;
    resetRival(rivalId);
    reread();
}

void TablePicker::dressProfiles() {
    const Palette &scheme = currentPalette();
    const int here = currentProfile();
    for (int i = 0; i < m_profiles.size(); ++i) {
        const int slot = i + 1;
        const ProfileRecord record = profileRecord(slot);
        QPushButton *button = m_profiles.at(i);
        const bool mine = slot == here;

        QString line = record.name;
        if (record.matchesPlayed > 0) {
            line += QStringLiteral("\n") +
                    tr("%1 parties, %2 gagnées").arg(record.matchesPlayed).arg(record.matchesWon);
        } else {
            line += QStringLiteral("\n") + tr("aucune partie encore");
        }
        button->setText(line);
        button->setStyleSheet(
            QStringLiteral("QPushButton { text-align: left; padding: 7px 14px;"
                           " border-radius: 8px; background: %1; color: %2;"
                           " border: %3px solid %4; font-weight: %5; }"
                           "QPushButton:hover { border-color: %6; }")
                .arg(mine ? blendColours(scheme.surface, scheme.accent, 0.18).name()
                          : scheme.surface.name(),
                     scheme.surfaceText.name())
                .arg(mine ? 2 : 1)
                .arg(mine ? scheme.accent.name() : scheme.edge.name())
                .arg(mine ? QStringLiteral("bold") : QStringLiteral("normal"),
                     scheme.accent.name()));
    }
}

void TablePicker::reread() {
    dressProfiles();

    const ProfileRecord record = profileRecord(currentProfile());
    m_profileNote->setText(
        tr("Vous jouez sous « %1 ». Chaque profil a sa propre expérience des adversaires. "
           "Double cliquez sur une plaque pour la renommer.")
            .arg(record.name));

    const Target &target = targetById(m_targetId.isEmpty() ? defaultTargetId() : m_targetId);
    for (int i = 0; i < m_tiles.size() && i < tableKinds().size(); ++i) {
        const TableKind &kind = tableKinds().at(i);
        const int points = targetPointsFor(target, kind.seats);
        m_tiles.at(i)->setCorner(points > 0 ? tr("%1 pts").arg(points) : tr("1 manche"));
    }
    m_footer->setText(tr("Presser une table lance la partie. %1 : %2.")
                          .arg(target.name, target.blurb.toLower()));

    const QVector<QString> who = invited();
    QStringList names;
    for (const QString &id : who) names.append(rivalById(id).name);
    const QString gesture = tr(" Un clic droit remet un adversaire à son niveau de départ.");
    if (names.isEmpty()) {
        m_rivalNote->setText(tr("Les places vont aux moins joués. Cliquez sur une carte pour "
                                "inviter quelqu'un.") +
                             gesture);
    } else {
        m_rivalNote->setText(tr("Invités : %1. Les places qui restent vont aux moins joués.")
                                 .arg(names.join(QStringLiteral(", "))) +
                             gesture);
    }

    for (RivalCard *card : m_cards) card->reread();
}

void TablePicker::refresh() {
    const Palette &scheme = currentPalette();
    QPalette page = palette();
    page.setColor(QPalette::Window, scheme.desk);
    page.setColor(QPalette::WindowText, scheme.surfaceText);
    setPalette(page);
    setAutoFillBackground(true);

    m_title->setStyleSheet(QStringLiteral("color: %1;").arg(scheme.accent.name()));
    m_subtitle->setStyleSheet(QStringLiteral("color: %1;").arg(scheme.hint.name()));
    for (QLabel *note : {m_profileNote, m_rivalNote, m_footer}) {
        note->setStyleSheet(QStringLiteral("color: %1;").arg(scheme.hint.name()));
    }
    recolourSectionHeadings(this);
    reread();
    update();
}
