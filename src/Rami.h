#pragma once

#include "Cards.h"
#include "Melds.h"

#include <QObject>
#include <QString>
#include <QVector>

// La partie : les règles du jeu, à l'exclusion de tout le reste.
//
// Rien dans ce fichier ne suppose l'existence d'un affichage ni le fait que
// certains joueurs soient simulés. Cette classe détient le jeu de cartes,
// l'ordre des tours, le contenu du tapis et le décompte final. Elle refuse
// tout coup non conforme aux règles en indiquant le motif du refus : une
// règle appliquée sans explication reste incomprise du joueur.
//
// Déroulement complet d'un tour :
//
//   1. piocher     une carte, au sommet de la pioche ou sur la défausse.
//   2. poser       autant de combinaisons que voulu ; lors de la première
//                  pose, elles doivent totaliser cinquante et un points avant
//                  la fin du tour, faute de quoi elles reviennent en main.
//   3. compléter   une fois ouvert : ajouter une carte à n'importe quelle
//                  combinaison du tapis, la sienne ou celle d'un autre.
//   4. défausser   une carte, face visible. S'il s'agissait de la dernière,
//                  la manche s'achève et les autres joueurs comptent les
//                  cartes restées en main.

enum class Phase { Idle, Draw, Build, HandOver, MatchOver };

struct SeatSpec {
    QString rivalId;     // vide pour le joueur humain
    QString name;
    int hue = 0;
};

struct Seat {
    QString rivalId;
    QString name;
    int hue = 0;

    QVector<Card> hand;
    // Pose commencée n'atteignant pas encore cinquante et un points. Elle est
    // placée devant le joueur et non sur le tapis : elle n'y est déposée que
    // lorsqu'elle atteint le total requis, et revient en main si le tour
    // s'achève avant.
    QVector<Meld> waiting;
    bool opened = false;

    int score = 0;        // total de la partie ; il s'agit d'une pénalité
    int lastHand = 0;     // pénalité de la manche qui vient de s'achever
    bool wentOut = false;

    bool human() const { return rivalId.isEmpty(); }
};

class Rami : public QObject {
    Q_OBJECT

public:
    explicit Rami(QObject *parent = nullptr);

    void startMatch(const QVector<SeatSpec> &seats, int targetPoints);
    void startHand();

    int seatCount() const { return m_seats.size(); }
    const Seat &seat(int at) const;
    const QVector<Seat> &seats() const { return m_seats; }
    int turn() const { return m_turn; }
    Phase phase() const { return m_phase; }
    int handNumber() const { return m_handNumber; }
    int dealer() const { return m_dealer; }
    int targetPoints() const { return m_target; }

    const QVector<Meld> &table() const { return m_table; }
    const Meld *meldById(int id) const;
    const QVector<Card> &pile() const { return m_pile; }
    Card pileTop() const;
    int stockLeft() const { return m_stock.size(); }
    // Nombre de fois où la défausse a été retournée pour reconstituer la
    // pioche. Une manche en admet deux au maximum : au-delà, plus aucun joueur
    // ne parviendra à sortir et la manche est interrompue en l'état.
    int reshuffles() const { return m_reshuffles; }

    int outSeat() const { return m_outSeat; }        // joueur sorti, ou -1
    int matchWinner() const { return m_matchWinner; } // vainqueur, ou -1

    // Total de la pose en attente devant le joueur `at`.
    int poseValue(int at) const;

    // Un joker racheté dans une combinaison doit être réemployé avant la fin
    // du tour. Retourne vrai tant qu'un joker racheté reste en main.
    bool owesJoker(int at) const;

    // Identité des jokers rachetés et non encore réemployés.
    //
    // Leur identité exacte est nécessaire, et pas seulement leur existence :
    // une main détenant deux jokers peut en poser un, mais si ce n'est pas
    // celui qui est dû, le tour ne peut toujours pas s'achever, ni par une
    // pose ni par une défausse. Connaître le joker concerné est le seul moyen
    // de lever la contrainte, et c'est également nécessaire pour formuler un
    // conseil au joueur.
    QVector<int> jokerDebt(int at) const;

    // Carte piochée durant ce tour et provenance de la pioche, afin que
    // l'affichage puisse la mettre en évidence.
    Card lastDrawn() const { return m_lastDrawn; }
    bool drewFromPile() const { return m_drewFromPile; }

    // Les coups. Chacun indique s'il a été autorisé et, dans le cas
    // contraire, renseigne `why` avec le motif du refus.
    bool drawStock(QString *why = nullptr);
    bool drawPile(QString *why = nullptr);
    bool lay(const QVector<int> &cardIds, QString *why = nullptr);
    bool layOff(int cardId, int meldId, QString *why = nullptr);
    bool buyJoker(int cardId, int meldId, QString *why = nullptr);
    bool discard(int cardId, QString *why = nullptr);

    // Défait une pose non encore déposée sur le tapis : en totalité, par
    // combinaison, ou carte par carte.
    //
    // Une pose en attente se trouve devant son joueur et non sur le tapis,
    // dans l'attente des cinquante et un points ; elle peut être défaite tant
    // que le tour n'est pas achevé. Une carte isolée ne revient en main que si
    // sa combinaison reste valide sans elle ; dans le cas contraire, c'est la
    // combinaison entière qui doit être reprise, à la demande de l'appelant.
    bool takeBackPose(QString *why = nullptr);
    bool takeBackMeld(int meldId, QString *why = nullptr);
    bool takeBackCard(int cardId, QString *why = nullptr);

    // Nombre de tours déjà joués dans la manche.
    //
    // C'est le seul compteur de progression qui avance en toutes
    // circonstances. La pioche n'en est pas un : deux joueurs se servant l'un
    // et l'autre sur la défausse ne l'entament jamais, et une manche peut se
    // prolonger ainsi sans qu'aucune réserve ne s'épuise. Les joueurs simulés
    // dont la stratégie dépend de l'avancement de la manche ont besoin de
    // cette information.
    int turnsPlayed() const { return m_turnsPlayed; }

    // Indique si une pose a été reprise en main durant ce tour.
    //
    // Un joueur qui vient de reprendre sa pose ne la redépose pas dans le même
    // tour : il la conserve et défausse. Cette information est rattachée au
    // tour ; sans elle, un joueur simulé ne sachant pas évaluer son total
    // avant d'ouvrir reposerait indéfiniment ce qu'il vient de reprendre.
    bool poseTakenBack() const { return m_tookBack; }

    // Combinaison de la pose en cours contenant cette carte, ou -1.
    int poseMeldOf(int cardId) const;

    // Indique si cette carte peut être reprise isolément. Retourne faux si sa
    // combinaison ne resterait pas valide sans elle, ou si l'état du tour ne
    // le permet pas.
    bool canTakeBackCard(int cardId) const;

    // Indique si le tour pourrait être achevé maintenant, sans l'achever.
    bool canDiscard(QString *why = nullptr) const;

    // Trie la main d'un joueur. L'ordre d'une main appartient à son
    // détenteur, et cette opération figure donc ici, auprès de la main : par
    // couleur pour repérer les suites, par rang pour repérer les séries.
    void sortSeatHand(int at, bool byRank);

Q_SIGNALS:
    void changed();
    // Description en une ligne du coup qui vient d'être joué, destinée au
    // bandeau inférieur de la table : c'est le seul compte rendu continu des
    // coups joués par les autres joueurs.
    void acted(int seat, const QString &tale);
    void turnBegan(int seat);
    void handFinished();
    void matchFinished();

private:
    void dealHand();
    void nextTurn();
    void finishHand();
    bool refillStock();
    Seat &here();
    int nextMeldId() { return ++m_meldIds; }

    QVector<Seat> m_seats;
    QVector<Card> m_stock;
    QVector<Card> m_pile;
    QVector<Meld> m_table;

    Phase m_phase = Phase::Idle;
    int m_turn = 0;
    int m_dealer = 0;
    int m_handNumber = 0;
    int m_target = 0;
    int m_meldIds = 0;
    int m_outSeat = -1;
    int m_matchWinner = -1;
    int m_reshuffles = 0;

    Card m_lastDrawn;
    bool m_drewFromPile = false;
    bool m_tookBack = false;
    int m_turnsPlayed = 0;
    // Jokers rachetés durant ce tour et non encore posés, désignés par leur
    // identifiant de carte.
    QVector<int> m_jokerDebt;
};
