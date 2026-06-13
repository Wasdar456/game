#include "ui/ResultPage.h"

#include <QLabel>
#include <QLinearGradient>
#include <QPainter>
#include <QPaintEvent>
#include <QPushButton>
#include <QRadialGradient>
#include <QResizeEvent>
#include <QShowEvent>
#include <QtMath>

#include <cmath>

namespace {

void drawCover(QPainter &painter, const QPixmap &pixmap, const QRect &target)
{
    QSize scaled = pixmap.size();
    scaled.scale(target.size(), Qt::KeepAspectRatioByExpanding);
    const QRect drawRect(QPoint(target.x() + (target.width() - scaled.width()) / 2,
                                target.y() + (target.height() - scaled.height()) / 2),
                         scaled);
    painter.drawPixmap(drawRect, pixmap);
}

QString outcomeTitle(BattleOutcome outcome)
{
    switch (outcome) {
    case BattleOutcome::Victory: return "战斗胜利";
    case BattleOutcome::Defeat: return "战斗失败";
    case BattleOutcome::Draw: return "平局";
    }
    return "战斗结束";
}

} // namespace

ResultPage::ResultPage(QWidget *parent)
    : QWidget(parent)
    , m_resultLabel(nullptr)
    , m_modeLabel(nullptr)
    , m_sideLabel(nullptr)
    , m_waveValue(nullptr)
    , m_killValue(nullptr)
    , m_coreValue(nullptr)
    , m_escapeValue(nullptr)
    , m_messageLabel(nullptr)
    , m_replayButton(nullptr)
    , m_lobbyButton(nullptr)
    , m_phase(0.0)
{
    initUI();
    m_animationTimer.setInterval(33);
    connect(&m_animationTimer, &QTimer::timeout, this, [this]() {
        m_phase += 0.035;
        update();
    });
    m_animationTimer.start();
}

void ResultPage::setResult(const BattleResult &result)
{
    m_result = result;
    updateContent();
    update();
}

void ResultPage::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    static QPixmap background(":/images/artwork/main_menu.jpg");
    if (!background.isNull()) {
        drawCover(painter, background, rect());
    } else {
        painter.fillRect(rect(), QColor(55, 68, 43));
    }

    painter.fillRect(rect(), QColor(24, 22, 16, 118));

    const bool victory = m_result.outcome == BattleOutcome::Victory;
    const QColor accent = victory ? QColor(255, 210, 91) : QColor(192, 73, 58);
    const qreal pulse = 0.5 + 0.5 * qSin(m_phase);

    QRadialGradient glow(m_boardRect.center(), m_boardRect.width() * 0.72);
    QColor center = accent;
    center.setAlpha(qRound(42 + pulse * 24));
    glow.setColorAt(0.0, center);
    glow.setColorAt(1.0, QColor(accent.red(), accent.green(), accent.blue(), 0));
    painter.fillRect(rect(), glow);

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(25, 16, 10, 105));
    painter.drawRoundedRect(m_boardRect.translated(0, 12), 10, 10);

    QLinearGradient parchment(m_boardRect.topLeft(), m_boardRect.bottomLeft());
    parchment.setColorAt(0.0, QColor(249, 226, 174));
    parchment.setColorAt(0.48, QColor(236, 205, 145));
    parchment.setColorAt(1.0, QColor(216, 177, 112));
    painter.setBrush(parchment);
    painter.setPen(QPen(QColor(77, 50, 29), 4));
    painter.drawRoundedRect(m_boardRect, 9, 9);

    painter.setPen(QPen(QColor(accent.red(), accent.green(), accent.blue(),
                                  qRound(145 + pulse * 80)), 3));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(m_boardRect.adjusted(9, 9, -9, -9), 6, 6);

    painter.setPen(QPen(QColor(105, 72, 38, 135), 2));
    const qreal lineY = m_boardRect.top() + m_boardRect.height() * 0.34;
    painter.drawLine(QPointF(m_boardRect.left() + 50, lineY),
                     QPointF(m_boardRect.right() - 50, lineY));

    for (int i = 0; i < 22; ++i) {
        const qreal seed = i * 0.83;
        const qreal x = width() * (0.08 + 0.84 * (i % 11) / 10.0);
        const qreal travel = std::fmod(m_phase * (18 + i % 5) + seed * 80.0,
                                      height() + 80.0);
        const qreal y = height() + 30.0 - travel;
        QColor particle = accent;
        particle.setAlpha(75 + (i % 4) * 25);
        painter.setPen(Qt::NoPen);
        painter.setBrush(particle);
        painter.drawEllipse(QPointF(x + qSin(m_phase + seed) * 18.0, y),
                            2.0 + i % 3, 2.0 + i % 3);
    }
}

void ResultPage::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateLayout();
}

void ResultPage::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    setGraphicsEffect(nullptr);
    updateContent();
    updateLayout();
}

void ResultPage::initUI()
{
    setAutoFillBackground(false);

    auto makeLabel = [this](Qt::Alignment alignment = Qt::AlignCenter) {
        auto *label = new QLabel(this);
        label->setAlignment(alignment);
        label->setAttribute(Qt::WA_TranslucentBackground);
        return label;
    };

    m_resultLabel = makeLabel();
    m_modeLabel = makeLabel();
    m_sideLabel = makeLabel();
    m_waveValue = makeLabel();
    m_killValue = makeLabel();
    m_coreValue = makeLabel();
    m_escapeValue = makeLabel();
    m_messageLabel = makeLabel();
    m_messageLabel->setWordWrap(true);

    m_replayButton = new QPushButton("再来一次", this);
    m_lobbyButton = new QPushButton("返回大厅", this);
    m_replayButton->setCursor(Qt::PointingHandCursor);
    m_lobbyButton->setCursor(Qt::PointingHandCursor);

    connect(m_replayButton, &QPushButton::clicked, this, &ResultPage::signalReplay);
    connect(m_lobbyButton, &QPushButton::clicked, this, &ResultPage::signalReturnToLobby);

    updateContent();
    updateLayout();
}

void ResultPage::updateContent()
{
    const bool victory = m_result.outcome == BattleOutcome::Victory;
    const QString accent = victory ? "#8a5a13" : "#8b2f24";
    m_resultLabel->setText(outcomeTitle(m_result.outcome));
    m_resultLabel->setStyleSheet(QString(
        "color:%1; font-family:'Microsoft YaHei UI','PingFang SC',sans-serif;"
        "font-size:44px; font-weight:900; background:transparent;").arg(accent));

    m_modeLabel->setText(m_result.isPvp ? "MULTIPLAYER PVP" : "SINGLE PVE");
    m_modeLabel->setStyleSheet(
        "color:#75502c; font-size:14px; font-weight:800; letter-spacing:0px;"
        "background:transparent;");

    if (m_result.isPvp) {
        QString localStatus;
        QString opponentStatus;
        if (m_result.outcome == BattleOutcome::Victory) {
            localStatus = "胜利";
            opponentStatus = "失败";
        } else if (m_result.outcome == BattleOutcome::Defeat) {
            localStatus = "失败";
            opponentStatus = "胜利";
        } else {
            localStatus = "平局";
            opponentStatus = "平局";
        }
        m_sideLabel->setText(QString("我方  %1 · 核心 %2     对方  %3 · 核心 %4")
                                 .arg(localStatus)
                                 .arg(m_result.localCoreHealth)
                                 .arg(opponentStatus)
                                 .arg(m_result.opponentCoreHealth));
        m_sideLabel->show();
    } else {
        m_sideLabel->hide();
    }
    m_sideLabel->setStyleSheet(
        "color:#4b321f; font-size:18px; font-weight:800; background:transparent;");

    const QString statStyle =
        "color:#3d291a; font-family:'Microsoft YaHei UI','PingFang SC',sans-serif;"
        "font-size:18px; font-weight:800; background:rgba(255,244,211,92);"
        "border:1px solid rgba(96,63,34,100); border-radius:5px;";
    m_waveValue->setText(QString("生存波次\n%1").arg(m_result.wave));
    m_killValue->setText(QString("消灭数量\n%1").arg(m_result.defeatedMonsters));
    m_coreValue->setText(QString("剩余核心\n%1").arg(m_result.localCoreHealth));
    m_escapeValue->setText(QString("漏怪数量\n%1").arg(m_result.escapedMonsters));
    m_waveValue->setStyleSheet(statStyle);
    m_killValue->setStyleSheet(statStyle);
    m_coreValue->setStyleSheet(statStyle);
    m_escapeValue->setStyleSheet(statStyle);

    if (m_result.outcome == BattleOutcome::Victory) {
        m_messageLabel->setText(m_result.isPvp
            ? "你的防线坚持到了最后，对方核心已经被摧毁。"
            : "全部 10 波敌人已清除，果汁湾守卫成功！");
    } else if (m_result.outcome == BattleOutcome::Draw) {
        m_messageLabel->setText("双方核心同时被摧毁，这是一场势均力敌的战斗。");
    } else {
        m_messageLabel->setText(m_result.isPvp
            ? "我方核心已被摧毁。调整卡组和部署，再发起一次挑战。"
            : "核心失守。重新安排卡组与部署，再守一次果汁湾。");
    }
    m_messageLabel->setStyleSheet(
        "color:#5b3d25; font-size:16px; font-weight:700; background:transparent;");

    m_replayButton->setText(m_result.isPvp ? "再次对战" : "再来一次");
    m_replayButton->setStyleSheet(buttonStyle(true));
    m_lobbyButton->setStyleSheet(buttonStyle(false));
}

void ResultPage::updateLayout()
{
    const qreal boardWidth = qMin<qreal>(840.0, width() * 0.74);
    const qreal boardHeight = qMin<qreal>(600.0, height() * 0.82);
    m_boardRect = QRectF((width() - boardWidth) / 2.0,
                         (height() - boardHeight) / 2.0,
                         boardWidth, boardHeight);

    const int left = qRound(m_boardRect.left());
    const int top = qRound(m_boardRect.top());
    const int boardW = qRound(m_boardRect.width());
    const int boardH = qRound(m_boardRect.height());

    m_resultLabel->setGeometry(left + 35, top + 30, boardW - 70, 62);
    m_modeLabel->setGeometry(left + 35, top + 92, boardW - 70, 28);
    m_sideLabel->setGeometry(left + 50, top + 122, boardW - 100, 34);

    const int statTop = top + qRound(boardH * 0.39);
    const int gap = 12;
    const int statW = (boardW - 100 - gap * 3) / 4;
    const int statH = qMax(78, qRound(boardH * 0.16));
    QLabel *stats[] = {m_waveValue, m_killValue, m_coreValue, m_escapeValue};
    for (int i = 0; i < 4; ++i) {
        stats[i]->setGeometry(left + 50 + i * (statW + gap), statTop, statW, statH);
    }

    m_messageLabel->setGeometry(left + 70, statTop + statH + 22, boardW - 140, 58);

    const int buttonW = qMin(235, (boardW - 120) / 2);
    const int buttonH = 58;
    const int buttonY = top + boardH - buttonH - 42;
    m_replayButton->setGeometry(left + boardW / 2 - buttonW - 12,
                                buttonY, buttonW, buttonH);
    m_lobbyButton->setGeometry(left + boardW / 2 + 12,
                               buttonY, buttonW, buttonH);
}

QString ResultPage::buttonStyle(bool primary) const
{
    const bool victory = m_result.outcome == BattleOutcome::Victory;
    const QString top = primary
        ? (victory ? "#e7b957" : "#c85e49")
        : "#d9ad68";
    const QString bottom = primary
        ? (victory ? "#b97825" : "#8f352c")
        : "#9a6537";
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
