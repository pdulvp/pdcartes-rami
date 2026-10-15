#pragma once

#include "TablePage.h"

#include <QDialog>
#include <QVector>

class QButtonGroup;
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class RivalCard;

// Les réglages: la longueur de la partie, trois ou quatre commodités, le nom
// du profil -- et les six adversaires, chacun avec le bouton qui le remet au
// niveau avec lequel il est arrivé.
//
// Ce dernier point est la raison d'être de la moitié de cette boîte. Une
// expérience qui ne monte jamais que dans un sens finit par rendre la table
// injouable, et un joueur qui a rendu Oriane imbattable doit pouvoir la
// remettre où elle était sans toucher aux cinq autres.
class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    SettingsDialog(const GameSettings &settings, QWidget *parent = nullptr);

    GameSettings settings() const;

private:
    void describeTargets();
    void fillDecks();
    void fillBacks();
    void describeDeck();

    QVector<RivalCard *> m_cards;
    QButtonGroup *m_targets = nullptr;
    QButtonGroup *m_paces = nullptr;
    QCheckBox *m_autoSort = nullptr;
    QCheckBox *m_offerHints = nullptr;
    QCheckBox *m_celebrate = nullptr;
    QLineEdit *m_profileName = nullptr;
    QLabel *m_targetNote = nullptr;
    QComboBox *m_deck = nullptr;
    QComboBox *m_back = nullptr;
    QLabel *m_deckNote = nullptr;
    GameSettings m_settings;
};
