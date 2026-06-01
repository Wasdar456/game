#include "ui/TechButton.h"

#include <QLinearGradient>
#include <QPainter>
#include <QPaintEvent>
#include <QPropertyAnimation>

TechButton::TechButton(const QString &text, QWidget *parent)
    : QPushButton(text, parent)
{
    setCursor(Qt::PointingHandCursor);
    setFlat(true);
}

void TechButton::setAccentColor(const QColor &color) { m_accentColor = color; update(); }
void TechButton::setFontSize(int size) { m_fontSize = size; update(); }
void TechButton::setBorderRadius(qreal radius) { m_borderRadius = radius; update(); }

qreal TechButton::hoverProgress() const { return m_hoverProgress; }

void TechButton::setHoverProgress(qreal progress)
{
    if (qFuzzyCompare(m_hoverProgress, progress)) return;
    m_hoverProgress = progress;
    update();
}

void TechButton::enterEvent(QEnterEvent *event)
{
    QPushButton::enterEvent(event);
    QPropertyAnimation *anim = new QPropertyAnimation(this, "hoverProgress");
    anim->setDuration(160);
    anim->setStartValue(m_hoverProgress);
    anim->setEndValue(1.0);
    anim->setEasingCurve(QEasingCurve::OutCubic);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void TechButton::leaveEvent(QEvent *event)
{
    QPushButton::leaveEvent(event);
    QPropertyAnimation *anim = new QPropertyAnimation(this, "hoverProgress");
    anim->setDuration(220);
    anim->setStartValue(m_hoverProgress);
    anim->setEndValue(0.0);
    anim->setEasingCurve(QEasingCurve::OutCubic);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void TechButton::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    QRectF r = rect().adjusted(2, 2, -2, -2);
    const qreal hp = m_hoverProgress;
    const bool pressed = isDown();

    if (!isEnabled()) {
        p.setPen(QPen(QColor(88, 62, 42, 150), 2));
        p.setBrush(QColor(104, 79, 54, 175));
        p.drawRoundedRect(r, m_borderRadius, m_borderRadius);
        p.setPen(QColor(166, 142, 106));
        p.setFont(QFont("Microsoft YaHei", m_fontSize, QFont::Medium));
        p.drawText(r, Qt::AlignCenter, text());
        return;
    }

    if (hp > 0.01 && !pressed) {
        QColor glow = m_accentColor;
        glow.setAlphaF(0.20 * hp);
        p.setPen(Qt::NoPen);
        p.setBrush(glow);
        p.drawRoundedRect(r.adjusted(-5 * hp, -4 * hp, 5 * hp, 4 * hp),
                          m_borderRadius + 5 * hp, m_borderRadius + 5 * hp);
    }

    QPixmap sign(":/images/ui/sign_banner.png");
    if (!sign.isNull()) {
        p.drawPixmap(r.toRect(), sign);
    } else {
        QLinearGradient wood(r.topLeft(), r.bottomLeft());
        wood.setColorAt(0.0, QColor(146 + int(18 * hp), 94 + int(12 * hp), 47));
        wood.setColorAt(0.48, QColor(105 + int(16 * hp), 65 + int(10 * hp), 32));
        wood.setColorAt(1.0, QColor(72, 42, 24));
        p.setPen(QPen(QColor(55, 32, 20), 2));
        p.setBrush(wood);
        p.drawRoundedRect(r, m_borderRadius, m_borderRadius);
    }

    QColor overlay = pressed ? QColor(35, 20, 13, 78) : QColor(255, 231, 155, int(26 * hp));
    p.setPen(Qt::NoPen);
    p.setBrush(overlay);
    p.drawRoundedRect(r.adjusted(4, 4, -4, -4), qMax<qreal>(2, m_borderRadius - 3), qMax<qreal>(2, m_borderRadius - 3));

    QColor border = m_accentColor;
    border.setAlphaF(pressed ? 0.85 : 0.34 + 0.42 * hp);
    p.setPen(QPen(border, pressed ? 2.5 : 1.8));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(r.adjusted(1, 1, -1, -1), m_borderRadius, m_borderRadius);

    QRectF textR = r;
    if (pressed) {
        textR.translate(0, 1);
    }
    p.setPen(QColor(255, 241, 196));
    p.setFont(QFont("Microsoft YaHei", m_fontSize, QFont::DemiBold));
    p.drawText(textR, Qt::AlignCenter, text());
}
