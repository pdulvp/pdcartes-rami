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

// Paramètres configurables d'une partie : une table, une durée de partie, et
// quelques options de confort.
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

// Page de jeu : la table, la barre de boutons située en dessous, et le
// pilotage des adversaires.
//
// C'est le point de jonction des deux parties du programme. D'un côté les
// règles, qui ignorent l'affichage ; de l'autre la logique de décision, qui
// ignore l'écoulement du temps. Cette page détient la partie, demande son coup
// au joueur dont c'est le tour, temporise son exécution pour qu'elle soit
// perceptible, et attribue l'expérience à la fin de chaque manche.
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
    // Taille des cartes, modifiée à la molette sur le tapis. La fenêtre
    // principale enregistre la valeur dans les réglages.
    void zoomChanged(int percent);
    // Émis depuis la feuille de marque : le joueur quitte la table.
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
    void forgetDeeds();
    void noteTakenFromPile(int at);

    void sort(bool byRank);
    void doLay();
    // Contenu de la pose en attente devant un joueur, carte par carte. Relevé
    // avant la pose, celle-ci vidant la zone d'attente en une seule
    // opération.
    QVector<int> waitingCards(int seat) const;
    // Cartes qui viennent d'être déposées sur le tapis : la combinaison posée,
    // et l'ensemble de la pose en attente lorsque c'est elle qui ouvre.
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

    QPushButton *m_lay = nullptr;
    QPushButton *m_layOff = nullptr;
    QPushButton *m_takeBack = nullptr;
    QPushButton *m_discard = nullptr;
    QPushButton *m_hint = nullptr;
    QLabel *m_pose = nullptr;

    GameSettings m_settings;
    QVector<Skill> m_skills;
    Watch m_watch;
    // Actions comptabilisées pour chaque joueur au titre de ses compétences
    // dans la manche en cours, et provenance de la carte au sommet de la
    // défausse, afin de déterminer à qui une carte reprise a porté
    // préjudice.
    QVector<Deeds> m_deeds;
    int m_pileFrom = -1;
    QTimer *m_beat = nullptr;
    int m_movesThisTurn = 0;
    bool m_handCounted = false;
};
