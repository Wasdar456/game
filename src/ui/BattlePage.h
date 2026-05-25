/**
 * @file BattlePage.h
 * @brief 战斗主页面头文件 —— 游戏核心交互界面
 *
 * 界面布局：
 *   ┌──────────────────────────────────────────────────┐
 *   │  🌊 波次: 3/10   🏰 核心: 800   💰 资源: 450   │  ← 顶部状态栏
 *   │──────────────────────────────────────────────────│
 *   │                                                  │
 *   │            主视口（2D 网格地图）                  │  ← 使用 BattleSnapshot 渲染
 *   │        QPainter 自绘：地形+单位+怪物+高亮        │
 *   │                                                  │
 *   │──────────────────────────────────────────────────│
 *   │  [卡1] [卡2] [卡3] [卡4] [卡5]  ⏸️ ⏩ ⚡       │  ← 底部操作栏
 *   └──────────────────────────────────────────────────┘
 *
 * 环形菜单（点击已部署单位弹出）：
 *        ┌──────┐
 *        │ 升级 │
 *   ┌────┼──────┼────┐
 *   │移动│ (单位) │撤回│
 *   └────┴──────┴────┘
 *
 * 与 dev 分支 core 模块的对接：
 *   - 使用 BattleManager 作为核心层入口
 *   - 所有渲染数据来自 BattleSnapshot（只读快照）
 *   - 用户操作通过 BattleManager 接口：
 *     * deployCard(CardKind, MapPosition) —— 部署
 *     * upgradeCard(unitId) —— 升级
 *     * moveCard(unitId, MapPosition) —— 瞬移
 *     * recallCard(unitId) —— 撤回
 *   - 游戏主循环由 QTimer 驱动，每帧调用 BattleManager::update()
 */

#ifndef BATTLEPAGE_H
#define BATTLEPAGE_H

#include <QWidget>
#include <QTimer>
#include <QLabel>
#include <QPushButton>
#include <QVector>
#include <QPainter>
#include <QPaintEvent>
#include <QMouseEvent>

// ========== 核心层头文件 ==========
#include "core/systems/BattleManager.h"     // 战斗总管理器
#include "core/snapshot/BattleSnapshot.h"   // 战斗快照（只读）
#include "core/base/CoreTypes.h"            // CardKind, TerrainType 等枚举
#include "core/map/MapPosition.h"           // 网格坐标

// ========== 网络模块 ==========
#include "ui/LobbyPage.h"                   // NetworkContext 结构体
#include "network/protocol/ProtocolDef.h"   // MsgType 枚举

/**
 * @class BattleView
 * @brief 战斗视口控件 —— 负责地图的绘制和鼠标交互
 */
class BattleView : public QWidget
{
    Q_OBJECT

public:
    static const int CELL_SIZE = 48;  ///< 每格像素大小

    explicit BattleView(QWidget *parent = nullptr);

    void updateFromSnapshot(const game::core::BattleSnapshot &snapshot);
    void setMapSize(int rows, int cols);
    void hideRadialMenu();

    // ========== 交互状态 ==========
    enum class InteractionMode {
        NONE,           ///< 无特殊交互
        DEPLOYING,      ///< 部署模式
        MOVING,         ///< 移动模式
        RADIAL_MENU     ///< 环形菜单模式
    };

    InteractionMode m_mode;
    game::core::CardKind m_selectedCardKind;
    int m_selectedUnitId;
    int m_moveRange;

    // ========== 核心与出生点位置 ==========
    game::core::MapPosition m_spawnPos;
    game::core::MapPosition m_corePos;
    bool m_localIsHost;
    bool m_interactionEnabled;

    // ========== 环形菜单按钮 ==========
    QPushButton *m_btnUpgrade;
    QPushButton *m_btnMove;
    QPushButton *m_btnRetreat;

    // ========== 动画帧计数 ==========
    int m_animFrame;              ///< 动画帧计数（用于脉冲/旋转效果）

    // ========== 鼠标悬停位置 ==========
    int m_hoverRow;
    int m_hoverCol;

signals:
    void signalDeployCard(game::core::CardKind kind, game::core::MapPosition position);
    void signalUpgradeCard(int unitId);
    void signalMoveCard(int unitId, game::core::MapPosition target);
    void signalRecallCard(int unitId);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

private:
    game::core::BattleSnapshot m_snapshot;
    int m_mapRows;
    int m_mapCols;

    void showRadialMenu(int unitId, int pixelX, int pixelY);
    int findUnitAt(int row, int col) const;
    QVector<game::core::MapPosition> getDeployableCells() const;
    QVector<game::core::MapPosition> getMovableCells(int unitId) const;

    // ========== 增强绘制方法 ==========
    void drawTerrain(QPainter &painter);           ///< 绘制地形（渐变+纹理感）
    void drawSpawnMarker(QPainter &painter);        ///< 绘制出生点（旋转传送门）
    void drawCoreMarker(QPainter &painter);         ///< 绘制核心（脉冲发光）
    void drawHighlights(QPainter &painter);         ///< 绘制部署/移动高亮
    void drawUnits(QPainter &painter);              ///< 绘制单位（阴影+发光）
    void drawMonsters(QPainter &painter);           ///< 绘制怪物（阴影+动画）
    void drawProjectiles(QPainter &painter);        ///< 绘制投射物占位特效
    void drawHoverCell(QPainter &painter);          ///< 绘制悬停格子高亮
};

/**
 * @class BattlePage
 * @brief 战斗主页面 —— 包含状态栏、战斗视口和操作栏
 *
 * 职责：
 *   1. 持有 BattleManager 引用（通过 MainWindow 获取）
 *   2. QTimer 驱动游戏主循环，每帧调用 BattleManager::update()
 *   3. 从 BattleManager::snapshot() 获取快照，传给 BattleView 渲染
 *   4. 处理 BattleView 的用户操作信号，调用 BattleManager 接口
 */
class BattlePage : public QWidget
{
    Q_OBJECT

public:
    explicit BattlePage(QWidget *parent = nullptr);

    /**
     * @brief 设置网络上下文（PVP 模式）
     * 在 startBattle() 之前由 MainWindow 调用
     */
    void setNetworkContext(const NetworkContext& ctx);

    /**
     * @brief 开始战斗
     * 在选卡页面确认后由 MainWindow 调用
     * 初始化战斗状态并启动游戏主循环
     */
    void startBattle();

signals:
    void signalBattleEnd();         ///< 战斗结束（核心被摧毁）
    void signalBackToDeploy();      ///< 怪物清空，返回部署阶段

private:
    // ========== UI 组件 ==========
    BattleView  *m_battleView;      ///< 战斗视口
    QTimer      *m_gameTimer;       ///< 游戏主循环定时器

    // ===== 顶部状态栏 =====
    QLabel *m_waveLabel;            ///< 波次显示
    QLabel *m_coreHpLabel;          ///< 核心血量
    QLabel *m_resourceLabel;        ///< 资源点数

    // ===== 底部操作栏 =====
    QVector<QPushButton*> m_cardButtons;  ///< 底部卡牌按钮（对应 CardKind）
    QPushButton *m_btnPause;
    QPushButton *m_btnSpeed;
    QPushButton *m_btnSkill;
    QPushButton *m_btnExit;  ///< 退出按钮

    // ========== 游戏状态 ==========
    bool m_isPaused;                ///< 是否暂停
    double m_speedMultiplier;       ///< 速度倍率（1.0 或 2.0）
    int m_currentWaveId;            ///< 当前波次 ID（用于自动推进）
    double m_waveTimer;             ///< 波次间隔计时器（秒）
    static constexpr double WAVE_INTERVAL = 15.0;  ///< 每波间隔 15 秒

    // ========== 核心层引用 ==========
    game::core::BattleManager *m_battleManager;  ///< 通过 MainWindow 获取

    // ========== 网络状态（PVP 模式） ==========
    NetworkContext m_netCtx;            ///< 网络上下文
    bool m_isPvp = false;               ///< 是否为 PVP 模式
    bool m_isHost = false;              ///< 是否为 Host 端
    bool m_inBattlePhase = false;       ///< 当前页面是否正在承载战斗阶段
    bool m_waveStarted = false;         ///< PVP 客户端是否已收到本波 WAVE_START
    bool m_localWaveClear = false;      ///< 本端本波怪物是否已清空
    bool m_peerWaveClear = false;       ///< 对端本波怪物是否已清空
    double m_stateSyncTimer = 0.0;      ///< Host 权威快照同步计时
    QLabel *m_opponentLabel;            ///< 对手信息标签

    // ========== 初始化方法 ==========
    void initUI();
    void connectSignals();
    void updateStatusBar(const game::core::BattleSnapshot &snapshot);

    // ========== 地图初始化 ==========
    void setupPveMap();                 ///< 初始化 PVE 地图（从 startBattle 提取）
    void setupPvpMap();                 ///< 初始化 PVP 对称地图

    // ========== 网络操作发送 ==========
    void sendDeployAction(game::core::CardKind kind, game::core::MapPosition pos);
    void sendUpgradeAction(int unitId);
    void sendMoveAction(int unitId, game::core::MapPosition target);
    void sendRecallAction(int unitId);
    void completePvpWave();
    void sendBattleState(const game::core::BattleSnapshot& snapshot);
    void handleRemoteBattleState(const game::core::BattleSnapshot& snapshot);

    // ========== 网络包处理 ==========
    void onNetworkPacket(game::network::MsgType type, const QByteArray& body);

    /**
     * @brief 游戏主循环回调
     * 由 QTimer 每 16ms 触发（约 60FPS）
     */
    void onGameTick();
};

#endif // BATTLEPAGE_H
