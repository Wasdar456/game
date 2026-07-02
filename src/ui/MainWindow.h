/**
 * @file MainWindow.h
 * @brief 主窗口类头文件 —— 游戏的顶层窗口，持有核心层 BattleManager 实例
 *
 * 设计思路：
 *   本游戏采用"核心-界面"分离架构（参考 dev 分支的 core/ 模块）：
 *   - BattleManager 是 core 层对 UI 暴露的主要入口
 *   - UI 层通过 BattleManager::snapshot() 获取只读快照来渲染界面
 *   - 用户操作（部署、升级、移动、撤回）通过 BattleManager 的接口完成
 *
 * MainWindow 的职责：
 *   1. 创建并持有 BattleManager 实例（游戏核心逻辑的管理器）
 *   2. 创建并持有所有子页面，使用 QStackedWidget 管理页面切换
 *   3. 连接各页面的导航信号到对应的槽函数
 *   4. 为子页面提供获取 BattleManager 的接口
 *
 * 页面索引映射（QStackedWidget 中的 index）：
 *   index 0 → StartPage    （起始页）
 *   index 1 → LobbyPage    （大厅/配置页）
 *   index 2 → DeckPage     （战前选卡/图鉴页）
 *   index 3 → BattlePage   （战斗页面）
 *   index 4 → SettingsPage （设置页面）
 */

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStackedWidget>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>

// ========== 核心层前向声明 ==========
// dev 分支的 core 模块使用 game::core 命名空间
// 这里只做前向声明，避免 .h 文件中 include 整个 core 头文件
namespace game::core {
    class BattleManager;    ///< 战斗总管理器 —— 核心层对 UI 的主要入口
}

// ========== UI 页面前向声明 ==========
class StartPage;
class LobbyPage;
class DeckPage;
class DeployPage;
class BattlePage;
class BattleReportPage;
class SettingsPage;
class ResultPage;
class LeafTransitionOverlay;

// NetworkContext 定义在 LobbyPage.h 中
#include "ui/LobbyPage.h"

/**
 * @class MainWindow
 * @brief 主窗口类 —— 游戏的顶层容器窗口
 *
 * 关键设计：MainWindow 持有 BattleManager 的唯一实例
 * 子页面通过 battleManager() 方法获取核心层接口
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    /**
     * @brief 获取 BattleManager 实例
     * @return BattleManager 指针，子页面通过此接口操作核心层
     *
     * 使用方式示例（在子页面中）：
     *   auto* bm = mainWindow->battleManager();
     *   bm->deployCard(CardKind::Attack, MapPosition(5, 3));
     *   BattleSnapshot snap = bm->snapshot();
     */
    game::core::BattleManager* battleManager() const { return m_battleManager; }

protected:
    void changeEvent(QEvent *event) override;

private:
    // ========== 核心层实例 ==========
    game::core::BattleManager *m_battleManager;  ///< 战斗管理器（游戏核心）

    // ========== UI 组件 ==========
    QStackedWidget *m_stackWidget;  ///< 堆叠窗口，管理所有页面的切换显示

    // ========== 各功能页面实例 ==========
    StartPage    *m_startPage;
    LobbyPage    *m_lobbyPage;
    DeckPage     *m_deckPage;
    DeployPage   *m_deployPage;  ///< 迷雾部署页面（PVP）
    BattlePage   *m_battlePage;
    BattleReportPage *m_battleReportPage;
    SettingsPage *m_settingsPage;
    ResultPage   *m_resultPage;
    LeafTransitionOverlay *m_transitionOverlay;

    // ========== 页面导航辅助 ==========
    QWidget *m_previousPage;  ///< 记录切换到 DeckPage 前的来源页面
    QWidget *m_settingsReturnPage;

    // ========== 网络上下文（PVP 模式） ==========
    NetworkContext m_networkContext;  ///< 从 LobbyPage 传递到 BattlePage 的联机信息
    bool m_battleRevealPlayed;

    // ========== 私有方法 ==========
    void initUI();           ///< 初始化 UI 组件和布局
    void connectSignals();   ///< 连接各页面的信号到对应的槽函数
    void fadeToPage(QWidget *page);  ///< 带淡入淡出效果的页面切换
};

#endif // MAINWINDOW_H
