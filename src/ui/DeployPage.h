/**
 * @file DeployPage.h
 * @brief 迷雾部署页面 —— PVP 模式下双方各自部署单位，互不可见
 *
 * 流程：
 *   1. 从 DeckPage 接收选好的卡组
 *   2. 显示地图和可部署区域
 *   3. 用户点击卡牌后点击地图格子部署单位
 *   4. 双方部署数据只保存在本地
 *   5. 点击"开战"后同步部署数据，进入战斗
 */

#ifndef DEPLOYPAGE_H
#define DEPLOYPAGE_H

#include <QWidget>
#include <QTimer>
#include <QLabel>
#include <QPushButton>
#include <QVector>
#include <QPainter>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QPixmap>

// ========== 核心层头文件 ==========
#include "core/systems/BattleManager.h"
#include "core/snapshot/BattleSnapshot.h"
#include "core/base/CoreTypes.h"
#include "core/map/MapPosition.h"

// ========== 网络模块 ==========
#include "ui/LobbyPage.h"  // NetworkContext
#include "ui/MapSceneRuntime.h"
#include "network/protocol/ProtocolDef.h"
#include "network/sync/DeploymentSync.h"

#include <set>

/**
 * @class DeployView
 * @brief 部署视口 —— 负责地图绘制和部署交互
 */
class DeployView : public QWidget
{
    Q_OBJECT

public:
    static const int CELL_SIZE = 48;

    explicit DeployView(QWidget *parent = nullptr);

    void updateFromSnapshot(const game::core::BattleSnapshot &snapshot);
    void setMapSize(int rows, int cols);
    bool setBackgroundImage(const QString& path);
    void clearBackgroundImage();
    void setImageCrop(int x, int y, int w, int h);
    void setImageOffset(int x, int y);
    void setArtworkOverlayMode(bool enabled);
    void setShowGrid(bool show);
    void setPvpDeploymentSide(bool enabled, bool isHost);
    void setUnitVisualScale(double scale);
    void setAllowedDeployCells(const std::vector<game::core::MapPosition>& cells);
    void clearAllowedDeployCells();

    // 交互状态
    enum class InteractionMode {
        NONE,
        DEPLOYING,
        MOVING,
        RADIAL_MENU
    };

    InteractionMode m_mode;
    game::core::CardKind m_selectedCardKind;
    int m_selectedUnitId;
    int m_hoverRow;
    int m_hoverCol;

    // 出生点和核心位置
    game::core::MapPosition m_spawnPos;
    game::core::MapPosition m_corePos;

    void hideRadialMenu();

signals:
    void signalDeployCard(game::core::CardKind kind, game::core::MapPosition position);
    void signalUpgradeUnit(int unitId);
    void signalMoveUnit(int unitId, game::core::MapPosition position);
    void signalRecallUnit(int unitId);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

private:
    struct DustEffect {
        int row;
        int col;
        qreal life;
    };

    game::core::BattleSnapshot m_snapshot;
    QVector<DustEffect> m_dustEffects;
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
    bool m_localIsHost;
    double m_unitVisualScale;
    bool m_useAllowedDeployCells;
    std::set<game::core::MapPosition> m_allowedDeployCells;
    int m_animFrame;
    QPushButton *m_btnUpgrade;
    QPushButton *m_btnMove;
    QPushButton *m_btnRecall;

    QVector<game::core::MapPosition> getDeployableCells() const;
    QVector<game::core::MapPosition> getMovableCells(int unitId) const;
    int findOwnUnitAt(int row, int col) const;
    double cellWidth() const;
    double cellHeight() const;
    double cellExtent() const;
    QRectF cellRect(int row, int col) const;
    QPointF cellCenter(int row, int col) const;
    int rowAtPixel(int y) const;
    int colAtPixel(int x) const;
    bool isDeploymentCellAllowed(game::core::MapPosition position) const;
    void showRadialMenu(int unitId, int pixelX, int pixelY);
    void drawTerrain(QPainter &painter);
    void drawDeployable(QPainter &painter);
    void drawUnits(QPainter &painter);
    void drawHoverCell(QPainter &painter);
    void drawDustEffects(QPainter &painter);
};

/**
 * @class DeployPage
 * @brief 迷雾部署页面
 */
class DeployPage : public QWidget
{
    Q_OBJECT

public:
    explicit DeployPage(QWidget *parent = nullptr);

    /**
     * @brief 设置网络上下文
     */
    void setNetworkContext(const NetworkContext& ctx);

    /**
     * @brief 设置选好的卡组
     */
    void setDeck(const QVector<game::core::CardKind>& deck);

    /**
     * @brief 初始化部署阶段（首次进入）
     */
    void initDeployment();

    /**
     * @brief 重新进入部署阶段（战斗结束后）
     * 保留现有单位，允许补充部署
     */
    void reEnter();
    void setShowGrid(bool show);

signals:
    void signalBattleStart();  ///< 开战，进入战斗页面
    void signalBack();         ///< 返回

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    struct PendingDeploy {
        game::core::CardKind kind;
        game::core::MapPosition position;
    };

    struct PendingUnitOp {
        game::network::MsgType type;
        int unitId = -1;
        game::core::MapPosition target;
    };

    // ========== UI 组件 ==========
    DeployView *m_deployView;
    QLabel *m_titleLabel;
    QLabel *m_deployCountLabel;
    QLabel *m_localCoreLabel;
    QLabel *m_enemyCoreLabel;
    QPushButton *m_btnBack;
    QPushButton *m_btnStartBattle;
    QVector<QPushButton*> m_cardButtons;
    QVector<QLabel*> m_cardNameLabels;
    QVector<QLabel*> m_cardCostLabels;

    // ========== 网络状态 ==========
    NetworkContext m_netCtx;
    bool m_isPvp = false;
    bool m_isHost = false;

    // ========== 部署数据 ==========
    QVector<game::core::CardKind> m_deck;
    game::core::BattleManager *m_battleManager;
    game::network::DeploymentSync m_deploySync;
    QVector<PendingDeploy> m_pendingOpponentDeploys;
    QVector<PendingUnitOp> m_pendingOpponentOps;
    int m_deployedCount = 0;
    int m_selectedUnitId = -1;

    // ========== 开战同步 ==========
    bool m_localReady = false;       // 本地是否点击了开战
    bool m_opponentReady = false;    // 对方是否点击了开战
    QLabel *m_opponentLabel;
    QPixmap m_pvpArtwork;
    QPixmap m_pvpOfficeMapArtwork;
    QPixmap m_deckArtwork;
    game::ui::ResolvedMapScene m_activeMapScene;

    void initUI();
    void connectSignals();
    void setupMap();
    void updateDeployCount();
    void refreshCardDisplay();
    void refreshSnapshot();
    void setupCardButtonConnections();  ///< 设置卡牌按钮连接（只一次）
    void applyPendingOpponentDeploys();
    void applyPendingOpponentOps();
    void layoutArtworkUi();
    QRect artworkRect() const;
    void loadActiveMapScene();
    void applyMapSceneToDeployView();
    void refreshLocalVisionAndDeployMask();
    void updateDeployViewSnapshot(const game::core::BattleSnapshot& snapshot);
    bool canLocalPvpDeployAt(game::core::MapPosition pos) const;

    // 网络处理
    void onNetworkPacket(game::network::MsgType type, const QByteArray& body);
    void sendDeploymentEnd();
    void sendDeployToNetwork(game::core::CardKind kind, game::core::MapPosition pos);
    void sendUpgradeToNetwork(int unitId);
    void sendMoveToNetwork(int unitId, game::core::MapPosition pos);
    void sendRecallToNetwork(int unitId);
};

#endif // DEPLOYPAGE_H
