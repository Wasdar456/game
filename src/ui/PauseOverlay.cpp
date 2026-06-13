#include "ui/PauseOverlay.h"

#include <QFrame>
#include <QGraphicsOpacityEffect>
#include <QLabel>
#include <QPainter>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>
#include <QtMath>

namespace {

QString buttonStyle(bool primary)
{
    const QString top = primary ? "#e7b957" : "#c58a4d";
    const QString bottom = primary ? "#a96322" : "#83502f";
    return QString(
        "QPushButton {"
        " color:#fff1c9;"
        " background:qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 %1,stop:1 %2);"
        " border:3px solid #53331f; border-radius:7px;"
        " font-family:'Microsoft YaHei UI','PingFang SC',sans-serif;"
        " font-size:18px; font-weight:900;"
        "}"
        "QPushButton:hover { border-color:#ffe19a; color:#ffffff; }"
        "QPushButton:pressed { padding-top:3px; background:%2; }")
        .arg(top, bottom);
}

} // namespace

PauseOverlay::PauseOverlay(QWidget *parent)
    : QWidget(parent)
    , m_panel(nullptr)
    , m_modeLabel(nullptr)
    , m_resumeButton(nullptr)
    , m_restartButton(nullptr)
    , m_exitButton(nullptr)
    , m_phase(0.0)
{
    setAttribute(Qt::WA_StyledBackground, true);
    hide();

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(24, 24, 24, 24);
    outer->addStretch();

    m_panel = new QFrame(this);
    m_panel->setFixedSize(470, 470);
    m_panel->setStyleSheet(
        "QFrame {"
        " background:qlineargradient(x1:0,y1:0,x2:0,y2:1,"
        " stop:0 rgba(249,226,174,248), stop:1 rgba(211,168,103,248));"
        " border:4px solid #55351f; border-radius:9px;"
        "}");
    auto *panelLayout = new QVBoxLayout(m_panel);
    panelLayout->setContentsMargins(48, 34, 48, 38);
    panelLayout->setSpacing(15);

    auto *title = new QLabel("战斗暂停", m_panel);
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet(
        "color:#633c1e; font-family:'Microsoft YaHei UI','PingFang SC',sans-serif;"
        "font-size:38px; font-weight:900; background:transparent; border:none;");
    panelLayout->addWidget(title);

    m_modeLabel = new QLabel("SINGLE PVE · 时间已停止", m_panel);
    m_modeLabel->setAlignment(Qt::AlignCenter);
    m_modeLabel->setStyleSheet(
        "color:#7a5735; font-size:14px; font-weight:800;"
        "background:transparent; border:none;");
    panelLayout->addWidget(m_modeLabel);
    panelLayout->addSpacing(12);

    m_resumeButton = new QPushButton("继续战斗", m_panel);
    m_restartButton = new QPushButton("重新开始", m_panel);
    m_exitButton = new QPushButton("返回大厅", m_panel);
    for (QPushButton *button : {m_resumeButton, m_restartButton, m_exitButton}) {
        button->setFixedHeight(60);
        button->setCursor(Qt::PointingHandCursor);
        button->setStyleSheet(buttonStyle(button == m_resumeButton));
        panelLayout->addWidget(button);
    }

    auto *tip = new QLabel("按 Esc 也可以继续战斗", m_panel);
    tip->setAlignment(Qt::AlignCenter);
    tip->setStyleSheet(
        "color:rgba(84,55,31,175); font-size:13px;"
        "background:transparent; border:none;");
    panelLayout->addStretch();
    panelLayout->addWidget(tip);

    outer->addWidget(m_panel, 0, Qt::AlignHCenter);
    outer->addStretch();

    connect(m_resumeButton, &QPushButton::clicked, this, &PauseOverlay::signalResume);
    connect(m_restartButton, &QPushButton::clicked, this, &PauseOverlay::signalRestart);
    connect(m_exitButton, &QPushButton::clicked, this, &PauseOverlay::signalExitToLobby);

    auto *timer = new QTimer(this);
    timer->setInterval(40);
    connect(timer, &QTimer::timeout, this, [this]() {
        if (isVisible()) {
            m_phase += 0.04;
            update();
        }
    });
    timer->start();
}

void PauseOverlay::setPvpMode(bool pvp)
{
    m_modeLabel->setText(pvp
        ? "MULTIPLAYER PVP · 本地画面已暂停"
        : "SINGLE PVE · 时间已停止");
    m_restartButton->setText(pvp ? "返回部署" : "重新开始");
}

void PauseOverlay::open()
{
    show();
    raise();
    auto *effect = new QGraphicsOpacityEffect(this);
    setGraphicsEffect(effect);
    auto *animation = new QPropertyAnimation(effect, "opacity", this);
    animation->setDuration(180);
    animation->setStartValue(0.0);
    animation->setEndValue(1.0);
    connect(animation, &QPropertyAnimation::finished, this, [this]() {
        setGraphicsEffect(nullptr);
    });
    animation->start(QAbstractAnimation::DeleteWhenStopped);
}

void PauseOverlay::closeMenu()
{
    hide();
    setGraphicsEffect(nullptr);
}

void PauseOverlay::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(rect(), QColor(18, 20, 15, 188));

    for (int i = 0; i < 18; ++i) {
        const qreal x = width() * (0.04 + (i % 9) * 0.115)
                        + qSin(m_phase + i) * 18.0;
        const qreal y = std::fmod(i * 83.0 + m_phase * (12 + i % 4),
                                 height() + 60.0) - 30.0;
        painter.save();
        painter.translate(x, y);
        painter.rotate(m_phase * 28.0 + i * 21.0);
        painter.setPen(Qt::NoPen);
        painter.setBrush(i % 2 ? QColor(119, 139, 65, 105)
                               : QColor(73, 106, 52, 118));
        painter.drawEllipse(QRectF(-8, -3, 16, 6));
        painter.restore();
    }
}
