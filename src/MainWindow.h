#pragma once

#include "TablePage.h"

#include <QMainWindow>
#include <QString>

class QLabel;
class QStackedWidget;
class QToolButton;
class TablePicker;

// La fenêtre : une bande de boutons en haut, et sous elle le menu ou la table.
//
// Deux pages et pas de troisième : choisir une table lance une partie, et la
// table reste là quand la partie finit — gagnée ou perdue, un tapis fini est
// une chose qu'on regarde. Tout ce que la fenêtre retient — la table, la
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

private:
    QWidget *buildTopBar();

    void startGame(const QString &tableId);
    void startSameTableAgain();
    void showMenu();
    void openSettings();
    void openHelp();
    void toggleDark();

    // Puts the scheme that is current now on the window and everything in it.
    void applyColours();
    void updateTopBar();
    // La ligne d'état est ce qui cède quand la barre est trop courte: elle est
    // la seule chose là-haut dont on peut couper la fin sans rien perdre.
    void setSubtitle(const QString &text);
    void elideSubtitle();

    void readSettings();
    void writeSettings();

    QStackedWidget *m_pages = nullptr;
    TablePicker *m_picker = nullptr;
    TablePage *m_table = nullptr;

    QWidget *m_topBar = nullptr;
    QLabel *m_title = nullptr;
    QLabel *m_subtitle = nullptr;
    QLabel *m_profile = nullptr;
    QLabel *m_score = nullptr;
    QToolButton *m_menuButton = nullptr;
    QToolButton *m_againButton = nullptr;
    QToolButton *m_settingsButton = nullptr;
    QToolButton *m_helpButton = nullptr;
    QToolButton *m_darkButton = nullptr;

    GameSettings m_settings;
    QString m_subtitleText;
};
