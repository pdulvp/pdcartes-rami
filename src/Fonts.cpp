#include "Fonts.h"

#include <QApplication>
#include <QFont>
#include <QFontDatabase>
#include <QFontMetricsF>
#include <QStringList>

#include <algorithm>
#include <cmath>

namespace {

// Charge une police depuis les ressources du binaire et retourne le nom de
// famille sous lequel Qt l'a enregistrée. Retourne une chaîne vide si la
// ressource est absente.
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
    // Deux graisses sont chargées sur les neuf disponibles : le romain et le
    // gras, seules employées par l'interface. Chaque fichier pèse près de deux
    // mégaoctets, une police japonaise comportant plusieurs milliers
    // d'idéogrammes qu'ils soient utilisés ou non ; charger les neuf
    // représenterait seize mégaoctets pour sept graisses jamais demandées.
    //
    // Les graisses statiques sont préférées à la police variable fournie à
    // côté : cette dernière réunit les neuf graisses en un fichier, mais son
    // exploitation suppose que Qt sache en extraire l'axe de graisse, ce qui
    // dépend de la version. Deux fichiers au comportement certain sont
    // préférables.
    //
    // Les deux graisses sont enregistrées sous la même famille : c'est Qt qui
    // sélectionne la bonne lorsque le gras est demandé, sans changement de
    // nom de famille.
    static const QString family = [] {
        const QString plain = familyFrom(
            QStringLiteral(":/fonts/M_PLUS_2/static/MPLUS2-Regular.ttf"));
        const QString heavy = familyFrom(
            QStringLiteral(":/fonts/M_PLUS_2/static/MPLUS2-Bold.ttf"));
        return plain.isEmpty() ? heavy : plain;
    }();
    return family;
}

void dressApplicationFont() {
    const QString family = proseFamily();
    if (family.isEmpty()) return;
    // Seule la famille est imposée : le corps reste celui défini par le
    // bureau, afin de respecter le réglage d'un utilisateur ayant agrandi
    // l'affichage de son système.
    QFont ui = QApplication::font();
    ui.setFamily(family);
    QApplication::setFont(ui);
}

QFont familyFont(const QFont &fallback, const QString &family, qreal pixels) {
    QFont font = fallback;
    if (!family.isEmpty()) {
        font.setFamily(family);
        font.setWeight(QFont::Normal);
    } else {
        // En l'absence de police d'affichage, le titre est composé en gras.
        font.setWeight(QFont::Black);
    }
    font.setPixelSize(std::max(7, int(std::lround(pixels))));
    return font;
}

QRectF capCentred(const QRectF &box, const QFontMetricsF &metrics) {
    // Position de la ligne de base retenue par Qt : le centre du rectangle,
    // augmenté de la moitié de la distance séparant le sommet des accents du
    // bas des jambages.
    const qreal asIs = (metrics.ascent() - metrics.descent()) / 2.0;
    // Position recherchée : le centre du rectangle, augmenté de la moitié de
    // la hauteur de capitale, ce qui aligne le centre des lettres sur celui du
    // cadre.
    //
    // Une police ne déclarant pas sa hauteur de capitale retourne zéro ; on se
    // rabat alors sur la hauteur d'œil ascendante, proche de la hauteur de
    // capitale dans les polices sans accents hauts, et surtout non nulle.
    const qreal cap = metrics.capHeight() > 0.0 ? metrics.capHeight() : metrics.ascent();
    return box.translated(0.0, cap / 2.0 - asIs);
}
