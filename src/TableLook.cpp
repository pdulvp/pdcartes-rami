#include "TableLook.h"

#include "CardDeck.h"
#include "CardSet.h"
#include "Profiles.h"

#include <QCoreApplication>
#include <QSettings>

namespace {

// Les clés, sous le profil: la cartothèque a les siennes, à plat, qui ne
// valent que pour le choix courant.
QString feltKey(int slot) { return profileKey(slot, QStringLiteral("tapis")); }
QString deckKey(int slot) { return profileKey(slot, QStringLiteral("cartes/jeu")); }
QString backKey(int slot) { return profileKey(slot, QStringLiteral("cartes/dos")); }
QString backDeckKey(int slot) { return profileKey(slot, QStringLiteral("cartes/dosJeu")); }

// Le jeu dont le dos est tiré, gardé chargé.
//
// La cartothèque en garde un, celui des faces ; celui-ci est le second, et il
// est à nous parce qu'elle n'en connaît pas la notion. Chargé à la demande et
// relu quand l'identifiant change, comme elle fait du sien.
//
// Rendu par une fonction plutôt que posé à la portée du fichier: un CardSet
// porte des QPixmap, et une QPixmap veut qu'une QGuiApplication existe avant
// d'être faite. Un static dans une fonction est fait au premier appel, qui est
// longtemps après.
struct BackDeck {
    CardSet set;
    QString loaded;
    bool tried = false;
};

BackDeck &backDeck() {
    static BackDeck kept;
    return kept;
}

// Celui qu'il faut charger: celui qu'on a dit, ou celui des faces quand on n'a
// rien dit.
QString wantedBackDeck() {
    const QString kept = backDeckId();
    return kept.isEmpty() ? CardDecks::instance().chosenId() : kept;
}

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

    // Rien d'écrit pour ce profil: on lui laisse le jeu dessiné plutôt que
    // celui du profil d'avant, qui n'est pas le sien.
    const QString deck =
        settings.value(deckKey(slot), CardDecks::builtInId()).toString();
    CardDecks::instance().choose(deck);
    // Le dos du profil d'avant est encore chargé: il faudra le relire, que ce
    // profil-ci en nomme un autre ou qu'il suive ses faces.
    backDeck().tried = false;
}

void rememberDeck(const QString &deckId) {
    CardDecks::instance().choose(deckId);
    QSettings().setValue(deckKey(currentProfile()), deckId);
    // Quand le dos suit les faces, il vient de changer de jeu lui aussi -- et
    // le numéro qu'on avait désignait un dos de l'autre jeu, qui n'est pas
    // celui-là.
    if (backDeckId().isEmpty()) {
        QSettings().setValue(backKey(currentProfile()), 0);
        backDeck().tried = false;
    }
}

QString backDeckId() {
    return QSettings().value(backDeckKey(currentProfile())).toString();
}

void rememberBackDeck(const QString &deckId) {
    QSettings settings;
    settings.setValue(backDeckKey(currentProfile()), deckId);
    // Un autre jeu, d'autres dos: le numéro qu'on avait ne désigne plus rien
    // de voulu.
    settings.setValue(backKey(currentProfile()), 0);
    backDeck().tried = false;
}

const CardSet *backCards() {
    const QString want = wantedBackDeck();
    BackDeck &kept = backDeck();
    if (!kept.tried || kept.loaded != want) {
        kept.tried = true;
        kept.loaded = want;
        kept.set = CardSet();
        if (want != CardDecks::builtInId()) {
            const DeckEntry *found = CardDecks::instance().entry(want);
            QString why;
            if (!found || found->path.isEmpty() || !kept.set.load(found->path, &why)) {
                kept.set = CardSet();
            }
        }
    }
    // Ses dos, et non ses faces: un jeu qui n'apporte que des dos est ici un
    // jeu qui sert, là où la cartothèque le tiendrait pour vide.
    return kept.set.backCount() > 0 ? &kept.set : nullptr;
}

int backIndex() {
    const CardSet *set = backCards();
    if (!set || set->backCount() == 0) return -1;
    const int at = QSettings().value(backKey(currentProfile()), 0).toInt();
    return qBound(0, at, set->backCount() - 1);
}

void rememberBack(int at) {
    QSettings().setValue(backKey(currentProfile()), qMax(0, at));
}

void forgetBackDrawings() { backDeck().set.forgetDrawings(); }
