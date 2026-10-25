#pragma once

#include "Cards.h"

#include <QPointF>
#include <QString>
#include <QVector>
#include <QWidget>

class QTimer;

// Animation de fin de partie victorieuse : les cartes se détachent du tapis
// et rebondissent dans la fenêtre, sur un fond de feu d'artifice.
//
// L'effet s'inspire des animations de fin de partie des jeux de cartes de
// Windows. Les étincelles reprennent les couleurs de la table, afin que
// l'animation reste rattachée visuellement à la partie qui vient de
// s'achever.
class VictoryEffect : public QWidget {
    Q_OBJECT

public:
    explicit VictoryEffect(QWidget *parent = nullptr);

    // `cards` décrit les cartes présentes sur le tapis et la position de
    // chacune à l'écran, de sorte qu'elles semblent s'en détacher plutôt que
    // d'apparaître.
    void start(const QVector<Card> &cards, const QVector<QPointF> &at, qreal height,
               const QString &headline, const QString &aside);
    void stop();

Q_SIGNALS:
    // Émis à la fin de l'animation, ou lorsqu'un clic l'interrompt.
    void finished();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    struct Flyer {
        Card card;
        QPointF at;
        QPointF speed;
        qreal angle = 0.0;
        qreal spin = 0.0;
    };

    struct Spark {
        QPointF at;
        QPointF speed;
        int life = 0;
        int span = 1;
        int shade = 0;
        qreal size = 2.0;
    };

    void tick();
    void burst(const QPointF &at);

    QTimer *m_ticker = nullptr;
    QVector<Flyer> m_flyers;
    QVector<Spark> m_sparks;
    QString m_headline;
    QString m_aside;
    qreal m_cardHeight = 54.0;
    int m_frame = 0;
};
