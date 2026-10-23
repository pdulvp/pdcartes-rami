#include "Fonts.h"

#include <QApplication>
#include <QFont>
#include <QFontDatabase>
#include <QFontMetricsF>
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
    // Deux graisses des neuf livrées: le normal et le gras, qui sont celles
    // dont une fenêtre se sert. Chacune pèse presque deux mégaoctets -- une
    // fonte japonaise porte ses milliers d'idéogrammes, qu'on les écrive ou
    // non -- et les neuf en feraient seize pour sept graisses jamais
    // demandées.
    //
    // Les graisses figées plutôt que la fonte variable livrée à côté: celle-ci
    // porte les neuf en un fichier, mais il faudrait que Qt sache en tirer
    // l'axe de graisse, et deux fichiers qui marchent valent mieux qu'un qui
    // dépend de la version.
    //
    // Les deux se rangent sous la même famille: c'est Qt qui prend la bonne
    // quand on demande du gras, et non nous qui changeons de nom.
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
    // La famille seulement: le corps est celui que le bureau a réglé, et un
    // jeu n'a pas à en décider pour quelqu'un qui a grossi tout son écran.
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
        // Sans la fonte d'affichage, c'est le gras qui fait le titre.
        font.setWeight(QFont::Black);
    }
    font.setPixelSize(std::max(7, int(std::lround(pixels))));
    return font;
}

QRectF capCentred(const QRectF &box, const QFontMetricsF &metrics) {
    // Où Qt pose la ligne de base: au milieu de la boîte, plus la moitié de
    // ce qui sépare le haut des accents du bas des jambages.
    const qreal asIs = (metrics.ascent() - metrics.descent()) / 2.0;
    // Où on la veut: au milieu de la boîte, plus la moitié d'une capitale --
    // ce qui met le milieu des lettres sur le milieu de la pastille.
    //
    // Une fonte qui ne déclare pas sa hauteur de capitale rend zéro; on
    // retombe alors sur la montante, qui en est proche dans les fontes sans
    // accents hauts, et surtout qui ne vaut pas zéro.
    const qreal cap = metrics.capHeight() > 0.0 ? metrics.capHeight() : metrics.ascent();
    return box.translated(0.0, cap / 2.0 - asIs);
}
