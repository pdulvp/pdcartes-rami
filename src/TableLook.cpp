#include "TableLook.h"

#include "CardDeck.h"
#include "CardSet.h"
#include "Profiles.h"

#include <QCoreApplication>
#include <QSettings>

namespace {

// Clés de réglages, rangées sous le profil. La cartothèque possède les
// siennes, non hiérarchisées, qui ne valent que pour la sélection courante.
QString feltKey(int slot) { return profileKey(slot, QStringLiteral("tapis")); }
QString deckKey(int slot) { return profileKey(slot, QStringLiteral("cartes/jeu")); }
QString backKey(int slot) { return profileKey(slot, QStringLiteral("cartes/dos")); }
QString backDeckKey(int slot) { return profileKey(slot, QStringLiteral("cartes/dosJeu")); }

// Jeu dont le dos est extrait, maintenu chargé.
//
// La cartothèque conserve celui des faces ; celui-ci est un second jeu, géré
// par ce module, la cartothèque n'ayant pas cette notion. Il est chargé à la
// demande et rechargé lorsque son identifiant change, selon le même principe.
//
// Il est fourni par une fonction plutôt que par une variable de portée
// fichier : un `CardSet` contient des `QPixmap`, dont la construction suppose
// qu'une `QGuiApplication` existe déjà. Une variable statique locale à une
// fonction est construite au premier appel, bien plus tard.
struct BackDeck {
    CardSet set;
    QString loaded;
    bool tried = false;
};

BackDeck &backDeck() {
    static BackDeck kept;
    return kept;
}

// Jeu à charger : celui qui a été désigné, ou celui des faces à défaut.
QString wantedBackDeck() {
    const QString kept = backDeckId();
    return kept.isEmpty() ? CardDecks::instance().chosenId() : kept;
}

} // namespace

const QVector<FeltColour> &feltColours() {
    // Six teintes, suffisamment éloignées les unes des autres pour être
    // distinguées immédiatement, et toutes assez sombres pour que les cartes
    // blanches posées dessus demeurent les éléments clairs de la table.
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
    // Une teinte saisie manuellement, ou provenant d'une version qui en
    // proposait d'autres, ne doit pas produire une couleur de tapis invalide.
    return (kept >= 0 && kept <= 359) ? kept : defaultFeltHue();
}

void setFeltHue(int hue) {
    QSettings().setValue(feltKey(currentProfile()), (hue >= 0 && hue <= 359) ? hue
                                                                            : defaultFeltHue());
}

void applyProfileLook() {
    const int slot = currentProfile();
    QSettings settings;

    // Aucune valeur enregistrée pour ce profil : le jeu dessiné lui est
    // attribué, plutôt que celui du profil précédent.
    const QString deck =
        settings.value(deckKey(slot), CardDecks::builtInId()).toString();
    CardDecks::instance().choose(deck);
    // Le dos du profil précédent est encore chargé ; il doit être rechargé,
    // que ce profil en désigne un autre ou qu'il suive son jeu de faces.
    backDeck().tried = false;
}

void rememberDeck(const QString &deckId) {
    CardDecks::instance().choose(deckId);
    QSettings().setValue(deckKey(currentProfile()), deckId);
    // Lorsque le dos suit le jeu des faces, il change de jeu avec lui, et
    // l'index enregistré désignait un dos de l'ancien jeu.
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
    // Un autre jeu fournit d'autres dos : l'index enregistré ne désigne plus
    // le dos attendu.
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
    // Ce sont ses dos qui sont requis et non ses faces : un jeu ne fournissant
    // que des dos est exploitable ici, alors que la cartothèque le
    // considérerait comme vide.
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
