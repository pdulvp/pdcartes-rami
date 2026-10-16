#include "Profiles.h"

#include <QCoreApplication>
#include <QHash>
#include <QSettings>

namespace {

int clampSlot(int slot) { return qBound(1, slot, kProfileCount); }

} // namespace

QString profileKey(int slot, const QString &suffix) {
    return QStringLiteral("profil/%1/%2").arg(clampSlot(slot)).arg(suffix);
}

int currentProfile() {
    QSettings settings;
    return clampSlot(settings.value(QStringLiteral("profil/courant"), 1).toInt());
}

void setCurrentProfile(int slot) {
    QSettings settings;
    settings.setValue(QStringLiteral("profil/courant"), clampSlot(slot));
}

QString profileName(int slot) {
    QSettings settings;
    const QString kept = settings.value(profileKey(slot, QStringLiteral("nom"))).toString();
    if (!kept.trimmed().isEmpty()) return kept.trimmed();
    return QCoreApplication::translate("profiles", "Profil %1").arg(clampSlot(slot));
}

void setProfileName(int slot, const QString &name) {
    QSettings settings;
    settings.setValue(profileKey(slot, QStringLiteral("nom")), name.trimmed());
}

ProfileRecord profileRecord(int slot) {
    QSettings settings;
    ProfileRecord record;
    record.slot = clampSlot(slot);
    record.name = profileName(slot);
    record.matchesPlayed = settings.value(profileKey(slot, QStringLiteral("parties")), 0).toInt();
    record.matchesWon = settings.value(profileKey(slot, QStringLiteral("gagnees")), 0).toInt();
    record.handsPlayed = settings.value(profileKey(slot, QStringLiteral("manches")), 0).toInt();
    record.handsWon = settings.value(profileKey(slot, QStringLiteral("manchesGagnees")), 0).toInt();
    record.bestScore = settings.value(profileKey(slot, QStringLiteral("meilleur")), -1).toInt();
    return record;
}

void noteHandPlayed(bool won) {
    const int slot = currentProfile();
    QSettings settings;
    settings.setValue(profileKey(slot, QStringLiteral("manches")),
                      settings.value(profileKey(slot, QStringLiteral("manches")), 0).toInt() + 1);
    if (!won) return;
    settings.setValue(
        profileKey(slot, QStringLiteral("manchesGagnees")),
        settings.value(profileKey(slot, QStringLiteral("manchesGagnees")), 0).toInt() + 1);
}

void noteMatchPlayed(bool won, int score) {
    const int slot = currentProfile();
    QSettings settings;
    settings.setValue(profileKey(slot, QStringLiteral("parties")),
                      settings.value(profileKey(slot, QStringLiteral("parties")), 0).toInt() + 1);
    if (!won) return;
    settings.setValue(profileKey(slot, QStringLiteral("gagnees")),
                      settings.value(profileKey(slot, QStringLiteral("gagnees")), 0).toInt() + 1);

    // The fewest points a game was ever won with, which is the only record
    // worth keeping in a game where the low score wins.
    const int standing = settings.value(profileKey(slot, QStringLiteral("meilleur")), -1).toInt();
    if (standing < 0 || score < standing) {
        settings.setValue(profileKey(slot, QStringLiteral("meilleur")), score);
    }
}

void forgetProfile(int slot) {
    const int at = clampSlot(slot);
    QSettings settings;

    // Les noms reviennent après: celui du profil, et ceux qu'on a donnés aux
    // adversaires. Effacer ses parties n'est pas débaptiser la table, et la
    // question posée avant d'arriver ici ne parle que de parties et de
    // niveaux.
    const QString kept = settings.value(profileKey(at, QStringLiteral("nom"))).toString();

    // Relevés à la clé plutôt que demandés aux adversaires: les profils n'ont
    // pas à savoir qui sont les six.
    const QString branch = QStringLiteral("profil/%1/rival").arg(at);
    QHash<QString, QString> called;
    settings.beginGroup(branch);
    for (const QString &key : settings.allKeys()) {
        if (!key.endsWith(QLatin1String("/nom"))) continue;
        const QString name = settings.value(key).toString();
        if (!name.trimmed().isEmpty()) called.insert(key, name);
    }
    settings.endGroup();

    // The rivals' experience is kept under the profile too, and an absent
    // figure means "the level this one starts at" -- so taking the whole
    // branch away is what putting the six of them back to how they were
    // actually is.
    settings.remove(QStringLiteral("profil/%1").arg(at));

    if (!kept.trimmed().isEmpty()) {
        settings.setValue(profileKey(at, QStringLiteral("nom")), kept.trimmed());
    }
    settings.beginGroup(branch);
    for (auto it = called.constBegin(); it != called.constEnd(); ++it) {
        settings.setValue(it.key(), it.value());
    }
    settings.endGroup();
}
