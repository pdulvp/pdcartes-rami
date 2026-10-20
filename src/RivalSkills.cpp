#include "RivalSkills.h"

#include <QCoreApplication>

#include <algorithm>
#include <cmath>

// Les huit compétences, et la seule fonction qui décide ce qu'elles valent.
//
// Tout l'équilibre du jeu tient dans composeSkill, et il tient en une phrase:
// les rangs portent la force, les innées portent la saveur. Un levier est tiré
// d'une seule compétence acquise -- la bourde de la Maîtrise, le seuil de la
// défausse de la Mémoire -- et une innée ne fait que le décaler, d'au plus ce
// qu'un rang vaut, borné sur place. C'est ce qui permet de régler un
// adversaire sans jamais lui faire dépasser le meilleur joueur que le jeu
// sache faire, ni descendre sous le débutant qu'il était.

namespace {

// Ce qu'il faut d'expérience pour chaque rang d'une compétence. Le premier est
// acquis d'emblée.
constexpr int kRungFloors[kRungCount] = {0, 60, 140, 230};

// Ce que chaque acquise apprend de lui. Une innée par acquise, pour que
// chacune des quatre serve à quelque chose et qu'on puisse dire laquelle:
// l'audace fait lire sa main, la prudence fait choisir ce qu'on donne, la
// vigilance fait retenir, le calme fait le geste juste.
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

// Ce que l'audace fait attendre avant d'ouvrir. Borné court exprès: un
// casse-cou qui attendrait trente points de plus ne poserait jamais, et une
// manche où personne n'ouvre n'est pas une manche.
constexpr int kOpeningSlack[kInnateTop + 1] = {0, 4, 10, 18};

// Ce que la prudence fait tenir: la réticence à dépenser un joker, et le prix
// qu'on met à ce qui pourrait servir.
constexpr qreal kJokerGrip[kInnateTop + 1] = {0.0, 0.25, 0.60, 1.00};
constexpr qreal kKeepWeight[kInnateTop + 1] = {0.80, 1.00, 1.20, 1.40};

// Ce que le calme rend de mémoire. Un soupe-au-lait a la mémoire qu'il a: il
// ne s'en sert qu'une fois sur deux, ce qui est très exactement « compte les
// cartes, la plupart du temps ».
constexpr qreal kRecall[kInnateTop + 1] = {0.55, 0.75, 0.90, 1.00};

// Et ce qu'elle retient de poser tout de suite.
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

    // Ce qu'il a appris, compétence par compétence. Chaque levier est tiré
    // d'une seule des quatre -- ce qui est la raison d'être des quatre: on
    // peut dire laquelle, et la fiche le montre.
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

    // Steep at the bottom and flat at the top: the difference between a brand
    // new player and one who has sat through ten manches should be the thing
    // you notice, and the difference between two experts should not be luck.
    skill.blunder = 0.30 * std::pow(1.0 - maitriseDepth, 1.6);
    skill.layoffSight = 0.55 + 0.45 * maitriseDepth;
    skill.pileThreshold = 10.0 - 8.5 * memoireDepth;
    skill.potentialWeight = 0.22 + 0.78 * lectureDepth;

    // Ce qu'il est. Deux d'entre elles décalent un levier que les rangs ont
    // déjà posé -- la vigilance pèse sur la défausse, le calme sur la bourde
    // -- et le décalage est borné à ce qu'un rang vaut, jamais plus: une
    // innée n'a pas le droit de faire d'un débutant un expert.
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
    // Jouer la fin de manche comme une fin de manche s'apprend -- et certains
    // l'ont de naissance.
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

    // Penché du côté de ce qu'il est: celui qui est aux aguets arrive avec de
    // la mémoire plutôt qu'avec de la lecture. Les poids sont doux -- deux et
    // demi contre un au plus -- parce que c'est une répartition, pas un don.
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

    // Ce que l'arrondi et les plafonds ont laissé de côté, posé là où il reste
    // de la place: l'expérience d'avant ne doit pas s'évaporer en chemin.
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
    // La part qu'il a déjà, renversée: une hyperbole plutôt qu'un palier, pour
    // qu'il n'y ait pas une manche où l'on sent que le tarif a changé. Elle
    // n'atteint jamais zéro -- un plafond qu'on ne peut plus approcher n'est
    // pas un plafond, c'est un mur.
    return qreal(kLearnFullUntil) / seen;
}

Innate dominantInnate(const Innates &innates) {
    // Celle qui s'écarte le plus du milieu -- zéro et trois frappent autant,
    // un et deux presque pas. À égalité, le niveau le plus haut, parce qu'un
    // excès se remarque plus qu'un manque.
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
