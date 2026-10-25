#pragma once

#include <QString>
#include <QVector>

// Les huit compétences d'un adversaire et leur effet sur son jeu.
//
// Ce fichier n'effectue aucune lecture ni écriture persistante : il contient
// la description des compétences et la fonction qui convertit un adversaire
// donné en les quelques valeurs numériques exploitées par sa logique de jeu.
// Le stockage de ces valeurs par profil relève de Rivals.h, et leur
// exploitation de Brain.cpp. Cette séparation permet à l'arène d'évaluer une
// logique de jeu sans profil ni affichage.
//
// Les compétences se répartissent en deux familles :
//
//   les innées    Au nombre de quatre, sur quatre niveaux, et invariables.
//                 Elles décrivent le tempérament du joueur : sa propension au
//                 risque, sa réticence à se séparer de ses cartes, son
//                 attention au jeu des autres, et son sang-froid. Elles sont
//                 modifiables depuis sa fiche, au prix de la perte de toute
//                 son expérience : l'expérience acquise avec un tempérament
//                 donné n'est pas transposable à un autre.
//   les acquises  Au nombre de quatre également, sur quatre rangs, chacune
//                 disposant de sa propre expérience alimentée par les parties
//                 jouées. Elles décrivent son savoir-faire : analyse de sa
//                 main, mémorisation du jeu, choix de sa défausse, et absence
//                 d'erreurs.
//
// Les innées déterminent la vitesse d'acquisition des acquises — un joueur
// peu attentif développe lentement sa mémoire — de sorte que deux adversaires
// ayant joué autant de manches ne sont pas devenus des joueurs équivalents.
//
// Principe d'équilibrage, valable pour l'ensemble du fichier : les rangs
// déterminent la force, les innées déterminent le style. Une innée décale un
// paramètre d'au plus la valeur d'un rang et ne débloque jamais un rang.
// Aucun réglage ne permet donc à un débutant de jouer comme un expert, et
// l'indicateur de niveau affiché sur la plaque reste exact.

// ---------------------------------------------------------------------------
// Le tempérament.

enum class Innate { Audace, Prudence, Vigilance, Calme };

// Le savoir-faire.
enum class Learned { Lecture, Memoire, Defausse, Maitrise };

inline constexpr int kInnateCount = 4;
inline constexpr int kLearnedCount = 4;

// Quatre niveaux pour une innée (0 à 3), quatre rangs pour une acquise.
inline constexpr int kInnateTop = 3;
inline constexpr int kRungCount = 4;

// Expérience maximale accumulable par une acquise. Les quatre réunies
// totalisent les 1200 points d'expérience affichés sur la carte de
// l'adversaire, dont est déduit son niveau : débutant, intermédiaire ou
// expert.
inline constexpr int kPoolCeiling = 300;

// Les quatre niveaux innés d'un adversaire et ses quatre expériences. Ces
// structures sont délibérément plates et copiables : elles transitent de la
// fiche à la logique de jeu sans allocation.
struct Innates {
    int level[kInnateCount] = {1, 1, 1, 1};

    int at(Innate which) const { return level[int(which)]; }
    void set(Innate which, int value) { level[int(which)] = qBound(0, value, kInnateTop); }
    bool operator==(const Innates &other) const;
    bool operator!=(const Innates &other) const { return !(*this == other); }
};

struct Pools {
    int xp[kLearnedCount] = {0, 0, 0, 0};

    int at(Learned which) const { return xp[int(which)]; }
    void set(Learned which, int value) { xp[int(which)] = qBound(0, value, kPoolCeiling); }
    int total() const;
};

// Actions accomplies par un adversaire au cours d'une manche, ventilées par
// compétence.
//
// La table les comptabilise au fil de la manche — une carte ramassée qui a
// servi, une carte défaussée qu'un adversaire a reprise, un joker racheté —
// et l'expérience de la manche est répartie en fonction de ces comptes. Le
// total attribué par manche reste inchangé : c'est la même somme qu'avant
// l'introduction des compétences, affectée là où une action a eu lieu plutôt
// que répartie en quatre parts égales. Un adversaire ayant principalement
// exploité la défausse progresse donc dans cette compétence.
struct Deeds {
    int count[kLearnedCount] = {0, 0, 0, 0};

    int at(Learned which) const { return count[int(which)]; }
    void did(Learned which, int times = 1) { count[int(which)] += times; }
    void clear() {
        for (int i = 0; i < kLearnedCount; ++i) count[i] = 0;
    }
};

// ---------------------------------------------------------------------------
// Le niveau, lecture d'ensemble des quatre acquises.
//
// Trois appellations sont associées à des plages d'expérience totale, un
// affichage tel que « niveau 437 » n'étant pas exploitable par le joueur :
//
//     débutant        0 - 199     se défausse de cartes utiles, ouvre
//                                 tardivement, analyse mal sa main
//     intermédiaire 200 - 599     analyse correctement sa main, surveille la
//                                 défausse, conserve ce qui peut servir
//     expert        600 - 1200    mémorise toutes les cartes défaussées,
//                                 évite d'alimenter les adversaires, et
//                                 rachète ses jokers

enum class Level { Debutant, Intermediaire, Expert };

// Expérience maximale : les quatre compétences au plafond.
inline constexpr int kMaxExperience = kLearnedCount * kPoolCeiling;

Level levelOf(int xp);
QString levelName(Level level);
int levelStars(Level level);           // une, deux ou trois
int levelFloor(Level level);
int levelCeiling(Level level);

// Progression de zéro à un sur l'échelle entière : taux de remplissage de la
// jauge figurant sur la carte de l'adversaire.
qreal masteryOf(int xp);

// Progression de zéro à un à l'intérieur du niveau courant, pour la jauge
// placée sous le nom.
qreal progressWithin(int xp);

// ---------------------------------------------------------------------------
// Libellés affichés.

// Description d'une innée : son nom, sa couleur, l'intitulé de chacun de ses
// quatre niveaux, et l'effet de ce niveau en cours de partie. Cet effet est
// formulé en termes de comportement observable et jamais sous forme de
// coefficient.
struct InnateSpec {
    QString name;
    int hue;                      // teinte propre, dans les tons chauds
    QString levels[kInnateTop + 1];
    QString says[kInnateTop + 1];
    QString blurbs[kInnateTop + 1];   // phrase de la carte, si cette innée domine
};

// Description d'une acquise : son nom, sa couleur, l'intitulé de chacun de
// ses quatre rangs, et l'effet de ce rang.
struct LearnedSpec {
    QString name;
    int hue;                      // teinte propre, dans les tons froids
    QString rungs[kRungCount];
    QString says[kRungCount];
};

const InnateSpec &innateSpec(Innate which);
const LearnedSpec &learnedSpec(Learned which);

// ---------------------------------------------------------------------------
// Les rangs.

// Expérience requise, dans une compétence donnée, pour atteindre un rang.
// Le premier rang est acquis d'emblée.
int rungFloor(int rung);
int rungOf(int poolXp);
// Expérience manquante pour atteindre le rang suivant ; zéro au dernier rang.
int rungGap(int poolXp);
// Rang en chiffres romains, de I à IV. Cette notation est employée partout où
// le rang est affiché — sur la fiche et dans l'aperçu — afin que tous les
// rangs soient présentés de façon uniforme.
QString rungRoman(int rung);
// Progression de zéro à un à l'intérieur du rang courant, pour la jauge de la
// ligne correspondante.
qreal rungProgress(int poolXp);

// Progression de zéro à un sur l'ensemble des quatre rangs : valeur continue
// d'une compétence, destinée aux paramètres qui varient sans palier, tels que
// la fréquence des erreurs ou le seuil d'acceptation de la défausse. Le rang
// est la grandeur affichée, celle-ci est la grandeur effectivement jouée.
qreal depthOf(int poolXp);

// ---------------------------------------------------------------------------
// Paramètres exploités par la logique de jeu.
//
// Un niveau n'est pas un coefficient appliqué à une force globale, mais un
// ensemble de comportements que le joueur adopte ou non, chacun observable en
// cours de partie. C'est la raison pour laquelle ils sont énumérés
// individuellement plutôt que résumés en un seul réglage de difficulté.
struct Skill {
    qreal mastery = 0.0;         // de zéro à un, sur l'échelle entière

    // Savoir-faire acquis, rang par rang. Chacun de ces champs correspond à un
    // rang précis de l'une des quatre acquises. C'est là que réside la force
    // du joueur : un adversaire progresse en atteignant ces rangs, non par
    // modification de ses réglages.
    bool thorough = false;       // Lecture II : analyse exactement sa main
    bool countsOpening = false;  // Lecture III : n'entame pas de pose insuffisante
    bool watches = false;        // Mémoire III : compte les cartes définitivement sorties
    bool readsTakes = false;     // Mémoire IV : retient les cartes ramassées par autrui
    bool guardsFeed = false;     // Défausse III : évite d'alimenter le tapis
    bool readsEndgame = false;   // Défausse IV : adapte son jeu en fin de manche
    bool buysJokers = false;     // Maîtrise IV : rachète les jokers
    qreal blunder = 0.0;         // Maîtrise : fréquence des coups erronés
    qreal layoffSight = 1.0;     // Maîtrise : probabilité de voir une carte plaçable
    qreal pileThreshold = 2.0;   // Mémoire : avantage minimal exigé de la défausse
    qreal potentialWeight = 1.0; // Lecture : valeur d'une combinaison à une carte près

    // Traits de tempérament. Ils ne rendent le joueur ni meilleur ni moins
    // bon, mais le rendent reconnaissable : un expert prudent et un expert
    // audacieux restent l'un et l'autre des experts.
    int openingSlack = 0;        // points attendus au-delà des cinquante et un
    qreal jokerGrip = 0.0;       // réticence à poser un joker
    qreal keepWeight = 1.0;      // attachement aux cartes susceptibles de servir
    qreal recall = 1.0;          // fiabilité effective de la mémoire
    qreal eagerness = 1.0;       // empressement à poser ce qui peut l'être
};

// Convertit un adversaire en les paramètres exploités par sa logique de jeu.
// C'est la fonction principale de ce fichier, et la seule habilitée à fixer la
// valeur d'une compétence.
Skill composeSkill(const Innates &innates, const Pools &pools);

// Paramètres du meilleur joueur que le programme sache simuler, employés par
// le bouton Conseil et comme référence dans l'arène : tous les rangs au
// maximum, toutes les innées à leur valeur la plus efficace, et aucune
// erreur.
Skill bestSkill();

// Répartit une expérience globale sur les quatre compétences, en la pondérant
// selon le tempérament de l'adversaire. Employée dans deux cas : pour fixer le
// niveau initial des six adversaires, et pour convertir l'expérience d'un
// profil antérieur à l'introduction des compétences.
Pools poolsFromXp(int xp, const Innates &innates);

// Vitesse d'acquisition d'une compétence donnée chez cet adversaire. La valeur
// est comprise entre un et un et demi : les innées accélèrent l'apprentissage
// sans s'y substituer.
qreal learnRate(const Innates &innates, Learned which);

// Coefficient appliqué au gain d'expérience d'une manche en fonction de
// l'expérience déjà acquise.
//
// L'expérience attribuée par manche ne dépendait que du classement
// (Rivals.cpp) : un adversaire à mille points en gagnait autant qu'un
// adversaire à dix. Le haut de l'échelle se remplissait donc aussi vite que
// le bas — dix parties à six joueurs suffisaient pour que les six atteignent
// tous leurs rangs, valeur mesurée à l'arène — alors que seul le bas de
// l'échelle avait été calibré pour une progression rapide.
//
// Le gain est donc intégral tant que l'adversaire débute, puis décroît :
// l'expérience déjà acquise ralentit l'acquisition suivante. Le gain ne
// devient jamais nul, un plancher étant appliqué dans `noteRivalHand`.
//
//     expérience   gain par manche   parties à six requises pour y parvenir
//            200   intégral           2
//            600   un tiers          11
//            920   un cinquième      25
//           1200   un sixième        41
qreal learnTaper(int xp);

// Seuil en deçà duquel le gain par manche reste intégral. Il coïncide
// exactement avec la sortie du niveau débutant : les trois ou quatre parties
// que cela demande constituent un calibrage délibéré.
inline constexpr int kLearnFullUntil = 200;

// Phrase décrivant l'adversaire, déduite de ses innées. Elle figure sur sa
// carte et est recalculée après modification de son tempérament, afin de
// rester conforme à ce qu'il est devenu.
QString blurbFrom(const Innates &innates);

// Innée la plus marquée, sur laquelle la phrase descriptive est fondée.
Innate dominantInnate(const Innates &innates);
