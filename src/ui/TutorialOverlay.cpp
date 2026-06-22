#include "ui/TutorialOverlay.h"

#include <QLabel>
#include <QKeyEvent>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QResizeEvent>

namespace {

QString actionButtonStyle()
{
    return
        "QPushButton { color:#fff4cf;"
        " background:qlineargradient(x1:0,y1:0,x2:0,y2:1,"
        " stop:0 #dca64c, stop:1 #985622);"
        " border:3px solid #53331f; border-radius:7px;"
        " font-family:'Microsoft YaHei UI','PingFang SC',sans-serif;"
        " font-size:17px; font-weight:900; padding:7px 18px; }"
        "QPushButton:hover { border-color:#ffe19a; color:#ffffff; }"
        "QPushButton:pressed { background:#7f461f; padding-top:10px; }";
}

} // namespace

TutorialOverlay::TutorialOverlay(QWidget *parent)
    : QWidget(parent)
    , m_speakerLabel(new QLabel(this))
    , m_textLabel(new QLabel(this))
    , m_actionButton(new QPushButton(this))
    , m_skipButton(new QPushButton("Skip guide", this))
    , m_focus(Focus::None)
    , m_portraitAtlas(":/images/artwork/deck_atlas.png")
    , m_phase(0.0)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setFocusPolicy(Qt::StrongFocus);
    hide();

    m_speakerLabel->setStyleSheet(
        "color:#7b431d; background:transparent; border:none;"
        "font-family:'Microsoft YaHei UI','PingFang SC',sans-serif;"
        "font-size:22px; font-weight:900;");
    m_textLabel->setWordWrap(true);
    m_textLabel->setStyleSheet(
        "color:#402b1c; background:transparent; border:none;"
        "font-family:'Microsoft YaHei UI','PingFang SC',sans-serif;"
        "font-size:17px; font-weight:700;");
    m_actionButton->setCursor(Qt::PointingHandCursor);
    m_actionButton->setStyleSheet(actionButtonStyle());
    m_skipButton->setCursor(Qt::PointingHandCursor);
    m_skipButton->setStyleSheet(
        "QPushButton { color:#5f432b; background:rgba(255,244,211,160);"
        "border:1px solid rgba(85,51,31,150); border-radius:5px;"
        "font-size:13px; font-weight:700; padding:5px 10px; }"
        "QPushButton:hover { background:rgba(255,244,211,225); }");

    connect(m_actionButton, &QPushButton::clicked,
            this, &TutorialOverlay::signalAction);
    connect(m_skipButton, &QPushButton::clicked,
            this, &TutorialOverlay::signalSkip);

    m_pulseTimer.setInterval(33);
    connect(&m_pulseTimer, &QTimer::timeout, this, [this]() {
        m_phase += 0.07;
        update();
    });
}

void TutorialOverlay::setTargets(const QRect& resource,
                                 const QRect& cards,
                                 const QRect& battlefield,
                                 const QRect& core)
{
    m_resourceTarget = resource;
    m_cardsTarget = cards;
    m_battlefieldTarget = battlefield;
    m_coreTarget = core;
    updateLayout();
    update();
}

void TutorialOverlay::showStep(const QString& speaker,
                               const QString& text,
                               Focus focus,
                               const QString& actionText)
{
    m_speakerLabel->setText(speaker);
    m_textLabel->setText(text);
    m_actionButton->setText(actionText);
    m_focus = focus;
    show();
    raise();
    setFocus(Qt::OtherFocusReason);
    updateLayout();
    if (!m_pulseTimer.isActive()) {
        m_pulseTimer.start();
    }
    update();
}

void TutorialOverlay::closeOverlay()
{
    hide();
    m_pulseTimer.stop();
    m_focus = Focus::None;
}

QRect TutorialOverlay::focusRect() const
{
    switch (m_focus) {
    case Focus::Resource: return m_resourceTarget;
    case Focus::Cards: return m_cardsTarget;
    case Focus::Battlefield: return m_battlefieldTarget;
    case Focus::Core: return m_coreTarget;
    case Focus::None: break;
    }
    return {};
}

void TutorialOverlay::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    QPainterPath shade;
    shade.setFillRule(Qt::OddEvenFill);
    shade.addRect(rect());
    const QRect target = focusRect().adjusted(-10, -10, 10, 10);
    if (target.isValid()) {
        shade.addRoundedRect(target, 12, 12);
    }
    painter.fillPath(shade, QColor(12, 18, 15, 178));

    if (target.isValid()) {
        const int glowAlpha = qRound(145 + (qSin(m_phase) + 1.0) * 42);
        painter.setPen(QPen(QColor(255, 218, 112, glowAlpha), 5));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(target, 12, 12);
        painter.setPen(QPen(QColor(255, 245, 194, 115), 2));
        painter.drawRoundedRect(target.adjusted(-7, -7, 7, 7), 16, 16);
    }

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(18, 12, 8, 105));
    painter.drawRoundedRect(m_panelRect.translated(0, 8), 10, 10);

    QLinearGradient parchment(m_panelRect.topLeft(), m_panelRect.bottomLeft());
    parchment.setColorAt(0.0, QColor(250, 231, 184));
    parchment.setColorAt(0.55, QColor(237, 205, 145));
    parchment.setColorAt(1.0, QColor(210, 165, 96));
    painter.setBrush(parchment);
    painter.setPen(QPen(QColor(79, 49, 27), 4));
    painter.drawRoundedRect(m_panelRect, 9, 9);
    painter.setPen(QPen(QColor(171, 119, 55, 155), 2));
    painter.drawRoundedRect(m_panelRect.adjusted(9, 9, -9, -9), 6, 6);

    const QRectF portraitRect(m_panelRect.left() + 24,
                              m_panelRect.top() + 20,
                              145,
                              m_panelRect.height() - 40);
    QPainterPath portraitClip;
    portraitClip.addRoundedRect(portraitRect, 8, 8);
    painter.save();
    painter.setClipPath(portraitClip);
    painter.fillRect(portraitRect, QColor(247, 224, 172));
    if (!m_portraitAtlas.isNull()) {
        painter.drawPixmap(portraitRect,
                           m_portraitAtlas,
                           QRectF(262, 192, 178, 235));
    }
    painter.restore();
    painter.setPen(QPen(QColor(92, 57, 31), 3));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(portraitRect, 8, 8);
}

void TutorialOverlay::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateLayout();
}

void TutorialOverlay::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        emit signalSkip();
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

void TutorialOverlay::updateLayout()
{
    const QRect target = focusRect();
    const int panelWidth = qMin(1160, qMax(760, width() - 70));
    const int panelHeight = qMin(235, qMax(205, height() / 3));
    const bool targetNearBottom = target.isValid() && target.center().y() > height() * 0.62;
    const int panelY = targetNearBottom
                           ? 34
                           : height() - panelHeight - 34;
    m_panelRect = QRectF((width() - panelWidth) / 2.0,
                         panelY,
                         panelWidth,
                         panelHeight);

    const int left = qRound(m_panelRect.left());
    const int top = qRound(m_panelRect.top());
    const int right = qRound(m_panelRect.right());
    m_speakerLabel->setGeometry(left + 190, top + 24,
                                right - left - 450, 34);
    m_textLabel->setGeometry(left + 190, top + 60,
                             right - left - 450,
                             qRound(m_panelRect.height()) - 82);
    m_actionButton->setGeometry(right - 244, top + 72, 210, 58);
    m_skipButton->setGeometry(right - 164,
                              qRound(m_panelRect.bottom()) - 38,
                              130, 26);
}
