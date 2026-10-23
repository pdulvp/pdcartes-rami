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
