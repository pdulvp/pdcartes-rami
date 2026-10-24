#include "MainWindow.h"

#include "DialogChrome.h"
#include "HelpDialog.h"
#include "MenuStyle.h"
#include "NameDialog.h"
#include "Palette.h"
#include "ProfileChip.h"
#include "Profiles.h"
#include "Rami.h"
#include "AskDialog.h"
#include "SettingsDialog.h"
#include "TablePicker.h"
#include "TableLook.h"
#include "Tables.h"
#include "TitleFlight.h"
#include "TitlePage.h"

#include <QAction>
#include <QEvent>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QMenu>
#include <QKeyEvent>
#include <QPainter>
#include <QLabel>
#include <QSettings>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>

namespace {

constexpr int kTitlePage = 0;
constexpr int kMenuPage = 1;
constexpr int kTablePage = 2;
constexpr int kTopBarHeight = 50;

// L'icône d'un bouton de la bande: son côté, l'air qu'on lui met à droite
// pour la séparer du mot, et de combien on la redescend. Voir paddedIcon().
constexpr int kIconSide = 18;
constexpr int kIconGap = 6;
constexpr int kIconDrop = 1;

QToolButton *barButton(QWidget *parent, const QString &text, const QStringList &iconNames,
                       const QString &tip) {
    auto *button = new QToolButton(parent);
    button->setText(text);
    button->setToolTip(tip);
    button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    button->setCursor(Qt::PointingHandCursor);
    button->setAutoRaise(true);
    button->setIconSize(QSize(kIconSide + kIconGap, kIconSide + kIconDrop * 2));
    button->setProperty("iconNames", iconNames);
    return button;
}

// L'icône d'un bouton de la bande, rembourrée de vide transparent.
//
// Deux défauts se corrigent là, et pas ailleurs. Qt centre l'icône et le mot
// chacun dans le même rectangle, si bien qu'un rembourrage asymétrique posé
// sur le bouton -- celui qui remonte le texte, trop bas dans une fonte
// japonaise -- remonte l'icône avec lui, alors qu'elle était bien placée. Et
// il ne laisse presque rien entre l'image et le mot. Comme on ne peut viser
// ni l'un ni l'autre séparément dans un bouton qu'il dessine lui-même, c'est
// l'image qui porte les deux corrections: du vide en haut pour la redescendre
// au milieu de son fond, du vide à droite pour l'écarter du mot.
QIcon paddedIcon(const QIcon &icon, int side, int rightGap, int topDrop) {
    if (icon.isNull()) return icon;

    QIcon padded;
    for (const qreal ratio : {1.0, 2.0}) {
        const QSize asked(int(side * ratio), int(side * ratio));
        QPixmap art = icon.pixmap(asked);
        if (art.isNull()) continue;

        QPixmap room(asked.width() + int(rightGap * ratio),
                     asked.height() + int(topDrop * ratio * 2.0));
        room.setDevicePixelRatio(ratio);
        room.fill(Qt::transparent);
        QPainter painter(&room);
        // En haut le vide, en bas rien: l'image se retrouve plus bas que le
        // milieu de ce qu'on rend, donc plus bas dans le bouton.
        painter.drawPixmap(QRect(0, int(topDrop * ratio * 2.0), asked.width(), asked.height()),
                           art);
        painter.end();
        padded.addPixmap(room);
    }
    return padded.isNull() ? icon : padded;
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
    setWindowTitle(tr("PDCartes Rami"));

    readSettings();
    // Le tapis du profil, et ses cartes: tous deux lui appartiennent.
    setCurrentPalette(feltHue());
    applyProfileLook();

    m_titleScreen = new TitlePage(this);
    connect(m_titleScreen, &TitlePage::start, this, &MainWindow::leaveTitle);

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
    m_pages->addWidget(m_titleScreen);
    m_pages->addWidget(m_picker);
    m_pages->addWidget(m_table);

    auto *centre = new QWidget(this);
    auto *rows = new QVBoxLayout(centre);
    rows->setContentsMargins(0, 0, 0, 0);
    rows->setSpacing(0);
    rows->addWidget(buildTopBar());
    rows->addWidget(m_pages, 1);
    setCentralWidget(centre);

    // Le calque du vol: enfant du centre et non de la pile, pour qu'il couvre
    // aussi la bande du haut -- le logo descend de plein cadre jusque sous
    // elle, et serait coupé net au bord de la pile. Hors de toute disposition,
    // sa géométrie est posée à la main dans resizeEvent().
    m_flight = new TitleFlight(centre);
    m_flight->setGeometry(centre->rect());
    connect(m_flight, &TitleFlight::landed, this, &MainWindow::flightLanded);

    applyColours();
    updateTopBar();
    resize(1220, 850);
}

QWidget *MainWindow::buildTopBar() {
    m_topBar = new QWidget(this);
    m_topBar->setObjectName(QStringLiteral("topBar"));
    m_topBar->setFixedHeight(kTopBarHeight);

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
    across->setContentsMargins(20, 0, 10, 0);
    across->setSpacing(6);
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

    connect(m_menuButton, &QToolButton::clicked, this, &MainWindow::askLeaveMatch);
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

void MainWindow::leaveTitle() {
    if (m_pages->currentIndex() != kTitlePage) return;

    // D'où il part, mesuré pendant que l'écran titre est encore à l'écran.
    const QRectF from = m_titleScreen->markRect().translated(
        m_titleScreen->mapTo(m_flight->parentWidget(), QPoint(0, 0)));

    showMenu();

    // Et où il arrive. Mesuré après le changement de page: le menu vient
    // seulement d'être disposé, et la bande du haut de paraître -- avant
    // cela, sa tête n'est pas encore à sa place.
    m_picker->ensurePolished();
    const QRectF to = m_picker->markRect().translated(
        m_picker->mapTo(m_flight->parentWidget(), QPoint(0, 0)));

    // Les deux pages lâchent le leur: c'est le calque qui le porte.
    m_titleScreen->setMarkHidden(true);
    m_picker->setMarkHidden(true);
    // Le menu monte pendant que le logo descend: les deux ensemble font une
    // page qui s'ouvre, là où l'un sans l'autre fait un logo qui passe
    // devant une page déjà là.
    m_picker->beginEntrance();
    m_flight->fly(from, to);
}

void MainWindow::flightLanded() {
    m_picker->setMarkHidden(false);
    m_titleScreen->setMarkHidden(false);
}

// La même perte, et donc la même question, pour les deux façons de quitter
// une partie en cours: revenir au menu, et en relancer une par-dessus. Seul
// le nom du bouton change.
//
// Rien à demander quand la partie est finie: il n'y a plus de manche à
// perdre, et une question dont la réponse est toujours oui est un clic de
// plus.
bool MainWindow::askAbandonMatch(const QString &doIt) {
    if (!m_table->playing()) return true;
    if (m_table->game()->phase() == Phase::MatchOver) return true;
    return askYesNo(this, tr("Quitter la partie"), tr("Abandonner la partie en cours ?"), doIt,
                    tr("Elle sera perdue : une partie quittée ne se reprend pas."));
}

void MainWindow::startSameTableAgain() {
    if (m_settings.tableId.isEmpty()) return;
    if (!askAbandonMatch(tr("Nouvelle partie"))) return;
    startGame(m_settings.tableId);
}

void MainWindow::showMenu() {
    m_picker->setTarget(m_settings.targetId);
    m_picker->reread();
    m_pages->setCurrentIndex(kMenuPage);
    updateTopBar();
}

void MainWindow::askLeaveMatch() {
    // La feuille de marque, elle, a déjà posé la question: elle appelle
    // showMenu() directement. Ici c'est le joueur qui s'en va de lui-même,
    // au milieu d'une donne.
    if (!askAbandonMatch(tr("Revenir au menu"))) return;
    showMenu();
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
    if (!askYesNo(this, tr("Oublier un profil"),
                  tr("Effacer tout ce qui est joué sous « %1 » ?").arg(record.name),
                  tr("Effacer"),
                  tr("Ses parties s'effacent, et les six adversaires reviennent à leur "
                     "niveau de départ sur ce profil."))) {
        return;
    }
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
    // La bande est sombre dans les deux moitiés du jeu de couleurs. Le tapis
    // l'est, et une bande pâle au-dessus d'un drap vert sombre est une bande
    // posée sur une autre application; son filet du bas est d'or, comme tout
    // ce qui borde quelque chose dans ce jeu.
    const QColor barFace = blendColours(scheme.feltEdge, QColor(0, 0, 0), 0.46);
    const QColor barInk = scheme.feltText;
    QColor barRule = scheme.gold;
    barRule.setAlphaF(0.30);
    m_topBar->setStyleSheet(
        QStringLiteral("QWidget#topBar { background: %5; border-bottom: 1px solid %6; }"
                       "QToolButton { border: none; border-radius: 7px;"
                       // Un pixel de moins en haut qu'en bas: Qt centre le
                       // texte sur la boîte de ligne de la fonte, haute dans
                       // une fonte japonaise, et le pose donc trop bas. Il
                       // dessine ces boutons lui-même -- le rembourrage est
                       // la seule prise. Voir `capCentred` dans Fonts.h.
                       " padding: 5px 10px 7px 10px; color: %1;"
                       " background: transparent; }"
                       "QToolButton:hover { background: %2; color: %3; }"
                       "QToolButton:pressed { background: %4; }")
            .arg(barInk.name(), blendColours(barFace, scheme.gold, 0.22).name(),
                 scheme.gold.name(), blendColours(barFace, scheme.gold, 0.34).name(),
                 barFace.name(), barRule.name(QColor::HexArgb)));

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
    bar.setColor(QPalette::Window, barFace);
    bar.setColor(QPalette::WindowText, barInk);
    m_topBar->setPalette(bar);

    QColor barAside = barInk;
    barAside.setAlphaF(0.62);
    inkLabel(m_subtitle, barAside);
    m_profileChip->setOnDark(true);
    m_profileChip->update();
    inkLabel(m_score, scheme.gold);

    m_darkButton->setToolTip(darkMode() ? tr("Couleurs claires") : tr("Couleurs sombres"));
    m_darkButton->setProperty(
        "iconNames", darkMode() ? QStringList{QStringLiteral("weather-clear-symbolic"),
                                              QStringLiteral("display-brightness-symbolic")}
                                : QStringList{QStringLiteral("weather-clear-night-symbolic"),
                                              QStringLiteral("night-light-symbolic")});

    for (QToolButton *button :
         {m_menuButton, m_againButton, m_helpButton, m_settingsButton, m_darkButton}) {
        const QStringList names = button->property("iconNames").toStringList();
        const QIcon icon = paddedIcon(appIcon(names, barInk), kIconSide, kIconGap, kIconDrop);
        button->setIcon(icon);
        if (button == m_darkButton) {
            button->setToolButtonStyle(icon.isNull() ? Qt::ToolButtonTextOnly
                                                     : Qt::ToolButtonIconOnly);
            button->setText(darkMode() ? tr("Clair") : tr("Sombre"));
        }
    }

    m_titleScreen->refresh();
    m_picker->refresh();
    m_table->refresh();
    update();
}

void MainWindow::updateTopBar() {
    const int page = m_pages->currentIndex();
    // L'écran titre se passe de la bande du haut: il n'y a là ni score à
    // lire, ni table à quitter, et le titre est plein cadre.
    m_topBar->setVisible(page != kTitlePage);

    const bool playing = page == kTablePage;
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
    // Le calque est hors disposition: il se recadre lui-même sur ce qu'il
    // couvre. Un vol en cours est abrégé plutôt que suivi -- les deux bouts
    // viennent de bouger sous lui.
    if (m_flight && m_flight->parentWidget()) {
        m_flight->setGeometry(m_flight->parentWidget()->rect());
        m_flight->cancel();
    }
    elideSubtitle();
}

void MainWindow::keyPressEvent(QKeyEvent *event) {
    switch (event->key()) {
    case Qt::Key_Escape:
        if (m_pages->currentIndex() == kTablePage) {
            askLeaveMatch();
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
