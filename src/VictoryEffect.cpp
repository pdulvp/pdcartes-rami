#include "VictoryEffect.h"

#include "CardArt.h"
#include "Palette.h"

#include <QFontMetricsF>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QRandomGenerator>
#include <QTimer>

#include <algorithm>
#include <cmath>

namespace {

constexpr int kFrameMs = 16;

// Combien de temps la chose dure. Assez pour lire la feuille de marque,
// assez peu pour que personne n'attende qu'elle finisse.
constexpr int kFrames = 360;

constexpr qreal kGravity = 0.40;
constexpr qreal kBounce = 0.56;
constexpr qreal kDrag = 0.995;

constexpr int kBurstEvery = 26;
constexpr int kSparksPerBurst = 44;
constexpr int kSparkLife = 56;
constexpr qreal kSparkGravity = 0.07;

constexpr int kCardFrom = 20;
constexpr int kCardOver = 40;

// Pas plus que ça de cartes en l'air. Cent huit cartes qui rebondissent en
// même temps, ce n'est pas une fête, c'est du grésil.
constexpr int kMostFlyers = 64;

qreal randomBetween(qreal low, qreal high) {
    return low + QRandomGenerator::global()->generateDouble() * (high - low);
}

} // namespace

VictoryEffect::VictoryEffect(QWidget *parent) : QWidget(parent) {
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::PointingHandCursor);
    setAttribute(Qt::WA_NoSystemBackground);
    hide();

    m_ticker = new QTimer(this);
    m_ticker->setInterval(kFrameMs);
    connect(m_ticker, &QTimer::timeout, this, &VictoryEffect::tick);
}

void VictoryEffect::start(const QVector<Card> &cards, const QVector<QPointF> &at, qreal height,
                          const QString &headline, const QString &aside) {
    m_headline = headline;
    m_aside = aside;
    m_cardHeight = std::max(30.0, height);
    m_frame = 0;
    m_sparks.clear();
    m_flyers.clear();

    if (parentWidget()) setGeometry(parentWidget()->rect());

    const int count = std::min(cards.size(), at.size());
    const int step = std::max(1, (count + kMostFlyers - 1) / kMostFlyers);
    for (int i = 0; i < count; i += step) {
        Flyer flyer;
        flyer.card = cards.at(i);
        flyer.at = at.at(i);
        // Lancées vers le haut et vers l'extérieur, pour que le tapis ait
        // l'air de se défaire plutôt que de se renverser.
        const qreal fromMiddle = flyer.at.x() - width() / 2.0;
        flyer.speed = QPointF(fromMiddle * 0.013 + randomBetween(-1.8, 1.8),
                              randomBetween(-12.0, -5.5));
        flyer.angle = randomBetween(-16.0, 16.0);
        flyer.spin = randomBetween(-5.5, 5.5);
        m_flyers.append(flyer);
    }

    show();
    raise();
    setFocus();
    m_ticker->start();
    update();
}

void VictoryEffect::stop() {
    m_ticker->stop();
    m_flyers.clear();
    m_sparks.clear();
    hide();
}

void VictoryEffect::burst(const QPointF &at) {
    const int shade = QRandomGenerator::global()->bounded(14);
    for (int i = 0; i < kSparksPerBurst; ++i) {
        const qreal angle = randomBetween(0.0, 2.0 * M_PI);
        const qreal speed = randomBetween(1.4, 5.0);
        Spark spark;
        spark.at = at;
        spark.speed = QPointF(std::cos(angle) * speed, std::sin(angle) * speed);
        spark.span = kSparkLife + QRandomGenerator::global()->bounded(18);
        spark.life = spark.span;
        spark.shade = shade + (i % 3);
        spark.size = randomBetween(2.2, 4.2);
        m_sparks.append(spark);
    }
}

void VictoryEffect::tick() {
    ++m_frame;

    if (m_frame % kBurstEvery == 1 && m_frame < kFrames - 70) {
        burst(QPointF(randomBetween(width() * 0.12, width() * 0.88),
                      randomBetween(height() * 0.10, height() * 0.52)));
    }

    const qreal floor = height() - m_cardHeight * 0.5;
    for (Flyer &flyer : m_flyers) {
        flyer.speed.setY(flyer.speed.y() + kGravity);
        flyer.speed *= kDrag;
        flyer.at += flyer.speed;
        flyer.angle += flyer.spin;

        if (flyer.at.y() > floor) {
            flyer.at.setY(floor);
            flyer.speed.setY(-flyer.speed.y() * kBounce);
            flyer.speed.setX(flyer.speed.x() * 0.86);
            flyer.spin *= 0.68;
            if (std::abs(flyer.speed.y()) < 1.2) flyer.speed.setY(0.0);
        }
        const qreal edge = m_cardHeight * 0.4;
        if (flyer.at.x() < edge) {
            flyer.at.setX(edge);
            flyer.speed.setX(std::abs(flyer.speed.x()) * 0.7);
        } else if (flyer.at.x() > width() - edge) {
            flyer.at.setX(width() - edge);
            flyer.speed.setX(-std::abs(flyer.speed.x()) * 0.7);
        }
    }

    for (Spark &spark : m_sparks) {
        spark.speed.setY(spark.speed.y() + kSparkGravity);
        spark.at += spark.speed;
        --spark.life;
    }
    m_sparks.erase(std::remove_if(m_sparks.begin(), m_sparks.end(),
                                  [](const Spark &spark) { return spark.life <= 0; }),
                   m_sparks.end());

    update();
    if (m_frame >= kFrames) {
        m_ticker->stop();
        Q_EMIT finished();
    }
}

void VictoryEffect::mousePressEvent(QMouseEvent *) { Q_EMIT finished(); }
void VictoryEffect::keyPressEvent(QKeyEvent *) { Q_EMIT finished(); }

void VictoryEffect::paintEvent(QPaintEvent *) {
    const Palette &scheme = currentPalette();

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    QColor veil = scheme.feltEdge;
    veil.setAlpha(196);
    painter.fillRect(rect(), veil);

    for (const Spark &spark : m_sparks) {
        const qreal left = spark.life / qreal(spark.span);
        QColor colour = festiveColour(spark.shade);
        colour.setAlphaF(std::clamp(left * left, 0.0, 1.0));
        painter.setPen(QPen(colour, spark.size, Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(spark.at, spark.at - spark.speed * 2.1);
    }

    const QSizeF card = cardSizeForHeight(m_cardHeight);
    for (const Flyer &flyer : m_flyers) {
        painter.save();
        painter.translate(flyer.at);
        painter.rotate(flyer.angle);
        drawCardFace(&painter,
                     QRectF(-card.width() / 2.0, -card.height() / 2.0, card.width(),
                            card.height()),
                     flyer.card, scheme);
        painter.restore();
    }

    const qreal appeared =
        std::clamp((m_frame - qreal(kCardFrom)) / (kCardOver - kCardFrom), 0.0, 1.0);
    if (appeared <= 0.0) return;

    QFont title = font();
    title.setBold(true);
    title.setCapitalization(QFont::AllUppercase);
    title.setLetterSpacing(QFont::PercentageSpacing, 170);
    title.setPixelSize(15);

    QFont big = font();
    big.setBold(true);
    big.setPixelSize(width() > 820 ? 42 : 32);

    const QFontMetricsF titleMetrics(title);
    const QFontMetricsF bigMetrics(big);

    const qreal cardWidth =
        std::max(bigMetrics.horizontalAdvance(m_headline), titleMetrics.horizontalAdvance(m_aside)) +
        82;
    const qreal cardHeight = titleMetrics.height() + bigMetrics.height() + 44;
    QRectF plaque(0, 0, cardWidth, cardHeight);
    plaque.moveCenter(QPointF(width() / 2.0, height() * 0.40));
    plaque.translate(0, (1.0 - appeared) * 14.0);

    painter.setOpacity(appeared);
    QPainterPath shape;
    shape.addRoundedRect(plaque, 16, 16);
    painter.setPen(Qt::NoPen);
    painter.fillPath(shape, scheme.surface);
    painter.setPen(QPen(scheme.gold, 2.4));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(shape);

    qreal y = plaque.top() + 16;
    painter.setFont(big);
    painter.setPen(scheme.gold);
    painter.drawText(QRectF(plaque.left(), y, plaque.width(), bigMetrics.height()),
                     Qt::AlignCenter, m_headline);
    y += bigMetrics.height() + 6;

    painter.setFont(title);
    painter.setPen(scheme.hint);
    painter.drawText(QRectF(plaque.left(), y, plaque.width(), titleMetrics.height()),
                     Qt::AlignCenter, m_aside);
    painter.setOpacity(1.0);
}
