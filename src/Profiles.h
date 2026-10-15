#pragma once

#include <QString>

// Les trois profils.
//
// One machine, more than one player -- or one player who would rather not
// have last month's run of luck counted against this month's. A profile is
// the name a game is played under and, more to the point, the place the
// rivals' experience is kept: the Lulu you have beaten forty times on the
// first profile is a different Lulu from the one on the second, and switching
// profiles is the only way back to a table of beginners.
inline constexpr int kProfileCount = 3;

// Ce qui tient sur une plaque du menu d'ouverture. Un nom plus long y serait
// coupé, et une plaque dont le nom est coupé ne dit plus lequel des trois
// elle est.
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

// Written as the game goes rather than at the end: a session that ends badly
// still ends with the games it played counted.
void noteHandPlayed(bool won);
void noteMatchPlayed(bool won, int score);

// Everything this profile has: its own tally and the experience of all six
// rivals, back to the day it was new.
void forgetProfile(int slot);

// Where a thing belonging to a profile is kept in the settings file. One
// place that knows how the key is spelled, so that reading and writing cannot
// drift apart.
QString profileKey(int slot, const QString &suffix);
