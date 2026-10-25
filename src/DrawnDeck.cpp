#include "DrawnDeck.h"

#include "CardArt.h"
#include "CardSet.h"
#include "Cards.h"
#include "Palette.h"
#include "TableLook.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QImage>
#include <QPainter>

namespace {

// Résolution suffisante pour l'affichage agrandi de la main, tout en
// conservant une taille de fichier raisonnable.
constexpr int kHeight = 420;

QByteArray renderOne(const Card &card, bool back) {
    const Palette &scheme = currentPalette();
    const QSizeF size = cardSizeForHeight(kHeight);
    QImage image(QSize(int(std::ceil(size.width())), int(std::ceil(size.height()))),
                 QImage::Format_ARGB32);
    image.fill(Qt::transparent);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    CardLook look;
    look.noShadow = true;

    const QRectF box(0.5, 0.5, image.width() - 1.0, image.height() - 1.0);
    if (back) {
        drawCardBack(&painter, box, scheme, look);
    } else {
        drawCardFace(&painter, box, card, scheme, look);
    }
    painter.end();

    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    if (!image.save(&buffer, "PNG")) return {};
    return bytes;
}

} // namespace

bool exportDrawnDeck(const QString &path, QString *why) {
    // Les cartes dessinées adoptent la teinte de la table : une teinte fixe
    // est donc imposée pour l'export.
    setDarkMode(false);
    setCurrentPalette(defaultFeltHue());

    CardSet set;
    set.name = QCoreApplication::translate("drawn", "Le jeu dessiné");
    set.author = QCoreApplication::translate("drawn", "pdcartes-rami");
    set.note = QCoreApplication::translate(
        "drawn", "Les cartes que pdcartes-rami dessine lui-même, rendues en images.");

    for (int slot = 0; slot < CardSet::kSlots; ++slot) {
        Card card;
        card.id = slot;
        card.rank = CardSet::rankOfSlot(slot);
        card.suit = Suit(CardSet::suitOfSlot(slot));
        if (slot == CardSet::kJokerSlot) card.rank = kJoker;

        Artwork art;
        art.kind = "png";
        art.bytes = renderOne(card, false);
        if (art.bytes.isEmpty()) {
            if (why) {
                *why = QCoreApplication::translate("drawn", "%1 ne se dessine pas.")
                           .arg(CardSet::slotName(slot));
            }
            return false;
        }
        set.setFace(slot, art);
    }

    Artwork back;
    back.kind = "png";
    back.bytes = renderOne(Card{}, true);
    if (!back.bytes.isEmpty()) {
        set.addBack(QCoreApplication::translate("drawn", "Treillis"), back);
    }

    return set.save(path, why);
}
