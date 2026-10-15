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
    static const QVector<Rival> list = {
        {QStringLiteral("lulu"), QCoreApplication::translate("rivals", "Lulu"),
         QCoreApplication::translate("rivals", "Joue vite, pose tard, et s'en moque"), 196, 0, 4},
        {QStringLiteral("nestor"), QCoreApplication::translate("rivals", "Nestor"),
         QCoreApplication::translate("rivals", "Garde ses jokers jusqu'à la fin, hélas"), 34, 70,
         3},
        {QStringLiteral("zephyr"), QCoreApplication::translate("rivals", "Zéphyr"),
         QCoreApplication::translate("rivals", "A le nez sur la défausse"), 268, 230, 5},
        {QStringLiteral("prune"), QCoreApplication::translate("rivals", "Prune"),
         QCoreApplication::translate("rivals", "Compte les cartes, la plupart du temps"), 328, 330,
         1},
        {QStringLiteral("barnabe"), QCoreApplication::translate("rivals", "Barnabé"),
         QCoreApplication::translate("rivals", "Ne jette jamais ce qui peut servir"), 140, 440, 2},
        {QStringLiteral("oriane"), QCoreApplication::translate("rivals", "Oriane"),
         QCoreApplication::translate("rivals", "A tout vu passer, et tout retenu"), 352, 660, 0},
    };
    return list;
}

QString rivalKey(int slot, const QString &rivalId, const QString &what) {
    return profileKey(slot, QStringLiteral("rival/%1/%2").arg(rivalId, what));
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
    // No figure written down means this one has not played under this profile
    // yet, and what they know then is what they sat down with.
    learning.xp = settings.value(rivalKey(slot, who.id, QStringLiteral("xp")), who.startXp).toInt();
    learning.xp = qBound(0, learning.xp, kMaxExperience);
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

void noteRivalHand(const QString &rivalId, int place, int seats, bool wentOut) {
    const Rival &who = rivalById(rivalId);
    const int slot = currentProfile();
    Learning learning = learningFor(who.id, slot);

    // A manche sat through is worth a little whatever happened in it -- time
    // at the table is time at the table -- and coming out well is worth more.
    // The curve this makes is deliberately gentle: a beginner should take
    // three or four games to stop being one, not one hand.
    const int others = std::max(1, seats - 1);
    const int standing = qBound(0, others - qBound(0, place, others), others);
    int gained = 5 + (9 * standing) / others;
    if (wentOut) gained += 8;

    learning.xp = qBound(0, learning.xp + gained, kMaxExperience);
    learning.handsPlayed += 1;
    if (wentOut) learning.handsWon += 1;

    QSettings settings;
    settings.setValue(rivalKey(slot, who.id, QStringLiteral("xp")), learning.xp);
    settings.setValue(rivalKey(slot, who.id, QStringLiteral("manches")), learning.handsPlayed);
    settings.setValue(rivalKey(slot, who.id, QStringLiteral("manchesGagnees")),
                      learning.handsWon);
}

void noteRivalMatch(const QString &rivalId, bool won) {
    const Rival &who = rivalById(rivalId);
    const int slot = currentProfile();
    Learning learning = learningFor(who.id, slot);
    learning.matchesPlayed += 1;
    if (won) learning.matchesWon += 1;

    QSettings settings;
    settings.setValue(rivalKey(slot, who.id, QStringLiteral("parties")), learning.matchesPlayed);
    settings.setValue(rivalKey(slot, who.id, QStringLiteral("partiesGagnees")),
                      learning.matchesWon);
    // Winning a whole game is worth a little more than winning a manche of
    // it, and it is the only place the figure moves outside a manche.
    if (!won) return;
    settings.setValue(rivalKey(slot, who.id, QStringLiteral("xp")),
                      qBound(0, learning.xp + 12, kMaxExperience));
}

void resetRival(const QString &rivalId) {
    const Rival &who = rivalById(rivalId);
    const int slot = currentProfile();
    QSettings settings;
    settings.remove(QStringLiteral("profil/%1/rival/%2").arg(slot).arg(who.id));
}

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
