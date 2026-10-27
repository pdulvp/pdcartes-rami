#include <QApplication>
#include <QCommandLineParser>
#include <QFileInfo>
#include <QIcon>
#include <QLocale>

#include <cstdio>

#include "CardDeck.h"
#include "DrawnDeck.h"
#include "Fonts.h"
#include "Palette.h"
#include "TableLook.h"
#include "MainWindow.h"
#include "Version.h"

namespace {

// Qt retient le thème « hicolor » lorsque le bureau n'indique aucun thème
// d'icônes. Ce thème ne contenant presque rien, les boutons du bandeau
// supérieur resteraient sans icône. Cette fonction désigne comme thème de
// recours un thème effectivement installé.
void ensureIconTheme() {
    const QString configured = QIcon::themeName();
    if (!configured.isEmpty() && configured != QLatin1String("hicolor")) return;

    const QStringList candidates = {QStringLiteral("Adwaita"), QStringLiteral("breeze"),
                                    QStringLiteral("oxygen"), QStringLiteral("Papirus"),
                                    QStringLiteral("gnome")};
    for (const QString &name : candidates) {
        for (const QString &dir : QIcon::themeSearchPaths()) {
            if (!QFileInfo::exists(dir + QLatin1Char('/') + name +
                                   QLatin1String("/index.theme"))) {
                continue;
            }
            QIcon::setFallbackThemeName(name);
            return;
        }
    }
}

// Icône de l'application, chargée depuis les ressources du binaire.
//
// Plusieurs tailles sont fournies plutôt qu'une seule à redimensionner :
// chacune est rendue directement à partir du dessin vectoriel. Une lettre
// rendue à seize pixels reste nette, alors qu'une réduction du rendu à 256
// pixels serait floue ; or seize pixels est la taille employée par la barre
// des tâches, où l'icône est la plus visible.
QIcon builtInIcon() {
    QIcon icon;
    for (int size : {16, 24, 32, 48, 64, 128, 256}) {
        icon.addFile(QStringLiteral(":/app/pdcartes-rami-%1.png").arg(size));
    }
    return icon;
}

} // namespace

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QApplication::setOrganizationName("pdulvp");
    QApplication::setApplicationName("pdcartes-rami");
    QApplication::setApplicationVersion(fullVersion());
    // Identifiant sous lequel le bureau reconnaît l'application. Sous Wayland,
    // il devient l'`app_id` de la fenêtre, que l'environnement rapproche du
    // fichier .desktop de même nom pour déterminer à quelle application une
    // fenêtre appartient ; c'est ce qui permet de l'épingler.
    QApplication::setDesktopFileName(QStringLiteral("pdcartes-rami"));

    // L'application étant en français, ses nombres et ses dates le sont
    // également, quels que soient les réglages du système.
    QLocale::setDefault(QLocale(QLocale::French, QLocale::France));

    ensureIconTheme();
    // Icône de la fenêtre, également affichée par la barre des tâches du
    // bureau.
    //
    // Le thème est consulté en premier, comme pour les icônes des boutons :
    // lorsque l'application est installée, son icône figure dans le thème
    // `hicolor`, et un environnement qui redessine les icônes de ses thèmes
    // fournit alors sa propre version. Les ressources du binaire servent de
    // recours, notamment lors d'une exécution depuis l'arbre de compilation
    // et sous Windows, où il n'existe pas de thème d'icônes. L'appel suit
    // `ensureIconTheme()`, qui vient de fixer le thème de recours.
    QApplication::setWindowIcon(
        QIcon::fromTheme(QStringLiteral("pdcartes-rami"), builtInIcon()));
    // La fonte de tout ce qui se lit dans la fenêtre. Posée avant la première
    // fenêtre: un widget garde la fonte qu'il avait en naissant.
    dressApplicationFont();

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QCoreApplication::translate("main", "pdcartes-rami — le rami sur tapis vert."));
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption exportDeck(
        QStringList{QStringLiteral("exporter-cartes")},
        QCoreApplication::translate(
            "main", "Écrire le jeu dessiné dans un fichier .cartes, et s'arrêter."),
        QCoreApplication::translate("main", "fichier"));
    parser.addOption(exportDeck);
    parser.process(app);

    if (parser.isSet(exportDeck)) {
        QString why;
        const QString path = parser.value(exportDeck);
        // Les cartes dessinées prennent les couleurs qu'on leur donne : une
        // teinte fixe est donc imposée pour l'export, celle du tapis par
        // défaut, plutôt que celle du dernier profil ouvert.
        setDarkMode(false);
        setCurrentPalette(defaultFeltHue());
        DrawnRecipe how;
        how.style = CardStyle::fromPalette(currentPalette());
        CardSet drawn = drawnCardSet(how);
        drawn.author = QStringLiteral("pdcartes-rami");
        if (!drawn.save(path, &why)) {
            fprintf(stderr, "%s\n", qPrintable(why));
            return 1;
        }
        printf("%s\n", qPrintable(QCoreApplication::translate("main", "Jeu dessiné écrit dans %1")
                                       .arg(path)));
        return 0;
    }

    // Les jeux installés, lus au démarrage ; relus à la demande depuis les
    // réglages.
    CardDecks::instance().refresh();

    MainWindow window;
    window.show();
    return app.exec();
}
