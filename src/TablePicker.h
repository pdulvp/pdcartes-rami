#pragma once

#include <QPixmap>
#include <QPoint>
#include <QRectF>
#include <QString>
#include <QVector>
#include <QWidget>

class QLabel;
class QPushButton;
class RivalCard;
class TileButton;

// Le menu d'ouverture: la longueur de la partie, les six adversaires, et les
// cinq tables.
//
// Il n'y a ni bouton « nouvelle partie » ni boîte de dialogue à traverser:
// presser une table est ce qui lance la partie. Tout ce qui se choisit est
// donc au-dessus d'elles -- la longueur, qui change le compte écrit dans leur
// coin, puis ce qui répond à « contre qui » -- et les tables ferment la page,
// parce qu'une fois pressées il n'y a plus de page.
//
// Sur le tapis, et non sur une page à part: c'est le même drap que l'écran
// titre et que la partie, et le logo descend de l'un pour se poser en tête de
// celui-ci. Ce qui est posé dessus est donc sombre et bordé d'or — ce qui se
// lit sur un drap vert sombre est clair, pas pâle.
//
// Et peu de mots. Le nom de chaque chose, ce qu'elle coûte en manches ou en
// points, et une seule ligne en bas pour les gestes qu'on ne devine pas: ce
// qui se disait ici en trois paragraphes est dit mieux par l'aperçu au survol
// et par la fiche de chacun.
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

    // Où le logo se pose, dans les coordonnées de la page: c'est là que le
    // vol depuis l'écran titre se termine.
    QRectF markRect() const;
    // Le temps du vol, c'est le calque qui porte le logo.
    void setMarkHidden(bool hidden);
    // La page paraît: ce qui est posé sur le drap monte de rien à tout
    // pendant que le logo descend. Sans cela le logo volerait au-dessus d'un
    // menu déjà entièrement là, et le vol n'aurait l'air de rien.
    void beginEntrance();

Q_SIGNALS:
    void tableChosen(const QString &id);
    // La longueur vient de changer: la fenêtre la garde dans ses réglages.
    void targetChosen(const QString &id);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void dressLengths();
    void rebuildFelt();

    void renameRival(const QString &rivalId);
    void showRivalMenu(RivalCard *card, const QPoint &where);
    void showRivalSheet(const QString &rivalId);
    void askResetRival(const QString &rivalId);

    // Le tapis, peint une fois: la page défile, et refaire le tissage à
    // chaque cran de molette serait du travail jeté.
    QPixmap m_felt;
    // La bande du haut, qui ne défile pas: le logo s'y tient.
    QWidget *m_header = nullptr;
    // Tout ce qui n'est pas la bande du logo: c'est lui qui paraît.
    QWidget *m_content = nullptr;
    QLabel *m_footer = nullptr;
    QVector<QWidget *> m_headings;
    // Une plaque de longueur: le bouton, et les deux lignes qu'il porte.
    // Deux étiquettes plutôt qu'un texte à deux lignes, parce qu'un bouton
    // n'a qu'une encre et qu'un corps de texte, et qu'il en faut deux de
    // chaque -- le nom, puis ce qu'il coûte, plus petit et dans l'or.
    struct LengthPlaque {
        QPushButton *button = nullptr;
        QLabel *name = nullptr;
        QLabel *blurb = nullptr;
    };
    QVector<LengthPlaque> m_lengths;
    QVector<TileButton *> m_tiles;
    QVector<RivalCard *> m_cards;
    QString m_targetId;
    bool m_markHidden = false;
};
