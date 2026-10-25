#pragma once

#include <QDialog>

class QTextBrowser;

// Règles du jeu et commandes à la souris.
//
// Les règles du rami ne se déduisent pas de l'interface : l'ouverture à
// cinquante et un points, le rachat d'un joker, ou la valeur de l'as — un ou
// onze selon sa position dans une suite — ne sont déductibles d'aucun élément
// affiché. Elles sont donc exposées intégralement dans cette fenêtre plutôt
// que dans un fichier distinct.
class HelpDialog : public QDialog {
    Q_OBJECT

public:
    explicit HelpDialog(QWidget *parent = nullptr);

private:
    QTextBrowser *m_pages = nullptr;
};
