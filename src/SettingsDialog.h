#pragma once

#include "TablePage.h"

#include <QDialog>
#include <QVector>

class CardStrip;
class QButtonGroup;
class QCheckBox;
class QComboBox;
class QHBoxLayout;
class QLabel;
class QListWidget;
class QSlider;
class QStackedWidget;
class QToolButton;
class QTreeWidget;
class QVBoxLayout;

// Boîte de réglages : les options de confort en cours de partie, les cartes,
// les dos, le tapis et les paramètres d'accessibilité.
//
// Les réglages sont répartis en cinq pages plutôt qu'en une colonne unique.
// L'ensemble tenait auparavant dans une page de huit cents pixels de haut, ce
// qui posait deux problèmes : la page n'était pas visible en entier sur un
// écran d'ordinateur portable, et la recherche d'un réglage imposait de faire
// défiler. Les groupes existaient déjà, avec leur titre et leur filet ; ils
// sont devenus des pages, la liste de gauche servant de sommaire.
//
// Les cartes occupent deux pages : les figures d'une part, les dos de l'autre.
// Les deux bandes d'images superposées formaient à elles seules une page plus
// haute que la boîte, et elles répondent de toute façon à deux questions
// distinctes : l'apparence des cartes du joueur et celle des cartes vues de
// face. Chacune des deux pages dispose donc de sa propre liste de jeux, le dos
// n'étant pas nécessairement issu du jeu des faces.
//
// Les paramètres propres à un profil ou à un adversaire ne figurent pas ici
// mais sur l'élément concerné, dans le menu d'ouverture : un profil se renomme
// par un double-clic sur sa plaque, et un adversaire se réinitialise par un
// clic droit sur sa carte. Cette boîte ne contient que les paramètres valables
// pour l'ensemble de la partie.
class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    SettingsDialog(const GameSettings &settings, QWidget *parent = nullptr);

    GameSettings settings() const;

private:
    // Crée une page et son entrée dans la liste de gauche. Retourne la
    // disposition verticale à remplir, le titre et le ressort inférieur étant
    // déjà en place.
    QVBoxLayout *addPage(const QString &name, const QString &heading);

    void buildDuringPage();
    void buildCardsPage();
    void buildBacksPage();
    void buildFeltPage();
    void buildAccessPage();

    // Ajoute les deux boutons « ouvrir le dossier » et « relire » en fin d'une
    // rangée portant une liste de jeux. Les deux pages concernées l'appellent.
    void addDeckFolderButtons(QWidget *page, QHBoxLayout *row);

    // Lien vers l'éditeur, au bas de ces deux mêmes pages. Les boutons
    // ci-dessus donnent accès au dossier où l'on installe un jeu ; ce lien
    // renvoie à l'outil qui permet d'en créer un.
    void addEditorLink(QWidget *page, QVBoxLayout *rows);

    void dressFelts();
    // Réécrit la ligne de lien des deux pages avec la couleur courante. La
    // couleur de l'ancre est portée par le texte lui-même, la feuille de
    // style de la fenêtre ne s'appliquant pas au texte enrichi.
    void dressEditorLinks();
    // Les deux listes portent leur propre feuille de style, composée avec les
    // couleurs courantes. La teinte étant modifiable en direct, cette feuille
    // doit pouvoir être réécrite.
    void dressLists();
    void fillDecks();
    // Liste des jeux dont le dos peut être extrait : les mêmes que pour les
    // faces, précédés de l'entrée « celui du jeu de cartes », qui est le cas
    // ordinaire.
    void fillBackDecks();
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
    QComboBox *m_backDeck = nullptr;
    QListWidget *m_backs = nullptr;
    // Figures du jeu sélectionné, présentées sous forme de bande non
    // interactive : la sélection porte sur le jeu entier et non sur une carte
    // particulière.
    CardStrip *m_faces = nullptr;
    QLabel *m_deckNote = nullptr;
    // Les deux étiquettes affichant l'adresse de l'éditeur, conservées afin
    // de pouvoir les recolorer lorsque la teinte du tapis change.
    QVector<QLabel *> m_editorLinks;
    QVector<QToolButton *> m_felts;
    GameSettings m_settings;
};
