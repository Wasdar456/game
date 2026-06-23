#include "ui/LeafTransitionOverlay.h"

#include <QEasingCurve>
#include <QPainter>
#include <QVariantAnimation>
#include <QtMath>

LeafTransitionOverlay::LeafTransitionOverlay(QWidget *parent)
    : QWidget(parent)
    , m_animation(new QVariantAnimation(this))
    , m_smoke(":/images/new_art/battle_smoke.png")
    , m_progress(0.0)
{
    setAttribute(Qt::WA_TranslucentBackground, true);
    hide();

    m_animation->setDuration(1350);
    m_animation->setStartValue(0.0);
    m_animation->setEndValue(1.0);
    m_animation->setEasingCurve(QEasingCurve::InOutCubic);
    connect(m_animation, &QVariantAnimation::valueChanged, this,
            [this](const QVariant &value) {
                m_progress = value.toReal();
                update();
            });
    connect(m_animation, &QVariantAnimation::finished, this, [this]() {
        hide();
        if (m_finishedAction) {
            auto action = std::move(m_finishedAction);
            action();
        }
    });
}

void LeafTransitionOverlay::play(std::function<void()> revealAction,
                                 std::function<void()> finishedAction)
{
    if (isRunning()) {
        return;
    }
    m_finishedAction = std::move(finishedAction);
    m_progress = 0.0;
    if (revealAction) {
        revealAction();
    }
    setGeometry(parentWidget()->rect());
    if (m_smokeViewport != size()) {
        m_smokeViewport = size();
        m_scaledSmoke = m_smoke.scaled(size(),
                                       Qt::KeepAspectRatioByExpanding,
                                       Qt::SmoothTransformation);
    }
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
    const qreal reveal = m_progress;
    const qreal remaining = 1.0 - reveal;
    painter.fillRect(rect(), QColor(18, 16, 12, qRound(remaining * 168)));

    if (!m_scaledSmoke.isNull()) {
        const qreal scale = 1.0 + reveal * 0.13;
        const QSizeF smokeSize(m_scaledSmoke.width() * scale,
                               m_scaledSmoke.height() * scale);
        const QPointF topLeft((width() - smokeSize.width()) * 0.5,
                              (height() - smokeSize.height()) * 0.5);
        painter.save();
        painter.setOpacity(qPow(remaining, 1.35));
        painter.drawPixmap(QRectF(topLeft, smokeSize),
                           m_scaledSmoke, QRectF(m_scaledSmoke.rect()));
        painter.restore();
    }
}
