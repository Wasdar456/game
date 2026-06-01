/**
 * @file StartPage.cpp
 * @brief Start page with scene_lab_03 splash, then the original simple menu.
 */

#include "ui/StartPage.h"

#include <QDebug>
#include <QFont>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QPropertyAnimation>
#include <QRandomGenerator>
#include <QVBoxLayout>
#include <QtMath>
#include <algorithm>
#include <random>

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
            p.opacity = std::max<qreal>(0.0, std::min<qreal>(1.0, 0.25 + 0.45 * qSin(m_frame * 0.02 + p.x * 0.01)));
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

    QColor colors[] = {
        QColor(0, 212, 255),
        QColor(79, 195, 247),
        QColor(144, 202, 249),
        QColor(200, 230, 255),
        QColor(0, 200, 220),
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

StartPage::StartPage(QWidget *parent)
    : QWidget(parent)
    , m_splashActive(true)
    , m_menuLayer(nullptr)
    , m_particles(nullptr)
    , m_pressHint(nullptr)
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

void StartPage::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    static QPixmap bg(":/images/ui/scene_lab_03.png");
    if (!bg.isNull()) {
        QSize scaled = bg.size();
        scaled.scale(size(), Qt::KeepAspectRatioByExpanding);
        QRect target(QPoint((width() - scaled.width()) / 2,
                            (height() - scaled.height()) / 2), scaled);
        painter.drawPixmap(target, bg);
    } else {
        painter.fillRect(rect(), QColor(47, 37, 30));
    }

    QLinearGradient vignette(0, 0, 0, height());
    vignette.setColorAt(0.0, QColor(34, 25, 20, m_splashActive ? 28 : 70));
    vignette.setColorAt(0.55, QColor(34, 25, 20, m_splashActive ? 0 : 18));
    vignette.setColorAt(1.0, QColor(23, 18, 15, m_splashActive ? 82 : 132));
    painter.fillRect(rect(), vignette);

    if (!m_splashActive) {
        QLinearGradient leftShade(0, 0, width() * 0.45, 0);
        leftShade.setColorAt(0.0, QColor(25, 16, 12, 152));
        leftShade.setColorAt(0.68, QColor(25, 16, 12, 72));
        leftShade.setColorAt(1.0, QColor(25, 16, 12, 0));
        painter.fillRect(rect(), leftShade);
    }
}

void StartPage::keyPressEvent(QKeyEvent *event)
{
    if (m_splashActive) {
        revealMenu();
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

void StartPage::mousePressEvent(QMouseEvent *event)
{
    if (m_splashActive) {
        revealMenu();
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void StartPage::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    setFocus(Qt::OtherFocusReason);
    if (!m_splashActive) {
        playEnterAnimation();
    }
}

void StartPage::playEnterAnimation()
{
    QList<QWidget*> items = {m_titleLabel, m_subtitleLabel,
                             m_btnPve, m_btnPvp, m_btnAtlas,
                             m_btnSettings, m_btnExit};

    for (QWidget *item : items) {
        if (!item) {
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
    }
}

void StartPage::initUI()
{
    setAutoFillBackground(false);
    setFocusPolicy(Qt::StrongFocus);

    m_menuLayer = new QWidget(this);
    m_menuLayer->setAttribute(Qt::WA_TranslucentBackground);
    m_menuLayer->hide();

    QHBoxLayout *pageLayout = new QHBoxLayout(m_menuLayer);
    pageLayout->setContentsMargins(56, 42, 56, 42);
    pageLayout->setSpacing(24);

    QVBoxLayout *mainLayout = new QVBoxLayout();
    mainLayout->setSpacing(12);
    pageLayout->addLayout(mainLayout);
    pageLayout->addStretch(1);

    mainLayout->addStretch(1);

    m_titleLabel = new QLabel("塔防对战", m_menuLayer);
    QFont titleFont("Microsoft YaHei", 52, QFont::Bold);
    m_titleLabel->setFont(titleFont);
    m_titleLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_titleLabel->setStyleSheet(
        "QLabel {"
        "  color: #FFF0C2;"
        "  padding: 5px;"
        "}"
    );
    mainLayout->addWidget(m_titleLabel);

    m_subtitleLabel = new QLabel("TOWER DEFENSE", m_menuLayer);
    QFont subFont("Consolas", 16, QFont::Light);
    m_subtitleLabel->setFont(subFont);
    m_subtitleLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_subtitleLabel->setStyleSheet(
        "QLabel { color: rgba(255,226,166,0.78); letter-spacing: 10px; }"
    );
    mainLayout->addWidget(m_subtitleLabel);
    mainLayout->addSpacing(30);

    m_btnPve      = createMenuButton("单人 PVE",  "");
    m_btnPvp      = createMenuButton("多人 PVP",  "");
    m_btnAtlas    = createMenuButton("图鉴 / 仓库", "");
    m_btnSettings = createMenuButton("游戏设置",   "");
    m_btnExit     = createMenuButton("退出游戏",   "", QColor(202, 86, 65));

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

    connect(m_btnPve,      &TechButton::clicked, this, &StartPage::signalPveClicked);
    connect(m_btnPvp,      &TechButton::clicked, this, &StartPage::signalPvpClicked);
    connect(m_btnAtlas,    &TechButton::clicked, this, &StartPage::signalAtlasClicked);
    connect(m_btnSettings, &TechButton::clicked, this, &StartPage::signalSettingsClicked);
    connect(m_btnExit,     &TechButton::clicked, this, &StartPage::signalExitClicked);

    m_particles = new ParticleWidget(this);
    m_particles->lower();
    m_particles->hide();

    m_pressHint = new QLabel("按任意键开始", this);
    m_pressHint->setAlignment(Qt::AlignCenter);
    m_pressHint->setStyleSheet(
        "QLabel {"
        "  color: #FFF2C4;"
        "  background-color: rgba(43, 28, 19, 0.48);"
        "  border: 2px solid rgba(255, 221, 145, 0.62);"
        "  border-radius: 8px;"
        "  padding: 10px 22px;"
        "  font-size: 22px;"
        "  font-weight: 800;"
        "}"
    );
    m_pressHint->setFixedSize(260, 58);
}

void StartPage::revealMenu()
{
    if (!m_splashActive) {
        return;
    }

    m_splashActive = false;
    if (m_pressHint) {
        m_pressHint->hide();
    }
    if (m_particles) {
        m_particles->show();
        m_particles->lower();
    }
    if (m_menuLayer) {
        m_menuLayer->show();
        m_menuLayer->raise();
    }

    update();
    playEnterAnimation();
}

TechButton* StartPage::createMenuButton(const QString &text, const QString &icon,
                                          const QColor &accent)
{
    TechButton *btn = new TechButton("", m_menuLayer);

    btn->setFixedHeight(64);
    btn->setMinimumWidth(330);
    btn->setMaximumWidth(430);

    btn->setFontSize(18);
    btn->setAccentColor(accent);
    btn->setBorderRadius(8);

    btn->setText(icon.isEmpty() ? text : QString("%1  %2").arg(icon).arg(text));

    return btn;
}

void StartPage::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (m_menuLayer) {
        m_menuLayer->setGeometry(rect());
    }
    if (m_particles) {
        m_particles->setGeometry(0, 0, width(), height());
    }
    if (m_pressHint) {
        m_pressHint->move((width() - m_pressHint->width()) / 2,
                          height() - m_pressHint->height() - 56);
    }
}
