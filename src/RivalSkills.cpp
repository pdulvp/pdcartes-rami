#include "RivalSkills.h"

#include <QCoreApplication>

#include <algorithm>
#include <cmath>

// Les huit compétences et la fonction qui détermine leur valeur effective.
//
// L'équilibrage du jeu repose entièrement sur `composeSkill`, selon le
// principe suivant : les rangs déterminent la force, les innées déterminent le
// style. Chaque paramètre est dérivé d'une seule compétence acquise — la
// fréquence des erreurs de la Maîtrise, le seuil d'acceptation de la défausse
// de la Mémoire — et une innée se borne à le décaler, d'au plus la valeur d'un
// rang, la borne étant appliquée sur place. C'est ce qui permet de régler un
// adversaire sans qu'il dépasse jamais le meilleur joueur simulé par le
// programme, ni qu'il descende sous le niveau débutant initial.

namespace {

// Expérience requise pour chaque rang d'une compétence. Le premier rang est
// acquis d'emblée.
constexpr int kRungFloors[kRungCount] = {0, 60, 140, 230};

// Innée associée à chaque acquise, déterminant sa vitesse d'acquisition.
// L'association est bijective afin que chacune des quatre innées ait un effet
// identifiable : l'audace favorise l'analyse de la main, la prudence le choix
// de la défausse, la vigilance la mémorisation, et le calme la justesse
// d'exécution.
Innate feeds(Learned which) {
    switch (which) {
    case Learned::Lecture: return Innate::Audace;
    case Learned::Memoire: return Innate::Vigilance;
    case Learned::Defausse: return Innate::Prudence;
    case Learned::Maitrise: return Innate::Calme;
    }
    return Innate::Audace;
}

const QVector<InnateSpec> &innateSpecs() {
    static const QVector<InnateSpec> list = {
        {QCoreApplication::translate("skills", "Audace"), 14,
         {QCoreApplication::translate("skills", "timide"),
          QCoreApplication::translate("skills", "mesuré"),
          QCoreApplication::translate("skills", "hardi"),
          QCoreApplication::translate("skills", "casse-cou")},
         {QCoreApplication::translate("skills", "ouvre dès qu'il a ses cinquante-et-un"),
          QCoreApplication::translate("skills", "ouvre quand il peut, sans se presser"),
          QCoreApplication::translate("skills", "attend quelques points de plus pour ouvrir"),
          QCoreApplication::translate("skills", "garde sa pose pour sortir d'un coup")},
         {QCoreApplication::translate("rivals", "Ouvre à la première occasion"),
          QCoreApplication::translate("rivals", "Ouvre quand il peut, sans se presser"),
          QCoreApplication::translate("rivals", "Attend son coup, et le joue"),
          QCoreApplication::translate("rivals", "Joue vite, pose tard, et s'en moque")}},

        {QCoreApplication::translate("skills", "Prudence"), 42,
         {QCoreApplication::translate("skills", "insouciant"),
          QCoreApplication::translate("skills", "ordinaire"),
          QCoreApplication::translate("skills", "regardant"),
          QCoreApplication::translate("skills", "économe")},
         {QCoreApplication::translate("skills", "pose ses jokers sans y penser"),
          QCoreApplication::translate("skills", "jette ce qui coûte, garde ce qui sert"),
          QCoreApplication::translate("skills", "garde ce qui peut servir, et le paye"),
          QCoreApplication::translate("skills", "ne lâche pas un joker, dût-il le payer")},
         {QCoreApplication::translate("rivals", "Donne ses cartes sans compter"),
          QCoreApplication::translate("rivals", "Jette ce qui coûte, garde ce qui sert"),
          QCoreApplication::translate("rivals", "Ne jette jamais ce qui peut servir"),
          QCoreApplication::translate("rivals", "Garde ses jokers jusqu'à la fin, hélas")}},

        {QCoreApplication::translate("skills", "Vigilance"), 320,
         {QCoreApplication::translate("skills", "distrait"),
          QCoreApplication::translate("skills", "ordinaire"),
          QCoreApplication::translate("skills", "attentif"),
          QCoreApplication::translate("skills", "aux aguets")},
         {QCoreApplication::translate("skills", "ne ramasse que ce qui finit une combinaison"),
          QCoreApplication::translate("skills", "regarde la défausse de temps en temps"),
          QCoreApplication::translate("skills", "pèse la défausse à chaque tour"),
          QCoreApplication::translate("skills", "voit tout ce qui passe, et vos prises")},
         {QCoreApplication::translate("rivals", "Ne voit pas ce que vous ramassez"),
          QCoreApplication::translate("rivals", "Regarde la défausse de temps en temps"),
          QCoreApplication::translate("rivals", "Compte les cartes, la plupart du temps"),
          QCoreApplication::translate("rivals", "A le nez sur la défausse")}},

        {QCoreApplication::translate("skills", "Calme"), 92,
         {QCoreApplication::translate("skills", "soupe au lait"),
          QCoreApplication::translate("skills", "ordinaire"),
          QCoreApplication::translate("skills", "posé"),
          QCoreApplication::translate("skills", "imperturbable")},
         {QCoreApplication::translate("skills", "s'emballe, et se trompe de carte"),
          QCoreApplication::translate("skills", "garde la tête froide, en général"),
          QCoreApplication::translate("skills", "ne se laisse pas bousculer sur la fin"),
          QCoreApplication::translate("skills", "joue la fin de manche comme le début")},
         {QCoreApplication::translate("rivals", "S'emballe dès que ça presse"),
          QCoreApplication::translate("rivals", "Garde la tête froide, en général"),
          QCoreApplication::translate("rivals", "Ne se laisse pas bousculer"),
          QCoreApplication::translate("rivals", "A tout vu passer, et rien ne l'émeut")}},
    };
    return list;
}

const QVector<LearnedSpec> &learnedSpecs() {
    static const QVector<LearnedSpec> list = {
        {QCoreApplication::translate("skills", "Lecture"), 206,
         {QCoreApplication::translate("skills", "lecture gloutonne"),
          QCoreApplication::translate("skills", "lecture exacte"),
          QCoreApplication::translate("skills", "compte avant d'ouvrir"),
          QCoreApplication::translate("skills", "voit à une carte près")},
         {QCoreApplication::translate("skills", "manque l'ouverture qui était là"),
          QCoreApplication::translate("skills", "lit sa main de toutes les façons à la fois"),
          QCoreApplication::translate("skills", "ne commence plus une pose qu'il reprendra"),
          QCoreApplication::translate("skills", "garde ce qui est à une carte d'être posable")}},

        {QCoreApplication::translate("skills", "Mémoire"), 268,
         {QCoreApplication::translate("skills", "rien"),
          QCoreApplication::translate("skills", "les dernières défausses"),
          QCoreApplication::translate("skills", "compte les exemplaires"),
          QCoreApplication::translate("skills", "retient vos prises")},
         {QCoreApplication::translate("skills", "oublie une carte dès qu'elle est passée"),
          QCoreApplication::translate("skills", "se souvient de ce qui vient d'être jeté"),
          QCoreApplication::translate("skills", "sait quelles cartes ne reviendront plus"),
          QCoreApplication::translate("skills", "ne vous redonne pas ce que vous ramassez")}},

        {QCoreApplication::translate("skills", "Défausse"), 174,
         {QCoreApplication::translate("skills", "jette le plus cher"),
          QCoreApplication::translate("skills", "pèse ce que la carte fait"),
          QCoreApplication::translate("skills", "ne nourrit pas le tapis"),
          QCoreApplication::translate("skills", "coupe les sorties")},
         {QCoreApplication::translate("skills", "jette les gros points, et ce qui servait"),
          QCoreApplication::translate("skills", "garde la carte qui travaille"),
          QCoreApplication::translate("skills", "ne vous donne plus ce qui va sur le tapis"),
          QCoreApplication::translate("skills", "voit venir une sortie et se vide")}},

        {QCoreApplication::translate("skills", "Maîtrise"), 146,
         {QCoreApplication::translate("skills", "bourdes fréquentes"),
          QCoreApplication::translate("skills", "bourdes rares"),
          QCoreApplication::translate("skills", "voit tout ce qui se pose"),
          QCoreApplication::translate("skills", "rachète les jokers")},
         {QCoreApplication::translate("skills", "joue la mauvaise carte un coup sur trois"),
          QCoreApplication::translate("skills", "ne se trompe presque plus"),
          QCoreApplication::translate("skills", "place chaque carte qui va sur le tapis"),
          QCoreApplication::translate("skills", "reprend un joker posé pour s'en servir")}},
    };
    return list;
}

// Points attendus au-delà des cinquante et un avant d'ouvrir, en fonction de
// l'audace. La borne est délibérément basse : un joueur qui attendrait trente
// points supplémentaires n'ouvrirait jamais, et une manche sans ouverture ne
// progresse pas.
constexpr int kOpeningSlack[kInnateTop + 1] = {0, 4, 10, 18};

// Effets de la prudence : la réticence à poser un joker, et la valeur
// attribuée aux cartes susceptibles de servir.
constexpr qreal kJokerGrip[kInnateTop + 1] = {0.0, 0.25, 0.60, 1.00};
constexpr qreal kKeepWeight[kInnateTop + 1] = {0.80, 1.00, 1.20, 1.40};

// Fiabilité d'exploitation de la mémoire, en fonction du calme. Un joueur peu
// calme dispose de la même mémoire mais ne l'exploite qu'une fois sur deux, ce
// qui correspond à un comptage des cartes effectué de façon irrégulière.
constexpr qreal kRecall[kInnateTop + 1] = {0.55, 0.75, 0.90, 1.00};

// Et son effet sur l'empressement à poser.
constexpr qreal kEagerness[kInnateTop + 1] = {1.00, 1.00, 0.85, 0.65};

} // namespace

// ---------------------------------------------------------------------------

bool Innates::operator==(const Innates &other) const {
    for (int i = 0; i < kInnateCount; ++i) {
        if (level[i] != other.level[i]) return false;
    }
    return true;
}

int Pools::total() const {
    int sum = 0;
    for (int i = 0; i < kLearnedCount; ++i) sum += qBound(0, xp[i], kPoolCeiling);
    return sum;
}

const InnateSpec &innateSpec(Innate which) {
    return innateSpecs().at(qBound(0, int(which), kInnateCount - 1));
}

const LearnedSpec &learnedSpec(Learned which) {
    return learnedSpecs().at(qBound(0, int(which), kLearnedCount - 1));
}

// ---------------------------------------------------------------------------
// Les rangs.

int rungFloor(int rung) { return kRungFloors[qBound(0, rung, kRungCount - 1)]; }

int rungOf(int poolXp) {
    const int xp = qBound(0, poolXp, kPoolCeiling);
    int rung = 0;
    for (int i = 0; i < kRungCount; ++i) {
        if (xp >= kRungFloors[i]) rung = i;
    }
    return rung;
}

int rungGap(int poolXp) {
    const int rung = rungOf(poolXp);
    if (rung >= kRungCount - 1) return 0;
    return std::max(0, kRungFloors[rung + 1] - qBound(0, poolXp, kPoolCeiling));
}

QString rungRoman(int rung) {
    switch (qBound(0, rung, kRungCount - 1)) {
    case 0: return QStringLiteral("I");
    case 1: return QStringLiteral("II");
    case 2: return QStringLiteral("III");
    default: return QStringLiteral("IV");
    }
}

qreal rungProgress(int poolXp) {
    const int rung = rungOf(poolXp);
    const int bottom = kRungFloors[rung];
    const int top = rung >= kRungCount - 1 ? kPoolCeiling : kRungFloors[rung + 1];
    if (top <= bottom) return 1.0;
    return qBound(0.0, qreal(qBound(bottom, poolXp, top) - bottom) / (top - bottom), 1.0);
}

qreal depthOf(int poolXp) {
    const int rung = rungOf(poolXp);
    return qBound(0.0, (rung + rungProgress(poolXp)) / kRungCount, 1.0);
}

// ---------------------------------------------------------------------------
// Le niveau.

Level levelOf(int xp) {
    if (xp >= 600) return Level::Expert;
    if (xp >= 200) return Level::Intermediaire;
    return Level::Debutant;
}

QString levelName(Level level) {
    switch (level) {
    case Level::Debutant: return QCoreApplication::translate("rivals", "Débutant");
    case Level::Intermediaire: return QCoreApplication::translate("rivals", "Intermédiaire");
    case Level::Expert: return QCoreApplication::translate("rivals", "Expert");
    }
    return QString();
}

int levelStars(Level level) {
    switch (level) {
    case Level::Debutant: return 1;
    case Level::Intermediaire: return 2;
    case Level::Expert: return 3;
    }
    return 1;
}

int levelFloor(Level level) {
    switch (level) {
    case Level::Debutant: return 0;
    case Level::Intermediaire: return 200;
    case Level::Expert: return 600;
    }
    return 0;
}

int levelCeiling(Level level) {
    switch (level) {
    case Level::Debutant: return 200;
    case Level::Intermediaire: return 600;
    case Level::Expert: return kMaxExperience;
    }
    return kMaxExperience;
}

qreal masteryOf(int xp) {
    return qBound(0.0, qreal(qBound(0, xp, kMaxExperience)) / kMaxExperience, 1.0);
}

qreal progressWithin(int xp) {
    const Level level = levelOf(xp);
    const int bottom = levelFloor(level);
    const int top = levelCeiling(level);
    if (top <= bottom) return 1.0;
    return qBound(0.0, qreal(qBound(bottom, xp, top) - bottom) / (top - bottom), 1.0);
}

// ---------------------------------------------------------------------------
// Ce que le cerveau joue.

Skill composeSkill(const Innates &innates, const Pools &pools) {
    const int total = pools.total();

    // Savoir-faire acquis, compétence par compétence. Chaque paramètre est
    // dérivé d'une seule des quatre acquises, ce qui justifie leur
    // distinction : la compétence responsable est identifiable et la fiche
    // l'indique.
    const int lecture = rungOf(pools.at(Learned::Lecture));
    const int memoire = rungOf(pools.at(Learned::Memoire));
    const int defausse = rungOf(pools.at(Learned::Defausse));
    const int maitrise = rungOf(pools.at(Learned::Maitrise));

    const qreal lectureDepth = depthOf(pools.at(Learned::Lecture));
    const qreal memoireDepth = depthOf(pools.at(Learned::Memoire));
    const qreal maitriseDepth = depthOf(pools.at(Learned::Maitrise));

    Skill skill;
    skill.mastery = masteryOf(total);

    skill.thorough = lecture >= 1;
    skill.countsOpening = lecture >= 2;
    skill.watches = memoire >= 2;
    skill.readsTakes = memoire >= 3;
    skill.guardsFeed = defausse >= 2;
    skill.buysJokers = maitrise >= 3;

    // Courbe raide au bas de l'échelle et plate au sommet : l'écart entre un
    // joueur débutant et un joueur ayant disputé dix manches doit être
    // perceptible, tandis que l'écart entre deux experts ne doit pas dépendre
    // du hasard.
    skill.blunder = 0.30 * std::pow(1.0 - maitriseDepth, 1.6);
    skill.layoffSight = 0.55 + 0.45 * maitriseDepth;
    skill.pileThreshold = 10.0 - 8.5 * memoireDepth;
    skill.potentialWeight = 0.22 + 0.78 * lectureDepth;

    // Traits de tempérament. Deux d'entre eux décalent un paramètre déjà fixé
    // par les rangs : la vigilance agit sur le seuil d'acceptation de la
    // défausse, le calme sur la fréquence des erreurs. Le décalage est borné à
    // la valeur d'un rang, de sorte qu'une innée ne puisse faire d'un débutant
    // un expert.
    const int vigilance = innates.at(Innate::Vigilance);
    const int calme = innates.at(Innate::Calme);
    static constexpr qreal kPileTilt[kInnateTop + 1] = {1.25, 1.00, 0.85, 0.70};
    static constexpr qreal kBlunderTilt[kInnateTop + 1] = {1.35, 1.00, 0.80, 0.60};
    static constexpr qreal kSightTilt[kInnateTop + 1] = {-0.12, 0.0, 0.06, 0.12};
    skill.pileThreshold = qBound(1.5, skill.pileThreshold * kPileTilt[vigilance], 10.0);
    skill.blunder = qBound(0.0, skill.blunder * kBlunderTilt[calme], 0.30);
    skill.layoffSight = qBound(0.55, skill.layoffSight + kSightTilt[vigilance], 1.0);

    skill.openingSlack = kOpeningSlack[innates.at(Innate::Audace)];
    skill.eagerness = kEagerness[innates.at(Innate::Audace)];
    skill.jokerGrip = kJokerGrip[innates.at(Innate::Prudence)];
    skill.keepWeight = kKeepWeight[innates.at(Innate::Prudence)];
    skill.recall = kRecall[calme];
    // L'adaptation du jeu à la fin de manche s'acquiert, mais une vigilance
    // élevée la procure également.
    skill.readsEndgame = defausse >= 3 || calme >= 2;
    return skill;
}

Skill bestSkill() {
    Pools full;
    for (int i = 0; i < kLearnedCount; ++i) full.xp[i] = kPoolCeiling;
    const Innates even;   // ordinaire partout: le Conseil n'a pas de caractère
    Skill skill = composeSkill(even, full);
    skill.blunder = 0.0;
    skill.layoffSight = 1.0;
    skill.recall = 1.0;
    skill.openingSlack = 0;
    skill.eagerness = 1.0;
    skill.jokerGrip = 0.0;
    skill.keepWeight = 1.0;
    skill.readsEndgame = true;
    return skill;
}

Pools poolsFromXp(int xp, const Innates &innates) {
    Pools pools;
    const int wanted = qBound(0, xp, kMaxExperience);
    if (wanted <= 0) return pools;

    // La répartition est pondérée selon le tempérament : un adversaire
    // vigilant débute avec davantage de mémoire que de capacité d'analyse. Les
    // poids restent modérés — au plus deux et demi contre un — s'agissant
    // d'une répartition d'une expérience donnée et non d'un bonus.
    qreal weights[kLearnedCount];
    qreal sum = 0.0;
    for (int i = 0; i < kLearnedCount; ++i) {
        weights[i] = 1.0 + 0.5 * innates.at(feeds(Learned(i)));
        sum += weights[i];
    }

    int given = 0;
    for (int i = 0; i < kLearnedCount; ++i) {
        const int share = qBound(0, int(qRound(wanted * weights[i] / sum)), kPoolCeiling);
        pools.xp[i] = share;
        given += share;
    }

    // Le reliquat dû aux arrondis et aux plafonds est reporté là où il reste
    // de la capacité, afin qu'aucune expérience antérieure ne soit perdue lors
    // de la conversion.
    int left = wanted - given;
    for (int round = 0; round < kLearnedCount && left != 0; ++round) {
        for (int i = 0; i < kLearnedCount && left != 0; ++i) {
            const int room = left > 0 ? kPoolCeiling - pools.xp[i] : -pools.xp[i];
            if (room == 0) continue;
            const int step = left > 0 ? std::min(left, room) : std::max(left, room);
            pools.xp[i] += step;
            left -= step;
        }
    }
    return pools;
}

qreal learnRate(const Innates &innates, Learned which) {
    return 1.0 + 0.15 * innates.at(feeds(which));
}

qreal learnTaper(int xp) {
    const int seen = qBound(0, xp, kMaxExperience);
    if (seen <= kLearnFullUntil) return 1.0;
    // Décroissance hyperbolique en fonction de l'expérience déjà acquise,
    // plutôt que par paliers, afin qu'aucune manche ne marque une rupture
    // perceptible du rythme de progression. Le coefficient n'atteint jamais
    // zéro : un plafond inatteignable ne serait plus un plafond.
    return qreal(kLearnFullUntil) / seen;
}

Innate dominantInnate(const Innates &innates) {
    // L'innée dont le niveau s'écarte le plus de la valeur médiane : les
    // niveaux zéro et trois sont également marquants, les niveaux un et deux
    // le sont peu. En cas d'égalité, le niveau le plus élevé est retenu, un
    // excès étant plus perceptible qu'un défaut.
    int best = 0;
    qreal far = -1.0;
    for (int i = 0; i < kInnateCount; ++i) {
        const qreal gap = std::abs(innates.level[i] - 1.5);
        if (gap > far + 0.0001 ||
            (std::abs(gap - far) <= 0.0001 && innates.level[i] > innates.level[best])) {
            far = gap;
            best = i;
        }
    }
    return Innate(best);
}

QString blurbFrom(const Innates &innates) {
    const Innate which = dominantInnate(innates);
    return innateSpec(which).blurbs[qBound(0, innates.at(which), kInnateTop)];
}
