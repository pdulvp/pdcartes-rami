#pragma once

#include <QString>

// Les trois profils.
//
// Une même machine peut servir à plusieurs joueurs, ou à un joueur souhaitant
// séparer plusieurs séries de parties. Un profil est le nom sous lequel une
// partie est jouée et, surtout, l'emplacement où est conservée l'expérience
// des adversaires : un adversaire affronté quarante fois sur le premier profil
// est distinct de son homonyme sur le deuxième, et changer de profil est le
// seul moyen de retrouver une table de débutants.
inline constexpr int kProfileCount = 3;

// Longueur maximale d'un nom de profil, déterminée par la largeur des plaques
// du menu d'ouverture. Un nom plus long y serait tronqué et ne permettrait
// plus d'identifier le profil.
inline constexpr int kProfileNameLength = 24;

struct ProfileRecord {
    int slot = 1;
    QString name;
    int matchesPlayed = 0;
    int matchesWon = 0;
    int handsPlayed = 0;
    int handsWon = 0;
    int bestScore = -1;      // the fewest points a won game was won with
};

int currentProfile();
void setCurrentProfile(int slot);

QString profileName(int slot);
void setProfileName(int slot, const QString &name);

ProfileRecord profileRecord(int slot);

// Enregistré au fil de la partie et non à son terme, afin qu'une session
// interrompue anormalement conserve le décompte des parties jouées.
void noteHandPlayed(bool won);
void noteMatchPlayed(bool won, int score);

// Réinitialise l'intégralité du profil : son propre décompte et l'expérience
// des six adversaires, ramenés à leur état initial.
void forgetProfile(int slot);

// Emplacement d'une donnée de profil dans le fichier de réglages. La
// composition de la clé est définie en un seul endroit, afin que la lecture et
// l'écriture ne puissent diverger.
QString profileKey(int slot, const QString &suffix);
