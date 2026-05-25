/**
 * @file StartPage.cpp
 * @brief 起始页面实现 —— 蓝白未来科技风格（明日方舟式）
 */

#include "ui/StartPage.h"

#include <QVBoxLayout>
#include <QFont>
#include <QPainter>
#include <QPaintEvent>
#include <QPropertyAnimation>
#include <QSequentialAnimationGroup>
#include <QGraphicsOpacityEffect>
#include <QRandomGenerator>
#include <QtMath>
#include <random>
#include <algorithm>
#include <QDebug>

// ============================================================================
// ParticleWidget —— 粒子星空（蓝白色调）
// ============================================================================

ParticleWidget::ParticleWidget(QWidget *parent)
    : QWidget(parent)
    , m_frame(0)
{
    initParticles();

    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, [this]() {
        m_frame++;
        for (auto &p : m_particles) {
            p.x += p.vx;
            p.y += p.vy;
            p.opacity = std::max(0.0, std::min(1.0, 0.25 + 0.45 * qSin(m_frame * 0.02 + p.x * 0.01)));
            if (p.y < -20) { p.y = height() + 20; p.x = QRandomGenerator::global()->bounded(qMax(1, width())); }
            if (p.x < -20) p.x = width() + 20;
            if (p.x > width() + 20) p.x = -20;
        }
        update();
    });
    m_timer->start(33);

    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_TranslucentBackground);
}

void ParticleWidget::initParticles()
{
    m_particles.reserve(100);
    std::mt19937 rng(42);
    std::uniform_real_distribution<qreal> distX(0, 1280);
    std::uniform_real_distribution<qreal> distY(0, 720);
    std::uniform_real_distribution<qreal> distSpeed(-0.3, -0.05);
    std::uniform_real_distribution<qreal> distSize(1, 4);

    // 蓝白色调粒子
    QColor colors[] = {
        QColor(0, 212, 255),    // 亮青
        QColor(79, 195, 247),   // 浅蓝
        QColor(144, 202, 249),  // 淡蓝
        QColor(200, 230, 255),  // 近白蓝
        QColor(0, 200, 220),    // 青绿
    };

    for (int i = 0; i < 100; ++i) {
        Particle p;
        p.x = distX(rng);
        p.y = distY(rng);
        p.vx = distSpeed(rng) * 0.5;
        p.vy = distSpeed(rng);
        p.size = distSize(rng);
        p.opacity = 0.25 + 0.4 * (qreal(i) / 100.0);
        p.color = colors[i % 5];
        m_particles.append(p);
    }
}

void ParticleWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    for (const auto &p : m_particles) {
        QColor glowColor = p.color;
        glowColor.setAlphaF(p.opacity * 0.15);
        painter.setPen(Qt::NoPen);
        painter.setBrush(glowColor);
        painter.drawEllipse(QPointF(p.x, p.y), p.size * 4, p.size * 4);

        QColor coreColor = p.color;
        coreColor.setAlphaF(p.opacity);
        painter.setBrush(coreColor);
        painter.drawEllipse(QPointF(p.x, p.y), p.size, p.size);
    }
}

// ============================================================================
// StartPage 实现
// ============================================================================

StartPage::StartPage(QWidget *parent)
    : QWidget(parent)
    , m_particles(nullptr)
    , m_titleLabel(nullptr)
    , m_subtitleLabel(nullptr)
    , m_btnPve(nullptr)
    , m_btnPvp(nullptr)
    , m_btnAtlas(nullptr)
    , m_btnSettings(nullptr)
    , m_btnExit(nullptr)
{
    initUI();
}

void StartPage::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    playEnterAnimation();
}

void StartPage::playEnterAnimation()
{
    qDebug() << "[StartPage] playEnterAnimation called";

    QList<QWidget*> items = {m_titleLabel, m_subtitleLabel,
                             m_btnPve, m_btnPvp, m_btnAtlas,
                             m_btnSettings, m_btnExit};

    for (int i = 0; i < items.size(); ++i) {
        QWidget *item = items[i];
        if (!item) {
            qDebug() << "[StartPage] item" << i << "is null!";
            continue;
        }

        item->setGraphicsEffect(nullptr);

        QGraphicsOpacityEffect *opacityEffect = new QGraphicsOpacityEffect(item);
        opacityEffect->setOpacity(0);
        item->setGraphicsEffect(opacityEffect);

        QPropertyAnimation *anim = new QPropertyAnimation(opacityEffect, "opacity");
        anim->setDuration(400);
        anim->setStartValue(0.0);
        anim->setEndValue(1.0);
        anim->setEasingCurve(QEasingCurve::OutCubic);
        anim->start(QAbstractAnimation::DeleteWhenStopped);

        qDebug() << "[StartPage] animating item" << i;
    }
}

void StartPage::initUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(60, 40, 60, 40);
    mainLayout->setSpacing(12);

    mainLayout->addStretch(1);

    // ----- 游戏标题 -----
    m_titleLabel = new QLabel("塔防对战", this);
    QFont titleFont("Microsoft YaHei", 52, QFont::Bold);
    m_titleLabel->setFont(titleFont);
    m_titleLabel->setAlignment(Qt::AlignCenter);
    m_titleLabel->setStyleSheet(
        "QLabel {"
        "  color: qlineargradient("
        "    x1:0, y1:0, x2:1, y2:0,"
        "    stop:0 #64B5F6, stop:0.5 #E3F2FD, stop:1 #64B5F6"
        "  );"
        "  padding: 5px;"
        "}"
    );
    mainLayout->addWidget(m_titleLabel);

    // 副标题
    m_subtitleLabel = new QLabel("TOWER DEFENSE", this);
    QFont subFont("Consolas", 16, QFont::Light);
    m_subtitleLabel->setFont(subFont);
    m_subtitleLabel->setAlignment(Qt::AlignCenter);
    m_subtitleLabel->setStyleSheet(
        "QLabel { color: rgba(0,212,255,0.6); letter-spacing: 10px; }"
    );
    mainLayout->addWidget(m_subtitleLabel);
    mainLayout->addSpacing(30);

    // ----- 菜单按钮（TechButton，默认就清晰可见） -----
    m_btnPve      = createMenuButton("单人 PVE",  "🎮");
    m_btnPvp      = createMenuButton("多人 PVP",  "⚔");
    m_btnAtlas    = createMenuButton("图鉴 / 仓库", "📖");
    m_btnSettings = createMenuButton("游戏设置",   "⚙");
    m_btnExit     = createMenuButton("退出游戏",   "✕", QColor(255, 82, 82));

    mainLayout->addWidget(m_btnPve);
    mainLayout->addSpacing(6);
    mainLayout->addWidget(m_btnPvp);
    mainLayout->addSpacing(6);
    mainLayout->addWidget(m_btnAtlas);
    mainLayout->addSpacing(6);
    mainLayout->addWidget(m_btnSettings);
    mainLayout->addSpacing(6);
    mainLayout->addWidget(m_btnExit);

    mainLayout->addStretch(2);

    // 信号连接
    connect(m_btnPve,      &TechButton::clicked, this, &StartPage::signalPveClicked);
    connect(m_btnPvp,      &TechButton::clicked, this, &StartPage::signalPvpClicked);
    connect(m_btnAtlas,    &TechButton::clicked, this, &StartPage::signalAtlasClicked);
    connect(m_btnSettings, &TechButton::clicked, this, &StartPage::signalSettingsClicked);
    connect(m_btnExit,     &TechButton::clicked, this, &StartPage::signalExitClicked);

    // ----- 粒子背景层 -----
    m_particles = new ParticleWidget(this);
    m_particles->lower();

    // ----- 蓝白科技风背景 -----
    this->setStyleSheet(
        "StartPage {"
        "  background: qlineargradient("
        "    x1:0, y1:0, x2:1, y2:1,"
        "    stop:0 #0B1622, stop:0.5 #0F1B2D, stop:1 #162544"
        "  );"
        "}"
    );
}

TechButton* StartPage::createMenuButton(const QString &text, const QString &icon,
                                          const QColor &accent)
{
    TechButton *btn = new TechButton("", this);

    btn->setFixedHeight(56);
    btn->setMinimumWidth(320);
    btn->setMaximumWidth(450);

    btn->setFontSize(16);
    btn->setAccentColor(accent);
    btn->setBorderRadius(14);

    btn->setText(QString("%1  %2").arg(icon).arg(text));

    return btn;
}

void StartPage::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (m_particles) {
        m_particles->setGeometry(0, 0, width(), height());
    }
}
