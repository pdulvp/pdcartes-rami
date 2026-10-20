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

// La fiche d'un adversaire: ce qu'il est, ce qu'il a appris, et les deux
// boutons.
//
// Elle s'ouvre d'un double clic sur sa carte, au menu d'ouverture. Huit
// lignes, quatre et quatre, la même forme deux fois: à gauche ce qu'il est,
// à droite ce qu'il a appris. Chaque compétence a sa couleur.
//
// Les deux boutons ne font pas la même chose, et la différence est tout le
// sujet:
//
//   Remettre son expérience à zéro   Les quatre expériences et le palmarès
//                                    repartent. Il reste lui-même, avec son
//                                    nom et son caractère.
//   Changer ce qu'il est             Les quatre innées se posent à la main --
//                                    et valider lui fait tout réapprendre,
//                                    parce que l'expérience gagnée par un
//                                    joueur n'est pas l'expérience qu'un
//                                    autre aurait. La moitié acquise
//                                    s'éteint pendant qu'on choisit, pour
//                                    qu'on voie ce qu'on s'apprête à
//                                    effacer.
//
// Rien ne s'écrit avant « Changer et réapprendre »: ouvrir l'éditeur et le
// refermer est gratuit, et reposer les mêmes niveaux n'efface rien.
class RivalSheet : public QDialog {
    Q_OBJECT

public:
    explicit RivalSheet(const QString &rivalId, QWidget *parent = nullptr);

Q_SIGNALS:
    // Son expérience ou son caractère a bougé: la page d'où l'on vient a des
    // cartes à redessiner.
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
    // Les deux qui agissent sur une moitié de la fiche se tiennent au bout du
    // titre de leur moitié, en icône: c'est là qu'on les cherche, et le bas de
    // la boîte reste à « Fermer ».
    QToolButton *m_change = nullptr;
    QToolButton *m_reset = nullptr;
    QPushButton *m_asArrived = nullptr;
    QPushButton *m_cancel = nullptr;
    QPushButton *m_commit = nullptr;
    QPushButton *m_close = nullptr;

    bool m_editing = false;
    Innates m_draft;
};
