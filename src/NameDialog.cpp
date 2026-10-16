#include "NameDialog.h"

#include "DialogChrome.h"
#include "Palette.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include <algorithm>

namespace {

constexpr int kHeadingGap = 8;

} // namespace

NameDialog::NameDialog(const QString &banner, const QString &question, const QString &aside,
                       const QString &value, int mostLetters, QWidget *parent)
    : QDialog(parent) {
    setWindowTitle(banner);

    const DialogChrome chrome = applyDialogChrome(this, banner);

    chrome.fields->addWidget(dialogSectionHeading(this, question));
    chrome.fields->addSpacing(kHeadingGap);

    m_field = new QLineEdit(value, this);
    m_field->setMaxLength(mostLetters);
    m_field->setClearButtonEnabled(true);
    // Haut et dans la couleur du corps: c'est la seule chose à faire dans
    // cette boîte, autant qu'elle en ait l'air.
    m_field->setMinimumHeight(34);
    QPalette ink = m_field->palette();
    ink.setColor(QPalette::Base, currentPalette().field);
    ink.setColor(QPalette::Text, currentPalette().surfaceText);
    m_field->setPalette(ink);
    QFont bigger = m_field->font();
    if (bigger.pointSizeF() > 0) bigger.setPointSizeF(bigger.pointSizeF() * 1.12);
    m_field->setFont(bigger);
    chrome.fields->addWidget(m_field);

    if (!aside.isEmpty()) {
        chrome.fields->addSpacing(6);
        auto *note = new QLabel(aside, this);
        note->setWordWrap(true);
        markHintLabel(note);
        chrome.fields->addWidget(note);
    }
    chrome.fields->addStretch(1);

    auto *buttons = new QDialogButtonBox(this);
    buttons->addButton(tr("Annuler"), QDialogButtonBox::RejectRole);
    QPushButton *ok = buttons->addButton(tr("Valider"), QDialogButtonBox::AcceptRole);
    markDefaultButton(ok);
    ok->setDefault(true);
    chrome.footer->addStretch();
    chrome.footer->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    // La frappe commence dans le champ, et le texte est pris: on retape un
    // nom plus souvent qu'on ne le corrige.
    m_field->setFocus();
    m_field->selectAll();

    // Large, parce qu'on y écrit un nom; haute de ce qu'il faut, parce que
    // la phrase du dessous tient sur une ligne ou sur trois selon ce qu'on
    // renomme, et qu'un bas de boîte vide se voit.
    resize(560, std::max(200, sizeHint().height()));
}

QString NameDialog::name() const { return m_field->text().trimmed(); }
