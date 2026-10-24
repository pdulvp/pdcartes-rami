#include <QApplication>
#include <QCommandLineParser>
#include <QFileInfo>
#include <QIcon>
#include <QLocale>

#include <cstdio>

#include "CardDeck.h"
#include "DrawnDeck.h"
#include "Fonts.h"
#include "MainWindow.h"
#include "Version.h"

namespace {

// Qt settles on the bare "hicolor" theme when the desktop has not told it
// which icon theme to use, and hicolor carries next to nothing -- which
// leaves the buttons along the top bare. Point the fallback at an icon theme
// that is actually installed.
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

// L'icône du jeu, tirée du binaire.
//
// Plusieurs tailles et non une seule qu'on réduirait : chacune est rendue
// depuis le dessin, et une lettre rendue à seize pixels est nette là où la
// même réduite depuis deux cent cinquante-six est molle. C'est la taille de
// la barre des tâches, celle où l'icône se voit le plus.
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
    QApplication::setOrganizationName("pdcartes-rami");
    QApplication::setApplicationName("pdcartes-rami");
    QApplication::setApplicationVersion(fullVersion());
    // What the desktop knows this application by. On Wayland it becomes the
    // window's app_id, and a shell matches that against the .desktop file of
    // the same name to work out which application a window belongs to --
    // which is what lets it be pinned.
    QApplication::setDesktopFileName(QStringLiteral("pdcartes-rami"));

    // The game is in French, so its numbers and its dates are French too,
    // whatever the machine is set to.
    QLocale::setDefault(QLocale(QLocale::French, QLocale::France));

    ensureIconTheme();
    // Ce que porte la fenêtre, et ce que le bureau montre dans sa barre.
    // Le thème d'abord, comme pour les icônes des boutons: installé, le jeu a
    // déposé la sienne sous `hicolor`, et un bureau qui habille ses icônes la
    // sert habillée. Le binaire ensuite -- lancé depuis l'arbre de build, et
    // sur Windows où il n'y a pas de thème, c'est le repli qui sert. Après
    // `ensureIconTheme()`, qui vient de dire où chercher.
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
        if (!exportDrawnDeck(path, &why)) {
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
