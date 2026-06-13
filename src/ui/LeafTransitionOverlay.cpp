#include "ui/LeafTransitionOverlay.h"

#include <QEasingCurve>
#include <QPainter>
#include <QPainterPath>
#include <QVariantAnimation>
#include <QtMath>

namespace {
constexpr qreal Pi = 3.14159265358979323846;
}

LeafTransitionOverlay::LeafTransitionOverlay(QWidget *parent)
    : QWidget(parent)
    , m_animation(new QVariantAnimation(this))
    , m_progress(0.0)
    , m_midpointDone(false)
{
    setAttribute(Qt::WA_TranslucentBackground, true);
    hide();

    m_animation->setDuration(620);
    m_animation->setStartValue(0.0);
    m_animation->setEndValue(1.0);
    m_animation->setEasingCurve(QEasingCurve::InOutCubic);
    connect(m_animation, &QVariantAnimation::valueChanged, this,
            [this](const QVariant &value) {
                m_progress = value.toReal();
                if (!m_midpointDone && m_progress >= 0.50) {
                    m_midpointDone = true;
                    if (m_midpointAction) {
                        m_midpointAction();
                    }
                }
                update();
            });
    connect(m_animation, &QVariantAnimation::finished, this, [this]() {
        hide();
        m_midpointAction = {};
    });
}

void LeafTransitionOverlay::play(std::function<void()> midpointAction)
{
    if (isRunning()) {
        return;
    }
    m_midpointAction = std::move(midpointAction);
    m_midpointDone = false;
    m_progress = 0.0;
    setGeometry(parentWidget()->rect());
    show();
    raise();
    m_animation->start();
}

bool LeafTransitionOverlay::isRunning() const
{
    return m_animation->state() == QAbstractAnimation::Running;
}

void LeafTransitionOverlay::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const qreal cover = qSin(m_progress * Pi);
    painter.fillRect(rect(), QColor(31, 28, 18, qRound(cover * 196)));

    const int leafCount = 34;
    for (int i = 0; i < leafCount; ++i) {
        const qreal lane = (i + 0.5) / leafCount;
        const qreal direction = (i % 2 == 0) ? 1.0 : -1.0;
        const qreal startX = direction > 0 ? -100.0 : width() + 100.0;
        const qreal endX = direction > 0 ? width() + 100.0 : -100.0;
        const qreal local = qBound(0.0, m_progress * 1.45 - (i % 7) * 0.035, 1.0);
        const qreal x = startX + (endX - startX) * local;
        const qreal y = height() * lane
                        + qSin(local * 8.0 + i * 0.71) * (22.0 + i % 4 * 5.0);
        const qreal size = 18.0 + (i % 5) * 4.0;

        painter.save();
        painter.translate(x, y);
        painter.rotate(direction * (local * 320.0 + i * 19.0));
        QPainterPath leaf;
        leaf.moveTo(-size, 0);
        leaf.cubicTo(-size * 0.35, -size * 0.72,
                     size * 0.55, -size * 0.52,
                     size, 0);
        leaf.cubicTo(size * 0.4, size * 0.56,
                     -size * 0.42, size * 0.65,
                     -size, 0);
        const QColor colors[] = {
            QColor(73, 112, 55, 238),
            QColor(120, 139, 62, 238),
            QColor(173, 143, 57, 236),
            QColor(67, 91, 48, 240)
        };
        painter.setPen(QPen(QColor(45, 63, 34, 210), 1.2));
        painter.setBrush(colors[i % 4]);
        painter.drawPath(leaf);
        painter.drawLine(QPointF(-size * 0.72, 0), QPointF(size * 0.72, 0));
        painter.restore();
    }

}
