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

// Marge intérieure de l'aperçu, espacement entre ses deux colonnes, et
// interligne. Ces valeurs sont inférieures à celles de la fiche, ce qui
// constitue l'objet du mode compact : huit lignes à l'interligne de la fiche
// produiraient un panneau plus haut que la plaque survolée.
constexpr int kPad = 14;
constexpr int kBetween = 24;
constexpr int kTight = 1;
constexpr qreal kCorner = 10.0;

// Décalage entre l'aperçu et l'élément survolé, afin que leurs bords ne se
// touchent pas.
constexpr int kOffset = 6;

// Période de vérification de la position du pointeur, une fois l'aperçu
// affiché.
constexpr int kWatchMs = 120;

// Aperçu actuellement affiché et élément l'ayant demandé. Un seul aperçu est
// admis à la fois, deux aperçus simultanés décrivant deux adversaires
// différents.
QPointer<RivalGlance> g_glance;
QPointer<QWidget> g_about;

// Détermine la position de l'aperçu : sous l'élément survolé, au-dessus, ou
// à côté — la première des quatre positions tenant à l'écran sans recouvrir ni
// l'élément survolé ni le pointeur. Un panneau placé sous le pointeur capte le
// survol de l'élément qu'il décrit, ce qui provoquait un clignotement.
QRect glancePlace(const QSize &size, const QRect &avoid, const QRect &room, const QPoint &mouse) {
    const int w = size.width();
    const int h = size.height();
    // Aligné sur le bord qu'il longe, puis ramené dans les limites de
    // l'écran : un aperçu commençant au bord gauche de la carte se lit comme
    // son prolongement.
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

    // L'aperçu ne capte aucun événement : ni clic, ni focus, ni survol. À
    // défaut, un aperçu placé sous le pointeur ferait sortir celui-ci de la
    // carte l'ayant demandé, et l'aperçu se refermerait aussitôt.
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
            // Un aperçu qui n'est plus l'aperçu courant est déjà en cours de
            // fermeture : il ne doit pas fermer celui qui l'a remplacé.
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

    // Le panneau, bordé de la couleur de l'adversaire décrit : affiché sur le
    // tapis à côté de cinq autres plaques, c'est ce qui l'identifie sans
    // répéter son nom.
    const QRectF box = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    painter.setPen(QPen(blendColours(scheme.edge, mine, 0.55), 1.0));
    painter.setBrush(scheme.surface);
    painter.drawRoundedRect(box, kCorner, kCorner);
}

void showRivalGlance(const QString &rivalId, const QRect &avoid, QWidget *about) {
    if (!about || rivalId.isEmpty()) return;
    // Un aperçu déjà affiché est conservé : il n'est pas reconstruit pour le
    // même adversaire à la même position, la reconstruction étant visible.
    if (g_glance && g_about == about && g_glance->rivalId() == rivalId) {
        g_glance->watch(avoid);
        return;
    }
    hideRivalGlance();

    // Reconstruit à chaque affichage plutôt que conservé : il reflète
    // l'expérience et les couleurs courantes, qu'une manche jouée entre deux
    // survols peut avoir modifiées.
    auto *glance = new RivalGlance(rivalId, about);
    glance->adjustSize();
    const QSize size = glance->size();

    const QScreen *screen = about->screen() ? about->screen() : QGuiApplication::primaryScreen();
    const QRect room = screen ? screen->availableGeometry() : avoid;

    QRect place = glancePlace(size, avoid, room, QCursor::pos());
    if (place.isNull()) {
        // Aucune position ne permet d'éviter tout recouvrement : l'aperçu est
        // placé en dessous et ramené de force dans l'écran. Le pointeur peut
        // alors passer dessus, l'aperçu assurant désormais lui-même la
        // surveillance et ne se fermant plus sur un événement de sortie.
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
    // Le pointeur est sorti de la carte, ou bien l'aperçu vient de s'afficher
    // par-dessus, ce qui produit le même événement. L'aperçu est conservé tant
    // que le pointeur se trouve sur lui ; sa propre surveillance assurera sa
    // fermeture.
    if (g_glance->wanted()) return;
    hideRivalGlance();
}
