#pragma once

#include <QDialog>
#include <QString>

class QLineEdit;

// Donner un nom à quelque chose: un profil, un adversaire.
//
// Une boîte à un seul champ, mais habillée comme les autres -- la bande de
// couleur sur la tranche gauche, le corps sur la surface pâle, les boutons
// dans leur bandeau. QInputDialog faisait l'affaire et ne ressemblait à rien
// du reste: une boîte système au milieu d'un jeu qui a sa propre table.
//
// Large plutôt que haute: on y écrit un nom, et un champ étroit donne
// l'impression qu'on n'a droit qu'à un mot.
class NameDialog : public QDialog {
    Q_OBJECT

public:
    // `banner` monte le long de la tranche, `question` est la ligne au-dessus
    // du champ, et `aside` la phrase en dessous qui dit ce qu'un nom vide
    // fait -- vide, il n'y a pas de phrase.
    NameDialog(const QString &banner, const QString &question, const QString &aside,
               const QString &value, int mostLetters, QWidget *parent = nullptr);

    // Ce qui a été écrit, débarrassé de ses blancs.
    QString name() const;

private:
    QLineEdit *m_field = nullptr;
};
