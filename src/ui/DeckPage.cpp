#include "ui/DeckPage.h"

#include "ui/ArtHotspot.h"
#include "ui/AudioManager.h"
#include "ui/CardCollection.h"

#include <QEasingCurve>
#include <QGraphicsOpacityEffect>
#include <QIcon>
#include <QLabel>
#include <QParallelAnimationGroup>
#include <QPainter>
#include <QPaintEvent>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QResizeEvent>
#include <QShowEvent>
#include <QStringList>
#include <QTimer>
#include <QToolTip>

namespace {

constexpr int kDesignWidth = 1672;
constexpr int kDesignHeight = 941;

const QVector<QRect> kCardRects = {
    {268, 207, 163, 210},
    {437, 207, 167, 210},
    {616, 207, 159, 210},
    {795, 207, 160, 210},
    {968, 207, 164, 210},
    {268, 423, 163, 201},
    {437, 423, 167, 201},
    {616, 423, 159, 201},
    {795, 423, 160, 201},
    {968, 423, 164, 201},
};

const QVector<QRect> kSlotRects = {
    {360, 704, 165, 171},
    {525, 704, 165, 171},
    {690, 704, 165, 171},
    {855, 704, 165, 171},
    {1020, 704, 165, 171},
};

const QRect kBackRect(42, 845, 196, 85);
const QRect kStartRect(1360, 844, 260, 86);
const QRect kDetailRect(1196, 257, 320, 426);

QRectF scaledRect(const QRect &source, const QRectF &canvas)
{
    const qreal sx = canvas.width() / kDesignWidth;
    const qreal sy = canvas.height() / kDesignHeight;
    return QRectF(canvas.left() + source.x() * sx,
                  canvas.top() + source.y() * sy,
                  source.width() * sx,
                  source.height() * sy);
}

QString cardTypeText(game::core::CardKind kind)
{
    if (game::core::isAttackCardKind(kind)) {
        return "攻击";
    }
    if (game::core::isProduceCardKind(kind)) {
        return "生产";
    }
    return "支援";
}

} // namespace

DeckPage::DeckPage(QWidget *parent)
    : QWidget(parent)
    , m_btnBack(nullptr)
    , m_titleLabel(nullptr)
    , m_detailPanel(nullptr)
    , m_btnStartBattle(nullptr)
    , m_ticketLabel(nullptr)
    , m_btnDrawPanel(nullptr)
    , m_drawOverlay(nullptr)
    , m_drawResultLabel(nullptr)
    , m_btnDrawOne(nullptr)
    , m_btnDrawTen(nullptr)
    , m_btnCloseDraw(nullptr)
    , m_btnUpgradeCard(nullptr)
    , m_selectedCardIndex(0)
    , m_cardPoolScroll(nullptr)
    , m_backHotspot(nullptr)
    , m_startHotspot(nullptr)
{
    CardCollection::initializeDefaults();
    createCardPoolData();

    m_selectedSlots = {0, 4, -1, -1, -1};
    initUI();
    connectSignals();
    refreshDetailPanel(m_selectedCardIndex);
    refreshDeckSlotsDisplay();
    updateStartBattleButton();
}

void DeckPage::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.fillRect(rect(), QColor(38, 52, 34));

    static QPixmap artwork(":/images/artwork/deck_atlas.png");
    if (!artwork.isNull()) {
        painter.drawPixmap(m_canvasRect, artwork, QRectF(artwork.rect()));
    }

    QLinearGradient shade(m_canvasRect.topLeft(), m_canvasRect.bottomRight());
    shade.setColorAt(0.0, QColor(30, 45, 27, 14));
    shade.setColorAt(0.56, QColor(30, 45, 27, 0));
    shade.setColorAt(1.0, QColor(23, 29, 19, 26));
    painter.fillRect(m_canvasRect, shade);
}

void DeckPage::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    setGraphicsEffect(nullptr);
    updateArtworkLayout();
    for (ArtHotspot *hotspot : m_cardHotspots) {
        hotspot->refreshVisual();
    }
    if (m_backHotspot) {
        m_backHotspot->refreshVisual();
    }
    if (m_startHotspot) {
        m_startHotspot->refreshVisual();
    }
    refreshDeckSlotsDisplay();
    updateStartBattleButton();
    refreshCollectionDisplay();
    update();
}

void DeckPage::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateArtworkLayout();
}

void DeckPage::createCardPoolData()
{
    m_allCards = {
        {game::core::CardKind::Produce, "Miner Pine", 3, 520, 30, 1, 2, 2.4,
         "优先采集资源，其次支援近处单位", "Lv2: 40  Lv3: 80", QColor(210, 151, 47)},
        {game::core::CardKind::Sniper, "Sniper Berry", 4, 420, 180, 8, 1, 1.6,
         "高血量敌人 > 普通敌人 > 资源单位", "Lv2: 40  Lv3: 80", QColor(119, 76, 151)},
        {game::core::CardKind::Specialist, "Berry Tank", 5, 900, 75, 2, 1, 2.0,
         "吸引最近敌人并保护后排", "Lv2: 55  Lv3: 110", QColor(89, 119, 54)},
        {game::core::CardKind::Heal, "Peach Healer", 4, 470, 60, 4, 2, 1.8,
         "最低血量友方 > 受伤友方", "Lv2: 45  Lv3: 90", QColor(220, 123, 118)},
        {game::core::CardKind::Attack, "Kiwi Scout", 2, 440, 105, 3, 5, 1.0,
         "最近敌人 > 远程敌人", "Lv2: 35  Lv3: 70", QColor(119, 142, 43)},
        {game::core::CardKind::Aoe, "Orange Bomber", 4, 500, 125, 3, 1, 1.5,
         "密集敌群 > 最近敌人", "Lv2: 50  Lv3: 100", QColor(220, 111, 37)},
        {game::core::CardKind::HeavyMedic, "Coco Defender", 3, 780, 45, 2, 1, 2.2,
         "受伤友方 > 自身周围单位", "Lv2: 50  Lv3: 100", QColor(104, 78, 46)},
        {game::core::CardKind::Arsenal, "Mango Engineer", 3, 540, 0, 0, 0, 0.0,
         "强化资源产出并支援防御设施", "Lv2: 45  Lv3: 90", QColor(218, 146, 47)},
        {game::core::CardKind::Attack, "Grape Blaster", 4, 520, 145, 4, 2, 1.3,
         "远程敌人 > 普通敌人 > 资源单位", "Lv2: 50  Lv3: 100", QColor(113, 70, 139)},
        {game::core::CardKind::Heal, "Papaya Support", 3, 430, 55, 4, 3, 1.7,
         "受伤友方 > 最低血量友方", "Lv2: 40  Lv3: 80", QColor(219, 132, 43)},
    };
}

void DeckPage::initUI()
{
    setAutoFillBackground(false);

    m_btnBack = new QPushButton(this);
    m_btnBack->hide();
    m_btnStartBattle = new QPushButton(this);
    m_btnStartBattle->hide();

    m_detailPanel = new QLabel(this);
    m_detailPanel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_detailPanel->setWordWrap(true);
    m_detailPanel->setStyleSheet(
        "QLabel {"
        " background-color: rgb(239, 219, 173);"
        " color: #3a2819;"
        " border: 2px solid rgba(92, 64, 36, 0.72);"
        " border-radius: 5px;"
        " padding: 14px;"
        " font-family: 'Microsoft YaHei UI', 'PingFang SC', sans-serif;"
        " font-size: 13px;"
        "}"
    );

    const QString artwork = ":/images/artwork/deck_atlas.png";
    for (int i = 0; i < m_allCards.size(); ++i) {
        auto *hotspot = new ArtHotspot(artwork, kCardRects[i], this);
        hotspot->setGlowColor(QColor(255, 220, 128));
        hotspot->setToolTip(m_allCards[i].name);
        hotspot->setClickHandler([this, i]() {
            refreshDetailPanel(i);
            if (!CardCollection::isOwned(m_allCards[i].kind)) {
                QToolTip::showText(mapToGlobal(scaledRect(kCardRects[i], m_canvasRect).center().toPoint()),
                                   "Locked. Open Supply Draw to unlock this card.", this);
                return;
            }
            for (int slot = 0; slot < MAX_DECK_SLOTS; ++slot) {
                if (m_selectedSlots[slot] == -1) {
                    m_selectedSlots[slot] = i;
                    animateCardToSlot(i, slot);
                    updateStartBattleButton();
                    return;
                }
            }
            QToolTip::showText(mapToGlobal(m_canvasRect.center().toPoint()),
                               "出战卡组已满，先点击下方卡牌移除一个槽位", this);
        });
        m_cardHotspots.append(hotspot);

        auto *lockLabel = new QLabel("LOCK", this);
        lockLabel->setAlignment(Qt::AlignCenter);
        lockLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
        lockLabel->setStyleSheet(
            "QLabel { color:#fff1c4; background:rgba(35,25,18,0.64);"
            " border:2px solid rgba(255,218,115,0.72); border-radius:5px;"
            " font-size:22px; font-weight:900; }");
        m_cardLockLabels.append(lockLabel);
    }

    for (int i = 0; i < MAX_DECK_SLOTS; ++i) {
        auto *slotButton = new QPushButton(this);
        slotButton->setCursor(Qt::PointingHandCursor);
        slotButton->setStyleSheet(
            "QPushButton {"
            " background-color: rgba(239, 219, 173, 0.97);"
            " color: #584025;"
            " border: 2px solid rgba(100, 70, 38, 0.76);"
            " border-radius: 4px;"
            " font-family: 'Microsoft YaHei UI', 'PingFang SC', sans-serif;"
            " font-size: 13px;"
            " font-weight: 700;"
            "}"
            "QPushButton:hover { border: 3px solid #efbf58; }"
            "QPushButton:pressed { background-color: rgba(222, 194, 138, 0.98); }"
        );
        connect(slotButton, &QPushButton::clicked, this, [this, i]() {
            if (m_selectedSlots[i] != -1) {
                m_selectedSlots[i] = -1;
                refreshDeckSlotsDisplay();
                updateStartBattleButton();
            }
        });
        m_slotButtons.append(slotButton);
    }

    m_backHotspot = new ArtHotspot(artwork, kBackRect, this);
    m_backHotspot->setGlowColor(QColor(255, 220, 128));
    m_backHotspot->setClickHandler([this]() { m_btnBack->click(); });

    m_startHotspot = new ArtHotspot(artwork, kStartRect, this);
    m_startHotspot->setGlowColor(QColor(255, 220, 128));
    m_startHotspot->setClickHandler([this]() {
        if (m_btnStartBattle->isEnabled()) {
            m_btnStartBattle->click();
        } else {
            QToolTip::showText(mapToGlobal(m_canvasRect.center().toPoint()),
                               m_btnStartBattle->toolTip(), this);
        }
    });

    m_ticketLabel = new QLabel(this);
    m_ticketLabel->setAlignment(Qt::AlignCenter);
    m_ticketLabel->setStyleSheet(
        "QLabel { color:#352314; background:rgba(244,224,174,0.96);"
        " border:2px solid rgba(92,64,36,0.8); border-radius:8px;"
        " font-size:15px; font-weight:900; padding:4px 8px; }");

    m_btnDrawPanel = new QPushButton("Draw", this);
    m_btnDrawPanel->setCursor(Qt::PointingHandCursor);
    m_btnDrawPanel->setStyleSheet(
        "QPushButton { color:#352314; background:rgba(246,218,147,0.97);"
        " border:2px solid #704821; border-radius:9px; font-size:15px; font-weight:900; }"
        "QPushButton:hover { background:#ffe4a0; border-color:#d4a047; }"
        "QPushButton:pressed { background:#c99653; }");
    connect(m_btnDrawPanel, &QPushButton::clicked, this, &DeckPage::openDrawPanel);

    m_drawOverlay = new QWidget(this);
    m_drawOverlay->hide();
    m_drawOverlay->setStyleSheet(
        "QWidget { background:rgba(37,51,35,0.88); border-radius:12px; }"
        "QLabel { color:#fff3ce; background:transparent; font-size:16px; font-weight:800; }"
        "QPushButton { color:#352314; background:#efd497; border:2px solid #704821;"
        " border-radius:9px; font-size:15px; font-weight:900; padding:6px; }"
        "QPushButton:hover { background:#ffe4a0; border-color:#d4a047; }"
        "QPushButton:pressed { background:#c99653; }");
    m_drawResultLabel = new QLabel(m_drawOverlay);
    m_drawResultLabel->setAlignment(Qt::AlignCenter);
    m_drawResultLabel->setWordWrap(true);
    m_btnDrawOne = new QPushButton("x1", m_drawOverlay);
    m_btnDrawTen = new QPushButton("x10", m_drawOverlay);
    m_btnCloseDraw = new QPushButton("Close", m_drawOverlay);
    connect(m_btnDrawOne, &QPushButton::clicked, this, [this]() { performDraw(1); });
    connect(m_btnDrawTen, &QPushButton::clicked, this, [this]() { performDraw(10); });
    connect(m_btnCloseDraw, &QPushButton::clicked, m_drawOverlay, &QWidget::hide);

    m_btnUpgradeCard = new QPushButton("Lv Up", this);
    m_btnUpgradeCard->setCursor(Qt::PointingHandCursor);
    m_btnUpgradeCard->setStyleSheet(m_btnDrawPanel->styleSheet());
    connect(m_btnUpgradeCard, &QPushButton::clicked, this, [this]() {
        if (m_selectedCardIndex < 0 || m_selectedCardIndex >= m_allCards.size()) return;
        if (CardCollection::upgrade(m_allCards[m_selectedCardIndex].kind)) {
            refreshDetailPanel(m_selectedCardIndex);
            refreshCollectionDisplay();
        }
    });

    updateArtworkLayout();
}

void DeckPage::updateArtworkLayout()
{
    const qreal scale = qMin(width() / qreal(kDesignWidth),
                             height() / qreal(kDesignHeight));
    const QSizeF canvasSize(kDesignWidth * scale, kDesignHeight * scale);
    const QPointF topLeft((width() - canvasSize.width()) / 2.0,
                          (height() - canvasSize.height()) / 2.0);
    m_canvasRect = QRectF(topLeft, canvasSize);

    for (int i = 0; i < m_cardHotspots.size(); ++i) {
        m_cardHotspots[i]->setCanvasRect(scaledRect(kCardRects[i], m_canvasRect));
    }
    for (int i = 0; i < m_slotButtons.size(); ++i) {
        m_slotButtons[i]->setGeometry(scaledRect(kSlotRects[i], m_canvasRect).toRect());
        m_slotButtons[i]->setIconSize(
            m_slotButtons[i]->size() - QSize(qMax(6, m_slotButtons[i]->width() / 18),
                                             qMax(6, m_slotButtons[i]->height() / 18)));
        m_slotButtons[i]->raise();
    }

    m_detailPanel->setGeometry(scaledRect(kDetailRect, m_canvasRect).toRect());
    m_detailPanel->raise();
    m_ticketLabel->setGeometry(scaledRect(QRect(1214, 688, 280, 42), m_canvasRect).toRect());
    m_btnDrawPanel->setGeometry(scaledRect(QRect(1214, 738, 132, 52), m_canvasRect).toRect());
    m_btnUpgradeCard->setGeometry(scaledRect(QRect(1362, 738, 132, 52), m_canvasRect).toRect());
    m_backHotspot->setCanvasRect(scaledRect(kBackRect, m_canvasRect));
    m_startHotspot->setCanvasRect(scaledRect(kStartRect, m_canvasRect));

    for (int i = 0; i < m_cardLockLabels.size(); ++i) {
        QRect lockRect = scaledRect(kCardRects[i], m_canvasRect).toRect().adjusted(8, 8, -8, -8);
        m_cardLockLabels[i]->setGeometry(lockRect);
        m_cardLockLabels[i]->raise();
    }
    QRect overlayRect = scaledRect(QRect(468, 170, 760, 590), m_canvasRect).toRect();
    m_drawOverlay->setGeometry(overlayRect);
    const int overlayW = overlayRect.width();
    const int overlayH = overlayRect.height();
    const int margin = qMax(18, overlayW / 26);
    const int gap = qMax(10, overlayW / 42);
    const int buttonW = (overlayW - margin * 2 - gap * 2) / 3;
    const int buttonH = qBound(42, overlayH / 9, 58);
    const int buttonY = overlayH - margin - buttonH;
    m_drawResultLabel->setGeometry(margin, margin, overlayW - margin * 2,
                                   qMax(160, buttonY - margin * 2));
    m_btnDrawOne->setGeometry(margin, buttonY, buttonW, buttonH);
    m_btnDrawTen->setGeometry(margin + buttonW + gap, buttonY, buttonW, buttonH);
    m_btnCloseDraw->setGeometry(margin + (buttonW + gap) * 2, buttonY, buttonW, buttonH);

    m_ticketLabel->raise();
    m_btnDrawPanel->raise();
    m_btnUpgradeCard->raise();
    m_backHotspot->raise();
    m_startHotspot->raise();
    if (m_drawOverlay->isVisible()) m_drawOverlay->raise();
    update();
}

void DeckPage::animateCardToSlot(int cardIndex, int slotIndex)
{
    if (cardIndex < 0 || cardIndex >= kCardRects.size()
        || slotIndex < 0 || slotIndex >= m_slotButtons.size()) {
        refreshDeckSlotsDisplay();
        return;
    }

    static QPixmap artwork(":/images/artwork/deck_atlas.png");
    const QPixmap cardArt = artwork.copy(kCardRects[cardIndex]);
    auto *flyingCard = new QLabel(this);
    flyingCard->setAttribute(Qt::WA_TransparentForMouseEvents);
    flyingCard->setPixmap(cardArt);
    flyingCard->setScaledContents(true);
    flyingCard->setStyleSheet(
        "background:rgba(255,240,190,0.18);"
        "border:3px solid #f2c65e; border-radius:5px;");

    const QRect start = scaledRect(kCardRects[cardIndex], m_canvasRect).toRect();
    const QRect end = m_slotButtons[slotIndex]->geometry().adjusted(6, 6, -6, -6);
    flyingCard->setGeometry(start);
    flyingCard->show();
    flyingCard->raise();

    auto *opacity = new QGraphicsOpacityEffect(flyingCard);
    flyingCard->setGraphicsEffect(opacity);
    auto *group = new QParallelAnimationGroup(flyingCard);
    auto *move = new QPropertyAnimation(flyingCard, "geometry", group);
    move->setDuration(420);
    move->setStartValue(start);
    move->setEndValue(end);
    move->setEasingCurve(QEasingCurve::InOutBack);
    auto *fade = new QPropertyAnimation(opacity, "opacity", group);
    fade->setDuration(420);
    fade->setStartValue(0.94);
    fade->setKeyValueAt(0.72, 1.0);
    fade->setEndValue(0.35);

    AudioManager::instance().playCardSelect();
    connect(group, &QParallelAnimationGroup::finished, this,
            [this, flyingCard]() {
                refreshDeckSlotsDisplay();
                flyingCard->deleteLater();
            });
    group->start(QAbstractAnimation::DeleteWhenStopped);
}

void DeckPage::refreshDeckSlotsDisplay()
{
    {
        static QPixmap artwork(":/images/artwork/deck_atlas.png");
        for (int i = 0; i < MAX_DECK_SLOTS; ++i) {
            QPushButton *button = m_slotButtons[i];
            const int cardIndex = m_selectedSlots[i];
            if (cardIndex < 0 || cardIndex >= m_allCards.size()
                || !CardCollection::isOwned(m_allCards[cardIndex].kind)) {
                m_selectedSlots[i] = -1;
                button->setIcon(QIcon());
                button->setText(QString("Empty %1").arg(i + 1));
                continue;
            }
            const QPixmap cardArt = artwork.copy(kCardRects[cardIndex]);
            button->setText(QString());
            button->setIcon(QIcon(cardArt));
            button->setToolTip(QString("%1 - click to remove").arg(m_allCards[cardIndex].name));
        }
        return;
    }
    static QPixmap artwork(":/images/artwork/deck_atlas.png");
    for (int i = 0; i < MAX_DECK_SLOTS; ++i) {
        QPushButton *button = m_slotButtons[i];
        const int cardIndex = m_selectedSlots[i];
        if (cardIndex < 0 || cardIndex >= m_allCards.size()) {
            button->setIcon(QIcon());
            button->setText(QString("空槽位 %1").arg(i + 1));
            continue;
        }

        const QPixmap cardArt = artwork.copy(kCardRects[cardIndex]);
        button->setText(QString());
        button->setIcon(QIcon(cardArt));
        button->setToolTip(QString("%1 - 点击移除").arg(m_allCards[cardIndex].name));
    }
}

void DeckPage::refreshDetailPanel(int cardIndex)
{
    {
        if (cardIndex < 0 || cardIndex >= m_allCards.size()) return;
        m_selectedCardIndex = cardIndex;
        for (int i = 0; i < m_cardHotspots.size(); ++i) {
            m_cardHotspots[i]->setSelected(i == cardIndex);
        }

        const auto &card = m_allCards[cardIndex];
        const bool owned = CardCollection::isOwned(card.kind);
        const int level = CardCollection::level(card.kind);
        const int fragments = CardCollection::fragments(card.kind);
        const int upgradeCost = CardCollection::upgradeCost(card.kind);
        m_detailPanel->setText(QString(
            "<div style='font-size:20px; font-weight:800; margin-bottom:10px;'>%1</div>"
            "<div style='color:#76552d; font-weight:700; margin-bottom:9px;'>%2 | %3</div>"
            "<hr style='border:0; border-top:1px solid #9b7545;'>"
            "<table cellspacing='5'>"
            "<tr><td><b>HP</b></td><td>%4</td></tr>"
            "<tr><td><b>ATK</b></td><td>%5</td></tr>"
            "<tr><td><b>Range</b></td><td>%6</td></tr>"
            "<tr><td><b>Interval</b></td><td>%7 s</td></tr>"
            "<tr><td><b>Move</b></td><td>%8 cells</td></tr>"
            "<tr><td><b>Cost</b></td><td>%9 Juice</td></tr>"
            "</table>"
            "<hr style='border:0; border-top:1px solid #9b7545;'>"
            "<div><b>Collection</b><br>Level %10 | Shards %11/%12</div>"
            "<div style='margin-top:9px;'><b>Battle Growth</b><br>%13</div>"
            "<div style='margin-top:9px;'><b>Target Priority</b><br>%14</div>")
            .arg(card.name)
            .arg(cardTypeText(card.kind))
            .arg(owned ? "Owned" : "Locked")
            .arg(card.maxHp)
            .arg(card.attack)
            .arg(card.attackRange)
            .arg(card.attackInterval)
            .arg(card.moveLimit)
            .arg(card.deployCost)
            .arg(level)
            .arg(fragments)
            .arg(upgradeCost > 0 ? upgradeCost : 0)
            .arg(card.upgradeCostDesc)
            .arg(card.priorityDesc));
        refreshCollectionDisplay();
        return;
    }
    if (cardIndex < 0 || cardIndex >= m_allCards.size()) {
        return;
    }

    m_selectedCardIndex = cardIndex;
    for (int i = 0; i < m_cardHotspots.size(); ++i) {
        m_cardHotspots[i]->setSelected(i == cardIndex);
    }

    const auto &card = m_allCards[cardIndex];
    m_detailPanel->setText(QString(
        "<div style='font-size:20px; font-weight:800; margin-bottom:10px;'>%1</div>"
        "<div style='color:#76552d; font-weight:700; margin-bottom:9px;'>%2</div>"
        "<hr style='border:0; border-top:1px solid #9b7545;'>"
        "<table cellspacing='5'>"
        "<tr><td><b>HP</b></td><td>%3</td></tr>"
        "<tr><td><b>攻击</b></td><td>%4</td></tr>"
        "<tr><td><b>射程</b></td><td>%5</td></tr>"
        "<tr><td><b>间隔</b></td><td>%6 秒</td></tr>"
        "<tr><td><b>移动</b></td><td>%7 格</td></tr>"
        "<tr><td><b>费用</b></td><td>%8</td></tr>"
        "</table>"
        "<hr style='border:0; border-top:1px solid #9b7545;'>"
        "<div><b>升级</b><br>%9</div>"
        "<div style='margin-top:9px;'><b>优先目标</b><br>%10</div>")
        .arg(card.name)
        .arg(cardTypeText(card.kind))
        .arg(card.maxHp)
        .arg(card.attack)
        .arg(card.attackRange)
        .arg(card.attackInterval)
        .arg(card.moveLimit)
        .arg(card.deployCost)
        .arg(card.upgradeCostDesc)
        .arg(card.priorityDesc));
}

void DeckPage::updateStartBattleButton()
{
    {
        int filledCount = 0;
        for (int value : m_selectedSlots) {
            if (value >= 0
                && value < m_allCards.size()
                && CardCollection::isOwned(m_allCards[value].kind)) {
                ++filledCount;
            }
        }
        const bool complete = filledCount == MAX_DECK_SLOTS;
        m_btnStartBattle->setEnabled(complete);
        m_btnStartBattle->setToolTip(
            complete ? "Start battle" : QString("Choose %1 more owned cards").arg(MAX_DECK_SLOTS - filledCount));
        if (m_startHotspot) {
            m_startHotspot->setSelected(complete);
        }
        return;
    }
    int filledCount = 0;
    for (int value : m_selectedSlots) {
        if (value >= 0) {
            ++filledCount;
        }
    }
    const bool complete = filledCount == MAX_DECK_SLOTS;
    m_btnStartBattle->setEnabled(complete);
    m_btnStartBattle->setToolTip(
        complete ? "开始战斗" : QString("还需要选择 %1 张卡牌").arg(MAX_DECK_SLOTS - filledCount));
    if (m_startHotspot) {
        m_startHotspot->setSelected(complete);
    }
}

QVector<game::core::CardKind> DeckPage::getSelectedKinds() const
{
    {
        QVector<game::core::CardKind> kinds;
        for (int cardIndex : m_selectedSlots) {
            if (cardIndex >= 0
                && cardIndex < m_allCards.size()
                && CardCollection::isOwned(m_allCards[cardIndex].kind)) {
                kinds.append(m_allCards[cardIndex].kind);
            }
        }
        return kinds;
    }
    QVector<game::core::CardKind> kinds;
    for (int cardIndex : m_selectedSlots) {
        if (cardIndex >= 0 && cardIndex < m_allCards.size()) {
            kinds.append(m_allCards[cardIndex].kind);
        }
    }
    return kinds;
}

void DeckPage::refreshCollectionDisplay()
{
    if (!m_ticketLabel) return;
    m_ticketLabel->setText(QString("Tickets %1").arg(CardCollection::tickets()));

    for (int i = 0; i < m_cardLockLabels.size() && i < m_allCards.size(); ++i) {
        const bool owned = CardCollection::isOwned(m_allCards[i].kind);
        m_cardLockLabels[i]->setVisible(!owned);
        if (!owned) {
            m_cardLockLabels[i]->raise();
        }
        m_cardHotspots[i]->setToolTip(owned
            ? QString("%1  Lv%2  Shards %3")
                  .arg(m_allCards[i].name)
                  .arg(CardCollection::level(m_allCards[i].kind))
                  .arg(CardCollection::fragments(m_allCards[i].kind))
            : QString("%1 - locked, draw to unlock").arg(m_allCards[i].name));
    }

    if (m_selectedCardIndex >= 0 && m_selectedCardIndex < m_allCards.size()) {
        const auto kind = m_allCards[m_selectedCardIndex].kind;
        m_btnUpgradeCard->setVisible(CardCollection::isOwned(kind));
        m_btnUpgradeCard->setEnabled(CardCollection::canUpgrade(kind));
        const int cost = CardCollection::upgradeCost(kind);
        m_btnUpgradeCard->setText(cost > 0
                                      ? QString("Lv Up %1/%2")
                                            .arg(CardCollection::fragments(kind))
                                            .arg(cost)
                                      : "Max Lv");
    }
    refreshDeckSlotsDisplay();
    updateStartBattleButton();
}

void DeckPage::openDrawPanel()
{
    m_drawResultLabel->setText(
        QString("<div style='font-size:24px; font-weight:900;'>Supply Draw</div>"
                "<div style='margin-top:18px;'>Tickets: %1</div>"
                "<div style='margin-top:22px; color:#ffe3a4;'>New cards unlock towers.</div>"
                "<div style='margin-top:12px;'>Duplicates become shards for small level bonuses.</div>")
            .arg(CardCollection::tickets()));
    m_drawOverlay->show();
    m_drawOverlay->raise();
}

void DeckPage::performDraw(int count)
{
    const QVector<DrawResult> results = CardCollection::drawMany(count);
    if (results.isEmpty()) {
        m_drawResultLabel->setText(
            "<div style='font-size:24px; font-weight:900;'>No tickets left</div>"
            "<div style='margin-top:22px;'>Finish a battle to earn more supply tickets.</div>");
        refreshCollectionDisplay();
        return;
    }

    QStringList lines;
    int newCount = 0;
    int shardCount = 0;
    for (const DrawResult& result : results) {
        if (result.isNew) {
            ++newCount;
            lines << QString("<span style='color:#fff4a8;'>NEW!</span> %1")
                         .arg(cardNameForKind(result.kind));
        } else {
            shardCount += result.fragmentsGained;
            lines << QString("%1  +%2 shards")
                         .arg(cardNameForKind(result.kind))
                         .arg(result.fragmentsGained);
        }
    }

    m_drawResultLabel->setText(QString(
        "<div style='font-size:24px; font-weight:900;'>Supply Opened</div>"
        "<div style='margin-top:12px;'>New cards: %1 &nbsp;&nbsp; Shards: %2</div>"
        "<div style='margin-top:20px; line-height:135%;'>%3</div>"
        "<div style='margin-top:18px;'>Tickets left: %4</div>")
        .arg(newCount)
        .arg(shardCount)
        .arg(lines.join("<br>"))
        .arg(CardCollection::tickets()));
    refreshDetailPanel(qMax(0, m_selectedCardIndex));
    refreshCollectionDisplay();
}

QString DeckPage::cardNameForKind(game::core::CardKind kind) const
{
    const int index = indexForKind(kind);
    return index >= 0 ? m_allCards[index].name : QString("Unknown Card");
}

int DeckPage::indexForKind(game::core::CardKind kind) const
{
    for (int i = 0; i < m_allCards.size(); ++i) {
        if (m_allCards[i].kind == kind) {
            return i;
        }
    }
    return -1;
}

void DeckPage::connectSignals()
{
    connect(m_btnBack, &QPushButton::clicked, this, &DeckPage::signalBack);
    connect(m_btnStartBattle, &QPushButton::clicked, this, [this]() {
        const QVector<game::core::CardKind> kinds = getSelectedKinds();
        emit signalDeckSelected(kinds);
        emit signalBattleStart();
    });
}
