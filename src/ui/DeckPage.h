/**
 * @file DeckPage.h
 * @brief 战前选卡与图鉴页面头文件
 *
 * 布局设计（参考《明日方舟》编队界面）：
 *   上半部分：全图鉴卡池（可滚动，点击查看详细属性）
 *   右侧：选中卡牌的详细属性面板（HP/攻击/射程/索敌/升级消耗）
 *   下半部分：5个出战卡槽（点击卡池填入，点击卡槽移除）
 *   底部：[开始战斗]按钮（卡槽满时可点击）
 *
 * 与 dev 分支 core 模块的对接：
 *   - 卡牌类型使用 CardKind 枚举（Attack/Produce/Heal）
 *   - 卡牌属性从 Card 类的接口获取（attackRange, moveLimit, deployCost 等）
 *   - 索敌优先级从 Card::priorityList() 获取（ObjectType 向量）
 *   - 选好的卡组通过 CardKind 列表传递给 BattlePage
 */

#ifndef DECKPAGE_H
#define DECKPAGE_H

#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QScrollArea>
#include <QVector>
#include <QRectF>

// ========== 核心层头文件 ==========
#include "core/base/CoreTypes.h"     // CardKind, ObjectType 枚举
#include "core/units/Card.h"         // Card 基类

class QShowEvent;
class QResizeEvent;
class ArtHotspot;

/**
 * @brief 卡牌展示信息
 * 用于选卡页面的 UI 展示，从 Card 类派生类的属性中提取
 */
struct CardDisplayInfo {
    game::core::CardKind kind;        ///< 卡牌种类（Attack/Produce/Heal）
    QString name;                     ///< 卡牌名称
    int deployCost;                   ///< 部署消耗
    int maxHp;                        ///< 最大血量
    int attack;                       ///< 攻击力
    int attackRange;                  ///< 攻击范围（格数）
    int moveLimit;                    ///< 瞬移最大距离
    double attackInterval;            ///< 攻击间隔（秒）
    QString priorityDesc;             ///< 索敌优先级描述（人可读文本）
    QString upgradeCostDesc;          ///< 升级消耗描述
    QColor themeColor;                ///< 卡牌主题色
};

class DeckPage : public QWidget
{
    Q_OBJECT

public:
    static const int MAX_DECK_SLOTS = 5;  ///< 出战卡槽固定数量

    explicit DeckPage(QWidget *parent = nullptr);

    /**
     * @brief 获取选定的 CardKind 列表
     * @return 出战卡组的 CardKind 向量
     */
    QVector<game::core::CardKind> getSelectedKinds() const;

signals:
    void signalBattleStart();  ///< 卡槽已满，点击开始战斗
    void signalBack();         ///< 返回上一页

    /**
     * @brief 战斗开始时携带选定的卡组
     * @param selectedKinds 选定的 CardKind 列表
     */
    void signalDeckSelected(const QVector<game::core::CardKind> &selectedKinds);

protected:
    void paintEvent(QPaintEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    // ========== UI 组件 ==========
    QPushButton *m_btnBack;
    QLabel      *m_titleLabel;
    QLabel      *m_detailPanel;          ///< 卡牌详细属性面板
    QPushButton *m_btnStartBattle;       ///< 开始战斗按钮

    // ========== 卡牌数据 ==========
    QVector<CardDisplayInfo> m_allCards;       ///< 全图鉴卡池
    QVector<int> m_selectedSlots;              ///< 出战卡槽（存储 CardKind 索引，-1=空）
    int m_selectedCardIndex;                   ///< 当前选中的卡牌索引（-1=无选中）

    // ========== 卡槽按钮列表 ==========
    QVector<QPushButton*> m_slotButtons;

    // ========== 卡池容器 ==========
    QScrollArea *m_cardPoolScroll;
    QRectF m_canvasRect;
    QVector<ArtHotspot*> m_cardHotspots;
    ArtHotspot *m_backHotspot;
    ArtHotspot *m_startHotspot;

    void initUI();
    void createCardPoolData();             ///< 创建卡牌展示数据
    void connectSignals();
    void updateArtworkLayout();
    void refreshDeckSlotsDisplay();        ///< 刷新卡槽显示
    void refreshDetailPanel(int cardIndex);///< 刷新详细属性面板
    void updateStartBattleButton();        ///< 更新"开始战斗"按钮状态
    void animateCardToSlot(int cardIndex, int slotIndex);
};

#endif // DECKPAGE_H
