#pragma once

#include <QDialog>
#include <QString>

class QLineEdit;

// Boîte de saisie d'un nom : profil ou adversaire.
//
// Elle ne comporte qu'un seul champ mais reprend la présentation des autres
// boîtes : bandeau de couleur sur la tranche gauche, corps sur fond clair,
// boutons dans leur pied. `QInputDialog` remplissait la même fonction mais ne
// s'accordait pas avec le reste de l'application.
//
// Elle est large plutôt que haute : un champ étroit suggérerait qu'un seul mot
// est attendu.
class NameDialog : public QDialog {
    Q_OBJECT

public:
    // `banner` s'affiche verticalement sur la tranche, `question` est la ligne
    // placée au-dessus du champ, et `aside` la phrase située en dessous,
    // précisant l'effet d'un nom vide ; si `aside` est vide, aucune phrase
    // n'est affichée.
    NameDialog(const QString &banner, const QString &question, const QString &aside,
               const QString &value, int mostLetters, QWidget *parent = nullptr);

    // Texte saisi, débarrassé de ses espaces de début et de fin.
    QString name() const;

private:
    QLineEdit *m_field = nullptr;
};
