#include "ui/DeckPage.h"

#include "ui/ArtHotspot.h"
#include "ui/AudioManager.h"

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
    , m_selectedCardIndex(1)
    , m_cardPoolScroll(nullptr)
    , m_backHotspot(nullptr)
    , m_startHotspot(nullptr)
{
    createCardPoolData();

    m_selectedSlots = {1, 2, 3, 5, 6};
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
    for (int i = 0; i < kCardRects.size(); ++i) {
        auto *hotspot = new ArtHotspot(artwork, kCardRects[i], this);
        hotspot->setGlowColor(QColor(255, 220, 128));
        hotspot->setToolTip(m_allCards[i].name);
        hotspot->setClickHandler([this, i]() {
            refreshDetailPanel(i);
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
    m_backHotspot->setCanvasRect(scaledRect(kBackRect, m_canvasRect));
    m_startHotspot->setCanvasRect(scaledRect(kStartRect, m_canvasRect));
    m_backHotspot->raise();
    m_startHotspot->raise();
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
    QVector<game::core::CardKind> kinds;
    for (int cardIndex : m_selectedSlots) {
        if (cardIndex >= 0 && cardIndex < m_allCards.size()) {
            kinds.append(m_allCards[cardIndex].kind);
        }
    }
    return kinds;
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
