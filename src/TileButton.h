#pragma once

#include <QAbstractButton>

// Grand bouton associant une image et un libellé.
//
// Ce type de bouton présente les choix principaux sous une forme directement
// désignable plutôt que sous forme d'entrées de liste. Il n'est plus employé
// que pour les cinq tables du menu d'ouverture, chacune dans sa couleur ; les
// durées de partie, qui l'utilisaient également, sont passées aux boutons
// radio des réglages.
class TileButton : public QAbstractButton {
    Q_OBJECT

public:
    explicit TileButton(QWidget *parent = nullptr);

    // Définit le nombre de places de la table représentée sur la tuile.
    void setSeats(int seats);

    // Définit la valeur affichée dans le coin de la tuile, par exemple le
    // nombre de points que demande une partie de cette durée.
    void setCorner(const QString &corner);

    void setTileSize(const QSize &size);
    // Indique que la tuile est posée sur le tapis et non sur un fond clair :
    // elle est alors rendue sombre et bordée d'or. Ce mode est demandé
    // explicitement par le menu, la même tuile servant ailleurs sur fond
    // clair.
    void setOnFelt(bool onFelt);

    QSize sizeHint() const override;

Q_SIGNALS:
    void pointerIn();
    void pointerOut();

protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    QString m_corner;
    int m_seats = 4;
    bool m_onFelt = false;
    QSize m_tileSize = QSize(206, 176);
    bool m_hovered = false;
};
