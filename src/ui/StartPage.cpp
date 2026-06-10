#include "ui/StartPage.h"

#include "ui/ArtHotspot.h"

#include <QKeyEvent>
#include <QLabel>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QRadialGradient>
#include <QResizeEvent>
#include <QShowEvent>
#include <QtMath>

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
    , m_splashActive(true)
    , m_menuLayer(nullptr)
    , m_pressHint(nullptr)
    , m_ambientPhase(0.0)
{
    initUI();

    m_ambientTimer.setInterval(40);
    connect(&m_ambientTimer, &QTimer::timeout, this, [this]() {
        m_ambientPhase += 0.025;
        if (!m_splashActive) {
            update();
        }
    });
    m_ambientTimer.start();
}

void StartPage::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

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
    if (!menu.isNull()) {
        painter.drawPixmap(m_canvasRect, menu, QRectF(menu.rect()));
    }

    const qreal pulse = 0.5 + 0.5 * qSin(m_ambientPhase);
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
    m_menuLayer->show();
    m_menuLayer->raise();
    for (ArtHotspot *button : m_buttons) {
        button->refreshVisual();
    }
    update();
}

void StartPage::initUI()
{
    setAutoFillBackground(false);
    setFocusPolicy(Qt::StrongFocus);

    m_menuLayer = new QWidget(this);
    m_menuLayer->setAttribute(Qt::WA_TranslucentBackground);
    m_menuLayer->hide();

    const QString artwork = ":/images/artwork/main_menu.jpg";
    for (int i = 0; i < kMenuHotspots.size(); ++i) {
        const auto &spec = kMenuHotspots[i];
        auto *button = new ArtHotspot(artwork, spec.rect, m_menuLayer);
        button->setToolTip(QString::fromUtf8(spec.tooltip));
        button->setGlowColor(QColor(255, 222, 132));
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

    updateArtworkLayout();
}

void StartPage::revealMenu()
{
    if (!m_splashActive) {
        return;
    }

    m_splashActive = false;
    m_pressHint->hide();
    m_menuLayer->show();
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

    for (int i = 0; i < m_buttons.size(); ++i) {
        const QRect source = kMenuHotspots[i].rect;
        const QRectF target(topLeft.x() + source.x() * scale,
                            topLeft.y() + source.y() * scale,
                            source.width() * scale,
                            source.height() * scale);
        m_buttons[i]->setCanvasRect(target);
        m_buttons[i]->raise();
    }
    update();
}
