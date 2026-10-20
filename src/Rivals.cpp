#include "Rivals.h"

#include "Profiles.h"

#include <QCoreApplication>
#include <QSettings>

#include <algorithm>

namespace {

const QVector<Rival> &builtRivals() {
    // Six, so that the biggest table is full and everybody has a seat. The
    // levels they start at are spread on purpose: the first table a new
    // player sits at should have somebody on it they can beat and somebody on
    // it they cannot.
    //
    // Les quatre niveaux innés sont dans l'ordre audace, prudence, vigilance,
    // calme, et c'est d'eux que leur phrase parle: Lulu est un casse-cou, et
    // c'est pour cela qu'il pose tard; Nestor est économe, et c'est pour cela
    // qu'il meurt avec ses jokers en main. Les changer, c'est faire un autre
    // joueur -- la fiche le dit, et le fait payer.
    static const QVector<Rival> list = {
        {QStringLiteral("lulu"), QCoreApplication::translate("rivals", "Lulu"),
         QCoreApplication::translate("rivals", "Joue vite, pose tard, et s'en moque"), 196, 0, 4,
         Innates{{3, 0, 0, 1}}},
        {QStringLiteral("nestor"), QCoreApplication::translate("rivals", "Nestor"),
         QCoreApplication::translate("rivals", "Garde ses jokers jusqu'à la fin, hélas"), 34, 70,
         3, Innates{{1, 3, 1, 1}}},
        {QStringLiteral("zephyr"), QCoreApplication::translate("rivals", "Zéphyr"),
         QCoreApplication::translate("rivals", "A le nez sur la défausse"), 268, 230, 5,
         Innates{{2, 1, 3, 1}}},
        {QStringLiteral("prune"), QCoreApplication::translate("rivals", "Prune"),
         QCoreApplication::translate("rivals", "Compte les cartes, la plupart du temps"), 328, 330,
         1, Innates{{1, 1, 2, 1}}},
        {QStringLiteral("barnabe"), QCoreApplication::translate("rivals", "Barnabé"),
         QCoreApplication::translate("rivals", "Ne jette jamais ce qui peut servir"), 140, 440, 2,
         Innates{{1, 2, 1, 2}}},
        {QStringLiteral("oriane"), QCoreApplication::translate("rivals", "Oriane"),
         QCoreApplication::translate("rivals", "A tout vu passer, et tout retenu"), 352, 660, 0,
         Innates{{2, 2, 2, 3}}},
    };
    return list;
}

QString rivalKey(int slot, const QString &rivalId, const QString &what) {
    return profileKey(slot, QStringLiteral("rival/%1/%2").arg(rivalId, what));
}

// Le nom sous lequel chaque compétence est rangée. Écrit plutôt que numéroté:
// un réglage qu'on peut lire est un réglage qu'on peut réparer à la main.
const char *innateName(Innate which) {
    switch (which) {
    case Innate::Audace: return "audace";
    case Innate::Prudence: return "prudence";
    case Innate::Vigilance: return "vigilance";
    case Innate::Calme: return "calme";
    }
    return "audace";
}

const char *learnedName(Learned which) {
    switch (which) {
    case Learned::Lecture: return "lecture";
    case Learned::Memoire: return "memoire";
    case Learned::Defausse: return "defausse";
    case Learned::Maitrise: return "maitrise";
    }
    return "lecture";
}

QString innateKey(int slot, const QString &rivalId, Innate which) {
    return rivalKey(slot, rivalId, QStringLiteral("inne/%1").arg(QLatin1String(innateName(which))));
}

QString poolKey(int slot, const QString &rivalId, Learned which) {
    return rivalKey(slot, rivalId,
                    QStringLiteral("acquis/%1").arg(QLatin1String(learnedName(which))));
}

Innates readInnate(const QSettings &settings, int slot, const Rival &who) {
    Innates innate = who.innate;
    for (int i = 0; i < kInnateCount; ++i) {
        const Innate which = Innate(i);
        innate.set(which, settings.value(innateKey(slot, who.id, which), innate.at(which)).toInt());
    }
    return innate;
}

Pools readPools(const QSettings &settings, int slot, const Rival &who, const Innates &innate) {
    // Rien d'écrit dans les quatre, c'est un profil d'avant les compétences --
    // ou un adversaire qui n'a pas encore joué. Dans les deux cas l'expérience
    // qu'il a se répartit sur les quatre, penchée du côté de ce qu'il est, et
    // sans qu'un seul point se perde.
    bool known = false;
    for (int i = 0; i < kLearnedCount && !known; ++i) {
        known = settings.contains(poolKey(slot, who.id, Learned(i)));
    }
    if (!known) {
        const int xp = settings.value(rivalKey(slot, who.id, QStringLiteral("xp")), who.startXp)
                           .toInt();
        return poolsFromXp(xp, innate);
    }

    Pools pools;
    for (int i = 0; i < kLearnedCount; ++i) {
        const Learned which = Learned(i);
        pools.set(which, settings.value(poolKey(slot, who.id, which), 0).toInt());
    }
    return pools;
}

// Ce qu'une manche rapporte, posé sur les quatre compétences à la fois.
//
// C'est le filet de l'ensemble: tout ce qui n'a pas de compétence précise à
// créditer -- le temps passé à la table, une partie gagnée -- se répartit au
// prorata de ce qu'il apprend vite. La somme reste celle d'avant les
// compétences, parce que la courbe de progression sur laquelle le jeu est
// réglé n'a aucune raison de changer: elle est seulement répartie.
void spreadGain(QSettings &settings, int slot, const Rival &who, const Innates &innate,
                Pools pools, int gained, const Deeds &deeds) {
    if (gained == 0) return;

    qreal weights[kLearnedCount];
    qreal sum = 0.0;
    for (int i = 0; i < kLearnedCount; ++i) {
        // Ce qu'il apprend vite, et ce qu'il vient de faire. Sans rien de
        // fait, les quatre se partagent la manche au prorata de son
        // caractère; dès qu'il s'est passé quelque chose, la manche va
        // surtout là.
        weights[i] = learnRate(innate, Learned(i)) * (1.0 + deeds.at(Learned(i)));
        sum += weights[i];
    }

    int given = 0;
    int shares[kLearnedCount];
    for (int i = 0; i < kLearnedCount; ++i) {
        shares[i] = int(qRound(gained * weights[i] / sum));
        given += shares[i];
    }
    // L'arrondi, rendu à la compétence qui apprend le plus vite.
    int left = gained - given;
    int fastest = 0;
    for (int i = 1; i < kLearnedCount; ++i) {
        if (weights[i] > weights[fastest]) fastest = i;
    }
    shares[fastest] += left;

    for (int i = 0; i < kLearnedCount; ++i) {
        const Learned which = Learned(i);
        pools.set(which, pools.at(which) + shares[i]);
        settings.setValue(poolKey(slot, who.id, which), pools.at(which));
    }
}

} // namespace

const QVector<Rival> &rivals() { return builtRivals(); }

const Rival &rivalById(const QString &id) {
    for (const Rival &rival : builtRivals()) {
        if (rival.id == id) return rival;
    }
    return builtRivals().first();
}

Learning learningFor(const QString &rivalId, int slot) {
    const Rival &who = rivalById(rivalId);
    QSettings settings;
    Learning learning;
    learning.pools = readPools(settings, slot, who, readInnate(settings, slot, who));
    learning.xp = learning.pools.total();
    learning.handsPlayed =
        settings.value(rivalKey(slot, who.id, QStringLiteral("manches")), 0).toInt();
    learning.handsWon =
        settings.value(rivalKey(slot, who.id, QStringLiteral("manchesGagnees")), 0).toInt();
    learning.matchesPlayed =
        settings.value(rivalKey(slot, who.id, QStringLiteral("parties")), 0).toInt();
    learning.matchesWon =
        settings.value(rivalKey(slot, who.id, QStringLiteral("partiesGagnees")), 0).toInt();
    return learning;
}

Learning learningFor(const QString &rivalId) {
    return learningFor(rivalId, currentProfile());
}

Innates rivalInnate(const QString &rivalId, int slot) {
    QSettings settings;
    return readInnate(settings, slot, rivalById(rivalId));
}

Innates rivalInnate(const QString &rivalId) {
    return rivalInnate(rivalId, currentProfile());
}

void setRivalInnate(const QString &rivalId, const Innates &innate) {
    const Rival &who = rivalById(rivalId);
    const int slot = currentProfile();
    // Rien qui change, rien qui se perd: ouvrir l'éditeur et le refermer ne
    // doit pas coûter une expérience.
    if (rivalInnate(who.id, slot) == innate) return;

    QSettings settings;
    for (int i = 0; i < kInnateCount; ++i) {
        const Innate which = Innate(i);
        settings.setValue(innateKey(slot, who.id, which), innate.at(which));
    }
    settings.sync();
    // Et ce qu'il avait appris s'en va, parce qu'il l'avait appris en étant
    // quelqu'un d'autre.
    resetRivalExperience(who.id);
}

void resetRivalInnate(const QString &rivalId) {
    setRivalInnate(rivalId, rivalById(rivalId).innate);
}

QString rivalBlurb(const QString &rivalId) {
    const Rival &who = rivalById(rivalId);
    const Innates innate = rivalInnate(who.id);
    return innate == who.innate ? who.blurb : blurbFrom(innate);
}

Skill skillFor(const QString &rivalId, int slot) {
    const Rival &who = rivalById(rivalId);
    QSettings settings;
    const Innates innate = readInnate(settings, slot, who);
    return composeSkill(innate, readPools(settings, slot, who, innate));
}

Skill skillFor(const QString &rivalId) { return skillFor(rivalId, currentProfile()); }

QString rivalName(const QString &rivalId, int slot) {
    const Rival &who = rivalById(rivalId);
    const QString called =
        QSettings().value(rivalKey(slot, who.id, QStringLiteral("nom"))).toString().trimmed();
    return called.isEmpty() ? who.name : called;
}

QString rivalName(const QString &rivalId) { return rivalName(rivalId, currentProfile()); }

void setRivalName(const QString &rivalId, const QString &name) {
    const Rival &who = rivalById(rivalId);
    const QString called = name.trimmed().left(kRivalNameLength);
    // Lui rendre son nom d'origine, c'est n'en avoir donné aucun: on efface
    // plutôt que d'écrire la même chose, pour que « a-t-il été rebaptisé »
    // ait une réponse et une seule.
    if (called.isEmpty() || called == who.name) {
        clearRivalName(rivalId);
        return;
    }
    QSettings().setValue(rivalKey(currentProfile(), who.id, QStringLiteral("nom")), called);
}

void clearRivalName(const QString &rivalId) {
    const Rival &who = rivalById(rivalId);
    QSettings().remove(rivalKey(currentProfile(), who.id, QStringLiteral("nom")));
}


void noteRivalHand(const QString &rivalId, int place, int seats, bool wentOut,
                   const Deeds &deeds) {
    const Rival &who = rivalById(rivalId);
    const int slot = currentProfile();
    const Learning learning = learningFor(who.id, slot);

    // A manche sat through is worth a little whatever happened in it -- time
    // at the table is time at the table -- and coming out well is worth more.
    // The curve this makes is deliberately gentle: a beginner should take
    // three or four games to stop being one, not one hand.
    const int others = std::max(1, seats - 1);
    const int standing = qBound(0, others - qBound(0, place, others), others);
    int gained = 5 + (9 * standing) / others;
    if (wentOut) gained += 8;

    QSettings settings;
    spreadGain(settings, slot, who, rivalInnate(who.id, slot), learning.pools, gained, deeds);
    settings.setValue(rivalKey(slot, who.id, QStringLiteral("manches")), learning.handsPlayed + 1);
    if (wentOut) {
        settings.setValue(rivalKey(slot, who.id, QStringLiteral("manchesGagnees")),
                          learning.handsWon + 1);
    }
}

void noteRivalMatch(const QString &rivalId, bool won) {
    const Rival &who = rivalById(rivalId);
    const int slot = currentProfile();
    const Learning learning = learningFor(who.id, slot);

    QSettings settings;
    settings.setValue(rivalKey(slot, who.id, QStringLiteral("parties")),
                      learning.matchesPlayed + 1);
    if (won) {
        settings.setValue(rivalKey(slot, who.id, QStringLiteral("partiesGagnees")),
                          learning.matchesWon + 1);
    }
    // Winning a whole game is worth a little more than winning a manche of
    // it, and it is the only place the figure moves outside a manche.
    if (!won) return;
    // Une partie gagnée est une partie gagnée: elle ne crédite pas une
    // compétence en particulier.
    spreadGain(settings, slot, who, rivalInnate(who.id, slot), learning.pools, 12, Deeds{});
}

void resetRivalExperience(const QString &rivalId) {
    const Rival &who = rivalById(rivalId);
    const int slot = currentProfile();
    QSettings settings;
    // Les quatre expériences, le palmarès, et l'ancien chiffre unique s'il
    // traîne encore. Ce qu'il est et le nom qu'on lui a donné ne sont pas de
    // l'expérience: les remettre au niveau de départ ne le débaptise pas et
    // ne le change pas de joueur.
    settings.remove(rivalKey(slot, who.id, QStringLiteral("acquis")));
    settings.remove(rivalKey(slot, who.id, QStringLiteral("xp")));
    settings.remove(rivalKey(slot, who.id, QStringLiteral("manches")));
    settings.remove(rivalKey(slot, who.id, QStringLiteral("manchesGagnees")));
    settings.remove(rivalKey(slot, who.id, QStringLiteral("parties")));
    settings.remove(rivalKey(slot, who.id, QStringLiteral("partiesGagnees")));
}
