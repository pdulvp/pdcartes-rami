#pragma once

#include "TablePage.h"

#include <QMainWindow>
#include <QString>

class QLabel;
class ProfileChip;
class QStackedWidget;
class QToolButton;
class TablePicker;
class TitleFlight;
class TitlePage;

// La fenêtre : une bande de boutons en haut, et sous elle l'écran titre, le
// menu ou la table.
//
// L'écran titre est le seuil de la séance et non une page où l'on repasse :
// on en sort au premier clic, et quitter une partie ramène au menu. La bande
// du haut s'efface tant qu'il est là — elle n'a rien à y offrir, et un titre
// plein cadre est ce qu'on attend d'un écran titre.
//
// Puis deux pages et pas de quatrième : choisir une table lance une partie, et
// la table reste là quand la partie finit — gagnée ou perdue, un tapis fini
// est une chose qu'on regarde. Tout ce que la fenêtre retient — la table, la
// longueur de la partie, les commodités, la moitié sombre du jeu de couleurs
// et le profil en cours — est écrit au moment où ça change plutôt qu'à la
// fermeture, pour qu'une séance qui finit mal finisse quand même avec ses
// réglages.
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    // La ligne d'état se recoupe quand sa propre largeur change, et c'est
    // elle seule qui sait quand: voir elideSubtitle().
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QWidget *buildTopBar();

    void startGame(const QString &tableId);
    // On quitte l'écran titre: le logo descend se mettre en tête du menu.
    void leaveTitle();
    void flightLanded();
    void startSameTableAgain();
    // La question posée avant de perdre une partie en cours. Rend vrai quand
    // on peut y aller: la partie est finie, ou le joueur a confirmé.
    bool askAbandonMatch(const QString &doIt);
    void showMenu();
    // Le geste du joueur qui quitte la table: on s'assure qu'il le veut,
    // puisque la partie en cours ne se reprend pas.
    void askLeaveMatch();
    void openSettings();

    // Le profil se tient dans la bande du haut: la plaque dit sous quel nom
    // on joue, et l'ouvre d'un clic. Elle disparaît le temps de la partie --
    // on ne change pas de profil en cours de route, l'expérience des six
    // étant comptée sur celui qui a commencé la manche.
    void showProfileMenu();
    void renameProfile(int slot);
    void askForgetProfile(int slot);
    void profileChanged();

    void openHelp();
    void toggleDark();

    // Puts the scheme that is current now on the window and everything in it.
    void applyColours();
    void updateTopBar();
    // La ligne d'état est ce qui cède quand la barre est trop courte: elle est
    // la seule chose là-haut dont on peut couper la fin sans rien perdre.
    //
    // Elle se coupe d'après la largeur qu'elle a, et cette largeur-là n'est
    // connue qu'une fois la bande redisposée -- bien après qu'on a écrit
    // dedans. C'est donc son propre redimensionnement qui déclenche la
    // coupe, et non le moment où le texte arrive.
    void setSubtitle(const QString &text);
    void elideSubtitle();

    void readSettings();
    void writeSettings();

    QStackedWidget *m_pages = nullptr;
    TitlePage *m_titleScreen = nullptr;
    TitleFlight *m_flight = nullptr;
    TablePicker *m_picker = nullptr;
    TablePage *m_table = nullptr;

    QWidget *m_topBar = nullptr;
    // Plus de titre écrit là-haut: le logo du menu le dit mieux, et deux
    // fois le même nom dans la même fenêtre est une fois de trop.
    QLabel *m_subtitle = nullptr;
    ProfileChip *m_profileChip = nullptr;
    QLabel *m_score = nullptr;
    QToolButton *m_menuButton = nullptr;
    QToolButton *m_againButton = nullptr;
    QToolButton *m_settingsButton = nullptr;
    QToolButton *m_helpButton = nullptr;
    QToolButton *m_darkButton = nullptr;

    GameSettings m_settings;
    QString m_subtitleText;
};
