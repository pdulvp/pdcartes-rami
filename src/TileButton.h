#pragma once

#include <QAbstractButton>

// Un grand bouton avec une image, un nom et une ligne d'explication.
//
// Le jeu est un jeu : ce qui le lance doit être une chose qu'on montre du
// doigt, pas une ligne dans une liste déroulante. Un seul widget sert aux
// deux endroits où ces boutons-là sont voulus -- les cinq tables du menu
// d'ouverture, chacune dans sa couleur, et les quatre longueurs de partie
// dans les réglages -- parce que la seule différence entre eux est ce qui est
// dessiné dans la moitié du haut.
class TileButton : public QAbstractButton {
    Q_OBJECT

public:
    explicit TileButton(QWidget *parent = nullptr);

    // La petite table dessinée sur la tuile, et combien de places elle a.
    void setSeats(int seats);

    // La ligne sous le nom, plus petite et plus claire.
    void setBlurb(const QString &blurb);

    // Un chiffre dans le coin: le nombre de points qu'une partie de cette
    // longueur demande, ce genre de chose.
    void setCorner(const QString &corner);

    // La teinte dans laquelle la tuile est peinte, qui est ce qui permet à
    // cinq tables d'être côte à côte chacune dans sa couleur. Sans teinte, la
    // tuile porte celle que le jeu porte.
    void setTileHue(int hue);

    void setTileSize(const QSize &size);

    QSize sizeHint() const override;

Q_SIGNALS:
    void pointerIn();
    void pointerOut();

protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    QString m_blurb;
    QString m_corner;
    int m_seats = 4;
    int m_hue = -1;
    QSize m_tileSize = QSize(206, 176);
    bool m_hovered = false;
};
