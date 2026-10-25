#pragma once

#include <QRectF>
#include <QWidget>

class QVariantAnimation;

// Calque assurant le déplacement animé du logo, de l'écran titre vers le
// menu.
//
// Il n'est actif que durant l'animation, d'une demi-seconde. Placé au-dessus
// de la pile de pages et transparent aux événements de pointeur, il dessine le
// logo à une taille interpolée entre celle qu'il occupait sur l'écran titre et
// celle qu'il occupera en tête du menu ; les deux pages masquent le leur
// pendant ce temps, afin qu'un seul logo soit visible.
//
// Une animation est préférée à une transition sèche parce que le fond est
// identique d'une page à l'autre : seul change ce qui y est posé, et le logo
// est le seul élément commun aux deux pages. Son déplacement visible établit
// la continuité entre elles.
class TitleFlight : public QWidget {
    Q_OBJECT

public:
    explicit TitleFlight(QWidget *parent = nullptr);

    // Déplace le logo de `from` à `to`, exprimés dans les coordonnées du
    // calque. `landed` est appelé à l'arrivée, ainsi qu'en cas d'interruption
    // de l'animation : la page du menu doit reprendre l'affichage de son logo
    // dans tous les cas.
    void fly(const QRectF &from, const QRectF &to);
    // Interrompt proprement une animation en cours, afin qu'un nouvel appel ne
    // laisse pas la précédente inachevée.
    void cancel();

Q_SIGNALS:
    void landed();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QVariantAnimation *m_walk = nullptr;
    QRectF m_from;
    QRectF m_to;
    qreal m_along = 0.0;
};
