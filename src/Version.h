#pragma once

#include <QString>

// What this is and when it was made.
//
// The version is the one in the VERSION file at the root of the source --
// three numbers, moved on by hand when something is worth calling a new
// version. The build is the moment the binary was compiled, as
// yymmddhhmmss: no two builds carry the same one, which is what tells
// yesterday's 1.0.0 from the one in front of you.
//
// A released binary carries no build at all -- its version is its name, and
// there is only one of it. So the build is empty there, and what reads it
// has to allow for that.
//
// Both are defined in a file written at each build; see
// cmake/GenerateVersion.cmake.
extern const char *const kPdcartesRamiVersion;
extern const char *const kPdcartesRamiBuildVersion;

inline QString appVersion() { return QString::fromLatin1(kPdcartesRamiVersion); }
inline QString buildVersion() { return QString::fromLatin1(kPdcartesRamiBuildVersion); }

// The two together, for a line that has room for both -- the version alone
// when this is a release, there being no build to tell it from.
inline QString fullVersion() {
    const QString build = buildVersion();
    if (build.isEmpty()) {
        return appVersion();
    }
    return appVersion() + QStringLiteral(" (build ") + build + QLatin1Char(')');
}
