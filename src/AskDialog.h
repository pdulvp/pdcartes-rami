#pragma once

#include <QString>

class QWidget;

// Boîte de confirmation à deux réponses, présentée comme les autres boîtes de
// l'application.
//
// Elle remplace `QMessageBox::question`, qui est la boîte du système : dans
// une application dessinant ses propres boîtes — bandeau de couleur sur la
// tranche, titre de section souligné, pied séparé — celle du système se
// distingue par sa police, ses marges et son icône. Ce motif étant employé à
// cinq reprises de façon identique, il est ici factorisé en une fonction.
//
// La question est formulée en deux parties, comme dans `NameDialog` :
// `banner` nomme l'action et s'affiche sur la tranche, `question` est la
// phrase courte lue en premier, et `aside` en précise les conséquences, en
// dessous et dans une teinte plus claire.
//
// `doIt` est le libellé du bouton qui exécute l'action — « Effacer »,
// « Revenir au menu » — et non « Oui » : une question mal lue peut encore être
// rattrapée par le libellé du bouton, dernier point de contrôle.
bool askYesNo(QWidget *parent, const QString &banner, const QString &question,
              const QString &doIt, const QString &aside = QString());
