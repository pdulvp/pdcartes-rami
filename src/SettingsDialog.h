#pragma once

#include "TablePage.h"

#include <QDialog>
#include <QVector>

class CardStrip;
class QButtonGroup;
class QCheckBox;
class QComboBox;
class QLabel;
class QListWidget;
class QSlider;
class QStackedWidget;
class QToolButton;
class QTreeWidget;
class QVBoxLayout;

// Les réglages: les commodités pendant le jeu, les cartes, les dos, et le
// tapis.
//
// Quatre pages plutôt qu'une seule colonne qui descend. Tout tenait dans une
// seule page haute de huit cents pixels, ce qui était deux choses à la fois:
// une page qu'un écran d'ordinateur portable ne montre pas en entier, et une
// page où l'on cherche un réglage en faisant glisser. Les groupes existaient
// déjà -- ils avaient leur titre et leur filet -- ils sont simplement devenus
// des pages, et la liste de gauche dit lesquelles il y a.
//
// Les cartes en font deux: les figures d'un côté, les dos de l'autre. Les deux
// bandes d'images l'une sous l'autre faisaient à elles seules une page plus
// haute que la boîte, et ce sont de toute façon deux questions -- à quoi
// ressemblent les cartes qu'on tient, et à quoi ressemblent celles qu'on voit
// en face.
//
// Ce qui appartient à un profil ou à un adversaire en particulier n'est pas
// ici mais sur la chose elle-même, au menu d'ouverture: on renomme un profil
// en double cliquant sa plaque, et on remet un adversaire à son niveau de
// départ d'un clic droit sur sa carte. Les réglages ne gardent que ce qui vaut
// pour toute la partie.
class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    SettingsDialog(const GameSettings &settings, QWidget *parent = nullptr);

    GameSettings settings() const;

private:
    // Une page, et sa ligne dans la liste de gauche. Rend le layout vertical à
    // remplir: le titre et le ressort du bas sont déjà posés.
    QVBoxLayout *addPage(const QString &name, const QString &heading);

    void buildDuringPage();
    void buildCardsPage();
    void buildBacksPage();
    void buildFeltPage();

    void dressFelts();
    // Les deux listes portent leur propre feuille de style, qui est écrite
    // avec les couleurs du moment: la teinte s'essayant en direct, il faut
    // pouvoir la réécrire.
    void dressLists();
    void fillDecks();
    void fillBacks();
    void describeDeck();

    QTreeWidget *m_sections = nullptr;
    QStackedWidget *m_pages = nullptr;

    QButtonGroup *m_paces = nullptr;
    QCheckBox *m_autoSort = nullptr;
    QCheckBox *m_offerHints = nullptr;
    QCheckBox *m_celebrate = nullptr;
    QComboBox *m_deck = nullptr;
    QSlider *m_zoom = nullptr;
    QLabel *m_zoomNote = nullptr;
    QListWidget *m_backs = nullptr;
    // Les figures du jeu choisi: une bande qu'on regarde, et qui n'est pas une
    // liste -- on ne choisit pas une carte, on choisit le jeu entier.
    CardStrip *m_faces = nullptr;
    QLabel *m_deckNote = nullptr;
    QVector<QToolButton *> m_felts;
    GameSettings m_settings;
};
