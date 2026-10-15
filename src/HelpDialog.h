#pragma once

#include <QDialog>

class QTextBrowser;

// Les règles, et comment on joue à la souris.
//
// Un jeu de cartes n'est pas un démineur : on ne devine pas le rami en
// cliquant. Il y a une ouverture à cinquante-et-un points, un joker qui
// s'achète, une suite où l'as vaut un d'un côté et onze de l'autre -- et rien
// de tout cela ne se lit sur le tapis. Alors c'est écrit, en entier, dans la
// fenêtre plutôt que dans un fichier que personne ne trouvera.
class HelpDialog : public QDialog {
    Q_OBJECT

public:
    explicit HelpDialog(QWidget *parent = nullptr);

private:
    QTextBrowser *m_pages = nullptr;
};
