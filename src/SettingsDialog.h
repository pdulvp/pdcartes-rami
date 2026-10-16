#pragma once

#include "TablePage.h"

#include <QDialog>

class QButtonGroup;
class QCheckBox;
class QComboBox;
class QLabel;
class QToolButton;
class QListWidget;
class QSlider;

// Les réglages: la longueur de la partie, trois ou quatre commodités pendant
// le jeu, et les cartes avec lesquelles on joue.
//
// Ce qui appartient à un profil ou à un adversaire en particulier n'est plus
// ici mais sur la chose elle-même, au menu d'ouverture: on renomme un profil
// en double cliquant sa plaque, et on remet un adversaire à son niveau de
// départ d'un clic droit sur sa carte. Les réglages ne gardent que ce qui
// vaut pour toute la partie -- ce qui les ramène à une page qui se lit d'un
// coup d'œil.
class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    SettingsDialog(const GameSettings &settings, QWidget *parent = nullptr);

    GameSettings settings() const;

private:
    void dressFelts();
    void fillDecks();
    void fillBacks();
    void describeDeck();

    QButtonGroup *m_paces = nullptr;
    QCheckBox *m_autoSort = nullptr;
    QCheckBox *m_offerHints = nullptr;
    QCheckBox *m_celebrate = nullptr;
    QComboBox *m_deck = nullptr;
    QSlider *m_zoom = nullptr;
    QLabel *m_zoomNote = nullptr;
    QListWidget *m_backs = nullptr;
    QLabel *m_deckNote = nullptr;
    QVector<QToolButton *> m_felts;
    GameSettings m_settings;
};
