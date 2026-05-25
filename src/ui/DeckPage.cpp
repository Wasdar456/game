/**
 * @file DeckPage.cpp
 * @brief 战前选卡与图鉴页面实现文件
 *
 * 核心逻辑：
 *   1. 加载所有卡牌数据到卡池（基于 CardKind 枚举和 Card 类属性）
 *   2. 点击卡池卡牌 → 添加到第一个空槽位 + 显示详细属性
 *   3. 点击已填入的卡槽 → 移除卡牌
 *   4. 5个槽位全部填满时"开始战斗"按钮变为可点击
 *   5. 确认出战时，收集 CardKind 列表传递给 BattlePage
 *
 * 与 dev 分支 core 模块的对接：
 *   - 卡牌种类由 CardKind 枚举定义：Attack, Produce, Heal
 *   - 对应的实体类：AttackUnit, ProduceUnit, HealUnit
 *   - 部署时 BattleManager::deployCard(CardKind, MapPosition) 创建对应实体
 *   - 所以选卡页面只需要传递 CardKind 列表即可
 */

#include "ui/DeckPage.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFont>
#include <QFrame>

// ========== 引入核心层头文件 ==========
// 用于获取卡牌的详细属性
#include "core/units/AttackUnit.h"   // 攻击型卡牌
#include "core/units/ProduceUnit.h"  // 生产型卡牌
#include "core/units/HealUnit.h"     // 治疗型卡牌
#include "core/map/MapPosition.h"    // 网格坐标（构造 Card 时需要）
#include "core/base/Constants.h"     // 游戏常量（MaxCardLevel 等）

namespace {
QString cardTypeText(game::core::CardKind kind)
{
    if (game::core::isAttackCardKind(kind)) return "攻击型";
    if (game::core::isProduceCardKind(kind)) return "生产型";
    return "治疗型";
}
}

// ========== 构造函数 ==========
DeckPage::DeckPage(QWidget *parent)
    : QWidget(parent)
    , m_btnBack(nullptr)
    , m_titleLabel(nullptr)
    , m_detailPanel(nullptr)
    , m_btnStartBattle(nullptr)
    , m_selectedCardIndex(-1)
    , m_cardPoolScroll(nullptr)
{
    // 初始化5个卡槽为空（-1 表示空槽位）
    m_selectedSlots.fill(-1, MAX_DECK_SLOTS);

    // 创建卡牌展示数据
    createCardPoolData();

    initUI();
    connectSignals();
}

// ========== createCardPoolData() —— 创建卡牌展示数据 ==========
// 根据 dev 分支 core 模块的 Card 派生类属性来填充
void DeckPage::createCardPoolData()
{
    // 创建临时的 Card 派生类实例来获取真实属性
    // 这里用 id=0, position=(0,0) 构造临时对象，仅用于读取属性
    game::core::MapPosition dummyPos(0, 0);

    // ===== 攻击型卡牌 =====
    {
        game::core::AttackUnit tempCard(0, dummyPos);
        m_allCards.append({
            game::core::CardKind::Attack,
            "突击手",                         // 名称
            tempCard.deployCost(),             // 从 Card::deployCost() 获取
            tempCard.maxHp(),                  // 从 Entity::maxHp() 获取
            tempCard.attack(),                 // 从 Entity::attack() 获取
            tempCard.attackRange(),            // 从 Card::attackRange() 获取
            tempCard.moveLimit(),              // 从 Card::moveLimit() 获取
            1.0,                               // 攻击间隔（简化）
            "[普通怪] > [资源单位] > [敌方核心]",  // 索敌优先级描述
            QString("Lv2: %1  Lv3: %2")       // 升级消耗描述
                .arg(tempCard.upgradeCost())
                .arg(tempCard.upgradeCost() * 2),
            QColor(255, 82, 82)               // 红色主题
        });
    }

    {
        game::core::AttackUnit tempCard2(1, dummyPos);  // 简化：复用 AttackUnit
        m_allCards.append({
            game::core::CardKind::Sniper,
            "狙击手",
            50,                                // 狙击手部署消耗更高
            400,                               // 血量较低
            200,                               // 攻击力更高
            5,                                 // 射程更远
            1,                                 // 瞬移距离短
            2.0,
            "[高血量怪] > [普通怪] > [资源单位]",
            "Lv2: 60  Lv3: 120",
            QColor(255, 82, 82)
        });
    }

    {
        // AOE 炮塔（攻击型变种）
        m_allCards.append({
            game::core::CardKind::Aoe,
            "AOE炮塔",
            60,
            500,
            80,
            3,
            1,
            1.5,
            "[最近怪] > [普通怪] > [资源单位]",
            "Lv2: 50  Lv3: 100",
            QColor(255, 82, 82)
        });
    }

    {
        // 特种兵（攻击型变种）
        m_allCards.append({
            game::core::CardKind::Specialist,
            "特种兵",
            55,
            450,
            180,
            4,
            3,
            1.2,
            "[远程怪] > [高血量怪] > [普通怪]",
            "Lv2: 55  Lv3: 110",
            QColor(255, 82, 82)
        });
    }

    // ===== 生产型卡牌 =====
    {
        game::core::ProduceUnit tempCard(0, dummyPos);
        m_allCards.append({
            game::core::CardKind::Produce,
            "采矿工",
            tempCard.deployCost(),
            tempCard.maxHp(),
            tempCard.attack(),
            tempCard.attackRange(),
            tempCard.moveLimit(),
            3.0,
            "[资源单位] > [怪物] > [敌方核心]",
            QString("Lv2: %1  Lv3: %2")
                .arg(tempCard.upgradeCost())
                .arg(tempCard.upgradeCost() * 2),
            QColor(0, 230, 118)              // 绿色主题
        });
    }

    {
        // 兵工厂（生产型变种）
        m_allCards.append({
            game::core::CardKind::Arsenal,
            "兵工厂",
            80,
            500,
            0,
            0,
            0,
            0.0,
            "无攻击能力 - 专注资源产出",
            "Lv2: 70  Lv3: 140",
            QColor(0, 230, 118)
        });
    }

    // ===== 治疗型卡牌 =====
    {
        game::core::HealUnit tempCard(0, dummyPos);
        m_allCards.append({
            game::core::CardKind::Heal,
            "医生",
            tempCard.deployCost(),
            tempCard.maxHp(),
            tempCard.attack(),
            tempCard.attackRange(),
            tempCard.moveLimit(),
            1.8,
            "[受伤友方] > [最低血量友方]",
            QString("Lv2: %1  Lv3: %2")
                .arg(tempCard.upgradeCost())
                .arg(tempCard.upgradeCost() * 2),
            QColor(68, 138, 255)             // 蓝色主题
        });
    }

    {
        // 重装医生（治疗型变种）
        m_allCards.append({
            game::core::CardKind::HeavyMedic,
            "重装医生",
            60,
            600,
            20,
            2,
            1,
            2.5,
            "[受伤友方] > [最低血量友方]",
            "Lv2: 55  Lv3: 110",
            QColor(68, 138, 255)
        });
    }
}

// ========== initUI() —— 初始化界面 ==========
void DeckPage::initUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(40, 25, 40, 25);
    mainLayout->setSpacing(15);

    // ----- 顶部导航栏 -----
    QHBoxLayout *topBar = new QHBoxLayout();

    m_btnBack = new QPushButton("← 返回", this);
    m_btnBack->setFixedSize(100, 40);
    m_btnBack->setStyleSheet(
        "QPushButton { background-color: rgba(20,40,70,0.70); color: #8AB4F8;"
        "  border: 2px solid rgba(0,212,255,0.50); border-radius: 8px; font-size: 14px; }"
        "QPushButton:hover { color: #00E5FF; border: 2px solid #00D4FF; }"
    );
    m_btnBack->setCursor(Qt::PointingHandCursor);

    m_titleLabel = new QLabel("📖 战前编队 & 图鉴", this);
    m_titleLabel->setStyleSheet("color: #FFFFFF; font-size: 22px; font-weight: bold;");

    topBar->addWidget(m_btnBack);
    topBar->addStretch();
    topBar->addWidget(m_titleLabel);
    topBar->addStretch();
    mainLayout->addLayout(topBar);

    // ===== 上半部分：卡池 + 详细属性面板 =====
    QHBoxLayout *upperLayout = new QHBoxLayout();

    // ----- 左侧：卡池 -----
    QVBoxLayout *cardPoolLayout = new QVBoxLayout();
    QLabel *poolLabel = new QLabel("🗂️ 全图鉴卡池（点击选择出战卡牌）", this);
    poolLabel->setStyleSheet("color: #E3F2FD; font-size: 15px; font-weight: bold;");
    cardPoolLayout->addWidget(poolLabel);

    m_cardPoolScroll = new QScrollArea(this);
    m_cardPoolScroll->setWidgetResizable(true);
    m_cardPoolScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_cardPoolScroll->setStyleSheet(
        "QScrollArea { background-color: transparent; border: 2px solid rgba(0,212,255,0.35); border-radius: 8px; }"
        "QScrollBar:vertical { width: 8px; background: transparent; }"
        "QScrollBar::handle:vertical { background: rgba(0,212,255,0.5); border-radius: 4px; min-height: 30px; }"
    );

    // 卡池内容容器（网格布局，每行4张）
    QWidget *cardPoolContainer = new QWidget(this);
    QGridLayout *cardGrid = new QGridLayout(cardPoolContainer);
    cardGrid->setSpacing(12);
    cardGrid->setContentsMargins(10, 10, 10, 10);

    int col = 0, row = 0;
    const int COLS = 4;
    for (int i = 0; i < m_allCards.size(); ++i) {
        const auto &card = m_allCards[i];
        QPushButton *cardBtn = new QPushButton(this);
        cardBtn->setFixedSize(140, 100);
        cardBtn->setText(QString("%1\n%2\n💰 %3")
                             .arg(card.name)
                             .arg(cardTypeText(card.kind))
                             .arg(card.deployCost));

        // 根据卡牌类型设置主题色 - 不透明实色背景，始终清晰可见
        QString colorHex = card.themeColor.name();
        // 计算深色实底背景（取主题色的暗色版本）
        int r = card.themeColor.red(), g = card.themeColor.green(), b = card.themeColor.blue();
        QString darkBg = QString("rgb(%1,%2,%3)").arg(r*3/10+15).arg(g*3/10+15).arg(b*3/10+15);
        QString midBg = QString("rgb(%1,%2,%3)").arg(r*4/10+20).arg(g*4/10+20).arg(b*4/10+20);
        QString hoverBg = QString("rgb(%1,%2,%3)").arg(r*5/10+25).arg(g*5/10+25).arg(b*5/10+25);
        QString hoverBg2 = QString("rgb(%1,%2,%3)").arg(r*3/10+30).arg(g*3/10+30).arg(b*3/10+30);
        cardBtn->setStyleSheet(
            QString(
                "QPushButton {"
                "  background: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
                "    stop:0 %1, stop:1 %2);"
                "  color: #FFFFFF;"
                "  border: 2px solid %3; border-radius: 10px;"
                "  font-size: 13px; font-weight: bold; text-align: center;"
                "}"
                "QPushButton:hover {"
                "  background: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
                "    stop:0 %4, stop:1 %5);"
                "  border: 2px solid %6;"
                "  color: #FFFFFF;"
                "}"
            )
            .arg(midBg)          // 顶部
            .arg(darkBg)         // 底部
            .arg(colorHex)       // 边框 - 实色
            .arg(hoverBg)        // hover 顶部
            .arg(hoverBg2)       // hover 底部
            .arg(colorHex)       // hover 边框
        );
        cardBtn->setCursor(Qt::PointingHandCursor);

        // 点击卡牌 → 添加到卡槽 + 显示详情
        connect(cardBtn, &QPushButton::clicked, this, [this, i]() {
            refreshDetailPanel(i);
            // 添加到第一个空槽位
            for (int s = 0; s < MAX_DECK_SLOTS; ++s) {
                if (m_selectedSlots[s] == -1) {
                    m_selectedSlots[s] = i;  // 存储卡牌索引
                    refreshDeckSlotsDisplay();
                    updateStartBattleButton();
                    break;
                }
            }
        });

        cardGrid->addWidget(cardBtn, row, col);
        col++;
        if (col >= COLS) { col = 0; row++; }
    }

    m_cardPoolScroll->setWidget(cardPoolContainer);
    cardPoolLayout->addWidget(m_cardPoolScroll);
    upperLayout->addLayout(cardPoolLayout, 2);

    // ----- 右侧：详细属性面板 -----
    m_detailPanel = new QLabel(this);
    m_detailPanel->setMinimumWidth(300);
    m_detailPanel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_detailPanel->setWordWrap(true);
    m_detailPanel->setText("👈 点击左侧卡牌查看详细属性");
    m_detailPanel->setStyleSheet(
        "QLabel {"
        "  background: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
        "    stop:0 rgba(15,30,55,0.92), stop:1 rgba(10,22,40,0.88));"
        "  color: #E3F2FD;"
        "  border: 2px solid rgba(0,212,255,0.40); border-radius: 12px;"
        "  padding: 15px; font-size: 14px;"
        "}"
    );
    upperLayout->addWidget(m_detailPanel, 1);
    mainLayout->addLayout(upperLayout, 3);

    // ===== 下半部分：出战卡槽 =====
    QVBoxLayout *deckLayout = new QVBoxLayout();
    QLabel *slotLabel = new QLabel("🎯 出战卡槽（点击已选卡牌可移除）", this);
    slotLabel->setStyleSheet("color: #E3F2FD; font-size: 15px; font-weight: bold;");
    deckLayout->addWidget(slotLabel);

    QHBoxLayout *slotRow = new QHBoxLayout();
    slotRow->setSpacing(15);
    for (int i = 0; i < MAX_DECK_SLOTS; ++i) {
        QPushButton *slotBtn = new QPushButton(this);
        slotBtn->setFixedSize(150, 100);
        slotBtn->setText(QString("槽位 %1\n(空)").arg(i + 1));
        slotBtn->setStyleSheet(
            "QPushButton {"
            "  background-color: rgba(15,30,55,0.60); color: #7AB8DD;"
            "  border: 2px dashed rgba(0,212,255,0.45); border-radius: 10px; font-size: 14px;"
            "}"
        );
        // 点击卡槽 → 移除卡牌
        connect(slotBtn, &QPushButton::clicked, this, [this, i]() {
            if (m_selectedSlots[i] != -1) {
                m_selectedSlots[i] = -1;
                refreshDeckSlotsDisplay();
                updateStartBattleButton();
            }
        });
        m_slotButtons.append(slotBtn);
        slotRow->addWidget(slotBtn);
    }
    deckLayout->addLayout(slotRow);

    // 开始战斗按钮
    m_btnStartBattle = new QPushButton("⚔ 开始战斗", this);
    m_btnStartBattle->setFixedSize(250, 55);
    m_btnStartBattle->setStyleSheet(
        "QPushButton {"
        "  background-color: #1A2742; color: #7AACCC;"
        "  border: 2px solid rgba(0,212,255,0.30); border-radius: 14px;"
        "  font-size: 18px; font-weight: bold;"
        "}"
        "QPushButton:enabled {"
        "  background: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
        "    stop:0 rgba(0,212,255,0.25), stop:1 rgba(0,180,255,0.12));"
        "  color: #00E5FF;"
        "  border: 2px solid rgba(0,212,255,0.70);"
        "}"
        "QPushButton:enabled:hover {"
        "  background: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
        "    stop:0 rgba(0,212,255,0.40), stop:1 rgba(0,180,255,0.22));"
        "}"
    );
    m_btnStartBattle->setEnabled(false);  // 初始不可用
    m_btnStartBattle->setCursor(Qt::PointingHandCursor);

    QHBoxLayout *battleBtnLayout = new QHBoxLayout();
    battleBtnLayout->addStretch();
    battleBtnLayout->addWidget(m_btnStartBattle);
    battleBtnLayout->addStretch();
    deckLayout->addLayout(battleBtnLayout);
    mainLayout->addLayout(deckLayout, 1);

    // 页面背景
    this->setStyleSheet(
        "DeckPage {"
        "  background: qlineargradient("
        "    x1:0, y1:0, x2:1, y2:1,"
        "    stop:0 #0B1622, stop:0.5 #0F1B2D, stop:1 #162544"
        "  );"
        "}"
    );
}

// ========== refreshDeckSlotsDisplay() —— 刷新卡槽显示 ==========
void DeckPage::refreshDeckSlotsDisplay()
{
    for (int i = 0; i < MAX_DECK_SLOTS; ++i) {
        QPushButton *btn = m_slotButtons[i];
        int cardIdx = m_selectedSlots[i];

        if (cardIdx == -1) {
            // 空槽位
            btn->setText(QString("槽位 %1\n(空)").arg(i + 1));
            btn->setStyleSheet(
                "QPushButton {"
                "  background-color: rgba(15,30,55,0.60); color: #7AB8DD;"
                "  border: 2px dashed rgba(0,212,255,0.45); border-radius: 10px; font-size: 14px;"
                "}"
            );
        } else {
            // 已填入卡牌
            const auto &card = m_allCards[cardIdx];
            QString typeStr = cardTypeText(card.kind);
            btn->setText(QString("%1\n%2").arg(card.name).arg(typeStr));
            QString colorHex = card.themeColor.name();
            int r = card.themeColor.red(), g = card.themeColor.green(), b = card.themeColor.blue();
            QString slotDarkBg = QString("rgb(%1,%2,%3)").arg(r*3/10+15).arg(g*3/10+15).arg(b*3/10+15);
            QString slotMidBg = QString("rgb(%1,%2,%3)").arg(r*4/10+20).arg(g*4/10+20).arg(b*4/10+20);
            QString slotHoverBg = QString("rgb(%1,%2,%3)").arg(r*5/10+25).arg(g*5/10+25).arg(b*5/10+25);
            QString slotHoverBg2 = QString("rgb(%1,%2,%3)").arg(r*3/10+30).arg(g*3/10+30).arg(b*3/10+30);
            btn->setStyleSheet(
                QString(
                    "QPushButton {"
                    "  background: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
                    "    stop:0 %1, stop:1 %2);"
                    "  color: #FFFFFF;"
                    "  border: 2px solid %3; border-radius: 10px;"
                    "  font-size: 14px; font-weight: bold;"
                    "}"
                    "QPushButton:hover {"
                    "  background: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
                    "    stop:0 %4, stop:1 %5);"
                    "  border: 2px solid #FF5252;"
                    "  color: #FFFFFF;"
                    "}"
                )
                .arg(slotMidBg)
                .arg(slotDarkBg)
                .arg(colorHex)
                .arg(slotHoverBg)
                .arg(slotHoverBg2)
            );
        }
    }
}

// ========== refreshDetailPanel() —— 刷新详细属性面板 ==========
void DeckPage::refreshDetailPanel(int cardIndex)
{
    m_selectedCardIndex = cardIndex;
    if (cardIndex < 0 || cardIndex >= m_allCards.size()) return;

    const auto &card = m_allCards[cardIndex];
    QString typeStr = cardTypeText(card.kind);

    // 使用 HTML 格式化显示
    QString html = QString(
        "<h3 style='color:%1;'>📋 %2</h3>"
        "<p style='color:#E3F2FD;'><b>类型：</b>%3</p>"
        "<hr style='border-color:rgba(0,212,255,0.2);'>"
        "<p style='color:#E3F2FD;'><b>❤️ HP：</b>%4</p>"
        "<p style='color:#E3F2FD;'><b>⚔️ 攻击力：</b>%5</p>"
        "<p style='color:#E3F2FD;'><b>🎯 射程：</b>%6 格</p>"
        "<p style='color:#E3F2FD;'><b>⏱️ 攻击间隔：</b>%7s</p>"
        "<p style='color:#E3F2FD;'><b>🏃 瞬移上限：</b>%8 格</p>"
        "<p style='color:#E3F2FD;'><b>💰 部署消耗：</b>%9</p>"
        "<hr style='border-color:rgba(0,212,255,0.2);'>"
        "<p style='color:#E3F2FD;'><b>🔍 索敌优先级：</b></p>"
        "<p style='margin-left:10px; color:#8AB4F8;'>%10</p>"
        "<hr style='border-color:rgba(0,212,255,0.2);'>"
        "<p style='color:#E3F2FD;'><b>⬆️ 升级消耗：</b></p>"
        "<p style='margin-left:10px; color:#8AB4F8;'>%11</p>"
    )
    .arg(card.themeColor.name())
    .arg(card.name)
    .arg(typeStr)
    .arg(card.maxHp)
    .arg(card.attack)
    .arg(card.attackRange)
    .arg(card.attackInterval)
    .arg(card.moveLimit)
    .arg(card.deployCost)
    .arg(card.priorityDesc)
    .arg(card.upgradeCostDesc);

    m_detailPanel->setText(html);
}

// ========== updateStartBattleButton() —— 更新开始战斗按钮 ==========
void DeckPage::updateStartBattleButton()
{
    bool allFilled = true;
    int filledCount = 0;
    for (int i = 0; i < MAX_DECK_SLOTS; ++i) {
        if (m_selectedSlots[i] != -1) filledCount++;
        else allFilled = false;
    }

    m_btnStartBattle->setEnabled(allFilled);
    m_btnStartBattle->setToolTip(
        allFilled ? "卡组已满，点击开始战斗！" :
                    QString("还需要选择 %1 张卡牌").arg(MAX_DECK_SLOTS - filledCount));
}

// ========== getSelectedKinds() —— 获取选定的 CardKind 列表 ==========
QVector<game::core::CardKind> DeckPage::getSelectedKinds() const
{
    QVector<game::core::CardKind> kinds;
    for (int i = 0; i < MAX_DECK_SLOTS; ++i) {
        if (m_selectedSlots[i] != -1) {
            kinds.append(m_allCards[m_selectedSlots[i]].kind);
        }
    }
    return kinds;
}

// ========== connectSignals() —— 连接信号槽 ==========
void DeckPage::connectSignals()
{
    // 返回按钮
    connect(m_btnBack, &QPushButton::clicked, this, &DeckPage::signalBack);

    // 开始战斗按钮 → 发出战斗开始信号 + 卡组选择信号
    connect(m_btnStartBattle, &QPushButton::clicked, this, [this]() {
        QVector<game::core::CardKind> kinds = getSelectedKinds();
        emit signalDeckSelected(kinds);   // 传递选定的卡组
        emit signalBattleStart();
    });
}
