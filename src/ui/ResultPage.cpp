#include "ui/ResultPage.h"

#include <QLabel>
#include <QFont>
#include <QPainter>
#include <QPaintEvent>
#include <QPushButton>
#include <QResizeEvent>
#include <QShowEvent>

namespace {

constexpr qreal DesignWidth = 1672.0;
constexpr qreal DesignHeight = 941.0;

QRectF fittedRect(const QSize& source, const QRect& target)
{
    QSizeF scaled = source;
    scaled.scale(target.size(), Qt::KeepAspectRatio);
    return QRectF(target.center().x() - scaled.width() * 0.5,
                  target.center().y() - scaled.height() * 0.5,
                  scaled.width(), scaled.height());
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
    , m_reportButton(nullptr)
    , m_phase(0.0)
{
    initUI();
}

void ResultPage::setResult(const BattleResult &result)
{
    m_result = result;
    updateContent();
    updateLayout();
    update();
}

void ResultPage::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.fillRect(rect(), QColor(18, 22, 18));

    static const QPixmap victory(":/images/new_art/result_victory_clean.png");
    static const QPixmap defeat(":/images/new_art/result_defeat_clean.png");
    const QPixmap &art = m_result.outcome == BattleOutcome::Victory ? victory : defeat;
    if (!art.isNull()) {
        m_boardRect = fittedRect(art.size(), rect());
        painter.drawPixmap(m_boardRect, art, QRectF(art.rect()));

        const auto mapped = [this](const QRectF& source) {
            return QRectF(m_boardRect.left() + source.x() * m_boardRect.width() / DesignWidth,
                          m_boardRect.top() + source.y() * m_boardRect.height() / DesignHeight,
                          source.width() * m_boardRect.width() / DesignWidth,
                          source.height() * m_boardRect.height() / DesignHeight);
        };

        const bool won = m_result.outcome == BattleOutcome::Victory;
        const qreal scale = m_boardRect.width() / DesignWidth;
        QFont statFont("Segoe UI", qMax(12, qRound(25 * scale)), QFont::DemiBold);
        painter.setFont(statFont);
        painter.setPen(QColor(56, 38, 24));

        const qreal rowY = won ? 327.0 : 292.0;
        const qreal rowGap = won ? 56.0 : 60.0;
        const qreal labelX = won ? 620.0 : 625.0;
        const qreal valueX = won ? 1000.0 : 990.0;
        const QString labels[] = {
            "Wave Reached:",
            "Core HP Left:",
            "Enemies Defeated:"
        };
        const QString values[] = {
            QString::number(m_result.wave),
            QString::number(m_result.localCoreHealth),
            QString::number(m_result.defeatedMonsters)
        };
        for (int i = 0; i < 3; ++i) {
            painter.drawText(mapped(QRectF(labelX, rowY + rowGap * i, 350, 42)),
                             Qt::AlignVCenter | Qt::AlignLeft, labels[i]);
            painter.drawText(mapped(QRectF(valueX, rowY + rowGap * i, 120, 42)),
                             Qt::AlignVCenter | Qt::AlignRight, values[i]);
        }

        QFont rewardFont("Segoe UI", qMax(13, qRound(28 * scale)), QFont::Bold);
        painter.setFont(rewardFont);
        const QString reward = m_result.isPvp
            ? QString("Supply +%1").arg(m_result.supplyTicketsEarned)
            : QString("Supply +%1").arg(m_result.supplyTicketsEarned);
        painter.drawText(mapped(won ? QRectF(780, 535, 355, 70)
                                    : QRectF(770, 535, 365, 70)),
                         Qt::AlignCenter, reward);
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

    auto makeLabel = [this]() {
        auto *label = new QLabel(this);
        label->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
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

    m_resultLabel->hide();
    m_modeLabel->hide();
    m_escapeValue->hide();
    m_messageLabel->hide();

    m_replayButton = new QPushButton(this);
    m_lobbyButton = new QPushButton(this);
    m_reportButton = new QPushButton(this);
    m_replayButton->setCursor(Qt::PointingHandCursor);
    m_lobbyButton->setCursor(Qt::PointingHandCursor);
    m_reportButton->setCursor(Qt::PointingHandCursor);

    connect(m_replayButton, &QPushButton::clicked, this, &ResultPage::signalReplay);
    connect(m_lobbyButton, &QPushButton::clicked, this, &ResultPage::signalReturnToLobby);
    connect(m_reportButton, &QPushButton::clicked, this, &ResultPage::signalReport);

    updateContent();
    updateLayout();
}

void ResultPage::updateContent()
{
    for (QLabel *label : {m_resultLabel, m_modeLabel, m_sideLabel,
                          m_waveValue, m_killValue, m_coreValue,
                          m_escapeValue, m_messageLabel}) {
        label->hide();
    }

    const QString hotspotStyle =
        "QPushButton { background:transparent; border:3px solid transparent;"
        " border-radius:7px; }"
        "QPushButton:hover { background:rgba(255,235,140,28);"
        " border-color:rgba(255,235,150,190); }"
        "QPushButton:pressed { background:rgba(70,45,24,48); }";
    m_replayButton->setStyleSheet(hotspotStyle);
    m_lobbyButton->setStyleSheet(hotspotStyle);
    m_reportButton->setStyleSheet(hotspotStyle);
    m_replayButton->setToolTip(m_result.isPvp ? "Rematch" : "Retry");
    m_lobbyButton->setToolTip("Back to Menu");
    m_reportButton->setToolTip("Battle replay and data report");
}

void ResultPage::updateLayout()
{
    m_boardRect = fittedRect(QSize(qRound(DesignWidth), qRound(DesignHeight)), rect());
    const auto mapped = [this](const QRectF& source) {
        return QRectF(m_boardRect.left() + source.x() * m_boardRect.width() / DesignWidth,
                      m_boardRect.top() + source.y() * m_boardRect.height() / DesignHeight,
                      source.width() * m_boardRect.width() / DesignWidth,
                      source.height() * m_boardRect.height() / DesignHeight).toRect();
    };

    const bool victory = m_result.outcome == BattleOutcome::Victory;
    if (victory) {
        m_replayButton->setGeometry(mapped(QRectF(320, 808, 292, 96)));
        m_lobbyButton->setGeometry(mapped(QRectF(653, 808, 315, 96)));
        m_reportButton->setGeometry(mapped(QRectF(1033, 808, 330, 96)));
    } else {
        m_replayButton->setGeometry(mapped(QRectF(305, 816, 290, 98)));
        m_lobbyButton->setGeometry(mapped(QRectF(667, 816, 320, 98)));
        m_reportButton->setGeometry(mapped(QRectF(1074, 816, 330, 98)));
    }

    for (QWidget *widget : {static_cast<QWidget*>(m_replayButton),
                            static_cast<QWidget*>(m_lobbyButton),
                            static_cast<QWidget*>(m_reportButton)}) {
        widget->raise();
    }
}

QString ResultPage::buttonStyle(bool primary) const
{
    Q_UNUSED(primary);
    return QString();
}
