#pragma once

#include <QString>

// Effectue le rendu des 53 faces et du dos dessinés, puis les écrit dans un
// fichier .cartes.
//
//     pdcartes-rami --exporter-cartes mon-jeu.cartes
bool exportDrawnDeck(const QString &path, QString *why);
