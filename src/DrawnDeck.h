#pragma once

#include <QString>

// Rend les 53 faces et le dos dessinés, et les écrit dans un fichier .cartes.
//
//     ramisol --exporter-cartes mon-jeu.cartes
bool exportDrawnDeck(const QString &path, QString *why);
