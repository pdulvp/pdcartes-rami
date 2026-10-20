#pragma once

#include <QPoint>
#include <QString>
#include <QWidget>

class QTimer;

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

    // Le survol fait paraître l'aperçu de ses huit compétences. La carte qui
    // est dans sa fiche ne le veut pas: elle est déjà sous les huit lignes
    // qu'il résume.
    void setGlanceWanted(bool wanted);

    // Après que l'expérience a bougé, ou que le profil a changé.
    void reread();

    QSize sizeHint() const override;

Q_SIGNALS:
    void invitedChanged();
    void menuWanted(const QPoint &where);
    // Double clic: on veut sa fiche. La carte ne sait pas la dresser -- c'est
    // la page qui l'ouvre, comme pour le menu.
    void sheetWanted();

protected:
    void paintEvent(QPaintEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    QString m_rivalId;
    // La minuterie du survol: l'aperçu attend qu'on soit resté sur la carte.
    QTimer *m_glanceBeat = nullptr;
    bool m_invited = false;
    bool m_pickable = false;
    bool m_hovered = false;
    bool m_glanceWanted = true;
};
