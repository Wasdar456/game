#include "ui/StartPage.h"

#include "ui/ArtHotspot.h"

#include <QKeyEvent>
#include <QLabel>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QRadialGradient>
#include <QResizeEvent>
#include <QShowEvent>
#include <QtMath>

#include <cmath>

namespace {

constexpr int kDesignWidth = 1672;
constexpr int kDesignHeight = 941;

struct MenuHotspotSpec {
    QRect rect;
    const char *tooltip;
};

const QVector<MenuHotspotSpec> kMenuHotspots = {
    {{806, 288, 490, 126}, "单人 PVE"},
    {{801, 412, 498, 126}, "多人 PVP"},
    {{742, 548, 345, 124}, "图鉴与卡组"},
    {{1110, 548, 332, 124}, "设置"},
    {{899, 675, 333, 123}, "退出游戏"},
};

void drawCover(QPainter &painter, const QPixmap &pixmap, const QRect &target)
{
    if (pixmap.isNull()) {
        painter.fillRect(target, QColor(41, 35, 25));
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

StartPage::StartPage(QWidget *parent)
    : QWidget(parent)
    , m_introActive(true)
    , m_splashActive(false)
    , m_menuLayer(nullptr)
    , m_fadeOverlay(nullptr)
    , m_pressHint(nullptr)
    , m_ambientPhase(0.0)
    , m_introElapsed(0.0)
    , m_menuRevealProgress(0.0)
{
    initUI();

    m_ambientTimer.setInterval(50);
    connect(&m_ambientTimer, &QTimer::timeout, this, [this]() {
        m_ambientPhase += 0.025;
        if (m_introActive) {
            m_introElapsed += 0.05;
            if (m_introElapsed >= 2.35) {
                finishIntro();
            }
            update();
            return;
        }
        if (m_menuRevealProgress < 1.0) {
            m_menuRevealProgress = qMin<qreal>(1.0, m_menuRevealProgress + 0.055);
            if (m_menuLayer) {
                m_menuLayer->show();
            }
            updateFadeOverlay();
        }
        update();
    });
    m_ambientTimer.start();
}

void StartPage::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    if (m_introActive) {
        painter.fillRect(rect(), QColor(0, 0, 0));
        const qreal fadeIn = qBound<qreal>(0.0, m_introElapsed / 0.75, 1.0);
        const qreal fadeOut = qBound<qreal>(0.0, (2.35 - m_introElapsed) / 0.55, 1.0);
        const qreal alpha = qMin(fadeIn, fadeOut);
        const qreal lift = (1.0 - fadeIn) * 18.0;

        painter.save();
        painter.setOpacity(alpha);
        QFont titleFont("Microsoft YaHei UI", qMax(28, height() / 15), QFont::Black);
        painter.setFont(titleFont);
        painter.setPen(QColor(255, 238, 178));
        QRectF titleRect(0, height() * 0.40 - lift, width(), height() * 0.12);
        painter.drawText(titleRect, Qt::AlignCenter, "Defense & Juice");

        QFont subFont("Microsoft YaHei UI", qMax(12, height() / 42), QFont::DemiBold);
        painter.setFont(subFont);
        painter.setPen(QColor(162, 229, 196, 215));
        painter.drawText(QRectF(0, titleRect.bottom() + 8, width(), 38),
                         Qt::AlignCenter, "Berrybyte Lab Defense Protocol");

        QRadialGradient glow(QPointF(width() * 0.5, height() * 0.50), width() * 0.32);
        glow.setColorAt(0.0, QColor(255, 210, 116, qRound(alpha * 42)));
        glow.setColorAt(0.55, QColor(92, 206, 170, qRound(alpha * 16)));
        glow.setColorAt(1.0, QColor(0, 0, 0, 0));
        painter.fillRect(rect(), glow);
        painter.restore();
        return;
    }

    if (m_splashActive) {
        static QPixmap splash(":/images/ui/scene_lab_03.png");
        drawCover(painter, splash, rect());

        QLinearGradient shade(0, 0, 0, height());
        shade.setColorAt(0.0, QColor(28, 21, 15, 22));
        shade.setColorAt(0.62, QColor(28, 21, 15, 0));
        shade.setColorAt(1.0, QColor(28, 21, 15, 105));
        painter.fillRect(rect(), shade);
        return;
    }

    painter.fillRect(rect(), QColor(36, 52, 38));
    static QPixmap menu(":/images/artwork/main_menu.jpg");
    if (!m_menuCache.isNull()) {
        painter.drawPixmap(m_canvasRect.topLeft(), m_menuCache);
    } else if (!menu.isNull()) {
        painter.drawPixmap(m_canvasRect, menu, QRectF(menu.rect()));
    }

    const qreal pulse = 0.5 + 0.5 * qSin(m_ambientPhase);
    const qreal sx = m_canvasRect.width() / kDesignWidth;
    const qreal sy = m_canvasRect.height() / kDesignHeight;

    // Redraw the foreground character with a tiny vertical scale pulse.
    // It reads as breathing while preserving the original painted artwork.
    if (!menu.isNull()) {
        const QRectF sourceCharacter(1370, 610, 270, 315);
        QRectF targetCharacter(m_canvasRect.left() + sourceCharacter.x() * sx,
                               m_canvasRect.top() + sourceCharacter.y() * sy,
                               sourceCharacter.width() * sx,
                               sourceCharacter.height() * sy);
        const qreal breathe = 1.0 + qSin(m_ambientPhase * 0.82) * 0.006;
        QRectF breathed = targetCharacter;
        breathed.setHeight(targetCharacter.height() * breathe);
        breathed.moveBottom(targetCharacter.bottom());
        painter.save();
        painter.setClipRect(targetCharacter.adjusted(-5, -8, 5, 4));
        painter.drawPixmap(breathed, menu, sourceCharacter);
        painter.restore();
    }

    // Gentle moving wave crests over the beach portion of the painting.
    painter.save();
    painter.setClipRect(QRectF(m_canvasRect.left(),
                               m_canvasRect.top() + m_canvasRect.height() * 0.38,
                               m_canvasRect.width() * 0.34,
                               m_canvasRect.height() * 0.47));
    for (int row = 0; row < 6; ++row) {
        QPainterPath wave;
        const qreal y = m_canvasRect.top() + m_canvasRect.height() * (0.48 + row * 0.055);
        const qreal offset = qSin(m_ambientPhase * 0.8 + row) * 13.0;
        wave.moveTo(m_canvasRect.left() - 30, y);
        for (int segment = 0; segment < 9; ++segment) {
            const qreal x = m_canvasRect.left() + segment * m_canvasRect.width() * 0.045 + offset;
            wave.cubicTo(x + 14, y - 4 - row,
                         x + 29, y + 5 + row * 0.5,
                         x + 43, y);
        }
        painter.setPen(QPen(QColor(239, 248, 224, 30 + row * 5), 1.4));
        painter.drawPath(wave);
    }
    painter.restore();

    // Small wind-blown leaves connect the static canopy with the page motion.
    for (int i = 0; i < 12; ++i) {
        const qreal travel = std::fmod(m_ambientPhase * (18.0 + i % 4 * 4.0)
                                      + i * 137.0,
                                      m_canvasRect.width() + 180.0);
        const qreal x = m_canvasRect.right() + 50.0 - travel;
        const qreal y = m_canvasRect.top() + m_canvasRect.height() * (0.08 + (i % 6) * 0.105)
                        + qSin(m_ambientPhase * 1.4 + i) * 18.0;
        painter.save();
        painter.translate(x, y);
        painter.rotate(m_ambientPhase * 42.0 + i * 31.0);
        const qreal leafSize = 4.5 + i % 3;
        painter.setPen(Qt::NoPen);
        painter.setBrush(i % 2 ? QColor(118, 132, 55, 120)
                               : QColor(73, 104, 49, 135));
        painter.drawEllipse(QRectF(-leafSize, -leafSize * 0.42,
                                   leafSize * 2.0, leafSize * 0.84));
        painter.restore();
    }

    QRadialGradient sunGlow(
        QPointF(m_canvasRect.left() + m_canvasRect.width() * 0.20,
                m_canvasRect.top() + m_canvasRect.height() * 0.08),
        m_canvasRect.width() * 0.34);
    sunGlow.setColorAt(0.0, QColor(255, 244, 183, qRound(18 + pulse * 14)));
    sunGlow.setColorAt(1.0, QColor(255, 244, 183, 0));
    painter.fillRect(m_canvasRect, sunGlow);

    QLinearGradient vignette(m_canvasRect.topLeft(), m_canvasRect.bottomRight());
    vignette.setColorAt(0.0, QColor(24, 35, 24, 16));
    vignette.setColorAt(0.5, QColor(24, 35, 24, 0));
    vignette.setColorAt(1.0, QColor(22, 26, 18, 30));
    painter.fillRect(m_canvasRect, vignette);

}

void StartPage::keyPressEvent(QKeyEvent *event)
{
    if (m_introActive) {
        finishIntro();
        event->accept();
        return;
    }
    if (m_splashActive) {
        revealMenu();
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

void StartPage::mousePressEvent(QMouseEvent *event)
{
    if (m_introActive) {
        finishIntro();
        event->accept();
        return;
    }
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
    if (!m_splashActive && !m_introActive) {
        updateArtworkLayout();
        playEnterAnimation();
    }
}

void StartPage::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (m_menuLayer) {
        m_menuLayer->setGeometry(rect());
    }
    if (m_fadeOverlay) {
        m_fadeOverlay->setGeometry(rect());
    }
    if (m_pressHint) {
        m_pressHint->move((width() - m_pressHint->width()) / 2,
                          height() - m_pressHint->height() - 54);
    }
    updateArtworkLayout();
}

void StartPage::playEnterAnimation()
{
    if (!m_menuLayer) {
        return;
    }

    m_menuLayer->setGraphicsEffect(nullptr);
    m_menuLayer->setVisible(!m_introActive);
    m_menuLayer->raise();
    for (ArtHotspot *button : m_buttons) {
        button->refreshVisual();
    }
    updateFadeOverlay();
    update();
}

void StartPage::initUI()
{
    setAutoFillBackground(false);
    setFocusPolicy(Qt::StrongFocus);

    m_menuLayer = new QWidget(this);
    m_menuLayer->setAttribute(Qt::WA_TranslucentBackground);
    m_menuLayer->hide();

    m_fadeOverlay = new QWidget(this);
    m_fadeOverlay->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_fadeOverlay->setGeometry(rect());
    m_fadeOverlay->hide();

    const QString artwork = ":/images/artwork/main_menu.jpg";
    for (int i = 0; i < kMenuHotspots.size(); ++i) {
        const auto &spec = kMenuHotspots[i];
        auto *button = new ArtHotspot(artwork, spec.rect, m_menuLayer);
        button->setToolTip(QString::fromUtf8(spec.tooltip));
        button->setGlowColor(QColor(255, 222, 132));
        button->setSwayEnabled(true);
        m_buttons.append(button);
    }

    m_buttons[0]->setClickHandler([this]() { emit signalPveClicked(); });
    m_buttons[1]->setClickHandler([this]() { emit signalPvpClicked(); });
    m_buttons[2]->setClickHandler([this]() { emit signalAtlasClicked(); });
    m_buttons[3]->setClickHandler([this]() { emit signalSettingsClicked(); });
    m_buttons[4]->setClickHandler([this]() { emit signalExitClicked(); });

    m_pressHint = new QLabel("按任意键开始", this);
    m_pressHint->setAlignment(Qt::AlignCenter);
    m_pressHint->setFixedSize(260, 58);
    m_pressHint->setStyleSheet(
        "QLabel {"
        " color: #fff4cd;"
        " background-color: rgba(55, 37, 20, 0.58);"
        " border: 2px solid rgba(255, 224, 151, 0.78);"
        " border-radius: 7px;"
        " padding: 9px 20px;"
        " font-family: 'Microsoft YaHei UI', 'PingFang SC', sans-serif;"
        " font-size: 21px;"
        " font-weight: 700;"
        "}"
    );
    m_pressHint->hide();

    updateArtworkLayout();
}

void StartPage::finishIntro()
{
    if (!m_introActive) {
        return;
    }
    m_introActive = false;
    m_splashActive = false;
    m_menuRevealProgress = 0.0;
    if (m_pressHint) {
        m_pressHint->hide();
    }
    if (m_menuLayer) {
        m_menuLayer->show();
    }
    updateArtworkLayout();
    updateFadeOverlay();
    update();
}

void StartPage::revealMenu()
{
    if (!m_splashActive) {
        return;
    }

    m_splashActive = false;
    m_menuRevealProgress = 1.0;
    m_pressHint->hide();
    m_menuLayer->show();
    updateFadeOverlay();
    updateArtworkLayout();
    playEnterAnimation();
}

void StartPage::updateArtworkLayout()
{
    if (!m_menuLayer) {
        return;
    }

    const qreal scale = qMin(width() / qreal(kDesignWidth),
                             height() / qreal(kDesignHeight));
    const QSizeF canvasSize(kDesignWidth * scale, kDesignHeight * scale);
    const QPointF topLeft((width() - canvasSize.width()) / 2.0,
                          (height() - canvasSize.height()) / 2.0);
    m_canvasRect = QRectF(topLeft, canvasSize);

    const QSize cacheSize(qMax(1, qRound(canvasSize.width())),
                          qMax(1, qRound(canvasSize.height())));
    if (cacheSize != m_menuCacheSize) {
        static QPixmap menu(":/images/artwork/main_menu.jpg");
        m_menuCache = menu.scaled(cacheSize, Qt::IgnoreAspectRatio,
                                  Qt::SmoothTransformation);
        m_menuCacheSize = cacheSize;
    }

    for (int i = 0; i < m_buttons.size(); ++i) {
        const QRect source = kMenuHotspots[i].rect;
        const QRectF target(topLeft.x() + source.x() * scale,
                            topLeft.y() + source.y() * scale,
                            source.width() * scale,
                            source.height() * scale);
        m_buttons[i]->setCanvasRect(target);
        m_buttons[i]->raise();
    }
    updateFadeOverlay();
    update();
}

void StartPage::updateFadeOverlay()
{
    if (!m_fadeOverlay) {
        return;
    }

    m_fadeOverlay->setGeometry(rect());
    if (m_introActive || m_menuRevealProgress >= 1.0) {
        m_fadeOverlay->hide();
        return;
    }

    const int alpha = qBound(0, qRound((1.0 - m_menuRevealProgress) * 255), 255);
    m_fadeOverlay->setStyleSheet(
        QString("background-color: rgba(0, 0, 0, %1);").arg(alpha));
    m_fadeOverlay->show();
    m_fadeOverlay->raise();
}
