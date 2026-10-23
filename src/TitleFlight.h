#pragma once

#include <QRectF>
#include <QWidget>

class QVariantAnimation;

// Le calque qui fait voler le logo de l'écran titre au menu.
//
// Il ne sert qu'une demi-seconde, et il ne sert qu'à ça. Posé par-dessus la
// pile des pages, transparent au pointeur, il dessine le logo à une taille
// prise entre celle qu'il avait sur l'écran titre et celle qu'il aura en tête
// du menu — les deux pages cachant le leur pendant ce temps-là, pour qu'il n'y
// en ait jamais deux.
//
// Un vol plutôt qu'une coupe franche, parce que le fond ne change pas d'une
// page à l'autre: c'est le même drap. Ce qui bouge, c'est ce qui est posé
// dessus, et le logo est la seule chose que les deux pages ont en commun —
// le voir descendre se mettre dans son coin dit que c'est la même maison.
class TitleFlight : public QWidget {
    Q_OBJECT

public:
    explicit TitleFlight(QWidget *parent = nullptr);

    // Part de `from`, arrive à `to`, toutes deux dans les coordonnées du
    // calque. `landed` est appelé à l'arrivée, et aussi quand le vol est
    // abrégé: la page du menu doit reprendre son logo dans tous les cas.
    void fly(const QRectF &from, const QRectF &to);
    // Rien à moitié fait si on repasse par là entre-temps.
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
