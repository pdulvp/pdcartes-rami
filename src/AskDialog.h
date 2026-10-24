#pragma once

#include <QString>

class QWidget;

// Une question à deux réponses, dans la tenue des autres boîtes.
//
// Elle remplace `QMessageBox::question`, qui est la boîte du système: sur une
// application qui dessine les siennes -- bandeau de couleur sur la tranche,
// titre de section souligné, pied à part -- celle du système arrive avec sa
// police, ses marges et son icône, et se voit. Elle était posée cinq fois,
// toujours de la même façon ; c'est maintenant une fonction.
//
// La question se dit en deux temps, comme sur `NameDialog`: `banner` nomme
// l'acte et court sur la tranche, `question` est la phrase courte qu'on lit
// en premier, et `aside` dit ce qu'il en coûte, sous elle et en plus clair.
//
// `doIt` est le texte du bouton qui fait la chose -- `Effacer`,
// `Revenir au menu` -- et non `Oui`: une question lue de travers se rattrape
// sur le bouton, qui est le dernier endroit où elle peut l'être.
bool askYesNo(QWidget *parent, const QString &banner, const QString &question,
              const QString &doIt, const QString &aside = QString());
