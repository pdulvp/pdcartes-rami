#pragma once

#include "RivalSkills.h"

#include <QDialog>
#include <QString>
#include <QVector>

class QLabel;
class QPushButton;
class QToolButton;
class QVBoxLayout;
class RivalCard;
class SkillRow;

// Fiche d'un adversaire : son tempérament, son expérience acquise, et les
// deux boutons d'action.
//
// Elle s'ouvre par un double-clic sur sa carte, dans le menu d'ouverture. Elle
// comporte huit lignes de même forme, réparties en deux colonnes : à gauche le
// tempérament, à droite l'expérience acquise. Chaque compétence a sa couleur.
//
// Les deux boutons ont des effets distincts :
//
//   Remettre son expérience à zéro   Les quatre expériences et le palmarès
//                                    sont réinitialisés. Le nom et le
//                                    tempérament sont conservés.
//   Changer ce qu'il est             Les quatre innées sont modifiables
//                                    manuellement ; la validation entraîne la
//                                    perte de toute l'expérience, celle-ci
//                                    n'étant pas transposable d'un
//                                    tempérament à un autre. La colonne des
//                                    acquises est grisée pendant la
//                                    modification, afin de montrer ce qui sera
//                                    effacé.
//
// Aucune écriture n'a lieu avant la validation par « Changer et
// réapprendre » : ouvrir puis refermer l'éditeur est sans effet, et
// réaffecter les mêmes niveaux n'efface rien.
class RivalSheet : public QDialog {
    Q_OBJECT

public:
    explicit RivalSheet(const QString &rivalId, QWidget *parent = nullptr);

Q_SIGNALS:
    // Émis lorsque l'expérience ou le tempérament a été modifié : la page
    // appelante doit redessiner ses cartes.
    void changed();

private:
    void setEditing(bool editing);
    void buildInnate(QVBoxLayout *rows);
    void buildLearned(QVBoxLayout *rows);
    void reread();
    void askResetExperience();
    void applyInnate();

    QString m_rivalId;
    RivalCard *m_card = nullptr;
    QVector<SkillRow *> m_innate;
    QVector<SkillRow *> m_learned;
    QLabel *m_learnedNote = nullptr;
    QLabel *m_warning = nullptr;
    // Les deux boutons agissant sur une colonne de la fiche sont placés en fin
    // de titre de cette colonne, sous forme d'icônes : c'est là qu'ils sont
    // attendus, et le bas de la boîte reste réservé au bouton « Fermer ».
    QToolButton *m_change = nullptr;
    QToolButton *m_reset = nullptr;
    QPushButton *m_asArrived = nullptr;
    QPushButton *m_cancel = nullptr;
    QPushButton *m_commit = nullptr;
    QPushButton *m_close = nullptr;

    bool m_editing = false;
    Innates m_draft;
};
