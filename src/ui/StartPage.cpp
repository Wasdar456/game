/**
 * @file StartPage.cpp
 * @brief Splash page plus image-cropped lobby buttons based on the UI demo.
 */

#include "ui/StartPage.h"

#include <QEasingCurve>
#include <QGraphicsOpacityEffect>
#include <QPainter>
#include <QPaintEvent>
#include <QPropertyAnimation>
#include <QRandomGenerator>
#include <QShowEvent>
#include <QVariantAnimation>
#include <QtMath>
#include <algorithm>
#include <functional>
#include <random>

namespace {

constexpr int kLobbyDesignWidth = 1672;
constexpr int kLobbyDesignHeight = 941;
constexpr qreal kHoverScale = 1.08;
constexpr qreal kPressedScale = 0.97;

struct LobbyButtonSpec {
    const char *id;
    const char *title;
    QRect rect;
    const char *asset;
};

const QVector<LobbyButtonSpec> kLobbyButtons = {
    {"settings", "Settings", {1308, 30, 155, 58}, ":/images/lobby_demo/settings.png"},
    {"profile", "Profile", {1485, 30, 151, 58}, ":/images/lobby_demo/profile.png"},
    {"pve", "PVE", {140, 337, 233, 96}, ":/images/lobby_demo/pve.png"},
    {"pvp", "PVP", {140, 458, 233, 96}, ":/images/lobby_demo/pvp.png"},
    {"map_left", "Map Left", {462, 418, 41, 76}, ":/images/lobby_demo/map_left.png"},
    {"map_right", "Map Right", {744, 418, 40, 76}, ":/images/lobby_demo/map_right.png"},
    {"map_dropdown", "Jungle Ruins", {495, 610, 279, 50}, ":/images/lobby_demo/map_dropdown.png"},
    {"easy", "Easy", {817, 356, 101, 51}, ":/images/lobby_demo/easy.png"},
    {"normal", "Normal", {927, 361, 95, 42}, ":/images/lobby_demo/normal.png"},
    {"hard", "Hard", {1032, 360, 88, 43}, ":/images/lobby_demo/hard.png"},
    {"wave_minus", "Wave minus", {823, 474, 42, 41}, ":/images/lobby_demo/wave_minus.png"},
    {"wave_plus", "Wave plus", {1056, 474, 39, 41}, ":/images/lobby_demo/wave_plus.png"},
    {"invite", "Invite", {1204, 633, 78, 47}, ":/images/lobby_demo/invite.png"},
    {"create_room", "Create Room", {1289, 633, 110, 47}, ":/images/lobby_demo/create_room.png"},
    {"join", "Join", {1407, 633, 73, 47}, ":/images/lobby_demo/join.png"},
    {"start", "Start", {425, 766, 190, 99}, ":/images/lobby_demo/start.png"},
    {"deck", "Deck", {638, 770, 185, 96}, ":/images/lobby_demo/deck.png"},
    {"atlas", "Atlas", {846, 771, 184, 95}, ":/images/lobby_demo/atlas.png"},
    {"back", "Back", {1052, 770, 185, 96}, ":/images/lobby_demo/back.png"},
};

void drawCoverPixmap(QPainter &painter, const QPixmap &pixmap, const QRect &target)
{
    if (pixmap.isNull()) {
        painter.fillRect(target, QColor(47, 37, 30));
        return;
    }

    QSize scaled = pixmap.size();
    scaled.scale(target.size(), Qt::KeepAspectRatioByExpanding);
    const QRect drawRect(QPoint(target.x() + (target.width() - scaled.width()) / 2,
                                target.y() + (target.height() - scaled.height()) / 2),
                         scaled);
    painter.drawPixmap(drawRect, pixmap);
}

} // namespace

class StartImageButton final : public QWidget
{
public:
    explicit StartImageButton(const LobbyButtonSpec &spec, QWidget *parent = nullptr)
        : QWidget(parent)
        , m_spec(spec)
        , m_pixmap(QString::fromUtf8(spec.asset))
        , m_animation(new QVariantAnimation(this))
    {
        setMouseTracking(true);
        setCursor(Qt::PointingHandCursor);
        setToolTip(QString::fromUtf8(spec.title));
        setAttribute(Qt::WA_TranslucentBackground);

        m_animation->setDuration(110);
        m_animation->setEasingCurve(QEasingCurve::OutCubic);
        connect(m_animation, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
            m_scale = value.toReal();
            update();
        });
    }

    void setClickHandler(std::function<void(QString, QString)> handler)
    {
        m_clickHandler = std::move(handler);
    }

    void setCanvasRect(const QRectF &rect)
    {
        const qreal xPad = rect.width() * (kHoverScale - 1.0) / 2.0 + 6.0;
        const qreal yPad = rect.height() * (kHoverScale - 1.0) / 2.0 + 6.0;
        setGeometry(rect.adjusted(-xPad, -yPad, xPad, yPad).toAlignedRect());
        m_baseSize = rect.size();
        update();
    }

    void refreshVisual()
    {
        m_animation->stop();
        m_scale = 1.0;
        m_pressed = false;
        show();
        raise();
        update();
    }

protected:
    void showEvent(QShowEvent *event) override
    {
        QWidget::showEvent(event);
        refreshVisual();
    }

    void enterEvent(QEnterEvent *event) override
    {
        QWidget::enterEvent(event);
        m_pressed = false;
        animateTo(kHoverScale);
    }

    void leaveEvent(QEvent *event) override
    {
        QWidget::leaveEvent(event);
        m_pressed = false;
        animateTo(1.0);
    }

    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton) {
            m_pressed = true;
            animateTo(kPressedScale);
            event->accept();
            return;
        }
        QWidget::mousePressEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton && m_pressed) {
            m_pressed = false;
            const bool inside = rect().contains(event->pos());
            animateTo(inside ? kHoverScale : 1.0);
            if (inside && m_clickHandler) {
                m_clickHandler(QString::fromUtf8(m_spec.id), QString::fromUtf8(m_spec.title));
            }
            event->accept();
            return;
        }
        QWidget::mouseReleaseEvent(event);
    }

    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
        painter.setRenderHint(QPainter::Antialiasing, true);

        QRectF drawRect(QPointF(0, 0), m_baseSize * m_scale);
        drawRect.moveCenter(QRectF(rect()).center());

        if (!m_pixmap.isNull()) {
            painter.drawPixmap(drawRect, m_pixmap, QRectF(m_pixmap.rect()));
        } else {
            painter.setPen(QPen(Qt::red, 2));
            painter.drawRect(drawRect);
        }
    }

private:
    void animateTo(qreal target)
    {
        m_animation->stop();
        m_animation->setStartValue(m_scale);
        m_animation->setEndValue(target);
        m_animation->start();
    }

    LobbyButtonSpec m_spec;
    QPixmap m_pixmap;
    QVariantAnimation *m_animation;
    std::function<void(QString, QString)> m_clickHandler;
    QSizeF m_baseSize;
    qreal m_scale = 1.0;
    bool m_pressed = false;
};

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
    , m_clickHint(nullptr)
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
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    if (m_splashActive) {
        static QPixmap splash(":/images/ui/scene_lab_03.png");
        drawCoverPixmap(painter, splash, rect());

        QLinearGradient vignette(0, 0, 0, height());
        vignette.setColorAt(0.0, QColor(34, 25, 20, 28));
        vignette.setColorAt(0.55, QColor(34, 25, 20, 0));
        vignette.setColorAt(1.0, QColor(23, 18, 15, 82));
        painter.fillRect(rect(), vignette);
        return;
    }

    painter.fillRect(rect(), QColor(27, 38, 29));
    static QPixmap lobbyBackground(":/images/lobby_demo/lobby_background.png");
    if (!lobbyBackground.isNull()) {
        painter.drawPixmap(m_demoCanvasRect, lobbyBackground, QRectF(lobbyBackground.rect()));
    } else {
        drawCoverPixmap(painter, QPixmap(":/images/ui/scene_lab_02.png"), rect());
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
        updateDemoLayout();
        playEnterAnimation();
    }
}

void StartPage::playEnterAnimation()
{
    if (!m_menuLayer) {
        return;
    }

    m_menuLayer->setGraphicsEffect(nullptr);
    m_menuLayer->show();
    m_menuLayer->raise();

    for (StartImageButton *button : m_demoButtons) {
        if (button) {
            button->refreshVisual();
        }
    }

    if (m_clickHint && m_clickHint->isVisible()) {
        m_clickHint->raise();
    }

    m_menuLayer->update();
    update();
}

void StartPage::initUI()
{
    setAutoFillBackground(false);
    setFocusPolicy(Qt::StrongFocus);

    m_menuLayer = new QWidget(this);
    m_menuLayer->setAttribute(Qt::WA_TranslucentBackground);
    m_menuLayer->hide();

    for (const auto &spec : kLobbyButtons) {
        auto *button = new StartImageButton(spec, m_menuLayer);
        button->setClickHandler([this](const QString &id, const QString &title) {
            handleDemoButton(id, title);
        });
        m_demoButtons.append(button);
    }

    m_clickHint = new QLabel(m_menuLayer);
    m_clickHint->setAlignment(Qt::AlignCenter);
    m_clickHint->setStyleSheet(
        "QLabel {"
        "  color: #2C210E;"
        "  background-color: rgba(249,222,142,210);"
        "  border: 2px solid rgba(74,43,19,210);"
        "  border-radius: 10px;"
        "  padding: 8px 18px;"
        "  font-size: 15px;"
        "  font-weight: 800;"
        "}"
    );
    m_clickHint->hide();

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

    updateDemoLayout();
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
    if (m_menuLayer) {
        m_menuLayer->show();
        m_menuLayer->raise();
    }

    updateDemoLayout();
    update();
    playEnterAnimation();
}

void StartPage::updateDemoLayout()
{
    if (!m_menuLayer) {
        return;
    }

    const qreal scale = qMin(width() / qreal(kLobbyDesignWidth),
                             height() / qreal(kLobbyDesignHeight));
    const QSizeF canvasSize(kLobbyDesignWidth * scale, kLobbyDesignHeight * scale);
    const QPointF topLeft((width() - canvasSize.width()) / 2.0,
                          (height() - canvasSize.height()) / 2.0);
    m_demoCanvasRect = QRectF(topLeft, canvasSize);

    for (int i = 0; i < m_demoButtons.size() && i < kLobbyButtons.size(); ++i) {
        const QRect source = kLobbyButtons[i].rect;
        const QRectF target(topLeft.x() + source.x() * scale,
                            topLeft.y() + source.y() * scale,
                            source.width() * scale,
                            source.height() * scale);
        m_demoButtons[i]->setCanvasRect(target);
        m_demoButtons[i]->raise();
        m_demoButtons[i]->update();
    }

    if (m_clickHint && m_clickHint->isVisible()) {
        m_clickHint->move((width() - m_clickHint->width()) / 2,
                          qRound(m_demoCanvasRect.bottom() - m_clickHint->height() - 24));
        m_clickHint->raise();
    }

    update();
}

void StartPage::handleDemoButton(const QString &id, const QString &title)
{
    showDemoHint(title);

    if (id == "settings") {
        emit signalSettingsClicked();
    } else if (id == "profile" || id == "deck" || id == "atlas") {
        emit signalAtlasClicked();
    } else if (id == "pve" || id == "start") {
        emit signalPveClicked();
    } else if (id == "pvp" || id == "invite" || id == "create_room" || id == "join") {
        emit signalPvpClicked();
    } else if (id == "back") {
        emit signalExitClicked();
    }
}

void StartPage::showDemoHint(const QString &text)
{
    if (!m_clickHint) {
        return;
    }

    m_clickHint->setText(text);
    m_clickHint->adjustSize();
    m_clickHint->move((width() - m_clickHint->width()) / 2,
                      qRound(m_demoCanvasRect.bottom() - m_clickHint->height() - 24));
    m_clickHint->show();
    m_clickHint->raise();
    QTimer::singleShot(900, m_clickHint, &QLabel::hide);
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
    updateDemoLayout();
}
