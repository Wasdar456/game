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
#include <QPixmap>
#include <QHash>
#include <QSet>

// ========== 核心层头文件 ==========
#include "core/systems/BattleManager.h"     // 战斗总管理器
#include "core/snapshot/BattleSnapshot.h"   // 战斗快照（只读）
#include "core/base/CoreTypes.h"            // CardKind, TerrainType 等枚举
#include "core/base/Constants.h"
#include "core/map/MapPosition.h"           // 网格坐标

// ========== 网络模块 ==========
#include "ui/LobbyPage.h"                   // NetworkContext 结构体
#include "network/protocol/ProtocolDef.h"   // MsgType 枚举
#include "ui/BattleResult.h"
#include "ui/PvpMapLayout.h"

class PauseOverlay;
class TutorialOverlay;
class QKeyEvent;
class QResizeEvent;

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
    bool setBackgroundImage(const QString& path);
    void clearBackgroundImage();
    void setImageCrop(int x, int y, int w, int h);
    void setImageOffset(int x, int y);
    void setArtworkOverlayMode(bool enabled);
    void setShowGrid(bool show);
    void setPvpDeploymentSide(bool enabled, bool isHost);
    void setPvpMapLayout(const game::ui::PvpMapLayout& layout);
    void setUnitVisualScale(double scale);
    void hideRadialMenu();
    void clearEffects();

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
    enum class EffectType {
        DeployDust,
        AttackFlash,
        HitFlash,
        UnitDeath
    };

    struct BattleEffect {
        EffectType type;
        int row;
        int col;
        qreal life;
        qreal duration;
    };

    game::core::BattleSnapshot m_snapshot;
    QVector<BattleEffect> m_effects;
    int m_lastEffectEventSequence;
    QPixmap m_backgroundImage;
    int m_mapRows;
    int m_mapCols;
    int m_imageCropX;
    int m_imageCropY;
    int m_imageCropW;
    int m_imageCropH;
    int m_imageOffsetX;
    int m_imageOffsetY;
    bool m_artworkOverlayMode;
    bool m_showGrid;
    bool m_restrictPvpDeployment;
    game::ui::PvpMapLayout m_pvpLayout;
    double m_unitVisualScale;

    void showRadialMenu(int unitId, int pixelX, int pixelY);
    int findUnitAt(int row, int col) const;
    QVector<game::core::MapPosition> getDeployableCells() const;
    QVector<game::core::MapPosition> getMovableCells(int unitId) const;
    double cellWidth() const;
    double cellHeight() const;
    double cellExtent() const;
    QRectF cellRect(int row, int col) const;
    QPointF cellCenter(int row, int col) const;
    int rowAtPixel(int y) const;
    int colAtPixel(int x) const;
    void addEffect(EffectType type, int row, int col, qreal duration);

    // ========== 增强绘制方法 ==========
    void drawTerrain(QPainter &painter);           ///< 绘制地形（渐变+纹理感）
    void drawSpawnMarker(QPainter &painter);        ///< 绘制出生点（旋转传送门）
    void drawCoreMarker(QPainter &painter);         ///< 绘制核心（脉冲发光）
    void drawHighlights(QPainter &painter);         ///< 绘制部署/移动高亮
    void drawUnits(QPainter &painter);              ///< 绘制单位（阴影+发光）
    void drawMonsters(QPainter &painter);           ///< 绘制怪物（阴影+动画）
    void drawProjectiles(QPainter &painter);        ///< 绘制投射物占位特效
    void drawHoverCell(QPainter &painter);          ///< 绘制悬停格子高亮
    void drawEffects(QPainter &painter);
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
    void setDeck(const QVector<game::core::CardKind>& deck);

    /**
     * @brief 开始战斗
     * 在选卡页面确认后由 MainWindow 调用
     * 初始化战斗状态并启动游戏主循环
     */
    void startBattle();
    void setRevealPaused(bool paused);
    void setShowGrid(bool show);
    void pauseForFocusLoss();

signals:
    void signalBattleFinished(const BattleResult &result);
    void signalBattleCancelled();
    void signalBattleRestartRequested();
    void signalSettingsRequested();
    void signalBackToDeploy();      ///< 怪物清空，返回部署阶段

protected:
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    // ========== UI 组件 ==========
    BattleView  *m_battleView;      ///< 战斗视口
    QTimer      *m_gameTimer;       ///< 游戏主循环定时器

    // ===== 顶部状态栏 =====
    QLabel *m_waveLabel;            ///< 波次显示
    QLabel *m_coreHpLabel;          ///< 核心血量
    QLabel *m_resourceLabel;        ///< 资源点数
    QLabel *m_phaseLabel;
    QLabel *m_syncLabel;

    // ===== 底部操作栏 =====
    QVector<QPushButton*> m_cardButtons;  ///< 底部卡牌按钮（对应 CardKind）
    QVector<QLabel*> m_cardNameLabels;
    QVector<QLabel*> m_cardCostLabels;
    QPushButton *m_btnPause;
    QPushButton *m_btnSpeed;
    QPushButton *m_btnSkill;
    QPushButton *m_btnExit;  ///< 退出按钮
    PauseOverlay *m_pauseOverlay;
    QPushButton *m_btnGuide;
    TutorialOverlay *m_tutorialOverlay;

    // ========== 游戏状态 ==========
    bool m_isPaused;                ///< 是否暂停
    double m_speedMultiplier;       ///< 速度倍率（1.0 或 2.0）
    int m_currentWaveId;            ///< 当前波次 ID（用于自动推进）
    int m_pveFinalWave;             ///< PVE 当前难度下的最终波次
    double m_waveTimer;             ///< 波次间隔计时器（秒）
    static constexpr double WAVE_INTERVAL = 15.0;  ///< 每波间隔 15 秒
    bool m_resultEmitted = false;
    bool m_pveEndlessMode = false;
    int m_renderTick = 0;
    BattleReplayData m_replayData;
    game::core::BattleSnapshot m_lastReplaySnapshot;
    QHash<int, BattleStatEntry> m_replayUnitStats;
    QHash<int, BattleMonsterStatEntry> m_replayMonsterStats;
    QSet<int> m_seenReplayUnits;
    QSet<int> m_seenReplayMonsters;
    QVector<int> m_deathHeatCells;
    QVector<int> m_deployHeatCells;
    double m_replayClock = 0.0;
    double m_replaySampleTimer = 0.0;
    int m_previousReplayResources = -1;
    bool m_hasReplaySnapshot = false;
    int m_lastReplayEventSequence = 0;
    QSet<int> m_eventResolvedReplayMonsters;

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
    bool m_hasPendingRemoteSnapshot = false;
    game::core::BattleSnapshot m_pendingRemoteSnapshot;
    int m_remoteChecksumMismatchCount = 0;
    int m_displayCoreHealth = game::core::constants::InitialBaseHealth;
    int m_displayOpponentCoreHealth = game::core::constants::InitialBaseHealth;
    int m_displayResources = -1;
    int m_selectedCardIndex = -1;
    enum class TutorialStage {
        Inactive,
        Intro,
        Resources,
        CardPrompt,
        WaitCard,
        DeployPrompt,
        WaitDeploy,
        Tactics,
        Final,
        CoreWarning
    };
    TutorialStage m_tutorialStage = TutorialStage::Inactive;
    bool m_tutorialPaused = false;
    bool m_tutorialSessionActive = false;
    bool m_coreWarningShown = false;
    QLabel *m_opponentLabel;            ///< 对手信息标签
    QPixmap m_pvpArtwork;
    QPixmap m_pvpOfficeMapArtwork;
    QPixmap m_pveArtwork;
    QPixmap m_pveUiOverlay;
    QPixmap m_labMap01;
    QPixmap m_labMap02;
    QPixmap m_deckArtwork;
    QVector<game::core::CardKind> m_deck;
    QString m_activePveMapId;

    // ========== 初始化方法 ==========
    void initUI();
    void connectSignals();
    void updateStatusBar(const game::core::BattleSnapshot &snapshot);
    game::core::BattleSnapshot refreshBattleStateAfterPlayerAction(bool forceReplaySnapshot = false);
    void finishBattle(const game::core::BattleSnapshot &snapshot, bool pveVictory = false);
    void resetReplayRecorder();
    void recordReplaySnapshot(const game::core::BattleSnapshot &snapshot,
                              double deltaSeconds,
                              bool force = false);
    void attachReplayToResult(BattleResult &result);
    void setPaused(bool paused);
    void refreshCardDisplay();
    void updateCardVisualState(int resources);
    void beginTutorial(bool replay);
    void advanceTutorial();
    void finishTutorial(bool skipped);
    void resumeFromTutorialPause(bool clearSelection);
    void updateTutorialTargets();
    void layoutArtworkUi();
    QRect artworkRect() const;

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
    void queueRemoteBattleState(const game::core::BattleSnapshot& snapshot);
    void applyPendingRemoteBattleState();
    void handleNetworkDisconnected();

    // ========== 网络包处理 ==========
    void onNetworkPacket(game::network::MsgType type, const QByteArray& body);

    /**
     * @brief 游戏主循环回调
     * 由 QTimer 每 16ms 触发（约 60FPS）
     */
    void onGameTick();
};

#endif // BATTLEPAGE_H
