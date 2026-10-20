#include "RivalGlance.h"

#include "Palette.h"
#include "Rivals.h"
#include "SkillRow.h"

#include "DialogChrome.h"

#include <QCursor>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QPainter>
#include <QPointer>
#include <QScreen>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

namespace {

// Ce dont l'aperçu s'écarte de son bord, l'air entre ses deux moitiés, et ce
// qui sépare deux lignes: moins que sur la fiche, qui est tout l'objet du
// resserrement -- huit lignes à l'écart d'une fiche feraient un panneau plus
// haut que la plaque qu'on survole.
constexpr int kPad = 14;
constexpr int kBetween = 24;
constexpr int kTight = 1;
constexpr qreal kCorner = 10.0;

// De combien il se décale de ce qu'on survole, pour que le bord de l'un ne
// touche pas le bord de l'autre.
constexpr int kOffset = 6;

// À quelle cadence il regarde où est le pointeur, une fois posé.
constexpr int kWatchMs = 120;

// Celui qui est à l'écran, et celui qui l'a demandé. Un seul: deux aperçus
// ouverts en même temps seraient deux adversaires décrits à la fois.
QPointer<RivalGlance> g_glance;
QPointer<QWidget> g_about;

// Où le poser: sous ce qu'on survole, ou dessus, ou à côté -- le premier des
// quatre qui tienne à l'écran sans recouvrir ni ce qu'on survole ni le
// pointeur. Un panneau posé sous le pointeur prend le survol de ce qu'il
// décrit, et c'est de là que venait le clignotement.
QRect glancePlace(const QSize &size, const QRect &avoid, const QRect &room, const QPoint &mouse) {
    const int w = size.width();
    const int h = size.height();
    // Aligné sur le bord qu'il longe, puis ramené dans l'écran: un aperçu qui
    // commence au bord gauche de la carte se lit comme sa suite.
    const int x = std::max(room.left() + 4, std::min(avoid.left(), room.right() - w - 4));
    const int y = std::max(room.top() + 4, std::min(avoid.top(), room.bottom() - h - 4));

    const QRect tries[] = {
        QRect(x, avoid.bottom() + kOffset, w, h),       // dessous
        QRect(x, avoid.top() - h - kOffset, w, h),      // dessus
        QRect(avoid.right() + kOffset, y, w, h),        // à droite
        QRect(avoid.left() - w - kOffset, y, w, h),     // à gauche
    };
    for (const QRect &box : tries) {
        if (!room.contains(box)) continue;
        if (box.intersects(avoid) || box.contains(mouse)) continue;
        return box;
    }
    return QRect();
}

} // namespace

RivalGlance::RivalGlance(const QString &rivalId, QWidget *about)
    : QWidget(about, Qt::ToolTip | Qt::FramelessWindowHint), m_rivalId(rivalId) {
    m_hue = rivalById(rivalId).hue;

    // Il ne prend rien: ni le clic, ni le focus, ni le survol -- sans quoi
    // l'aperçu qui se pose sous le pointeur ferait sortir le pointeur de la
    // carte qui l'a demandé, et l'aperçu se refermerait aussitôt.
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setAttribute(Qt::WA_TranslucentBackground);
    setFocusPolicy(Qt::NoFocus);

    auto *columns = new QHBoxLayout(this);
    columns->setContentsMargins(kPad, kPad, kPad, kPad);
    columns->setSpacing(kBetween);

    auto *left = new QVBoxLayout;
    left->setSpacing(kTight);
    auto *right = new QVBoxLayout;
    right->setSpacing(kTight);
    columns->addLayout(left);
    columns->addLayout(right);

    buildInnate(left);
    buildLearned(right);
}

void RivalGlance::buildInnate(QVBoxLayout *column) {
    const Innates innate = rivalInnate(m_rivalId);

    column->addWidget(dialogSectionHeading(this, tr("Ce qu'il est")));
    column->addSpacing(5);
    for (int i = 0; i < kInnateCount; ++i) {
        const Innate which = Innate(i);
        const int level = innate.at(which);
        auto *row = new SkillRow(this);
        row->setCompact(true);
        row->setTitle(innateSpec(which).name);
        row->setHue(innateSpec(which).hue);
        row->setLevel(level, kInnateTop + 1);
        row->setWord(innateSpec(which).levels[level]);
        column->addWidget(row);
    }
    column->addStretch(1);
}

void RivalGlance::buildLearned(QVBoxLayout *column) {
    const Learning learning = learningFor(m_rivalId);

    column->addWidget(dialogSectionHeading(this, tr("Ce qu'il a appris")));
    column->addSpacing(5);
    for (int i = 0; i < kLearnedCount; ++i) {
        const Learned which = Learned(i);
        const int rung = rungOf(learning.pools.at(which));
        auto *row = new SkillRow(this);
        row->setCompact(true);
        row->setTitle(learnedSpec(which).name);
        row->setHue(learnedSpec(which).hue);
        row->setLevel(rung, kRungCount);
        row->setRank(rungRoman(rung));
        row->setWord(learnedSpec(which).rungs[rung]);
        column->addWidget(row);
    }
    column->addStretch(1);
}

void RivalGlance::watch(const QRect &avoid) {
    m_avoid = avoid;
    if (!m_watch) {
        m_watch = new QTimer(this);
        m_watch->setInterval(kWatchMs);
        connect(m_watch, &QTimer::timeout, this, [this] {
            // Celui qui n'est plus l'aperçu en cours est déjà en train de
            // partir: il n'a pas à fermer celui qui l'a remplacé.
            if (!wanted() && g_glance == this) hideRivalGlance();
        });
    }
    m_watch->start();
}

bool RivalGlance::wanted() const {
    const QPoint mouse = QCursor::pos();
    return m_avoid.contains(mouse) || frameGeometry().contains(mouse) ||
           geometry().contains(mouse);
}

void RivalGlance::paintEvent(QPaintEvent *) {
    const Palette &scheme = currentPalette();
    const QColor mine = rivalColour(m_hue);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    // Le panneau, et son bord dans la couleur de celui qu'il décrit: posé sur
    // le tapis, à côté de cinq autres plaques, c'est ce qui dit de qui il
    // parle sans avoir à le réécrire.
    const QRectF box = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    painter.setPen(QPen(blendColours(scheme.edge, mine, 0.55), 1.0));
    painter.setBrush(scheme.surface);
    painter.drawRoundedRect(box, kCorner, kCorner);
}

void showRivalGlance(const QString &rivalId, const QRect &avoid, QWidget *about) {
    if (!about || rivalId.isEmpty()) return;
    // Celui qui est déjà là reste là: on ne le refait pas pour le même
    // adversaire au même endroit, parce que le refaire se verrait.
    if (g_glance && g_about == about && g_glance->rivalId() == rivalId) {
        g_glance->watch(avoid);
        return;
    }
    hideRivalGlance();

    // Refait à chaque fois plutôt que gardé: il porte l'expérience du moment
    // et les couleurs du moment, et une manche jouée entre deux survols a
    // changé les deux.
    auto *glance = new RivalGlance(rivalId, about);
    glance->adjustSize();
    const QSize size = glance->size();

    const QScreen *screen = about->screen() ? about->screen() : QGuiApplication::primaryScreen();
    const QRect room = screen ? screen->availableGeometry() : avoid;

    QRect place = glancePlace(size, avoid, room, QCursor::pos());
    if (place.isNull()) {
        // Nulle part où le poser sans rien recouvrir: dessous, rentré de
        // force, et le pointeur peut bien passer dessus -- c'est lui qui
        // surveille maintenant, et il ne se referme plus sur un leave.
        const int x = std::max(room.left() + 4, std::min(avoid.left(), room.right() -
                                                                           size.width() - 4));
        const int y = std::max(room.top() + 4,
                               std::min(avoid.bottom() + kOffset,
                                        room.bottom() - size.height() - 4));
        place = QRect(QPoint(x, y), size);
    }

    glance->move(place.topLeft());
    glance->show();
    glance->watch(avoid);
    g_glance = glance;
    g_about = about;
}

void hideRivalGlance(const QWidget *about) {
    if (about && g_about && g_about != about) return;
    if (g_glance) {
        g_glance->hide();
        g_glance->deleteLater();
    }
    g_glance = nullptr;
    g_about = nullptr;
}

void releaseRivalGlance(const QWidget *about) {
    if (!g_glance) return;
    if (about && g_about && g_about != about) return;
    // Le pointeur est sorti de la carte -- ou bien l'aperçu vient de se poser
    // dessus, ce qui ressemble exactement à la même chose. Il reste tant qu'il
    // est sous le pointeur, et c'est sa surveillance qui le fermera.
    if (g_glance->wanted()) return;
    hideRivalGlance();
}
