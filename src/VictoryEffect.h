#pragma once

#include "Cards.h"

#include <QPointF>
#include <QString>
#include <QVector>
#include <QWidget>

class QTimer;

// Ce qui se passe quand la partie est gagnée: les cartes s'envolent du tapis
// et rebondissent dans la fenêtre pendant qu'un feu d'artifice monte derrière
// elles.
//
// Dans l'esprit des jeux de cartes de Windows -- la carte qui tombe en
// cascade à la fin d'une réussite est à peu près tout ce que ce jeu-là avait
// à offrir, et c'était assez. Les étincelles sont tirées des couleurs de la
// table, pour que la fête appartienne encore à la partie qui vient de finir.
class VictoryEffect : public QWidget {
    Q_OBJECT

public:
    explicit VictoryEffect(QWidget *parent = nullptr);

    // `cards` est ce qui était sur le tapis, avec l'endroit où chaque carte se
    // trouvait à l'écran: elles ont l'air de se soulever du tapis plutôt que
    // d'arriver de nulle part.
    void start(const QVector<Card> &cards, const QVector<QPointF> &at, qreal height,
               const QString &headline, const QString &aside);
    void stop();

Q_SIGNALS:
    // Dit quand la fête a fait son temps, ou quand un clic l'a abrégée.
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
