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
    // Le bouton d'annulation reçoit le focus à l'ouverture. Ce choix est
    // délibéré : la touche Entrée est fréquemment actionnée sans lecture
    // préalable, et ces confirmations portent sur des opérations
    // destructrices.
    go->setDefault(false);
    go->setAutoDefault(false);
    chrome.footer->addStretch();
    chrome.footer->addWidget(buttons);

    QObject::connect(buttons, &QDialogButtonBox::accepted, &box, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &box, &QDialog::reject);

    // Largeur identique à celle des autres boîtes, hauteur déterminée par la
    // phrase inférieure, qui occupe une à trois lignes selon le cas.
    //
    // La hauteur minimale est en revanche imposée par le bandeau et non par le
    // texte : celui-ci est affiché verticalement sur la tranche, et ce qui
    // excède la hauteur de la boîte est tronqué aux deux extrémités. Deux cent
    // vingt pixels suffisent aux libellés employés ici, « OUBLIER UN PROFIL »
    // étant le plus long ; il était tronqué à cent quatre-vingts.
    box.resize(560, std::max(220, box.sizeHint().height()));
    return box.exec() == QDialog::Accepted;
}
