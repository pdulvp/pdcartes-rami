#include "RivalSheet.h"

#include "DialogChrome.h"
#include "MenuStyle.h"
#include "Palette.h"
#include "Profiles.h"
#include "RivalCard.h"
#include "Rivals.h"
#include "SkillRow.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>

RivalSheet::RivalSheet(const QString &rivalId, QWidget *parent)
    : QDialog(parent), m_rivalId(rivalId) {
    const Rival &who = rivalById(rivalId);
    setWindowTitle(rivalName(who.id));
    const DialogChrome chrome = applyDialogChrome(this, rivalName(who.id));
    setMinimumWidth(880);

    // Sa carte, telle qu'au menu: le médaillon, le nom, le niveau, la jauge
    // et son palmarès sous ce profil. La même carte plutôt qu'un en-tête de
    // plus, pour que la fiche montre bien l'adversaire qu'on a cliqué.
    m_card = new RivalCard(who.id, this);
    m_card->setPickable(false);
    // Et pas d'aperçu au survol: les huit lignes qu'il résume sont juste
    // dessous, en entier.
    m_card->setGlanceWanted(false);
    chrome.fields->addWidget(m_card);
    chrome.fields->addSpacing(14);

    buildInnate(chrome.fields);
    chrome.fields->addSpacing(16);
    buildLearned(chrome.fields);
    chrome.fields->addStretch(1);

    m_asArrived = new QPushButton(tr("Comme à son arrivée"), this);
    m_cancel = new QPushButton(tr("Annuler"), this);
    m_commit = new QPushButton(tr("Changer et réapprendre"), this);
    m_close = new QPushButton(tr("Fermer"), this);
    markDefaultButton(m_close);

    chrome.footer->addWidget(m_asArrived);
    chrome.footer->addStretch();
    chrome.footer->addWidget(m_cancel);
    chrome.footer->addWidget(m_commit);
    chrome.footer->addWidget(m_close);

    connect(m_reset, &QToolButton::clicked, this, &RivalSheet::askResetExperience);
    connect(m_change, &QToolButton::clicked, this, [this] { setEditing(true); });
    connect(m_cancel, &QPushButton::clicked, this, [this] { setEditing(false); });
    connect(m_commit, &QPushButton::clicked, this, &RivalSheet::applyInnate);
    connect(m_close, &QPushButton::clicked, this, &QDialog::accept);
    connect(m_asArrived, &QPushButton::clicked, this, [this] {
        m_draft = rivalById(m_rivalId).innate;
        reread();
    });

    m_draft = rivalInnate(who.id);
    setEditing(false);
}

void RivalSheet::buildInnate(QVBoxLayout *rows) {
    auto *head = new QHBoxLayout;
    head->setSpacing(10);
    head->addWidget(dialogSectionHeading(this, tr("Ce qu'il est")), 1);
    m_change = iconButton(this,
                          {QStringLiteral("document-edit-symbolic"),
                           QStringLiteral("document-edit"), QStringLiteral("edit-entry"),
                           QStringLiteral("gtk-edit")},
                          tr("Changer"),
                          tr("Changer ce qu'il est — il réapprendra tout"));
    head->addWidget(m_change, 0, Qt::AlignVCenter);
    rows->addLayout(head);

    auto *aside = new QLabel(tr("Ne s'apprend pas, et ne bouge pas en jouant. Le changer lui "
                                "fait tout réapprendre."),
                             this);
    aside->setWordWrap(true);
    markHintLabel(aside);
    rows->addWidget(aside);
    rows->addSpacing(6);

    for (int i = 0; i < kInnateCount; ++i) {
        const Innate which = Innate(i);
        auto *row = new SkillRow(this);
        row->setTitle(innateSpec(which).name);
        row->setHue(innateSpec(which).hue);
        connect(row, &SkillRow::levelChosen, this, [this, i](int level) {
            m_draft.set(Innate(i), level);
            reread();
        });
        m_innate.append(row);
        rows->addWidget(row);
    }

    rows->addSpacing(6);
    m_warning = new QLabel(this);
    m_warning->setWordWrap(true);
    markHintLabel(m_warning);
    rows->addWidget(m_warning);
}

void RivalSheet::buildLearned(QVBoxLayout *rows) {
    auto *head = new QHBoxLayout;
    head->setSpacing(10);
    head->addWidget(dialogSectionHeading(this, tr("Ce qu'il a appris")), 1);
    m_reset = iconButton(this,
                         {QStringLiteral("edit-undo-symbolic"), QStringLiteral("edit-undo"),
                          QStringLiteral("view-refresh-symbolic"),
                          QStringLiteral("view-refresh")},
                         tr("Remettre à zéro"),
                         tr("Remettre son expérience à zéro — il garde son nom et ce qu'il est"));
    head->addWidget(m_reset, 0, Qt::AlignVCenter);
    rows->addLayout(head);
    m_learnedNote = new QLabel(this);
    m_learnedNote->setWordWrap(true);
    markHintLabel(m_learnedNote);
    rows->addWidget(m_learnedNote);
    rows->addSpacing(6);

    for (int i = 0; i < kLearnedCount; ++i) {
        const Learned which = Learned(i);
        auto *row = new SkillRow(this);
        row->setTitle(learnedSpec(which).name);
        row->setHue(learnedSpec(which).hue);
        m_learned.append(row);
        rows->addWidget(row);
    }
}

void RivalSheet::reread() {
    const Rival &who = rivalById(m_rivalId);
    const Learning learning = learningFor(who.id);
    // En édition, les lignes montrent ce qu'on est en train de choisir; hors
    // édition, ce qu'il est.
    const Innates innate = m_editing ? m_draft : rivalInnate(who.id);

    for (int i = 0; i < m_innate.size(); ++i) {
        const Innate which = Innate(i);
        const int level = innate.at(which);
        SkillRow *row = m_innate.at(i);
        row->setLevel(level, kInnateTop + 1);
        row->setWord(innateSpec(which).levels[level]);
        row->setSays(innateSpec(which).says[level]);
        row->setEditable(m_editing);
    }

    for (int i = 0; i < m_learned.size(); ++i) {
        const Learned which = Learned(i);
        const int xp = learning.pools.at(which);
        const int rung = rungOf(xp);
        SkillRow *row = m_learned.at(i);
        row->setLevel(rung, kRungCount);
        row->setRank(rungRoman(rung));
        row->setWord(learnedSpec(which).rungs[rung]);
        row->setSays(learnedSpec(which).says[rung]);
        // Ce qui manque pour le rang suivant, ou le total quand il n'y a plus
        // de rang à prendre.
        const int gap = rungGap(xp);
        row->setGauge(rungProgress(xp),
                      gap > 0 ? tr("%1 avant %2").arg(gap).arg(rungRoman(rung + 1))
                              : tr("%1 xp").arg(xp));
        row->setDim(m_editing);
    }

    m_learnedNote->setText(tr("%1 xp en tout, sur « %2 ». Chaque compétence a son expérience, "
                              "et c'est ce qu'il fait à la table qui la remplit.")
                               .arg(learning.xp)
                               .arg(profileName(currentProfile())));

    if (m_editing) {
        const bool moved = m_draft != rivalInnate(who.id);
        m_warning->setText(moved ? tr("%1 réapprendra tout : %2 xp, %3 manches et %4 sorties "
                                      "s'effacent. Ce qu'il est maintenant, il l'apprendra "
                                      "autrement.")
                                       .arg(rivalName(who.id))
                                       .arg(learning.xp)
                                       .arg(learning.handsPlayed)
                                       .arg(learning.handsWon)
                                 : tr("Cliquez une pastille pour poser un niveau. Rien n'est "
                                      "écrit tant que vous n'avez pas validé."));
    } else {
        m_warning->clear();
    }

    m_reset->setEnabled(learning.xp > 0 || learning.handsPlayed > 0);
    m_card->reread();
}

void RivalSheet::setEditing(bool editing) {
    m_editing = editing;
    if (!editing) m_draft = rivalInnate(m_rivalId);

    m_change->setVisible(!editing);
    m_reset->setVisible(!editing);
    m_close->setVisible(!editing);
    m_asArrived->setVisible(editing);
    m_cancel->setVisible(editing);
    m_commit->setVisible(editing);
    m_warning->setVisible(editing);
    reread();
}

void RivalSheet::askResetExperience() {
    const Learning learning = learningFor(m_rivalId);
    const auto answer = QMessageBox::question(
        this, tr("pdcartes-rami"),
        tr("Remettre l'expérience de %1 à zéro, sur « %2 » ? Ses %3 xp, ses %4 manches et son "
           "palmarès s'effacent. Il garde son nom et ce qu'il est.")
            .arg(rivalName(m_rivalId))
            .arg(profileName(currentProfile()))
            .arg(learning.xp)
            .arg(learning.handsPlayed),
        QMessageBox::Cancel | QMessageBox::Yes);
    if (answer != QMessageBox::Yes) return;
    resetRivalExperience(m_rivalId);
    reread();
    Q_EMIT changed();
}

void RivalSheet::applyInnate() {
    // Reposer les mêmes niveaux n'est pas un changement: on referme, et rien
    // ne s'efface.
    if (m_draft == rivalInnate(m_rivalId)) {
        setEditing(false);
        return;
    }

    const Learning learning = learningFor(m_rivalId);
    const auto answer = QMessageBox::question(
        this, tr("pdcartes-rami"),
        tr("Changer ce que %1 est ? Il réapprendra tout : %2 xp, %3 manches et son palmarès "
           "s'effacent, sur « %4 ».")
            .arg(rivalName(m_rivalId))
            .arg(learning.xp)
            .arg(learning.handsPlayed)
            .arg(profileName(currentProfile())),
        QMessageBox::Cancel | QMessageBox::Yes);
    if (answer != QMessageBox::Yes) return;

    // Les deux d'un seul coup, et c'est setRivalInnate qui s'en charge: les
    // niveaux écrits et l'expérience remise, sans que l'un puisse arriver
    // sans l'autre.
    setRivalInnate(m_rivalId, m_draft);
    setEditing(false);
    Q_EMIT changed();
}
