#include "TableLook.h"

#include "CardDeck.h"
#include "Profiles.h"

#include <QCoreApplication>
#include <QSettings>

namespace {

// Les clés, sous le profil: la cartothèque a les siennes, à plat, qui ne
// valent que pour le choix courant.
QString feltKey(int slot) { return profileKey(slot, QStringLiteral("tapis")); }
QString deckKey(int slot) { return profileKey(slot, QStringLiteral("cartes/jeu")); }
QString backKey(int slot) { return profileKey(slot, QStringLiteral("cartes/dos")); }

} // namespace

const QVector<FeltColour> &feltColours() {
    // Six draps, assez loin les uns des autres pour qu'on les distingue d'un
    // coup d'œil, et tous assez sombres pour que les cartes blanches posées
    // dessus restent les choses claires de la table.
    static const QVector<FeltColour> list = {
        {142, QCoreApplication::translate("felt", "Vert")},
        {168, QCoreApplication::translate("felt", "Jade")},
        {196, QCoreApplication::translate("felt", "Bleu")},
        {268, QCoreApplication::translate("felt", "Prune")},
        {348, QCoreApplication::translate("felt", "Bordeaux")},
        {32, QCoreApplication::translate("felt", "Tabac")},
    };
    return list;
}

int defaultFeltHue() { return feltColours().first().hue; }

int feltHue() {
    const int kept =
        QSettings().value(feltKey(currentProfile()), defaultFeltHue()).toInt();
    // Une teinte écrite à la main, ou venue d'une version qui en proposait
    // d'autres, ne doit pas donner un tapis d'une couleur impossible.
    return (kept >= 0 && kept <= 359) ? kept : defaultFeltHue();
}

void setFeltHue(int hue) {
    QSettings().setValue(feltKey(currentProfile()), (hue >= 0 && hue <= 359) ? hue
                                                                            : defaultFeltHue());
}

void applyProfileLook() {
    const int slot = currentProfile();
    QSettings settings;
    CardDecks &decks = CardDecks::instance();

    // Rien d'écrit pour ce profil: on lui laisse le jeu dessiné plutôt que
    // celui du profil d'avant, qui n'est pas le sien.
    const QString deck =
        settings.value(deckKey(slot), CardDecks::builtInId()).toString();
    decks.choose(deck);
    decks.chooseBack(settings.value(backKey(slot), 0).toInt());
}

void rememberDeck(const QString &deckId) {
    CardDecks::instance().choose(deckId);
    QSettings().setValue(deckKey(currentProfile()), deckId);
}

void rememberBack(int at) {
    CardDecks::instance().chooseBack(at);
    QSettings().setValue(backKey(currentProfile()), at);
}
