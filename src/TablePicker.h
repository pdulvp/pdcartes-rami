#pragma once

#include <QPoint>
#include <QString>
#include <QVector>
#include <QWidget>

class QLabel;
class QPushButton;
class RivalCard;
class TileButton;

// Le menu d'ouverture: la longueur de la partie, les cinq tables, et les six
// adversaires.
//
// Il n'y a ni bouton « nouvelle partie » ni boîte de dialogue à traverser:
// presser une table est ce qui lance la partie. Ce qui se choisit avant est
// donc au-dessus d'elles -- la longueur, qui change le compte écrit dans leur
// coin -- et ce qui répond à « contre qui » est en dessous.
//
// Le profil n'est plus ici: il se tient dans la bande du haut, où il reste
// visible pendant la partie. Ce qui se fait d'un adversaire se fait sur lui,
// d'un clic droit, plutôt que dans les réglages.
class TablePicker : public QWidget {
    Q_OBJECT

public:
    explicit TablePicker(QWidget *parent = nullptr);

    // La longueur gardée d'une fois sur l'autre, posée sur les plaques au
    // démarrage. Le choix se fait ensuite ici, et remonte par targetChosen().
    void setTarget(const QString &targetId);

    // Qui a été invité. Les sièges qui restent vont aux moins joués.
    QVector<QString> invited() const;

    // Après un changement de profil, ou quand l'expérience a bougé.
    void reread();
    void refresh();

Q_SIGNALS:
    void tableChosen(const QString &id);
    // La longueur vient de changer: la fenêtre la garde dans ses réglages.
    void targetChosen(const QString &id);

private:
    void dressLengths();

    void renameRival(const QString &rivalId);
    void showRivalMenu(RivalCard *card, const QPoint &where);
    void showRivalSheet(const QString &rivalId);
    void askResetRival(const QString &rivalId);

    QLabel *m_title = nullptr;
    QLabel *m_subtitle = nullptr;
    QLabel *m_lengthNote = nullptr;
    QLabel *m_rivalNote = nullptr;
    QLabel *m_footer = nullptr;
    // Une plaque de longueur: le bouton, et les deux lignes qu'il porte.
    // Deux étiquettes plutôt qu'un texte à deux lignes, parce qu'un bouton
    // n'a qu'une encre et qu'un corps de texte, et qu'il en faut deux de
    // chaque -- le nom, puis ce qu'il veut dire, plus petit et plus clair.
    struct LengthPlaque {
        QPushButton *button = nullptr;
        QLabel *name = nullptr;
        QLabel *blurb = nullptr;
    };
    QVector<LengthPlaque> m_lengths;
    QVector<TileButton *> m_tiles;
    QVector<RivalCard *> m_cards;
    QString m_targetId;
};
