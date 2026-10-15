#pragma once

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

private:
    void dressProfiles();

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
