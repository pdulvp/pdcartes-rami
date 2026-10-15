#pragma once

#include <QPoint>
#include <QString>
#include <QWidget>

// La carte d'un adversaire: son médaillon, son nom, son niveau, sa jauge, et
// ce qu'il a fait sous ce profil-là.
//
// Elle vit au menu d'ouverture, là où l'on regarde les six avant de choisir
// la table: un clic invite à la table, un clic droit demande le petit menu.
// La carte ne fait que le demander -- c'est la page qui le dresse, parce que
// c'est elle qui sait ce qu'on peut faire d'un adversaire.
class RivalCard : public QWidget {
    Q_OBJECT

public:
    explicit RivalCard(const QString &rivalId, QWidget *parent = nullptr);

    QString rivalId() const { return m_rivalId; }

    void setInvited(bool invited);
    bool invited() const { return m_invited; }

    // Au menu, un clic invite. Ailleurs, la carte ne fait que dire.
    void setPickable(bool pickable);

    // Après que l'expérience a bougé, ou que le profil a changé.
    void reread();

    QSize sizeHint() const override;

Q_SIGNALS:
    void invitedChanged();
    void menuWanted(const QPoint &where);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    QString m_rivalId;
    bool m_invited = false;
    bool m_pickable = false;
    bool m_hovered = false;
};
