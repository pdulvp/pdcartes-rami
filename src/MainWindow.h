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

// Fenêtre principale : un bandeau de boutons en haut, surmontant l'écran
// titre, le menu ou la table.
//
// L'écran titre n'est affiché qu'au démarrage : le premier clic le fait
// disparaître, et quitter une partie ramène au menu. Le bandeau supérieur est
// masqué tant qu'il est affiché, n'ayant alors aucune fonction, et l'écran
// titre occupe toute la fenêtre.
//
// Les deux autres pages sont le menu et la table ; il n'en existe pas de
// troisième. Choisir une table lance une partie, et la table reste affichée
// une fois la partie terminée, afin que son état final reste consultable.
//
// Tous les réglages conservés par la fenêtre — la table, la durée de partie,
// les options de confort, le mode sombre et le profil courant — sont
// enregistrés au moment où ils changent et non à la fermeture, afin qu'une
// session interrompue anormalement conserve ses réglages.
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    // La ligne d'état est retronquée lorsque sa largeur change ; elle est seule
    // à connaître ce moment. Voir `elideSubtitle()`.
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QWidget *buildTopBar();

    void startGame(const QString &tableId);
    // Sortie de l'écran titre : le logo est animé vers sa position en tête du
    // menu.
    void leaveTitle();
    void flightLanded();
    void startSameTableAgain();
    // Demande confirmation avant d'abandonner une partie en cours. Retourne
    // vrai si l'opération peut avoir lieu : partie déjà terminée, ou
    // confirmation du joueur.
    bool askAbandonMatch(const QString &doIt);
    void showMenu();
    // Traite la demande du joueur de quitter la table ; une confirmation est
    // requise, une partie abandonnée ne pouvant être reprise.
    void askLeaveMatch();
    void openSettings();

    // Le profil est accessible depuis le bandeau supérieur : la plaque indique
    // le profil courant et ouvre un menu au clic. Elle est masquée pendant une
    // partie, le profil ne pouvant être changé en cours de jeu, l'expérience
    // des adversaires étant attribuée à celui sous lequel la manche a
    // commencé.
    void showProfileMenu();
    void renameProfile(int slot);
    void askForgetProfile(int slot);
    void profileChanged();

    void openHelp();
    void toggleDark();

    // Applique la palette courante à la fenêtre et à tout son contenu.
    void applyColours();
    void updateTopBar();
    // La ligne d'état est l'élément tronqué lorsque le bandeau manque de
    // place : c'est le seul dont la fin puisse être supprimée sans perte
    // d'information essentielle.
    //
    // La troncature dépend de sa largeur effective, laquelle n'est connue
    // qu'une fois le bandeau redisposé, bien après l'écriture du texte. Elle
    // est donc déclenchée par son propre redimensionnement et non par
    // l'affectation du texte.
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
    // Aucun titre n'est affiché dans le bandeau : le logo du menu remplit
    // cette fonction, et le nom de l'application n'a pas à figurer deux fois
    // dans la même fenêtre.
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
