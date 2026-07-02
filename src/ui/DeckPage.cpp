#include "ui/DeckPage.h"

#include "core/data/CardSpecs.h"
#include "ui/ArtHotspot.h"
#include "ui/AudioManager.h"
#include "ui/CardCollection.h"

#include <QEasingCurve>
#include <QComboBox>
#include <QGraphicsOpacityEffect>
#include <QIcon>
#include <QInputDialog>
#include <QLabel>
#include <QFontMetrics>
#include <QLineEdit>
#include <QMessageBox>
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

const QVector<QRect> kSquadRects = {
    {248, 636, 150, 49},
    {406, 637, 138, 43},
    {553, 637, 137, 43},
    {700, 637, 132, 44},
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

QColor themeColorForCardKind(game::core::CardKind kind)
{
    switch (kind) {
    case game::core::CardKind::Produce: return QColor(210, 151, 47);
    case game::core::CardKind::Sniper: return QColor(119, 76, 151);
    case game::core::CardKind::Specialist: return QColor(89, 119, 54);
    case game::core::CardKind::Heal: return QColor(220, 123, 118);
    case game::core::CardKind::Attack: return QColor(119, 142, 43);
    case game::core::CardKind::Aoe: return QColor(220, 111, 37);
    case game::core::CardKind::HeavyMedic: return QColor(104, 78, 46);
    case game::core::CardKind::Arsenal: return QColor(218, 146, 47);
    case game::core::CardKind::Attack2: return QColor(113, 70, 139);
    case game::core::CardKind::Heal2: return QColor(219, 132, 43);
    }
    return QColor(160, 128, 84);
}

QString skillTextForSpec(const game::core::CardSpec& spec)
{
    if (game::core::isProduceCardKind(spec.kind)) {
        return QString("每次技能冷却完成产出 %1 资源").arg(spec.resourceYield);
    }
    if (game::core::isHealCardKind(spec.kind)) {
        return QString("每次技能冷却完成治疗 %1 生命").arg(spec.healAmount);
    }
    if (spec.projectileKind == game::core::ProjectileKind::Aoe) {
        return QString("发射范围弹体，命中后溅射半径 %1 格").arg(spec.splashRadius);
    }
    if (spec.projectileKind == game::core::ProjectileKind::Sniper) {
        return "远距离单体狙击弹";
    }
    return "单体攻击单位";
}

QString roleNoteForSpec(const game::core::CardSpec& spec)
{
    if (game::core::isProduceCardKind(spec.kind)) {
        return "Economy unit. Protect it early to accelerate deployments.";
    }
    if (game::core::isHealCardKind(spec.kind)) {
        return "Support unit. Keeps damaged allies alive inside its range.";
    }
    if (spec.projectileKind == game::core::ProjectileKind::Aoe) {
        return "Area attacker. Best against clustered enemies on bends.";
    }
    if (spec.projectileKind == game::core::ProjectileKind::Sniper) {
        return "Long-range attacker. Covers distant lanes from safe tiles.";
    }
    return "Frontline attacker. Reliable damage with flexible placement.";
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
    , m_drawTitleLabel(nullptr)
    , m_drawBodyLabel(nullptr)
    , m_drawResultLabel(nullptr)
    , m_drawCardsPanel(nullptr)
    , m_btnDrawOne(nullptr)
    , m_btnDrawTen(nullptr)
    , m_btnCloseDraw(nullptr)
    , m_btnUpgradeCard(nullptr)
    , m_presetTitleLabel(nullptr)
    , m_presetSelector(nullptr)
    , m_btnSavePreset(nullptr)
    , m_btnLoadPreset(nullptr)
    , m_btnDeletePreset(nullptr)
    , m_selectedCardIndex(0)
    , m_cardPoolScroll(nullptr)
    , m_backHotspot(nullptr)
    , m_startHotspot(nullptr)
    , m_lastDrawResultCount(0)
    , m_currentSquadIndex(0)
    , m_drawOverlayVisible(false)
{
    CardCollection::initializeDefaults();
    createCardPoolData();

    m_selectedSlots = {0, 4, -1, -1, -1};
    initUI();
    connectSignals();
    loadSquadToDeck(m_currentSquadIndex);
    refreshDetailPanel(m_selectedCardIndex);
    refreshDeckSlotsDisplay();
    updateStartBattleButton();
    refreshSquadVisuals();
    updateArtworkLayout();
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
    for (ArtHotspot *hotspot : m_squadHotspots) {
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
    refreshPresetControls();
    refreshSquadVisuals();
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
        {game::core::CardKind::Produce,
         QString::fromUtf8(game::core::cardName(game::core::CardKind::Produce)),
         game::core::cardSpec(game::core::CardKind::Produce).deployCost,
         game::core::cardSpec(game::core::CardKind::Produce).maxHp,
         game::core::cardSpec(game::core::CardKind::Produce).attack,
         game::core::cardSpec(game::core::CardKind::Produce).attackRange,
         game::core::cardSpec(game::core::CardKind::Produce).moveLimit,
         game::core::cardSpec(game::core::CardKind::Produce).skillCooldownSeconds,
         skillTextForSpec(game::core::cardSpec(game::core::CardKind::Produce)),
         QString::fromUtf8(game::core::cardSpec(game::core::CardKind::Produce).priorityText),
         themeColorForCardKind(game::core::CardKind::Produce)},
        {game::core::CardKind::Sniper,
         QString::fromUtf8(game::core::cardName(game::core::CardKind::Sniper)),
         game::core::cardSpec(game::core::CardKind::Sniper).deployCost,
         game::core::cardSpec(game::core::CardKind::Sniper).maxHp,
         game::core::cardSpec(game::core::CardKind::Sniper).attack,
         game::core::cardSpec(game::core::CardKind::Sniper).attackRange,
         game::core::cardSpec(game::core::CardKind::Sniper).moveLimit,
         game::core::cardSpec(game::core::CardKind::Sniper).skillCooldownSeconds,
         skillTextForSpec(game::core::cardSpec(game::core::CardKind::Sniper)),
         QString::fromUtf8(game::core::cardSpec(game::core::CardKind::Sniper).priorityText),
         themeColorForCardKind(game::core::CardKind::Sniper)},
        {game::core::CardKind::Specialist,
         QString::fromUtf8(game::core::cardName(game::core::CardKind::Specialist)),
         game::core::cardSpec(game::core::CardKind::Specialist).deployCost,
         game::core::cardSpec(game::core::CardKind::Specialist).maxHp,
         game::core::cardSpec(game::core::CardKind::Specialist).attack,
         game::core::cardSpec(game::core::CardKind::Specialist).attackRange,
         game::core::cardSpec(game::core::CardKind::Specialist).moveLimit,
         game::core::cardSpec(game::core::CardKind::Specialist).skillCooldownSeconds,
         skillTextForSpec(game::core::cardSpec(game::core::CardKind::Specialist)),
         QString::fromUtf8(game::core::cardSpec(game::core::CardKind::Specialist).priorityText),
         themeColorForCardKind(game::core::CardKind::Specialist)},
        {game::core::CardKind::Heal,
         QString::fromUtf8(game::core::cardName(game::core::CardKind::Heal)),
         game::core::cardSpec(game::core::CardKind::Heal).deployCost,
         game::core::cardSpec(game::core::CardKind::Heal).maxHp,
         game::core::cardSpec(game::core::CardKind::Heal).attack,
         game::core::cardSpec(game::core::CardKind::Heal).attackRange,
         game::core::cardSpec(game::core::CardKind::Heal).moveLimit,
         game::core::cardSpec(game::core::CardKind::Heal).skillCooldownSeconds,
         skillTextForSpec(game::core::cardSpec(game::core::CardKind::Heal)),
         QString::fromUtf8(game::core::cardSpec(game::core::CardKind::Heal).priorityText),
         themeColorForCardKind(game::core::CardKind::Heal)},
        {game::core::CardKind::Attack,
         QString::fromUtf8(game::core::cardName(game::core::CardKind::Attack)),
         game::core::cardSpec(game::core::CardKind::Attack).deployCost,
         game::core::cardSpec(game::core::CardKind::Attack).maxHp,
         game::core::cardSpec(game::core::CardKind::Attack).attack,
         game::core::cardSpec(game::core::CardKind::Attack).attackRange,
         game::core::cardSpec(game::core::CardKind::Attack).moveLimit,
         game::core::cardSpec(game::core::CardKind::Attack).skillCooldownSeconds,
         skillTextForSpec(game::core::cardSpec(game::core::CardKind::Attack)),
         QString::fromUtf8(game::core::cardSpec(game::core::CardKind::Attack).priorityText),
         themeColorForCardKind(game::core::CardKind::Attack)},
        {game::core::CardKind::Aoe,
         QString::fromUtf8(game::core::cardName(game::core::CardKind::Aoe)),
         game::core::cardSpec(game::core::CardKind::Aoe).deployCost,
         game::core::cardSpec(game::core::CardKind::Aoe).maxHp,
         game::core::cardSpec(game::core::CardKind::Aoe).attack,
         game::core::cardSpec(game::core::CardKind::Aoe).attackRange,
         game::core::cardSpec(game::core::CardKind::Aoe).moveLimit,
         game::core::cardSpec(game::core::CardKind::Aoe).skillCooldownSeconds,
         skillTextForSpec(game::core::cardSpec(game::core::CardKind::Aoe)),
         QString::fromUtf8(game::core::cardSpec(game::core::CardKind::Aoe).priorityText),
         themeColorForCardKind(game::core::CardKind::Aoe)},
        {game::core::CardKind::HeavyMedic,
         QString::fromUtf8(game::core::cardName(game::core::CardKind::HeavyMedic)),
         game::core::cardSpec(game::core::CardKind::HeavyMedic).deployCost,
         game::core::cardSpec(game::core::CardKind::HeavyMedic).maxHp,
         game::core::cardSpec(game::core::CardKind::HeavyMedic).attack,
         game::core::cardSpec(game::core::CardKind::HeavyMedic).attackRange,
         game::core::cardSpec(game::core::CardKind::HeavyMedic).moveLimit,
         game::core::cardSpec(game::core::CardKind::HeavyMedic).skillCooldownSeconds,
         skillTextForSpec(game::core::cardSpec(game::core::CardKind::HeavyMedic)),
         QString::fromUtf8(game::core::cardSpec(game::core::CardKind::HeavyMedic).priorityText),
         themeColorForCardKind(game::core::CardKind::HeavyMedic)},
        {game::core::CardKind::Arsenal,
         QString::fromUtf8(game::core::cardName(game::core::CardKind::Arsenal)),
         game::core::cardSpec(game::core::CardKind::Arsenal).deployCost,
         game::core::cardSpec(game::core::CardKind::Arsenal).maxHp,
         game::core::cardSpec(game::core::CardKind::Arsenal).attack,
         game::core::cardSpec(game::core::CardKind::Arsenal).attackRange,
         game::core::cardSpec(game::core::CardKind::Arsenal).moveLimit,
         game::core::cardSpec(game::core::CardKind::Arsenal).skillCooldownSeconds,
         skillTextForSpec(game::core::cardSpec(game::core::CardKind::Arsenal)),
         QString::fromUtf8(game::core::cardSpec(game::core::CardKind::Arsenal).priorityText),
         themeColorForCardKind(game::core::CardKind::Arsenal)},
        {game::core::CardKind::Attack2,
         QString::fromUtf8(game::core::cardName(game::core::CardKind::Attack2)),
         game::core::cardSpec(game::core::CardKind::Attack2).deployCost,
         game::core::cardSpec(game::core::CardKind::Attack2).maxHp,
         game::core::cardSpec(game::core::CardKind::Attack2).attack,
         game::core::cardSpec(game::core::CardKind::Attack2).attackRange,
         game::core::cardSpec(game::core::CardKind::Attack2).moveLimit,
         game::core::cardSpec(game::core::CardKind::Attack2).skillCooldownSeconds,
         skillTextForSpec(game::core::cardSpec(game::core::CardKind::Attack2)),
         QString::fromUtf8(game::core::cardSpec(game::core::CardKind::Attack2).priorityText),
         themeColorForCardKind(game::core::CardKind::Attack2)},
        {game::core::CardKind::Heal2,
         QString::fromUtf8(game::core::cardName(game::core::CardKind::Heal2)),
         game::core::cardSpec(game::core::CardKind::Heal2).deployCost,
         game::core::cardSpec(game::core::CardKind::Heal2).maxHp,
         game::core::cardSpec(game::core::CardKind::Heal2).attack,
         game::core::cardSpec(game::core::CardKind::Heal2).attackRange,
         game::core::cardSpec(game::core::CardKind::Heal2).moveLimit,
         game::core::cardSpec(game::core::CardKind::Heal2).skillCooldownSeconds,
         skillTextForSpec(game::core::cardSpec(game::core::CardKind::Heal2)),
         QString::fromUtf8(game::core::cardSpec(game::core::CardKind::Heal2).priorityText),
         themeColorForCardKind(game::core::CardKind::Heal2)},
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
        " background-color: rgba(239, 219, 173, 0.98);"
        " color: #3a2819;"
        " border: 3px solid rgba(92, 64, 36, 0.78);"
        " border-radius: 7px;"
        " padding: 16px;"
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
                    saveCurrentSquad();
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
            " font-size:18px; font-weight:900; letter-spacing:0px; }");
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
                saveCurrentSquad();
            }
        });
        m_slotButtons.append(slotButton);
    }

    for (int i = 0; i < kSquadRects.size(); ++i) {
        auto *hotspot = new ArtHotspot(artwork, kSquadRects[i], this);
        hotspot->setGlowColor(QColor(255, 218, 92));
        hotspot->setClickHandler([this, i]() {
            switchSquad(i);
        });
        m_squadHotspots.append(hotspot);
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
    m_drawTitleLabel = new QLabel(m_drawOverlay);
    m_drawTitleLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_drawTitleLabel->setStyleSheet(
        "QLabel { color:#fff3ce; background:transparent; border:none;"
        " font-size:26px; font-weight:900; }");
    m_drawBodyLabel = new QLabel(m_drawOverlay);
    m_drawBodyLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_drawBodyLabel->setWordWrap(true);
    m_drawBodyLabel->setStyleSheet(
        "QLabel { color:#ffe7b0; background:transparent; border:none;"
        " font-size:15px; font-weight:800; line-height:1.25; }");
    m_drawResultLabel = new QLabel(m_drawOverlay);
    m_drawResultLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_drawResultLabel->setWordWrap(true);
    m_drawResultLabel->setStyleSheet(
        "QLabel { color:#fff1c8; background:transparent; border:none;"
        " font-size:15px; font-weight:800; }");
    m_drawCardsPanel = new QWidget(m_drawOverlay);
    m_drawCardsPanel->setStyleSheet("QWidget { background: transparent; border: none; }");
    m_btnDrawOne = new QPushButton("x1", m_drawOverlay);
    m_btnDrawTen = new QPushButton("x10", m_drawOverlay);
    m_btnCloseDraw = new QPushButton("Close", m_drawOverlay);
    connect(m_btnDrawOne, &QPushButton::clicked, this, [this]() { performDraw(1); });
    connect(m_btnDrawTen, &QPushButton::clicked, this, [this]() { performDraw(10); });
    connect(m_btnCloseDraw, &QPushButton::clicked, this, [this]() { setDrawOverlayVisible(false); });

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

    const QString presetButtonStyle =
        "QPushButton { color:#352314; background:rgba(246,218,147,0.97);"
        " border:2px solid #704821; border-radius:7px; font-size:13px; font-weight:900; }"
        "QPushButton:hover { background:#ffe4a0; border-color:#d4a047; }"
        "QPushButton:pressed { background:#c99653; }"
        "QPushButton:disabled { color:#8f7352; background:rgba(210,188,145,0.72); border-color:#8a7354; }";
    m_presetTitleLabel = new QLabel("卡组预设", this);
    m_presetTitleLabel->setAlignment(Qt::AlignCenter);
    m_presetTitleLabel->setStyleSheet(
        "QLabel { color:#352314; background:rgba(244,224,174,0.96);"
        " border:2px solid rgba(92,64,36,0.78); border-radius:7px;"
        " font-size:14px; font-weight:900; padding:3px 8px; }");

    m_presetSelector = new QComboBox(this);
    m_presetSelector->setCursor(Qt::PointingHandCursor);
    m_presetSelector->setStyleSheet(
        "QComboBox { color:#352314; background:rgba(244,224,174,0.97);"
        " border:2px solid rgba(92,64,36,0.82); border-radius:7px;"
        " font-size:13px; font-weight:850; padding:3px 8px; }"
        "QComboBox::drop-down { width:24px; border:none; }"
        "QComboBox QAbstractItemView { color:#352314; background:#f1dca9;"
        " selection-background-color:#d4a047; selection-color:#24170d; }");
    m_btnSavePreset = new QPushButton("保存当前", this);
    m_btnLoadPreset = new QPushButton("套用", this);
    m_btnDeletePreset = new QPushButton("删除", this);
    for (QPushButton *button : {m_btnSavePreset, m_btnLoadPreset, m_btnDeletePreset}) {
        button->setCursor(Qt::PointingHandCursor);
        button->setStyleSheet(presetButtonStyle);
    }
    m_btnSavePreset->setToolTip("把下方当前出战卡组保存为一个新预设");
    m_btnLoadPreset->setToolTip("把上方选中的预设套用到下方出战卡槽");
    m_btnDeletePreset->setToolTip("删除上方选中的自定义预设");
    m_presetTitleLabel->hide();
    m_presetSelector->hide();
    m_btnSavePreset->hide();
    m_btnLoadPreset->hide();
    m_btnDeletePreset->hide();
    connect(m_btnSavePreset, &QPushButton::clicked, this, &DeckPage::saveCurrentDeckAsPreset);
    connect(m_btnLoadPreset, &QPushButton::clicked, this, &DeckPage::loadSelectedPreset);
    connect(m_btnDeletePreset, &QPushButton::clicked, this, &DeckPage::deleteSelectedPreset);
    connect(m_presetSelector, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this]() {
                const bool savedPreset = m_presetSelector->currentData().toString() != "default";
                m_btnDeletePreset->setEnabled(savedPreset);
            });
    refreshPresetControls();

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
    for (int i = 0; i < m_squadHotspots.size(); ++i) {
        m_squadHotspots[i]->setCanvasRect(scaledRect(kSquadRects[i], m_canvasRect));
        m_squadHotspots[i]->raise();
    }

    m_detailPanel->setGeometry(scaledRect(kDetailRect, m_canvasRect).toRect());
    m_detailPanel->raise();
    m_ticketLabel->setGeometry(scaledRect(QRect(1214, 688, 280, 42), m_canvasRect).toRect());
    m_btnDrawPanel->setGeometry(scaledRect(QRect(1214, 738, 132, 52), m_canvasRect).toRect());
    m_btnUpgradeCard->setGeometry(scaledRect(QRect(1362, 738, 132, 52), m_canvasRect).toRect());
    m_presetTitleLabel->setGeometry(scaledRect(QRect(1214, 130, 280, 30), m_canvasRect).toRect());
    m_presetSelector->setGeometry(scaledRect(QRect(1214, 166, 280, 34), m_canvasRect).toRect());
    m_btnSavePreset->setGeometry(scaledRect(QRect(1214, 210, 104, 38), m_canvasRect).toRect());
    m_btnLoadPreset->setGeometry(scaledRect(QRect(1330, 210, 72, 38), m_canvasRect).toRect());
    m_btnDeletePreset->setGeometry(scaledRect(QRect(1414, 210, 80, 38), m_canvasRect).toRect());
    m_backHotspot->setCanvasRect(scaledRect(kBackRect, m_canvasRect));
    m_startHotspot->setCanvasRect(scaledRect(kStartRect, m_canvasRect));

    for (int i = 0; i < m_cardLockLabels.size(); ++i) {
        QRect lockRect = scaledRect(kCardRects[i], m_canvasRect).toRect().adjusted(8, 8, -8, -8);
        m_cardLockLabels[i]->setGeometry(lockRect);
    }
    QRect overlayRect = scaledRect(QRect(468, 170, 760, 590), m_canvasRect).toRect();
    m_drawOverlay->setGeometry(overlayRect);
    refreshDrawOverlayLayout();

    m_ticketLabel->raise();
    m_btnDrawPanel->raise();
    m_btnUpgradeCard->raise();
    if (m_presetTitleLabel->isVisible()) m_presetTitleLabel->raise();
    if (m_presetSelector->isVisible()) m_presetSelector->raise();
    if (m_btnSavePreset->isVisible()) m_btnSavePreset->raise();
    if (m_btnLoadPreset->isVisible()) m_btnLoadPreset->raise();
    if (m_btnDeletePreset->isVisible()) m_btnDeletePreset->raise();
    m_backHotspot->raise();
    m_startHotspot->raise();
    updateLockLabelVisibilityForOverlay();
    if (m_drawOverlayVisible) m_drawOverlay->raise();
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
    if (cardIndex < 0 || cardIndex >= m_allCards.size()) {
        return;
    }

    m_selectedCardIndex = cardIndex;
    for (int i = 0; i < m_cardHotspots.size(); ++i) {
        m_cardHotspots[i]->setSelected(i == cardIndex);
    }

    const auto &card = m_allCards[cardIndex];
    const bool owned = CardCollection::isOwned(card.kind);
    const int level = CardCollection::level(card.kind);
    const int fragments = CardCollection::fragments(card.kind);
    const int upgradeCost = CardCollection::upgradeCost(card.kind);
    const int battleUpgradeLv2 = game::core::constants::UpgradeBaseCost;
    const int battleUpgradeLv3 = game::core::constants::UpgradeBaseCost * 2;
    const int recallPercent = game::core::constants::RecallRefundPercent;

    const auto& spec = game::core::cardSpec(card.kind);
    const QString attackText = card.attack > 0 ? QString::number(card.attack) : QString("-");
    const QString rangeText = card.attackRange > 0 ? QString::number(card.attackRange) : QString("-");
    const QString cooldownText = card.attackInterval > 0.0
                                     ? QString("%1 s").arg(card.attackInterval, 0, 'f', 1)
                                     : QString("-");

    m_detailPanel->setText(QString(
        "<div style='font-size:22px; font-weight:900; margin-bottom:4px;'>%1</div>"
        "<div style='color:#684723; font-size:13px; font-weight:850; margin-bottom:10px;'>%2 · %3 · Lv %4</div>"
        "<table width='100%' cellspacing='5' cellpadding='3' style='font-size:13px;'>"
        "<tr>"
        "<td bgcolor='#f7e6bd'><b>Cost</b><br><span style='font-size:17px;'>%5</span></td>"
        "<td bgcolor='#f7e6bd'><b>HP</b><br><span style='font-size:17px;'>%6</span></td>"
        "<td bgcolor='#f7e6bd'><b>ATK</b><br><span style='font-size:17px;'>%7</span></td>"
        "</tr><tr>"
        "<td bgcolor='#eed19a'><b>Range</b><br>%8</td>"
        "<td bgcolor='#eed19a'><b>Cooldown</b><br>%9</td>"
        "<td bgcolor='#eed19a'><b>Move</b><br>%10</td>"
        "</tr>"
        "</table>"
        "<div style='margin-top:8px; padding:7px; background:#f6e2b3;'>"
        "<b>Role</b><br>%11</div>"
        "<div style='margin-top:8px; padding:7px; background:#efd39b;'>"
        "<b>Special</b><br>%12</div>"
        "<div style='margin-top:8px; padding:7px; background:#f6e2b3;'>"
        "<b>Collection</b><br>Shards %13 · Next %14</div>"
        "<div style='margin-top:8px; color:#6f4b25; font-size:12px;'>"
        "Battle upgrades: Lv2 %15 Juice · Lv3 %16 Juice · Recall refund %17%%<br>"
        "Priority: %18</div>")
        .arg(card.name)
        .arg(cardTypeText(card.kind))
        .arg(owned ? "Owned" : "Locked")
        .arg(level)
        .arg(card.deployCost)
        .arg(card.maxHp)
        .arg(attackText)
        .arg(rangeText)
        .arg(cooldownText)
        .arg(card.moveLimit)
        .arg(roleNoteForSpec(spec))
        .arg(card.skillDesc)
        .arg(fragments)
        .arg(upgradeCost > 0 ? QString("%1 shards").arg(upgradeCost) : QString("Max"))
        .arg(battleUpgradeLv2)
        .arg(battleUpgradeLv3)
        .arg(recallPercent)
        .arg(card.priorityDesc));
    refreshCollectionDisplay();
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
    updateLockLabelVisibilityForOverlay();
    refreshDeckSlotsDisplay();
    updateStartBattleButton();
}

void DeckPage::ensureDrawCardWidgets(int count)
{
    while (m_drawCardFrames.size() < count) {
        auto *frame = new QWidget(m_drawCardsPanel);
        frame->hide();
        frame->setStyleSheet(
            "QWidget { background: rgba(246,228,179,0.98);"
            " border: 2px solid rgba(112,72,33,0.92); border-radius: 10px; }");

        auto *artLabel = new QLabel(frame);
        artLabel->setAlignment(Qt::AlignCenter);
        artLabel->setStyleSheet("QLabel { background: transparent; border: none; }");

        auto *nameLabel = new QLabel(frame);
        nameLabel->setAlignment(Qt::AlignCenter);
        nameLabel->setWordWrap(true);
        nameLabel->setStyleSheet(
            "QLabel { color:#362313; background: transparent; border: none;"
            " font-size:12px; font-weight:900; padding:0px 4px; }");

        auto *badgeLabel = new QLabel(frame);
        badgeLabel->setAlignment(Qt::AlignCenter);
        badgeLabel->setStyleSheet(
            "QLabel { color:#2d1b0f; background: rgba(255,236,167,0.96);"
            " border: 2px solid rgba(148,104,38,0.95); border-radius: 10px;"
            " font-size:11px; font-weight:900; padding:1px 6px; }");

        m_drawCardFrames.append(frame);
        m_drawCardArtLabels.append(artLabel);
        m_drawCardNameLabels.append(nameLabel);
        m_drawCardBadgeLabels.append(badgeLabel);
    }
}

void DeckPage::layoutDrawCards()
{
    if (!m_drawCardsPanel) return;

    const QRect panelRect = m_drawCardsPanel->rect();
    const int count = m_lastDrawResultCount;
    for (int i = 0; i < m_drawCardFrames.size(); ++i) {
        const bool visible = i < count;
        m_drawCardFrames[i]->setVisible(visible);
        if (!visible) {
            continue;
        }

        QRect frameRect;
        if (count == 1) {
            const int width = qMin(panelRect.width(), qMax(220, panelRect.width() * 45 / 100));
            const int height = panelRect.height();
            frameRect = QRect((panelRect.width() - width) / 2, 0, width, height);
        } else {
            const int columns = qMin(5, count);
            const int rows = (count + columns - 1) / columns;
            const int gap = qMax(8, qMin(panelRect.width(), panelRect.height()) / 28);
            const int tileW = (panelRect.width() - gap * (columns - 1)) / columns;
            const int tileH = (panelRect.height() - gap * qMax(0, rows - 1)) / qMax(1, rows);
            const int row = i / columns;
            const int col = i % columns;
            frameRect = QRect(col * (tileW + gap), row * (tileH + gap), tileW, tileH);
        }

        m_drawCardFrames[i]->setGeometry(frameRect);

        const int badgeW = qMin(frameRect.width() - 12, qMax(70, frameRect.width() / 2));
        const int badgeH = qBound(22, frameRect.height() / 7, 30);
        m_drawCardBadgeLabels[i]->setGeometry(frameRect.width() - badgeW - 8, 8, badgeW, badgeH);

        const int nameH = qBound(28, frameRect.height() / 5, 48);
        m_drawCardNameLabels[i]->setGeometry(8, frameRect.height() - nameH - 8,
                                             frameRect.width() - 16, nameH);

        const QRect artRect(8, 12 + badgeH, frameRect.width() - 16,
                            qMax(40, frameRect.height() - nameH - badgeH - 28));
        m_drawCardArtLabels[i]->setGeometry(artRect);

        const int cardIndex = m_drawCardFrames[i]->property("cardIndex").toInt();
        if (cardIndex >= 0 && cardIndex < kCardRects.size()) {
            static QPixmap artwork(":/images/artwork/deck_atlas.png");
            const QPixmap cardArt = artwork.copy(kCardRects[cardIndex]);
            m_drawCardArtLabels[i]->setPixmap(cardArt.scaled(
                artRect.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }
    }
}

void DeckPage::clearDrawCards()
{
    m_lastDrawResultCount = 0;
    for (int i = 0; i < m_drawCardFrames.size(); ++i) {
        m_drawCardFrames[i]->hide();
        m_drawCardArtLabels[i]->clear();
        m_drawCardNameLabels[i]->clear();
        m_drawCardBadgeLabels[i]->clear();
        m_drawCardFrames[i]->setProperty("cardIndex", -1);
    }
}

void DeckPage::showDrawInstructions()
{
    clearDrawCards();
    m_drawTitleLabel->setText("Supply Draw");
    m_drawBodyLabel->setText(
        QString("Tickets: %1\n\nNew cards unlock unique towers.\nDuplicates become shards for the exact same card.")
            .arg(CardCollection::tickets()));
    m_drawBodyLabel->show();
    m_drawResultLabel->clear();
    m_drawResultLabel->hide();
    refreshDrawOverlayLayout();
}

void DeckPage::populateDrawResults(const QVector<DrawResult>& results)
{
    m_lastDrawResultCount = results.size();
    ensureDrawCardWidgets(results.size());

    int newCount = 0;
    int shardCount = 0;
    for (int i = 0; i < results.size(); ++i) {
        const DrawResult& result = results[i];
        const int cardIndex = indexForKind(result.kind);
        m_drawCardFrames[i]->setProperty("cardIndex", cardIndex);
        m_drawCardNameLabels[i]->setText(cardNameForKind(result.kind));
        if (result.isNew) {
            ++newCount;
            m_drawCardBadgeLabels[i]->setText("NEW");
            m_drawCardBadgeLabels[i]->setStyleSheet(
                "QLabel { color:#2d1b0f; background: rgba(255,236,167,0.98);"
                " border: 2px solid rgba(148,104,38,0.95); border-radius: 10px;"
                " font-size:11px; font-weight:900; padding:1px 6px; }");
        } else {
            shardCount += result.fragmentsGained;
            m_drawCardBadgeLabels[i]->setText(QString("+%1 shards").arg(result.fragmentsGained));
            m_drawCardBadgeLabels[i]->setStyleSheet(
                "QLabel { color:#16351f; background: rgba(199,244,196,0.98);"
                " border: 2px solid rgba(61,128,76,0.95); border-radius: 10px;"
                " font-size:11px; font-weight:900; padding:1px 6px; }");
        }
    }

    m_drawTitleLabel->setText("Supply Opened");
    m_drawBodyLabel->hide();
    m_drawResultLabel->setText(QString(
        "New cards: %1    Shards: %2\nTickets left: %3")
        .arg(newCount)
        .arg(shardCount)
        .arg(CardCollection::tickets()));
    m_drawResultLabel->show();
    refreshDrawOverlayLayout();
    AudioManager::instance().playCardSelect();
    for (int i = 0; i < results.size(); ++i) {
        QWidget *frame = m_drawCardFrames[i];
        QRect finalRect = frame->geometry();
        QRect startRect = finalRect.adjusted(finalRect.width() / 10, finalRect.height() / 10,
                                             -finalRect.width() / 10, -finalRect.height() / 10);
        frame->setGeometry(startRect);
        frame->show();
        frame->raise();

        auto *effect = qobject_cast<QGraphicsOpacityEffect*>(frame->graphicsEffect());
        if (!effect) {
            effect = new QGraphicsOpacityEffect(frame);
            frame->setGraphicsEffect(effect);
        }
        effect->setOpacity(0.0);

        auto *group = new QParallelAnimationGroup(frame);
        auto *move = new QPropertyAnimation(frame, "geometry", group);
        move->setDuration(260);
        move->setStartValue(startRect);
        move->setEndValue(finalRect);
        move->setEasingCurve(QEasingCurve::OutBack);

        auto *fade = new QPropertyAnimation(effect, "opacity", group);
        fade->setDuration(220);
        fade->setStartValue(0.0);
        fade->setEndValue(1.0);

        QTimer::singleShot(i * 70, frame, [group]() {
            group->start(QAbstractAnimation::DeleteWhenStopped);
        });
    }
}

void DeckPage::openDrawPanel()
{
    showDrawInstructions();
    setDrawOverlayVisible(true);
}

void DeckPage::performDraw(int count)
{
    const QVector<DrawResult> results = CardCollection::drawMany(count);
    if (results.isEmpty()) {
        m_drawTitleLabel->setText("No tickets left");
        m_drawBodyLabel->setText("Finish a battle to earn more supply tickets.");
        m_drawBodyLabel->show();
        m_drawResultLabel->clear();
        m_drawResultLabel->hide();
        clearDrawCards();
        refreshDrawOverlayLayout();
        refreshCollectionDisplay();
        return;
    }
    populateDrawResults(results);
    refreshDetailPanel(qMax(0, m_selectedCardIndex));
    refreshCollectionDisplay();
}

QString DeckPage::cardNameForKind(game::core::CardKind kind) const
{
    return QString::fromUtf8(game::core::cardName(kind));
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

void DeckPage::refreshDrawOverlayLayout()
{
    if (!m_drawOverlay || !m_drawTitleLabel || !m_drawBodyLabel || !m_drawResultLabel || !m_drawCardsPanel) {
        return;
    }

    const QRect overlayRect = m_drawOverlay->rect();
    const int overlayW = overlayRect.width();
    const int overlayH = overlayRect.height();
    const int margin = qMax(18, overlayW / 26);
    const int gap = qMax(10, overlayW / 42);
    const int contentW = qMax(0, overlayW - margin * 2);
    const int buttonW = (overlayW - margin * 2 - gap * 2) / 3;
    const int buttonH = qBound(42, overlayH / 9, 58);
    const int buttonY = overlayH - margin - buttonH;
    const int blockGap = qMax(10, overlayH / 40);
    const int sectionGap = qMax(14, overlayH / 30);
    const auto wrappedHeight = [contentW](QLabel *label, int minHeight, int maxHeight) {
        if (!label || !label->isVisible() || label->text().isEmpty() || contentW <= 0) {
            return 0;
        }
        const QFontMetrics metrics(label->font());
        const QRect bounds = metrics.boundingRect(QRect(0, 0, contentW, 2000),
                                                  Qt::TextWordWrap, label->text());
        return qBound(minHeight, bounds.height() + 8, maxHeight);
    };

    int top = margin;
    const int titleH = qBound(42, overlayH / 10, 60);
    m_drawTitleLabel->setGeometry(margin, top, contentW, titleH);
    top += titleH + blockGap;

    const int bodyH = wrappedHeight(m_drawBodyLabel, 70, qMax(110, overlayH / 3));
    if (bodyH > 0) {
        m_drawBodyLabel->setGeometry(margin, top, contentW, bodyH);
        top += bodyH + sectionGap;
    } else {
        m_drawBodyLabel->setGeometry(0, 0, 0, 0);
    }

    const int summaryH = wrappedHeight(m_drawResultLabel, 42, qMax(72, overlayH / 5));
    if (summaryH > 0) {
        m_drawResultLabel->setGeometry(margin, top, contentW, summaryH);
        top += summaryH + sectionGap;
    } else {
        m_drawResultLabel->setGeometry(0, 0, 0, 0);
    }

    const int cardsBottom = buttonY - qMax(14, overlayH / 34);
    const bool showCards = m_lastDrawResultCount > 0;
    m_drawCardsPanel->setVisible(showCards);
    m_drawCardsPanel->setGeometry(margin, top, contentW, qMax(0, cardsBottom - top));
    layoutDrawCards();

    m_btnDrawOne->setGeometry(margin, buttonY, buttonW, buttonH);
    m_btnDrawTen->setGeometry(margin + buttonW + gap, buttonY, buttonW, buttonH);
    m_btnCloseDraw->setGeometry(margin + (buttonW + gap) * 2, buttonY, buttonW, buttonH);
}

void DeckPage::setDrawOverlayVisible(bool visible)
{
    m_drawOverlayVisible = visible;
    if (m_drawOverlay) {
        m_drawOverlay->setVisible(visible);
        if (visible) {
            m_drawOverlay->raise();
        }
    }
    updateLockLabelVisibilityForOverlay();
    if (!visible) {
        refreshCollectionDisplay();
    }
}

void DeckPage::updateLockLabelVisibilityForOverlay()
{
    for (int i = 0; i < m_cardLockLabels.size() && i < m_allCards.size(); ++i) {
        const bool owned = CardCollection::isOwned(m_allCards[i].kind);
        if (m_drawOverlayVisible) {
            m_cardLockLabels[i]->hide();
            continue;
        }
        m_cardLockLabels[i]->setVisible(!owned);
        if (!owned) {
            m_cardLockLabels[i]->raise();
        }
    }
}

void DeckPage::refreshPresetControls(const QString& selectedId)
{
    if (!m_presetSelector) return;

    const QString currentId = selectedId.isEmpty()
                                  ? m_presetSelector->currentData().toString()
                                  : selectedId;
    m_presets = m_presetManager.listPresets();
    m_presetSelector->blockSignals(true);
    m_presetSelector->clear();
    int selectedIndex = 0;
    for (int i = 0; i < m_presets.size(); ++i) {
        const auto& preset = m_presets[i];
        const QString suffix = preset.id == "default"
                                   ? QString("内置")
                                   : preset.updatedAt.toLocalTime().toString("MM-dd hh:mm");
        m_presetSelector->addItem(QString("%1  (%2)").arg(preset.displayName, suffix), preset.id);
        if (preset.id == currentId) {
            selectedIndex = i;
        }
    }
    m_presetSelector->setCurrentIndex(selectedIndex);
    m_presetSelector->blockSignals(false);
    const bool savedPreset = m_presetSelector->currentData().toString() != "default";
    m_btnDeletePreset->setEnabled(savedPreset);
    m_btnDeletePreset->setToolTip(savedPreset
                                      ? "删除上方选中的自定义预设"
                                      : "内置默认卡组不能删除");
    m_presetSelector->setToolTip(QString("选择一个已保存的卡组预设。文件位置：%1").arg(m_presetManager.storagePath()));
}

bool DeckPage::applyDeckKinds(const QVector<game::core::CardKind>& kinds)
{
    QVector<int> presetSlots(MAX_DECK_SLOTS, -1);
    int slotIndex = 0;
    for (game::core::CardKind kind : kinds) {
        if (slotIndex >= MAX_DECK_SLOTS) break;
        const int index = indexForKind(kind);
        if (index < 0) continue;
        if (!CardCollection::isOwned(kind)) continue;
        presetSlots[slotIndex++] = index;
    }
    if (slotIndex == 0) {
        return false;
    }
    m_selectedSlots = presetSlots;
    refreshDeckSlotsDisplay();
    updateStartBattleButton();
    return true;
}

void DeckPage::saveCurrentDeckAsPreset()
{
    const QVector<game::core::CardKind> deck = getSelectedKinds();
    if (deck.isEmpty()) {
        QToolTip::showText(mapToGlobal(m_canvasRect.center().toPoint()),
                           "请先在下方选择至少一张已拥有的卡牌。", this);
        return;
    }

    bool ok = false;
    const QString name = QInputDialog::getText(this,
                                               "保存卡组预设",
                                               "预设名称：",
                                               QLineEdit::Normal,
                                               QString("卡组 %1").arg(m_presets.size()),
                                               &ok).trimmed();
    if (!ok) return;

    QString id;
    QString error;
    if (!m_presetManager.savePreset(name, deck, &id, &error)) {
        QMessageBox::warning(this, "预设保存失败", error);
        return;
    }
    refreshPresetControls(id);
}

void DeckPage::loadSelectedPreset()
{
    if (!m_presetSelector) return;
    DeckPreset preset;
    if (!m_presetManager.loadPreset(m_presetSelector->currentData().toString(), preset)) {
        QMessageBox::warning(this, "预设读取失败", "无法读取当前选中的卡组预设。");
        refreshPresetControls();
        return;
    }
    if (!applyDeckKinds(preset.cards)) {
        QMessageBox::information(this,
                                 "预设无法套用",
                                 "这个预设里的卡牌目前都还没有解锁。");
        return;
    }
    saveCurrentSquad();
    QToolTip::showText(mapToGlobal(m_canvasRect.center().toPoint()),
                       QString("已套用：%1").arg(preset.displayName), this);
}

void DeckPage::deleteSelectedPreset()
{
    if (!m_presetSelector) return;
    const QString id = m_presetSelector->currentData().toString();
    if (id == "default") {
        QToolTip::showText(mapToGlobal(m_canvasRect.center().toPoint()),
                           "内置默认卡组不能删除。", this);
        return;
    }
    const QString name = m_presetSelector->currentText();
    if (QMessageBox::question(this,
                              "删除卡组预设",
                              QString("确定删除 %1 吗？").arg(name))
        != QMessageBox::Yes) {
        return;
    }
    QString error;
    if (!m_presetManager.deletePreset(id, &error)) {
        QMessageBox::warning(this, "预设删除失败", error);
    }
    refreshPresetControls();
}

void DeckPage::switchSquad(int squadIndex)
{
    if (squadIndex < 0 || squadIndex >= m_squadHotspots.size()) {
        return;
    }
    if (squadIndex == m_currentSquadIndex) {
        return;
    }

    saveCurrentSquad();
    loadSquadToDeck(squadIndex);
}

void DeckPage::saveCurrentSquad()
{
    QString error;
    if (!m_presetManager.saveSquad(m_currentSquadIndex, getSelectedKinds(), &error)) {
        QToolTip::showText(mapToGlobal(m_canvasRect.center().toPoint()),
                           QString("Squad 保存失败：%1").arg(error), this);
    }
}

void DeckPage::loadSquadToDeck(int squadIndex)
{
    DeckPreset preset;
    if (!m_presetManager.loadSquad(squadIndex, preset)) {
        return;
    }

    QVector<int> squadSlots(MAX_DECK_SLOTS, -1);
    int slotIndex = 0;
    for (game::core::CardKind kind : preset.cards) {
        if (slotIndex >= MAX_DECK_SLOTS) break;
        const int index = indexForKind(kind);
        if (index < 0 || !CardCollection::isOwned(kind)) {
            continue;
        }
        squadSlots[slotIndex++] = index;
    }
    m_currentSquadIndex = squadIndex;
    m_selectedSlots = squadSlots;
    refreshDeckSlotsDisplay();
    updateStartBattleButton();
    refreshSquadVisuals();
}

void DeckPage::refreshSquadVisuals()
{
    for (int i = 0; i < m_squadHotspots.size(); ++i) {
        m_squadHotspots[i]->setSelected(i == m_currentSquadIndex);
        m_squadHotspots[i]->setToolTip(QString());
    }
}
