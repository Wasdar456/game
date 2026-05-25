#include "ui/TechButton.h"

#include <QPainter>
#include <QPaintEvent>
#include <QPropertyAnimation>
#include <QLinearGradient>
#include <QRadialGradient>

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
    anim->setDuration(180);
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

    QRectF r = rect();
    qreal rad = m_borderRadius;
    bool pressed = isDown();
    bool enabled = isEnabled();

    // --- 禁用状态 ---
    if (!enabled) {
        p.setPen(QPen(QColor(40, 55, 80), 1));
        p.setBrush(QColor(15, 22, 38, 200));
        p.drawRoundedRect(r, rad, rad);
        p.setPen(QColor(60, 80, 110));
        p.setFont(QFont("Microsoft YaHei", m_fontSize, QFont::Medium));
        p.drawText(r, Qt::AlignCenter, text());
        return;
    }

    qreal hp = m_hoverProgress;

    // --- 外发光层（hover时扩展的光晕，模拟放大效果） ---
    if (hp > 0.01 && !pressed) {
        qreal expand = 4 * hp;
        QRectF glowRect = r.adjusted(-expand, -expand, expand, expand);
        QColor gc = m_accentColor;
        gc.setAlphaF(hp * 0.12);
        p.setPen(Qt::NoPen);
        p.setBrush(gc);
        p.drawRoundedRect(glowRect, rad + expand, rad + expand);
    }

    // --- 背景渐变 ---
    QLinearGradient bg(r.topLeft(), r.bottomLeft());
    if (pressed) {
        bg.setColorAt(0, QColor(6, 14, 30));
        bg.setColorAt(1, QColor(4, 8, 22));
    } else {
        bg.setColorAt(0, QColor(20 + int(28 * hp), 36 + int(40 * hp), 66 + int(55 * hp)));
        bg.setColorAt(1, QColor(10 + int(14 * hp), 20 + int(22 * hp), 42 + int(38 * hp)));
    }

    // --- 边框 ---
    QColor bc = m_accentColor;
    bc.setAlphaF(pressed ? 0.95 : (0.30 + 0.55 * hp));

    p.setPen(QPen(bc, pressed ? 2.0 : 1.5));
    p.setBrush(bg);
    p.drawRoundedRect(r, rad, rad);

    // --- 顶部高光线 ---
    if (hp > 0.01 && !pressed) {
        QLinearGradient tl(r.topLeft(), r.topRight());
        QColor ac = m_accentColor;
        tl.setColorAt(0.0, QColor(ac.red(), ac.green(), ac.blue(), 0));
        tl.setColorAt(0.25, QColor(ac.red(), ac.green(), ac.blue(), int(110 * hp)));
        tl.setColorAt(0.75, QColor(ac.red(), ac.green(), ac.blue(), int(110 * hp)));
        tl.setColorAt(1.0, QColor(ac.red(), ac.green(), ac.blue(), 0));
        p.setPen(QPen(QBrush(tl), 2));
        p.drawLine(QPointF(r.left() + rad, r.top() + 1), QPointF(r.right() - rad, r.top() + 1));
    }

    // --- 内部径向辉光 ---
    if (hp > 0.01 && !pressed) {
        QRadialGradient glow(r.center(), qMax(r.width(), r.height()) * 0.55);
        QColor gc2 = m_accentColor;
        gc2.setAlphaF(hp * 0.07);
        glow.setColorAt(0, gc2);
        glow.setColorAt(1, QColor(0, 0, 0, 0));
        p.setPen(Qt::NoPen);
        p.setBrush(glow);
        p.drawRoundedRect(r, rad, rad);
    }

    // --- 文字 ---
    QColor tc = pressed ? QColor(160, 200, 255) : QColor(255, 255, 255, int(210 + 45 * hp));
    QRectF textR = r;
    if (pressed) textR.translate(0.5, 1.0);
    p.setPen(tc);
    p.setFont(QFont("Microsoft YaHei", m_fontSize, QFont::Medium));
    p.drawText(textR, Qt::AlignCenter, text());
}
