#include "TitleFlight.h"

#include "Palette.h"
#include "TitleMark.h"

#include <QEasingCurve>
#include <QPainter>
#include <QVariantAnimation>

namespace {

// Durée de l'animation : suffisante pour que le déplacement du logo soit
// suivi du regard, assez brève pour ne pas retarder l'affichage du menu.
constexpr int kFlightMs = 520;

QRectF between(const QRectF &from, const QRectF &to, qreal along) {
    return QRectF(from.left() + (to.left() - from.left()) * along,
                  from.top() + (to.top() - from.top()) * along,
                  from.width() + (to.width() - from.width()) * along,
                  from.height() + (to.height() - from.height()) * along);
}

} // namespace

TitleFlight::TitleFlight(QWidget *parent) : QWidget(parent) {
    // Le calque est placé au-dessus de tout et ne capte aucun événement : les
    // clics atteignent la page sous-jacente, y compris pendant l'animation.
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_TranslucentBackground);
    hide();

    m_walk = new QVariantAnimation(this);
    m_walk->setDuration(kFlightMs);
    m_walk->setStartValue(0.0);
    m_walk->setEndValue(1.0);
    // Progression rapide au départ et ralentie à l'arrivée, ce qui produit un
    // mouvement d'amortissement plutôt qu'un déplacement linéaire.
    m_walk->setEasingCurve(QEasingCurve::InOutCubic);
    connect(m_walk, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        m_along = value.toReal();
        update();
    });
    connect(m_walk, &QVariantAnimation::finished, this, [this] {
        hide();
        Q_EMIT landed();
    });
}

void TitleFlight::fly(const QRectF &from, const QRectF &to) {
    m_walk->stop();
    m_from = from;
    m_to = to;
    m_along = 0.0;
    raise();
    show();
    m_walk->start();
}

void TitleFlight::cancel() {
    if (m_walk->state() != QAbstractAnimation::Running) return;
    m_walk->stop();
    hide();
    Q_EMIT landed();
}

void TitleFlight::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    drawTitleMark(&painter, between(m_from, m_to, m_along), font(), currentPalette());
}
