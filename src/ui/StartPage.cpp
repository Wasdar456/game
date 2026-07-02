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
#include <QStringList>
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
    {{786, 411, 534, 129}, "多人 PVP"},
    {{738, 560, 322, 101}, "图鉴与卡组"},
    {{1110, 548, 332, 124}, "设置"},
    {{906, 696, 314, 91}, "退出游戏"},
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

qreal easeOutCubic(qreal value)
{
    value = qBound<qreal>(0.0, value, 1.0);
    const qreal inverse = 1.0 - value;
    return 1.0 - inverse * inverse * inverse;
}

qreal easeInOutCubic(qreal value)
{
    value = qBound<qreal>(0.0, value, 1.0);
    if (value < 0.5) {
        return 4.0 * value * value * value;
    }
    const qreal f = -2.0 * value + 2.0;
    return 1.0 - f * f * f / 2.0;
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
            if (m_introElapsed >= 2.55) {
                finishIntro();
            }
            update();
            return;
        }
        if (m_menuRevealProgress < 1.0) {
            m_menuRevealProgress = qMin<qreal>(1.0, m_menuRevealProgress + 0.042);
            if (m_menuLayer) {
                m_menuLayer->show();
            }
            updateArtworkLayout();
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
        const qreal fadeIn = easeOutCubic(m_introElapsed / 0.80);
        const qreal fadeOut = qBound<qreal>(0.0, (2.55 - m_introElapsed) / 0.62, 1.0);
        const qreal alpha = qMin(fadeIn, easeInOutCubic(fadeOut));
        const qreal lift = (1.0 - fadeIn) * 22.0;
        const qreal shine = qBound<qreal>(0.0, (m_introElapsed - 0.55) / 0.85, 1.0);

        QLinearGradient bg(0, 0, width(), height());
        bg.setColorAt(0.0, QColor(23, 35, 31));
        bg.setColorAt(0.45, QColor(55, 40, 28));
        bg.setColorAt(1.0, QColor(13, 18, 20));
        painter.fillRect(rect(), bg);

        QRadialGradient fruitGlow(QPointF(width() * 0.52, height() * 0.40), width() * 0.42);
        fruitGlow.setColorAt(0.0, QColor(255, 206, 92, qRound(alpha * 58)));
        fruitGlow.setColorAt(0.44, QColor(101, 194, 127, qRound(alpha * 34)));
        fruitGlow.setColorAt(1.0, QColor(0, 0, 0, 0));
        painter.fillRect(rect(), fruitGlow);

        painter.save();
        painter.setOpacity(alpha);
        QFont titleFont("Impact", qMax(54, height() / 9), QFont::Black);
        titleFont.setStyleStrategy(QFont::PreferAntialias);
        painter.setFont(titleFont);
        const QStringList titleLines = {"CRAZY", "FRUITY", "FIGHT"};
        const qreal lineHeight = qMax<qreal>(62.0, height() * 0.105);
        const qreal titleTop = height() * 0.18 - lift;
        for (int i = 0; i < titleLines.size(); ++i) {
            QRectF lineRect(width() * 0.18,
                            titleTop + i * lineHeight,
                            width() * 0.64,
                            lineHeight);
            QPainterPath textPath;
            textPath.addText(QPointF(0, 0), titleFont, titleLines[i]);
            const QRectF bounds = textPath.boundingRect();
            const qreal x = lineRect.center().x() - bounds.width() * 0.5 - bounds.left();
            const qreal y = lineRect.center().y() + bounds.height() * 0.5 - bounds.bottom();
            QPainterPath centeredPath;
            centeredPath.addText(QPointF(x, y), titleFont, titleLines[i]);

            QColor shadow(35, 20, 12, qRound(alpha * 220));
            painter.setPen(QPen(shadow, qMax<qreal>(6.0, height() * 0.010), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter.setBrush(Qt::NoBrush);
            painter.drawPath(centeredPath.translated(0, 5));

            QColor outline(77, 45, 23, qRound(alpha * 245));
            painter.setPen(QPen(outline, qMax<qreal>(4.0, height() * 0.007), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter.drawPath(centeredPath);

            QLinearGradient fill(lineRect.left(), lineRect.top(), lineRect.right(), lineRect.bottom());
            fill.setColorAt(0.0, i == 0 ? QColor(255, 219, 79) : (i == 1 ? QColor(239, 103, 48) : QColor(108, 185, 77)));
            fill.setColorAt(0.52, QColor(255, 244, 194));
            fill.setColorAt(1.0, i == 0 ? QColor(255, 139, 49) : (i == 1 ? QColor(182, 72, 43) : QColor(46, 124, 55)));
            painter.setPen(Qt::NoPen);
            painter.setBrush(fill);
            painter.drawPath(centeredPath);
        }

        const QRectF titleRect(0, titleTop, width(), lineHeight * titleLines.size());
        QLinearGradient slash(titleRect.left(), 0, titleRect.right(), 0);
        slash.setColorAt(0.0, QColor(255, 255, 255, 0));
        slash.setColorAt(qBound<qreal>(0.0, shine - 0.12, 1.0), QColor(255, 255, 255, 0));
        slash.setColorAt(qBound<qreal>(0.0, shine, 1.0), QColor(255, 252, 204, qRound(alpha * 155)));
        slash.setColorAt(qBound<qreal>(0.0, shine + 0.12, 1.0), QColor(255, 255, 255, 0));
        painter.setPen(QPen(QBrush(slash), qMax<qreal>(3.0, height() * 0.006)));
        painter.drawLine(QPointF(width() * 0.22, titleTop + lineHeight * 1.15),
                         QPointF(width() * 0.78, titleTop + lineHeight * 0.72));

        QFont subFont("Microsoft YaHei UI", qMax(18, height() / 34), QFont::Black);
        painter.setFont(subFont);
        painter.setPen(QColor(255, 240, 188, 235));
        painter.drawText(QRectF(0, titleRect.bottom() + height() * 0.035, width(), 44),
                         Qt::AlignCenter, "A Game By");

        QFont nameFont("Microsoft YaHei UI", qMax(15, height() / 46), QFont::DemiBold);
        painter.setFont(nameFont);
        painter.setPen(QColor(238, 255, 214, 226));
        painter.drawText(QRectF(width() * 0.08, titleRect.bottom() + height() * 0.095,
                                width() * 0.84, 72),
                         Qt::AlignCenter | Qt::TextWordWrap,
                         "王梓涵   曹杰帅   朱志文   白杨   吕尚莹   马意轩");
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
    const qreal reveal = easeOutCubic(m_menuRevealProgress);
    QRectF menuDrawRect = m_canvasRect;
    if (m_menuRevealProgress < 1.0) {
        const qreal zoom = 1.035 - reveal * 0.035;
        const QPointF center = m_canvasRect.center();
        menuDrawRect.setSize(m_canvasRect.size() * zoom);
        menuDrawRect.moveCenter(center + QPointF(0, (1.0 - reveal) * height() * 0.018));
    }

    if (!m_menuCache.isNull()) {
        painter.drawPixmap(menuDrawRect.toRect(), m_menuCache);
    } else if (!menu.isNull()) {
        painter.drawPixmap(menuDrawRect, menu, QRectF(menu.rect()));
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
        button->setGlowColor(QColor(255, 222, 132, 0));
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
        const qreal delay = i * 0.055;
        const qreal buttonProgress = easeOutCubic((m_menuRevealProgress - delay) / 0.42);
        const qreal slide = (1.0 - buttonProgress) * qMax<qreal>(18.0, 42.0 * scale);
        const QRectF target(topLeft.x() + source.x() * scale,
                            topLeft.y() + source.y() * scale + slide,
                            source.width() * scale,
                            source.height() * scale);
        m_buttons[i]->setCanvasRect(target);
        m_buttons[i]->setVisible(buttonProgress > 0.01 || m_menuRevealProgress >= 1.0);
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
