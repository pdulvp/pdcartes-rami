#pragma once

#include <QString>
#include <QWidget>

class QPushButton;

// La carte d'un adversaire: son médaillon, son nom, son niveau, sa jauge, et
// ce qu'il a fait sous ce profil-là.
//
// Elle sert aux deux endroits où l'on regarde les six: au menu, où on peut
// les inviter à la table d'un clic, et dans les réglages, où chacun porte un
// bouton pour le remettre au niveau avec lequel il est arrivé. C'est la même
// carte parce que ce qu'il y a à savoir d'un joueur est la même chose dans
// les deux cas.
class RivalCard : public QWidget {
    Q_OBJECT

public:
    explicit RivalCard(const QString &rivalId, QWidget *parent = nullptr);

    QString rivalId() const { return m_rivalId; }

    void setInvited(bool invited);
    bool invited() const { return m_invited; }

    // Au menu, un clic invite. Dans les réglages, la carte ne fait que dire.
    void setPickable(bool pickable);

    // Dans les réglages, le bouton qui le remet à son niveau de départ.
    void showReset(bool show);

    // Après que l'expérience a bougé, ou que le profil a changé.
    void reread();

    QSize sizeHint() const override;

Q_SIGNALS:
    void invitedChanged();
    void wantsReset();

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    void dressReset();

    QString m_rivalId;
    bool m_invited = false;
    bool m_pickable = false;
    bool m_hovered = false;
    QPushButton *m_reset = nullptr;
};
