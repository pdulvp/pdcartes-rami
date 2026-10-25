#include "Rivals.h"

#include "Profiles.h"

#include <QCoreApplication>
#include <QSettings>

#include <algorithm>

namespace {

const QVector<Rival> &builtRivals() {
    // Six adversaires, de sorte que la table la plus grande soit complète.
    // Leurs niveaux initiaux sont volontairement étalés : la première table à
    // laquelle s'assied un nouveau joueur doit comporter des adversaires qu'il
    // puisse battre et d'autres qu'il ne puisse pas.
    //
    // Les quatre niveaux innés sont donnés dans l'ordre audace, prudence,
    // vigilance, calme, et la phrase descriptive de chaque adversaire en
    // découle : Lulu est audacieux, ce qui explique qu'il ouvre tardivement ;
    // Nestor est prudent, ce qui explique qu'il conserve ses jokers jusqu'à la
    // fin. Modifier ces niveaux produit un joueur différent, ce que la fiche
    // signale et sanctionne par la perte de l'expérience.
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

// Clé sous laquelle chaque compétence est enregistrée. Les clés sont nommées
// et non numérotées, afin qu'un fichier de réglages reste lisible et
// modifiable manuellement.
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
    // Aucune des quatre valeurs n'est enregistrée : il s'agit d'un profil
    // antérieur à l'introduction des compétences, ou d'un adversaire n'ayant
    // pas encore joué. Dans les deux cas, l'expérience disponible est répartie
    // sur les quatre compétences selon le tempérament, sans perte.
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

// Répartit le gain d'expérience d'une manche sur les quatre compétences.
//
// Cette fonction traite les gains qui ne se rattachent à aucune compétence
// précise — le temps passé en partie, une partie gagnée — en les répartissant
// au prorata des vitesses d'apprentissage. Le total reste celui qui
// s'appliquait avant l'introduction des compétences : la courbe de
// progression sur laquelle le jeu est calibré est inchangée, seule sa
// ventilation diffère.
void spreadGain(QSettings &settings, int slot, const Rival &who, const Innates &innate,
                Pools pools, int gained, const Deeds &deeds) {
    if (gained == 0) return;

    qreal weights[kLearnedCount];
    qreal sum = 0.0;
    for (int i = 0; i < kLearnedCount; ++i) {
        // Pondération combinant la vitesse d'apprentissage et les actions
        // effectivement accomplies. En l'absence d'action, les quatre
        // compétences se partagent le gain au prorata du tempérament ; dès
        // qu'une action a eu lieu, le gain est affecté majoritairement à la
        // compétence correspondante.
        weights[i] = learnRate(innate, Learned(i)) * (1.0 + deeds.at(Learned(i)));
        sum += weights[i];
    }

    int given = 0;
    int shares[kLearnedCount];
    for (int i = 0; i < kLearnedCount; ++i) {
        shares[i] = int(qRound(gained * weights[i] / sum));
        given += shares[i];
    }
    // Le reliquat d'arrondi est attribué à la compétence dont la vitesse
    // d'apprentissage est la plus élevée.
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
    // Aucun changement effectif, aucune perte : ouvrir puis refermer
    // l'éditeur ne doit pas entraîner la perte de l'expérience.
    if (rivalInnate(who.id, slot) == innate) return;

    QSettings settings;
    for (int i = 0; i < kInnateCount; ++i) {
        const Innate which = Innate(i);
        settings.setValue(innateKey(slot, who.id, which), innate.at(which));
    }
    settings.sync();
    // L'expérience acquise est annulée, ayant été obtenue avec un tempérament
    // différent.
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
    // Restituer le nom d'origine équivaut à n'avoir attribué aucun nom : la
    // clé est supprimée plutôt que réécrite à l'identique, afin que la
    // question de savoir si l'adversaire a été renommé admette une réponse
    // unique.
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

    // Toute manche disputée rapporte un gain minimal, indépendamment de son
    // déroulement, et un bon classement rapporte davantage. La courbe qui en
    // résulte est volontairement progressive : un débutant doit disputer trois
    // ou quatre parties avant de changer de niveau, et non une seule manche.
    const int others = std::max(1, seats - 1);
    const int standing = qBound(0, others - qBound(0, place, others), others);
    int gained = 5 + (9 * standing) / others;
    if (wentOut) gained += 8;

    // Le gain est ensuite pondéré par l'expérience déjà acquise. Sans cette
    // pondération, le gain était constant : dix parties à six joueurs
    // suffisaient à amener les six au plafond, la manche d'un adversaire à
    // mille points rapportant autant que celle d'un débutant. Le plancher d'un
    // point maintient le principe énoncé plus haut jusqu'au bout : toute
    // manche disputée rapporte, et le dernier rang reste atteignable.
    gained = std::max(1, qRound(gained * learnTaper(learning.xp)));

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
    // Gagner une partie complète rapporte un peu plus que gagner une manche,
    // et c'est le seul cas où l'expérience varie en dehors d'une manche.
    if (!won) return;
    // Une partie gagnée ne se rattache à aucune compétence particulière. Le
    // gain est pondéré comme celui d'une manche, et pour la même raison : il
    // s'agit de la même expérience.
    const int prize = std::max(1, qRound(12 * learnTaper(learning.xp)));
    spreadGain(settings, slot, who, rivalInnate(who.id, slot), learning.pools, prize, Deeds{});
}

void resetRivalExperience(const QString &rivalId) {
    const Rival &who = rivalById(rivalId);
    const int slot = currentProfile();
    QSettings settings;
    // Sont réinitialisés : les quatre expériences, le palmarès, et l'ancienne
    // valeur unique si elle subsiste. Le tempérament et le nom attribué ne
    // relèvent pas de l'expérience : la réinitialisation ne renomme pas
    // l'adversaire et ne modifie pas son tempérament.
    settings.remove(rivalKey(slot, who.id, QStringLiteral("acquis")));
    settings.remove(rivalKey(slot, who.id, QStringLiteral("xp")));
    settings.remove(rivalKey(slot, who.id, QStringLiteral("manches")));
    settings.remove(rivalKey(slot, who.id, QStringLiteral("manchesGagnees")));
    settings.remove(rivalKey(slot, who.id, QStringLiteral("parties")));
    settings.remove(rivalKey(slot, who.id, QStringLiteral("partiesGagnees")));
}
