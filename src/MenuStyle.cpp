#include "MenuStyle.h"

#include "Palette.h"

#include <QFile>
#include <QPainter>
#include <QPixmap>

#include <QMenu>
#include <QString>
#include <QToolButton>

void dressMenu(QMenu *menu) {
    if (!menu) return;
    const Palette &scheme = currentPalette();
    menu->setStyleSheet(
        QStringLiteral("QMenu { background: %1; color: %2; border: 1px solid %3;"
                       " border-radius: 8px; padding: 5px; }"
                       // Un pixel de moins en haut qu'en bas, comme partout où Qt
                       // effectue lui-même le centrage : voir `capCentred` dans
                       // Fonts.h.
                       "QMenu::item { padding: 5px 22px 7px 14px;"
                       " border-radius: 5px; }"
                       "QMenu::item:selected { background: %4; color: %5; }"
                       "QMenu::item:disabled { color: %6; }"
                       "QMenu::separator { height: 1px; background: %3; margin: 5px 8px; }")
            .arg(scheme.surface.name(), scheme.surfaceText.name(), scheme.edge.name(),
                 scheme.accent.name(), scheme.accentText.name(), scheme.hint.name()));
}

namespace {

// Applique la même coloration que `tintedIcon`, mais à une image fournie en
// argument : la forme est conservée et la couleur d'origine remplacée, ce qui
// constitue l'intégralité du traitement pour une icône monochrome.
QIcon tintedFile(const QString &path, const QColor &ink) {
    const QIcon source(path);
    if (source.isNull()) return {};

    QIcon tinted;
    for (int size : {16, 22, 24, 32, 48}) {
        QPixmap art = source.pixmap(size, size);
        if (art.isNull()) continue;

        QPixmap coloured(art.size());
        coloured.setDevicePixelRatio(art.devicePixelRatio());
        coloured.fill(Qt::transparent);

        QPainter painter(&coloured);
        painter.drawPixmap(0, 0, art);
        painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
        painter.fillRect(coloured.rect(), ink);
        painter.end();

        tinted.addPixmap(coloured);
    }
    return tinted;
}

} // namespace

QIcon appIcon(const QStringList &names, const QColor &ink) {
    // Le thème du bureau est consulté en premier : sur une machine qui en
    // fournit un, l'application adopte ses icônes plutôt que les siennes.
    const QIcon theirs = tintedIcon(names, ink);
    if (!theirs.isNull()) return theirs;

    for (const QString &name : names) {
        const QString path = QStringLiteral(":/icons/%1.svg").arg(name);
        if (!QFile::exists(path)) continue;
        const QIcon ours = tintedFile(path, ink);
        if (!ours.isNull()) return ours;
    }
    return {};
}

QToolButton *iconButton(QWidget *parent, const QStringList &iconNames, const QString &fallback,
                        const QString &tip) {
    const Palette &scheme = currentPalette();
    auto *button = new QToolButton(parent);
    button->setToolTip(tip);
    button->setCursor(Qt::PointingHandCursor);
    button->setFocusPolicy(Qt::NoFocus);
    button->setIconSize(QSize(17, 17));

    const QIcon icon = appIcon(iconNames, scheme.surfaceText);
    if (icon.isNull()) {
        button->setText(fallback);
        button->setToolButtonStyle(Qt::ToolButtonTextOnly);
    } else {
        button->setIcon(icon);
        button->setToolButtonStyle(Qt::ToolButtonIconOnly);
        button->setFixedSize(32, 30);
    }
    button->setStyleSheet(QStringLiteral("QToolButton { background: %1; color: %2;"
                                         " border: 1px solid %3; border-radius: 6px;"
                                         " padding: 3px 7px 5px 7px; }"
                                         "QToolButton:hover { border: 1px solid %4; }"
                                         "QToolButton:pressed { background: %5; }")
                              .arg(scheme.field.name(), scheme.surfaceText.name(),
                                   scheme.edge.name(), scheme.accent.name(),
                                   blendColours(scheme.field, scheme.accent, 0.18).name()));
    return button;
}
