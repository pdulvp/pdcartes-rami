#include "Fonts.h"

#include <QFont>
#include <QFontDatabase>
#include <QStringList>

#include <algorithm>
#include <cmath>

namespace {

// Une fonte tirée du binaire, et la famille sous laquelle Qt l'a rangée.
// Vide quand la ressource manque.
QString familyFrom(const QString &path) {
    const int id = QFontDatabase::addApplicationFont(path);
    if (id < 0) return QString();
    const QStringList families = QFontDatabase::applicationFontFamilies(id);
    return families.isEmpty() ? QString() : families.constFirst();
}

} // namespace

QString signFamily() {
    static const QString family =
        familyFrom(QStringLiteral(":/fonts/ZenTokyoZoo/ZenTokyoZoo-Regular.ttf"));
    return family;
}

QString capsFamily() {
    static const QString family =
        familyFrom(QStringLiteral(":/fonts/ZenDots/ZenDots-Regular.ttf"));
    return family;
}

QString proseFamily() {
    // Le gras seul, et non les cinq graisses livrées avec: c'est la seule
    // dont le jeu se sert, et chacune pèse deux mégaoctets -- une fonte
    // japonaise porte ses milliers d'idéogrammes, qu'on les écrive ou non.
    static const QString family = familyFrom(
        QStringLiteral(":/fonts/Zen_Kaku_Gothic_New/ZenKakuGothicNew-Bold.ttf"));
    return family;
}

QFont familyFont(const QFont &fallback, const QString &family, qreal pixels) {
    QFont font = fallback;
    if (!family.isEmpty()) {
        font.setFamily(family);
        font.setWeight(QFont::Normal);
    } else {
        // Sans la fonte d'affichage, c'est le gras qui fait le titre.
        font.setWeight(QFont::Black);
    }
    font.setPixelSize(std::max(7, int(std::lround(pixels))));
    return font;
}
