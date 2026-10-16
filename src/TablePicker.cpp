#include "TablePicker.h"

#include "DialogChrome.h"
#include "Palette.h"
#include "Profiles.h"
#include "MenuStyle.h"
#include "NameDialog.h"
#include "RivalCard.h"
#include "Rivals.h"
#include "Tables.h"
#include "TileButton.h"

#include <QAction>
#include <QFontMetricsF>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

#include <algorithm>

namespace {

const QSize kTableTile(200, 182);

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

    // La longueur de la partie. Au-dessus des tables, parce qu'elle change ce
    // qui est écrit dans leur coin -- et parce que presser une table lance la
    // partie: ce qui se choisit avant doit se lire avant.
    rows->addWidget(dialogSectionHeading(page, tr("La longueur de la partie")));
    rows->addSpacing(8);
    auto *lengths = new QHBoxLayout;
    lengths->setSpacing(9);
    for (const Target &target : targets()) {
        auto *button = new QPushButton(page);
        button->setCursor(Qt::PointingHandCursor);
        button->setFocusPolicy(Qt::NoFocus);
        button->setMinimumHeight(52);

        auto *stack = new QVBoxLayout(button);
        stack->setContentsMargins(14, 7, 14, 7);
        stack->setSpacing(1);
        auto *name = new QLabel(target.name, button);
        auto *blurb = new QLabel(target.blurb.toLower(), button);
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
    rows->addSpacing(6);
    m_lengthNote = new QLabel(page);
    m_lengthNote->setWordWrap(true);
    markHintLabel(m_lengthNote);
    rows->addWidget(m_lengthNote);
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
                             .arg(rivalName(rival.id)));
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

void TablePicker::showRivalMenu(RivalCard *card, const QPoint &where) {
    const Rival &who = rivalById(card->rivalId());
    const Learning learning = learningFor(who.id);

    QMenu menu(this);
    dressMenu(&menu);
    const QString called = rivalName(who.id);
    QAction *invite = menu.addAction(card->invited() ? tr("Ne pas inviter %1").arg(called)
                                                     : tr("Inviter %1 à la table").arg(called));
    menu.addSeparator();
    QAction *rename = menu.addAction(tr("Renommer…"));
    menu.addSeparator();
    QAction *reset = menu.addAction(tr("Niveau de départ"));
    // Rien à remettre quand il n'a rien appris: l'entrée le dit en étant
    // éteinte, plutôt qu'en posant une question dont la réponse ne fait rien.
    reset->setEnabled(learning.xp > who.startXp);
    reset->setToolTip(tr("Remettre %1 au niveau avec lequel il arrive, sur ce profil "
                         "seulement")
                          .arg(called));

    const QAction *chosen = menu.exec(where);
    if (chosen == invite) {
        card->setInvited(!card->invited());
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

void TablePicker::askResetRival(const QString &rivalId) {
    const Rival &who = rivalById(rivalId);
    const auto answer = QMessageBox::question(
        this, tr("Ramisol"),
        tr("Remettre %1 à %2 points d'expérience (%3), sur « %4 » ?")
            .arg(rivalName(rivalId))
            .arg(who.startXp)
            .arg(levelName(levelOf(who.startXp)).toLower(), profileName(currentProfile())),
        QMessageBox::Cancel | QMessageBox::Yes);
    if (answer != QMessageBox::Yes) return;
    resetRival(rivalId);
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
        plaque.button->setMinimumWidth(int(std::ceil(room)) + 30);
        plaque.name->setStyleSheet(
            QStringLiteral("color: %1; background: transparent;").arg(scheme.surfaceText.name()));
        // La ligne du dessous dans l'encre des apartés: elle dit ce que la
        // longueur veut dire, elle n'est pas ce qu'on vient lire.
        plaque.blurb->setStyleSheet(
            QStringLiteral("color: %1; background: transparent;").arg(scheme.hint.name()));

        plaque.button->setStyleSheet(
            QStringLiteral("QPushButton { border-radius: 8px; background: %1;"
                           " border: %2px solid %3; }"
                           "QPushButton:hover { border-color: %4; }")
                .arg(mine ? blendColours(scheme.surface, scheme.accent, 0.18).name()
                          : scheme.surface.name())
                .arg(mine ? 2 : 1)
                .arg(mine ? scheme.accent.name() : scheme.edge.name(), scheme.accent.name()));
    }
}

void TablePicker::reread() {
    dressLengths();

    const Target &target = targetById(m_targetId.isEmpty() ? defaultTargetId() : m_targetId);
    m_lengthNote->setText(tr("Le but s'ajuste à la table: le compte est écrit dans le coin "
                             "de chacune, et le moins de points l'emporte."));
    for (int i = 0; i < m_tiles.size() && i < tableKinds().size(); ++i) {
        const TableKind &kind = tableKinds().at(i);
        const int points = targetPointsFor(target, kind.seats);
        m_tiles.at(i)->setCorner(points > 0 ? tr("%1 pts").arg(points) : tr("1 manche"));
    }
    m_footer->setText(tr("Presser une table lance la partie. %1 : %2.")
                          .arg(target.name, target.blurb.toLower()));

    const QVector<QString> who = invited();
    QStringList names;
    for (const QString &id : who) names.append(rivalName(id));
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
    for (QLabel *note : {m_lengthNote, m_rivalNote, m_footer}) {
        note->setStyleSheet(QStringLiteral("color: %1;").arg(scheme.hint.name()));
    }
    recolourSectionHeadings(this);
    reread();
    update();
}
