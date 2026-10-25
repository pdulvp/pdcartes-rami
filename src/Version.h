#pragma once

#include <QString>

// Identification de la version et de la date de compilation.
//
// La version est celle du fichier `VERSION` situé à la racine des sources :
// trois numéros, incrémentés manuellement lors d'une publication.
// L'horodatage de compilation indique le moment où le binaire a été produit,
// au format aammjjhhmmss ; deux compilations ne portent jamais le même, ce qui
// permet de distinguer deux binaires d'une même version.
//
// Un binaire publié ne porte aucun horodatage : sa version suffit à
// l'identifier, puisqu'il n'en existe qu'un. L'horodatage est alors vide, et
// tout code le lisant doit prévoir ce cas.
//
// Les deux valeurs sont définies dans un fichier généré à chaque compilation ;
// voir cmake/GenerateVersion.cmake.
extern const char *const kPdcartesRamiVersion;
extern const char *const kPdcartesRamiBuildVersion;

inline QString appVersion() { return QString::fromLatin1(kPdcartesRamiVersion); }
inline QString buildVersion() { return QString::fromLatin1(kPdcartesRamiBuildVersion); }

// Les deux valeurs réunies, pour un affichage disposant de la place
// nécessaire. Seule la version est retournée pour un binaire publié, celui-ci
// ne portant pas d'horodatage.
inline QString fullVersion() {
    const QString build = buildVersion();
    if (build.isEmpty()) {
        return appVersion();
    }
    return appVersion() + QStringLiteral(" (build ") + build + QLatin1Char(')');
}
