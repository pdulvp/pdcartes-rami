#include "AskDialog.h"

#include "DialogChrome.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include <algorithm>

namespace {

constexpr int kHeadingGap = 8;

} // namespace

bool askYesNo(QWidget *parent, const QString &banner, const QString &question,
              const QString &doIt, const QString &aside) {
    QDialog box(parent);
    box.setWindowTitle(banner);

    const DialogChrome chrome = applyDialogChrome(&box, banner);

    chrome.fields->addWidget(dialogSectionHeading(&box, question));

    if (!aside.isEmpty()) {
        chrome.fields->addSpacing(kHeadingGap);
        auto *note = new QLabel(aside, &box);
        note->setWordWrap(true);
        markHintLabel(note);
        chrome.fields->addWidget(note);
    }
    chrome.fields->addStretch(1);

    auto *buttons = new QDialogButtonBox(&box);
    buttons->addButton(QObject::tr("Annuler"), QDialogButtonBox::RejectRole);
    QPushButton *go = buttons->addButton(doIt, QDialogButtonBox::AcceptRole);
    markDefaultButton(go);
    // Le bouton qui annule a le focus en arrivant, et c'est voulu: on tape
    // Entrée sur une boîte qu'on n'a pas lue, et ces questions-là effacent.
    go->setDefault(false);
    go->setAutoDefault(false);
    chrome.footer->addStretch();
    chrome.footer->addWidget(buttons);

    QObject::connect(buttons, &QDialogButtonBox::accepted, &box, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &box, &QDialog::reject);

    // Large comme les autres, et haute de ce que la phrase du dessous demande:
    // elle tient sur une ligne ou sur trois selon ce qu'on demande.
    //
    // Le plancher, lui, est celui du bandeau et non celui du texte: il court
    // sur la tranche, tourné d'un quart de tour, et ce qui dépasse de la
    // hauteur de la boîte est coupé aux deux bouts. Deux cent vingt pixels
    // logent les actes qu'on nomme ici -- `OUBLIER UN PROFIL` est le plus
    // long, et il était rogné à cent quatre-vingts.
    box.resize(560, std::max(220, box.sizeHint().height()));
    return box.exec() == QDialog::Accepted;
}
