#include "ui/ArtHotspot.h"
#include "ui/AudioManager.h"

#include <QEasingCurve>
#include <QEnterEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QShowEvent>
#include <QTimer>
#include <QtMath>

namespace {
constexpr qreal kHoverScale = 1.045;
constexpr qreal kPressedScale = 0.985;
}

ArtHotspot::ArtHotspot(const QString &imagePath, const QRect &sourceRect,
                       QWidget *parent)
    : QWidget(parent)
    , m_sourceImage(imagePath)
    , m_sourceRect(sourceRect)
    , m_animation(new QVariantAnimation(this))
    , m_glowColor(255, 220, 128)
    , m_scale(1.0)
    , m_glow(0.0)
    , m_selected(false)
    , m_pressed(false)
    , m_swayEnabled(false)
    , m_idlePhase(sourceRect.x() * 0.013 + sourceRect.y() * 0.007)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setMouseTracking(true);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::StrongFocus);

    m_animation->setDuration(135);
    m_animation->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_animation, &QVariantAnimation::valueChanged, this,
            [this](const QVariant &value) {
                const QPointF state = value.toPointF();
                m_scale = state.x();
                m_glow = state.y();
                update();
            });

    auto *idleTimer = new QTimer(this);
    idleTimer->setInterval(40);
    connect(idleTimer, &QTimer::timeout, this, [this]() {
        if (m_swayEnabled && isVisible()) {
            m_idlePhase += 0.035;
            update();
        }
    });
    idleTimer->start();
}

void ArtHotspot::setCanvasRect(const QRectF &rect)
{
    const qreal padding = qMax<qreal>(10.0, qMin(rect.width(), rect.height()) * 0.10);
    setGeometry(rect.adjusted(-padding, -padding, padding, padding).toAlignedRect());
    m_baseSize = rect.size();
    update();
}

void ArtHotspot::setClickHandler(std::function<void()> handler)
{
    m_clickHandler = std::move(handler);
}

void ArtHotspot::setSelected(bool selected)
{
    m_selected = selected;
    m_glow = selected ? 1.0 : 0.0;
    update();
}

void ArtHotspot::setGlowColor(const QColor &color)
{
    m_glowColor = color;
    update();
}

void ArtHotspot::setSwayEnabled(bool enabled)
{
    m_swayEnabled = enabled;
    update();
}

void ArtHotspot::refreshVisual()
{
    m_animation->stop();
    m_scale = 1.0;
    m_glow = m_selected ? 1.0 : 0.0;
    m_pressed = false;
    show();
    raise();
    update();
}

void ArtHotspot::enterEvent(QEnterEvent *event)
{
    QWidget::enterEvent(event);
    animateTo(kHoverScale, 1.0);
}

void ArtHotspot::leaveEvent(QEvent *event)
{
    QWidget::leaveEvent(event);
    m_pressed = false;
    animateTo(1.0, m_selected ? 1.0 : 0.0);
}

void ArtHotspot::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_pressed = true;
        animateTo(kPressedScale, 1.0);
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void ArtHotspot::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_pressed) {
        m_pressed = false;
        const bool inside = rect().contains(event->pos());
        animateTo(inside ? kHoverScale : 1.0,
                  inside || m_selected ? 1.0 : 0.0);
        if (inside && m_clickHandler) {
            AudioManager::instance().playWoodClick();
            m_clickHandler();
        }
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void ArtHotspot::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    QRectF drawRect(QPointF(0, 0), m_baseSize * m_scale);
    drawRect.moveCenter(QRectF(rect()).center());
    const qreal sway = m_swayEnabled
        ? qSin(m_idlePhase) * 0.7 + (m_pressed ? -0.8 : 0.0)
        : 0.0;

    painter.translate(drawRect.center());
    painter.rotate(sway);
    painter.translate(-drawRect.center());

    if (!m_sourceImage.isNull()) {
        painter.drawPixmap(drawRect, m_sourceImage, QRectF(m_sourceRect));
    }

    const qreal emphasis = qMax(m_glow, m_selected ? 0.82 : 0.0);
    if (emphasis > 0.01 && m_glowColor.alpha() > 0) {
        QColor wash = m_glowColor;
        wash.setAlphaF(0.07 * emphasis);
        painter.setPen(Qt::NoPen);
        painter.setBrush(wash);
        painter.drawRoundedRect(drawRect.adjusted(2, 2, -2, -2), 8, 8);

        QColor outer = m_glowColor;
        outer.setAlphaF(0.28 * emphasis);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(outer, 8.0));
        painter.drawRoundedRect(drawRect.adjusted(2, 2, -2, -2), 9, 9);

        QColor edge = m_glowColor;
        edge.setAlphaF(0.92 * emphasis);
        painter.setPen(QPen(edge, m_selected ? 3.2 : 2.2));
        painter.drawRoundedRect(drawRect.adjusted(2, 2, -2, -2), 8, 8);
    }
}

void ArtHotspot::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    refreshVisual();
}

void ArtHotspot::animateTo(qreal scale, qreal glow)
{
    m_animation->stop();
    m_animation->setStartValue(QPointF(m_scale, m_glow));
    m_animation->setEndValue(QPointF(scale, glow));
    m_animation->start();
}
