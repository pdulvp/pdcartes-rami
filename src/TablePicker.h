#pragma once

#include <QPoint>
#include <QString>
#include <QVector>
#include <QWidget>

class QLabel;
class QPushButton;
class RivalCard;
class TileButton;

// Le menu d'ouverture: le profil sous lequel on joue, les cinq tables, et les
// six adversaires.
//
// Il n'y a ni bouton « nouvelle partie » ni boîte de dialogue à traverser:
// presser une table est ce qui lance la partie, parce que choisir combien on
// sera est le seul choix qu'une nouvelle partie demande. Tout le reste de la
// page est là pour répondre à la question d'avant: contre qui, et sont-ils
// devenus meilleurs depuis la dernière fois.
//
// Ce qui se faisait d'un profil et d'un adversaire se fait donc ici, sur la
// chose elle-même, plutôt que dans les réglages: un profil se renomme d'un
// double clic, et un clic droit -- sur un profil ou sur un adversaire --
// ouvre le reste. Les réglages ne gardent que ce qui vaut pour toute la
// partie.
class TablePicker : public QWidget {
    Q_OBJECT

public:
    explicit TablePicker(QWidget *parent = nullptr);

    // La longueur de partie choisie dans les réglages, écrite sur les tuiles
    // pour qu'elle ne soit pas une surprise.
    void setTarget(const QString &targetId);

    // Qui a été invité. Les sièges qui restent vont aux moins joués.
    QVector<QString> invited() const;

    // Après un changement de profil, ou quand l'expérience a bougé.
    void reread();
    void refresh();

Q_SIGNALS:
    void tableChosen(const QString &id);
    void profileChosen(int slot);
    // Le nom d'un profil, ou ce qu'il a joué, vient de changer: la bande du
    // haut porte le nom et doit le relire.
    void profileChanged();

protected:
    // Les gestes sur les plaques de profil: le double clic renomme, le clic
    // droit ouvre le menu. Un filtre, plutôt qu'un bouton dérivé pour deux
    // événements que QPushButton ne rapporte pas.
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void dressProfiles();

    void showProfileMenu(int slot, const QPoint &where);
    void renameProfile(int slot);
    void askForgetProfile(int slot);

    void showRivalMenu(RivalCard *card, const QPoint &where);
    void askResetRival(const QString &rivalId);

    QLabel *m_title = nullptr;
    QLabel *m_subtitle = nullptr;
    QLabel *m_profileNote = nullptr;
    QLabel *m_rivalNote = nullptr;
    QLabel *m_footer = nullptr;
    QVector<QPushButton *> m_profiles;
    QVector<TileButton *> m_tiles;
    QVector<RivalCard *> m_cards;
    QString m_targetId;
};
