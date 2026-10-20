#pragma once

#include <QRect>
#include <QString>
#include <QWidget>

class QTimer;
class QVBoxLayout;

// L'aperçu d'un adversaire: ses huit compétences, le temps qu'on le survole.
//
// Deux endroits le montrent sans dire ce qu'il sait faire -- sa carte au menu
// d'ouverture, sa plaque en haut du tapis -- et dans les deux la question qui
// vient est la même: qu'est-ce qu'il est, celui-là, et qu'est-ce qu'il a
// appris. Sa fiche y répond, mais elle s'ouvre, et on n'ouvre pas une fiche au
// milieu d'une manche.
//
// C'est donc la fiche en plus petit, et rien n'y est cliquable: les quatre
// innées d'un côté, les quatre acquises de l'autre, et de chaque ligne les
// trois premières colonnes seules -- le nom, les pastilles, le rang et son
// mot. Ce qu'un niveau change à la table et ce qui manque pour le suivant
// restent à la fiche: un aperçu qu'il faut lire n'est plus un aperçu.
class RivalGlance : public QWidget {
    Q_OBJECT

public:
    RivalGlance(const QString &rivalId, QWidget *about);

    const QString &rivalId() const { return m_rivalId; }

    // Il se pose, et dès lors il surveille le pointeur lui-même.
    //
    // Le survol seul ne suffit pas à le refermer: posé sur ce qu'on survole --
    // et il faut parfois bien le recouvrir, en bas de l'écran -- il prend le
    // pointeur, la carte dessous reçoit un leave, et un aperçu qui se ferme
    // là-dessus se rouvre aussitôt, puisque le pointeur est resté sur la
    // carte. Il se retire donc quand le pointeur n'est plus ni sur ce qu'on
    // survolait, ni sur lui.
    void watch(const QRect &avoid);
    // Le pointeur est-il encore sur l'un des deux ?
    bool wanted() const;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void buildInnate(QVBoxLayout *column);
    void buildLearned(QVBoxLayout *column);

    QString m_rivalId;
    QRect m_avoid;
    QTimer *m_watch = nullptr;
    int m_hue = 0;
};

// Un seul aperçu à l'écran, et c'est le survol qui le demande.
//
// `avoid` est la place de ce qu'on survole, en coordonnées d'écran: l'aperçu
// se pose dessous, ou dessus quand le bas de l'écran est trop près. `about`
// est celui qui le demande, et à qui il appartient.
void showRivalGlance(const QString &rivalId, const QRect &avoid, QWidget *about);

// Le referme, pourvu qu'il soit bien le sien: deux survols qui se suivent ne
// se ferment pas l'un l'autre. Sans personne, il se referme quoi qu'il arrive.
void hideRivalGlance(const QWidget *about = nullptr);

// Le lâche: il se referme si le pointeur l'a quitté, et reste s'il est encore
// dessus ou sur ce qu'il décrit. C'est ce qu'on appelle quand le pointeur sort
// de la carte ou de la plaque -- qui est aussi ce qui arrive quand l'aperçu
// lui-même vient de se poser dessus.
void releaseRivalGlance(const QWidget *about);

// Le temps qu'on reste sur un adversaire avant que son aperçu paraisse.
//
// Celui d'une infobulle, à peu près: assez pour que le pointeur traverse la
// table sans allumer six panneaux au passage, assez peu pour qu'on n'ait pas
// l'impression d'attendre.
inline constexpr int kGlanceDelayMs = 420;
