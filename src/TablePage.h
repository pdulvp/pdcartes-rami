#pragma once

#include "Brain.h"
#include "Rami.h"
#include "Tables.h"

#include <QString>
#include <QVector>
#include <QWidget>

class QLabel;
class QPushButton;
class QTimer;
class Rami;
class ScoreBoard;
class TableView;
class VictoryEffect;

// Tout ce qu'on peut régler, et ce n'est pas grand-chose: un rami demande une
// table, une longueur de partie, et deux ou trois commodités.
struct GameSettings {
    QString tableId;
    QString targetId;
    bool autoSort = true;       // la main se retrie quand elle change
    bool offerHints = true;     // le bouton Conseil
    bool celebrate = true;      // le feu d'artifice à la fin
    int pace = 1;               // 0 posé, 1 normal, 2 vif : le temps de réflexion
    int zoom = 100;             // la taille des cartes, en pour cent
    int hue = 142;              // la couleur du drap, choisie et gardée par profil
    QVector<QString> invited;   // les adversaires demandés, les autres au tour de rôle
};

// La page de jeu: la table, la rangée de boutons sous elle, et ce qui fait
// jouer les cinq autres.
//
// C'est ici que les deux moitiés du programme se rencontrent. En dessous, les
// règles, qui ne savent rien de l'écran ; à côté, les cerveaux, qui ne savent
// rien du temps qui passe. Cette page-là tient la partie, demande un coup à
// celui dont c'est le tour, l'attend un instant pour qu'on le voie arriver,
// et compte l'expérience à la fin de chaque manche.
class TablePage : public QWidget {
    Q_OBJECT

public:
    explicit TablePage(QWidget *parent = nullptr);

    void startMatch(const GameSettings &settings);
    void applySettings(const GameSettings &settings);
    void refresh();

    const Rami *game() const { return m_game; }
    bool playing() const;
    QString subtitle() const;

Q_SIGNALS:
    void stateChanged();
    // La taille des cartes, changée à la molette sur le tapis: la fenêtre
    // l'écrit dans les réglages.
    void zoomChanged(int percent);
    // Demandé par la feuille de marque: on quitte la table.
    void wantsMenu();

protected:
    void resizeEvent(QResizeEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    QWidget *buildActions();

    void onTurnBegan(int seat);
    void onActed(int seat, const QString &tale);
    void onHandFinished();
    void onMatchFinished();
    void playRival();

    void sort(bool byRank);
    void doLay();
    // Ce qui attend devant un joueur, carte par carte. Relevé avant de poser,
    // parce que la pose qui ouvre le vide d'un coup.
    QVector<int> waitingCards(int seat) const;
    // Ce qui vient d'arriver sur le tapis: la combinaison posée, et tout ce
    // qui attendait avec elle quand c'est elle qui ouvre.
    QVector<int> landedCards(int seat, const QVector<int> &laid,
                             const QVector<int> &waitingBefore, bool wasOpen) const;
    void doLayOff();
    void doTakeBack();
    void doDiscard();
    void doHint();
    void onMeldPressed(int meldId);
    void onMeldRightPressed(int meldId);
    void onPoseCardPressed(int cardId);
    void onCardDoubleClicked(int cardId);
    void onStockPressed();
    void onPilePressed();

    void updateActions();
    void sayWhatToDo();
    void grumble(const QString &why);
    int mySeat() const;
    int thinkingMs(bool first) const;
    QVector<SeatSpec> chooseSeats(const TableKind &table) const;

    Rami *m_game = nullptr;
    TableView *m_view = nullptr;
    ScoreBoard *m_board = nullptr;
    VictoryEffect *m_victory = nullptr;
    QWidget *m_actions = nullptr;

    QPushButton *m_sortSuit = nullptr;
    QPushButton *m_sortRank = nullptr;
    QPushButton *m_lay = nullptr;
    QPushButton *m_layOff = nullptr;
    QPushButton *m_takeBack = nullptr;
    QPushButton *m_discard = nullptr;
    QPushButton *m_hint = nullptr;
    QLabel *m_pose = nullptr;

    GameSettings m_settings;
    QVector<Skill> m_skills;
    Watch m_watch;
    QTimer *m_beat = nullptr;
    int m_movesThisTurn = 0;
    bool m_handCounted = false;
};
