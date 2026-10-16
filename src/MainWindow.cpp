#include "MainWindow.h"

#include "DialogChrome.h"
#include "HelpDialog.h"
#include "MenuStyle.h"
#include "NameDialog.h"
#include "Palette.h"
#include "ProfileChip.h"
#include "Profiles.h"
#include "Rami.h"
#include "SettingsDialog.h"
#include "TablePicker.h"
#include "TableLook.h"
#include "Tables.h"

#include <QAction>
#include <QEvent>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QMenu>
#include <QMessageBox>
#include <QKeyEvent>
#include <QLabel>
#include <QSettings>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>

namespace {

constexpr int kMenuPage = 0;
constexpr int kTablePage = 1;
constexpr int kTopBarHeight = 50;

QToolButton *barButton(QWidget *parent, const QString &text, const QStringList &iconNames,
                       const QString &tip) {
    auto *button = new QToolButton(parent);
    button->setText(text);
    button->setToolTip(tip);
    button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    button->setCursor(Qt::PointingHandCursor);
    button->setAutoRaise(true);
    button->setIconSize(QSize(18, 18));
    button->setProperty("iconNames", iconNames);
    return button;
}

// What colour a label in the strip is written in.
//
// Said with a style sheet of the label's own rather than with a palette,
// which is the obvious way and does not work: the strip carries a style
// sheet, and a widget under one is repolished whenever anything near it
// changes -- each repolish handing it back the palette it had when the sheet
// went on. A rule on the widget itself outranks all of that.
void inkLabel(QLabel *label, const QColor &colour) {
    const QString rule = QStringLiteral("color: %1;").arg(colour.name());
    if (label->styleSheet() == rule) return;
    label->setStyleSheet(rule);
}

} // namespace

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setWindowTitle(tr("Ramisol"));

    readSettings();
    // Le tapis du profil, et ses cartes: tous deux lui appartiennent.
    setCurrentPalette(feltHue());
    applyProfileLook();

    m_picker = new TablePicker(this);
    m_picker->setTarget(m_settings.targetId);
    connect(m_picker, &TablePicker::tableChosen, this, &MainWindow::startGame);
    connect(m_picker, &TablePicker::targetChosen, this, [this](const QString &id) {
        m_settings.targetId = id;
        writeSettings();
    });
    // Un profil renommé depuis le menu: la bande du haut porte son nom.

    m_table = new TablePage(this);
    connect(m_table, &TablePage::stateChanged, this, &MainWindow::updateTopBar);
    connect(m_table, &TablePage::wantsMenu, this, &MainWindow::showMenu);
    // La molette sur le tapis change la taille des cartes: écrite tout de
    // suite, comme tout le reste de ce que la fenêtre retient.
    connect(m_table, &TablePage::zoomChanged, this, [this](int percent) {
        m_settings.zoom = percent;
        writeSettings();
    });

    m_pages = new QStackedWidget(this);
    m_pages->addWidget(m_picker);
    m_pages->addWidget(m_table);

    auto *centre = new QWidget(this);
    auto *rows = new QVBoxLayout(centre);
    rows->setContentsMargins(0, 0, 0, 0);
    rows->setSpacing(0);
    rows->addWidget(buildTopBar());
    rows->addWidget(m_pages, 1);
    setCentralWidget(centre);

    applyColours();
    updateTopBar();
    resize(1220, 850);
}

QWidget *MainWindow::buildTopBar() {
    m_topBar = new QWidget(this);
    m_topBar->setObjectName(QStringLiteral("topBar"));
    m_topBar->setFixedHeight(kTopBarHeight);

    m_title = new QLabel(tr("Ramisol"), m_topBar);
    QFont name = m_title->font();
    name.setBold(true);
    if (name.pointSizeF() > 0) name.setPointSizeF(name.pointSizeF() * 1.22);
    name.setLetterSpacing(QFont::PercentageSpacing, 106);
    m_title->setFont(name);

    m_subtitle = new QLabel(m_topBar);
    // Elle absorbe tout l'espace qui reste et rend ce qu'il faut quand il n'y
    // en a plus : sans cela, c'est le score à droite qui se fait couper, et un
    // score coupé se lit comme un autre score -- « 106 pts » devenait
    // « 06 pts ».
    m_subtitle->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    // Elle se recoupe elle-même quand la bande lui donne une autre largeur.
    m_subtitle->installEventFilter(this);
    m_profileChip = new ProfileChip(m_topBar);
    connect(m_profileChip, &ProfileChip::clicked, this, &MainWindow::showProfileMenu);

    m_score = new QLabel(m_topBar);
    QFont counter = m_score->font();
    counter.setBold(true);
    if (counter.pointSizeF() > 0) counter.setPointSizeF(counter.pointSizeF() * 1.16);
    counter.setStyleHint(QFont::Monospace);
    m_score->setFont(counter);
    m_score->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    m_againButton = barButton(m_topBar, tr("Nouvelle partie"),
                              {QStringLiteral("view-refresh-symbolic"),
                               QStringLiteral("view-refresh")},
                              tr("Une autre partie à la même table (F5)"));
    m_menuButton = barButton(m_topBar, tr("Menu"),
                             {QStringLiteral("go-home-symbolic"),
                              QStringLiteral("user-home-symbolic"),
                              QStringLiteral("go-previous-symbolic")},
                             tr("Revenir au choix de la table"));
    m_helpButton = barButton(m_topBar, tr("Règles"),
                             {QStringLiteral("help-contents-symbolic"),
                              QStringLiteral("help-about-symbolic"),
                              QStringLiteral("help-browser")},
                             tr("Les règles du rami, et comment on joue à la souris"));
    m_settingsButton = barButton(m_topBar, tr("Réglages"),
                                 {QStringLiteral("emblem-system-symbolic"),
                                  QStringLiteral("preferences-system-symbolic")},
                                 tr("Longueur de la partie, commodités, et le niveau des six"));
    m_darkButton = barButton(m_topBar, QString(), {}, QString());

    auto *across = new QHBoxLayout(m_topBar);
    across->setContentsMargins(16, 0, 10, 0);
    across->setSpacing(6);
    across->addWidget(m_title);
    across->addSpacing(12);
    across->addWidget(m_subtitle, 1);
    across->addWidget(m_profileChip);
    across->addSpacing(14);
    across->addWidget(m_score);
    across->addSpacing(16);
    across->addWidget(m_againButton);
    across->addWidget(m_menuButton);
    across->addSpacing(10);
    across->addWidget(m_helpButton);
    across->addWidget(m_settingsButton);
    across->addWidget(m_darkButton);

    connect(m_menuButton, &QToolButton::clicked, this, &MainWindow::showMenu);
    connect(m_againButton, &QToolButton::clicked, this, &MainWindow::startSameTableAgain);
    connect(m_helpButton, &QToolButton::clicked, this, &MainWindow::openHelp);
    connect(m_settingsButton, &QToolButton::clicked, this, &MainWindow::openSettings);
    connect(m_darkButton, &QToolButton::clicked, this, &MainWindow::toggleDark);

    return m_topBar;
}

void MainWindow::startGame(const QString &tableId) {
    const TableKind &kind = tableKindById(tableId);
    m_settings.tableId = kind.id;
    m_settings.invited = m_picker->invited();

    m_table->startMatch(m_settings);
    m_pages->setCurrentIndex(kTablePage);
    m_table->setFocus();
    updateTopBar();
    writeSettings();
}

void MainWindow::startSameTableAgain() {
    if (m_settings.tableId.isEmpty()) return;
    startGame(m_settings.tableId);
}

void MainWindow::showMenu() {
    m_picker->setTarget(m_settings.targetId);
    m_picker->reread();
    m_pages->setCurrentIndex(kMenuPage);
    updateTopBar();
}

void MainWindow::openSettings() {
    SettingsDialog dialog(m_settings, this);
    const bool kept = dialog.exec() == QDialog::Accepted;
    // La teinte s'essaie en direct dans la boîte: acceptée elle reste, et
    // annulée c'est celle du profil qui revient. Dans les deux cas la fenêtre
    // se repeint, puisque dans les deux cas elle a pu changer en chemin.
    setCurrentPalette(kept ? dialog.settings().hue : feltHue());
    applyColours();
    if (!kept) return;

    m_settings = dialog.settings();
    setFeltHue(m_settings.hue);
    writeSettings();
    m_table->applySettings(m_settings);
    m_picker->setTarget(m_settings.targetId);
    m_picker->reread();
    updateTopBar();
}

void MainWindow::showProfileMenu() {
    QMenu menu(this);
    dressMenu(&menu);

    // Rien à éteindre pour cause de partie en cours: la plaque qui ouvre ce
    // menu n'est à l'écran qu'au menu d'ouverture.
    const int here = currentProfile();
    QVector<QAction *> switches;
    for (int slot = 1; slot <= kProfileCount; ++slot) {
        const ProfileRecord record = profileRecord(slot);
        QString line = record.name;
        if (record.matchesPlayed > 0) {
            line += tr("  —  %1 parties, %2 gagnées")
                        .arg(record.matchesPlayed)
                        .arg(record.matchesWon);
        } else {
            line += tr("  —  aucune partie encore");
        }
        QAction *pick = menu.addAction(line);
        pick->setCheckable(true);
        pick->setChecked(slot == here);
        pick->setEnabled(slot != here);
        switches.append(pick);
    }

    menu.addSeparator();
    QAction *rename = menu.addAction(tr("Renommer…"));
    QAction *forget = menu.addAction(tr("Tout oublier sur ce profil"));

    // Sous la plaque et alignée sur son bord gauche, comme un menu déroulant.
    const QAction *chosen = menu.exec(m_profileChip->mapToGlobal(
        QPoint(0, m_profileChip->height() + 4)));
    if (!chosen) return;

    const int picked = switches.indexOf(const_cast<QAction *>(chosen));
    if (picked >= 0) {
        setCurrentProfile(picked + 1);
        // Le tapis, le jeu et le dos appartiennent au profil: ils changent
        // avec lui, sans quoi on jouerait la partie suivante avec le goût du
        // précédent.
        setCurrentPalette(feltHue());
        applyProfileLook();
        applyColours();
        profileChanged();
        m_picker->reread();
        return;
    }
    if (chosen == rename) {
        renameProfile(here);
    } else if (chosen == forget) {
        askForgetProfile(here);
    }
}

void MainWindow::renameProfile(int slot) {
    NameDialog dialog(tr("Renommer"), tr("Le nom de ce profil :"),
                      tr("Chaque profil garde ses parties et sa propre expérience des six "
                         "adversaires."),
                      profileName(slot), kProfileNameLength, this);
    if (dialog.exec() != QDialog::Accepted) return;
    // Un nom vide rendrait la plaque muette: on garde alors l'ancien.
    if (dialog.name().isEmpty()) return;
    setProfileName(slot, dialog.name());
    profileChanged();
}

void MainWindow::askForgetProfile(int slot) {
    const ProfileRecord record = profileRecord(slot);
    const auto answer = QMessageBox::question(
        this, tr("Ramisol"),
        tr("Effacer les parties de « %1 » et remettre les six adversaires à leur "
           "niveau de départ ?")
            .arg(record.name),
        QMessageBox::Cancel | QMessageBox::Yes);
    if (answer != QMessageBox::Yes) return;
    forgetProfile(slot);
    profileChanged();
    m_picker->reread();
}

void MainWindow::profileChanged() {
    m_profileChip->reread();
    updateTopBar();
}

void MainWindow::openHelp() {
    HelpDialog dialog(this);
    dialog.exec();
}

void MainWindow::toggleDark() {
    setDarkMode(!darkMode());
    applyColours();
    writeSettings();
}

void MainWindow::applyColours() {
    const Palette &scheme = currentPalette();

    // La feuille de style de la bande d'abord, avant qu'une seule palette
    // soit distribuée. En poser une fait dépolir tout le sous-arbre par le
    // style, et rend à chaque widget la palette qu'il avait au premier
    // polissage -- donc une palette posée avant cet appel est une palette
    // jetée.
    m_topBar->setStyleSheet(
        QStringLiteral("QWidget#topBar { background: %5; border-bottom: 1px solid %6; }"
                       "QToolButton { border: none; border-radius: 7px;"
                       " padding: 6px 10px; color: %1; background: transparent; }"
                       "QToolButton:hover { background: %2; color: %3; }"
                       "QToolButton:pressed { background: %4; }")
            .arg(scheme.faceText.name(), blendColours(scheme.face, scheme.accent, 0.22).name(),
                 scheme.faceText.name(),
                 blendColours(scheme.face, scheme.accent, 0.36).name(), scheme.face.name(),
                 scheme.edge.name()));

    QPalette window = palette();
    window.setColor(QPalette::Window, scheme.desk);
    window.setColor(QPalette::WindowText, scheme.surfaceText);
    window.setColor(QPalette::Base, scheme.field);
    window.setColor(QPalette::Text, scheme.surfaceText);
    window.setColor(QPalette::Button, scheme.face);
    window.setColor(QPalette::ButtonText, scheme.faceText);
    window.setColor(QPalette::Highlight, scheme.accent);
    window.setColor(QPalette::HighlightedText, scheme.accentText);
    setPalette(window);

    QPalette bar = m_topBar->palette();
    bar.setColor(QPalette::Window, scheme.face);
    bar.setColor(QPalette::WindowText, scheme.faceText);
    m_topBar->setPalette(bar);

    inkLabel(m_title, scheme.accent);
    inkLabel(m_subtitle, scheme.hint);
    m_profileChip->update();
    inkLabel(m_score, scheme.accent);

    m_darkButton->setToolTip(darkMode() ? tr("Couleurs claires") : tr("Couleurs sombres"));
    m_darkButton->setProperty(
        "iconNames", darkMode() ? QStringList{QStringLiteral("weather-clear-symbolic"),
                                              QStringLiteral("display-brightness-symbolic")}
                                : QStringList{QStringLiteral("weather-clear-night-symbolic"),
                                              QStringLiteral("night-light-symbolic")});

    for (QToolButton *button :
         {m_menuButton, m_againButton, m_helpButton, m_settingsButton, m_darkButton}) {
        const QStringList names = button->property("iconNames").toStringList();
        const QIcon icon = tintedIcon(names, scheme.faceText);
        button->setIcon(icon);
        if (button == m_darkButton) {
            button->setToolButtonStyle(icon.isNull() ? Qt::ToolButtonTextOnly
                                                     : Qt::ToolButtonIconOnly);
            button->setText(darkMode() ? tr("Clair") : tr("Sombre"));
        }
    }

    m_picker->refresh();
    m_table->refresh();
    update();
}

void MainWindow::updateTopBar() {
    const bool playing = m_pages->currentIndex() == kTablePage;
    m_menuButton->setVisible(playing);
    m_againButton->setVisible(playing);
    m_score->setVisible(playing);
    // La plaque du profil s'efface le temps de la partie: on n'en change pas
    // en cours de route, et la place est mieux employée par le score.
    m_profileChip->setVisible(!playing);

    m_profileChip->reread();

    if (!playing) {
        setSubtitle(tr("Le rami, sur tapis vert"));
        return;
    }

    setSubtitle(m_table->subtitle());

    const Rami *game = m_table->game();
    if (!game || game->seatCount() < 2) {
        m_score->setText(QString());
        return;
    }
    int me = -1;
    int best = -1;
    for (int at = 0; at < game->seatCount(); ++at) {
        if (game->seat(at).human()) me = at;
        if (best < 0 || game->seat(at).score < game->seat(best).score) best = at;
    }
    if (me < 0) {
        m_score->setText(QString());
        return;
    }
    // Votre score, et si vous menez. C'est une pénalité : le moins possible.
    const int mine = game->seat(me).score;
    if (me == best) {
        m_score->setText(tr("%1 pts — vous menez").arg(mine));
    } else {
        m_score->setText(tr("%1 pts — %2 mène").arg(mine).arg(game->seat(best).name));
    }
}

void MainWindow::setSubtitle(const QString &text) {
    m_subtitleText = text;
    elideSubtitle();
}

void MainWindow::elideSubtitle() {
    if (!m_subtitle) return;
    const QFontMetrics metrics(m_subtitle->font());
    const int room = std::max(60, m_subtitle->width());
    const QString cut = metrics.elidedText(m_subtitleText, Qt::ElideRight, room);
    // Rien à faire si la coupe ne change pas -- et surtout, ne pas réécrire
    // pour rien: réécrire relance la mise en place, qui nous rappellerait.
    if (cut == m_subtitle->text()) return;
    m_subtitle->setText(cut);
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event) {
    // La ligne d'état est écrite avant que la bande soit redisposée: entrer
    // en partie fait paraître trois boutons et disparaître la plaque du
    // profil, et la ligne était coupée d'après la largeur qu'elle avait
    // avant. Elle le restait jusqu'au premier coup joué, qui repassait par
    // updateTopBar() avec une bande enfin en place.
    if (watched == m_subtitle && event->type() == QEvent::Resize) elideSubtitle();
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::resizeEvent(QResizeEvent *event) {
    QMainWindow::resizeEvent(event);
    elideSubtitle();
}

void MainWindow::keyPressEvent(QKeyEvent *event) {
    switch (event->key()) {
    case Qt::Key_Escape:
        if (m_pages->currentIndex() == kTablePage) {
            showMenu();
            return;
        }
        break;
    case Qt::Key_F5:
    case Qt::Key_F2:
        if (m_pages->currentIndex() == kTablePage) {
            startSameTableAgain();
            return;
        }
        break;
    case Qt::Key_F1:
        openHelp();
        return;
    default:
        break;
    }
    QMainWindow::keyPressEvent(event);
}

void MainWindow::readSettings() {
    QSettings settings;
    m_settings.tableId = settings.value(QStringLiteral("table/taille"), defaultTableId()).toString();
    // A table from a settings file that is not one of the tables on offer --
    // written by a later version, or edited by hand -- is not sat at.
    if (tableKindById(m_settings.tableId).id != m_settings.tableId) {
        m_settings.tableId = defaultTableId();
    }
    m_settings.targetId =
        settings.value(QStringLiteral("table/but"), defaultTargetId()).toString();
    if (targetById(m_settings.targetId).id != m_settings.targetId) {
        m_settings.targetId = defaultTargetId();
    }
    m_settings.autoSort = settings.value(QStringLiteral("aide/ranger"), true).toBool();
    m_settings.offerHints = settings.value(QStringLiteral("aide/conseil"), true).toBool();
    m_settings.celebrate = settings.value(QStringLiteral("aide/feu"), true).toBool();
    m_settings.pace = qBound(0, settings.value(QStringLiteral("aide/rythme"), 1).toInt(), 2);
    m_settings.zoom = qBound(80, settings.value(QStringLiteral("aide/zoom"), 100).toInt(), 200);
    setDarkMode(settings.value(QStringLiteral("look/sombre"), false).toBool());
    // La teinte du drap n'est pas rangée là: elle appartient au profil, et
    // c'est TableLook qui la garde sous lui.
    m_settings.hue = feltHue();
}

void MainWindow::writeSettings() {
    QSettings settings;
    settings.setValue(QStringLiteral("table/taille"), m_settings.tableId);
    settings.setValue(QStringLiteral("table/but"), m_settings.targetId);
    settings.setValue(QStringLiteral("aide/ranger"), m_settings.autoSort);
    settings.setValue(QStringLiteral("aide/conseil"), m_settings.offerHints);
    settings.setValue(QStringLiteral("aide/feu"), m_settings.celebrate);
    settings.setValue(QStringLiteral("aide/rythme"), m_settings.pace);
    settings.setValue(QStringLiteral("aide/zoom"), m_settings.zoom);
    settings.setValue(QStringLiteral("look/sombre"), darkMode());
}
