/**
 * @file LobbyPage.h
 * @brief 大厅/配置页面头文件 —— PVE 地图难度选择 与 PVP 房间创建/加入
 *
 * PVE 模式布局：
 *   ┌─────────────────────────────────────┐
 *   │  ← 返回        PVE 配置            │
 *   │   选择地图：[草原平原 ▼]            │
 *   │   选择难度：[简单] [普通] [困难]    │
 *   │          [确认并选卡]               │
 *   └─────────────────────────────────────┘
 *
 * PVP 模式布局：
 *   ┌─────────────────────────────────────┐
 *   │  ← 返回        PVP 大厅            │
 *   │   [🏠 创建房间 (Host)]              │
 *   │   ──────── 或 ────────              │
 *   │   IP: [____________] [加入]         │
 *   │   状态日志...                       │
 *   └─────────────────────────────────────┘
 *
 * 设计说明：
 *   - PVE 和 PVP 两种模式共用一个页面，通过 setMode() 切换显示内容
 *   - PVP 网络逻辑使用 dev 分支 network/ 模块的 GameServer/GameClient/LobbyManager
 *   - 本页面仅负责 UI 展示和用户交互，网络操作由 network 模块处理
 */

#ifndef LOBBYPAGE_H
#define LOBBYPAGE_H

#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QButtonGroup>
#include <QStackedWidget>
#include <QTextEdit>
#include <QRectF>
#include <QVector>

class ArtHotspot;
class QResizeEvent;
class QShowEvent;

// ========== 网络模块前向声明 ==========
// dev 分支 network/ 模块中定义的类
namespace game::network {
    class GameServer;      ///< Host 端：监听端口，接受客户端连接
    class GameClient;      ///< Client 端：连接到 Host
    class LobbyManager;    ///< 大厅状态机：管理 JOIN/READY/START 流程
}

/**
 * @brief 网络上下文 —— 从 LobbyPage 传递到 BattlePage 的联机信息
 */
struct NetworkContext {
    bool isPvp = false;                           ///< 是否为 PVP 模式
    bool isHost = false;                          ///< 是否为 Host 端
    QString pveMapId = "lab_map_01";              ///< PVE selected map config id
    quint32 seed = 0;                             ///< 随机数种子（PVP 同步用）
    game::network::GameServer* server = nullptr;  ///< Host 端服务器指针
    game::network::GameClient* client = nullptr;  ///< Client 端客户端指针
};

class LobbyPage : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief 游戏模式枚举
     */
    enum class Mode { PVE, PVP };

    explicit LobbyPage(QWidget *parent = nullptr);

    /**
     * @brief 设置当前显示的模式（PVE 或 PVP）
     * 由 MainWindow 在切换到此页面时调用
     */
    void setMode(Mode mode);

signals:
    void signalConfigDone(const QString& mapId);  ///< 配置完成，进入选卡页面（PVE）
    void signalPvpReady(const NetworkContext& ctx);  ///< PVP 配置完成，携带网络上下文
    void signalBack();        ///< 返回上一页

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    Mode m_currentMode;       ///< 当前显示的模式

    // ========== 公共组件 ==========
    QLabel      *m_titleLabel;
    QPushButton *m_btnBack;

    // ========== PVE 面板组件 ==========
    QWidget     *m_pvePanel;          ///< PVE 配置面板容器
    QComboBox   *m_mapSelector;       ///< 地图选择下拉框
    QButtonGroup *m_difficultyGroup;  ///< 难度选择按钮组（互斥）
    QPushButton *m_btnPveConfirm;     ///< PVE 确认按钮

    // ========== PVP 面板组件 ==========
    QWidget     *m_pvpPanel;          ///< PVP 大厅面板容器
    QPushButton *m_btnCreateRoom;     ///< 创建房间按钮
    QLineEdit   *m_ipInput;          ///< IP 地址输入框
    QPushButton *m_btnJoinRoom;       ///< 加入房间按钮
    QPushButton *m_btnReady;          ///< 准备按钮
    QLabel      *m_readyStatusLabel;  ///< 准备状态显示
    QTextEdit   *m_statusLog;        ///< 连接状态日志

    // ========== PVP 网络模块实例 ==========
    // 这些在 PVP 模式下按需创建，PVE 模式下为 nullptr
    game::network::GameServer   *m_server;        ///< Host 端服务器
    game::network::GameClient   *m_client;        ///< Client 端客户端
    game::network::LobbyManager *m_lobbyManager;  ///< 大厅状态管理器

    // ========== 内部堆叠窗口 ==========
    QStackedWidget *m_panelStack;     ///< 用于切换 PVE/PVP 面板

    QRectF m_canvasRect;
    QVector<ArtHotspot*> m_pveHotspots;
    QVector<ArtHotspot*> m_pvpHotspots;
    QVector<ArtHotspot*> m_pveMapHotspots;
    QVector<ArtHotspot*> m_pveDifficultyHotspots;
    QVector<ArtHotspot*> m_pvpMapHotspots;
    int m_selectedDifficulty;
    int m_selectedPvpMap;

    void initUI();
    void createPvePanel();
    void createPvpPanel();
    void connectSignals();
    void updateArtworkLayout();
    void refreshSelectionVisuals();
    void showStatus(const QString &message);

    /**
     * @brief 初始化 PVP 网络模块
     * 在 PVP 模式下按需调用，创建 GameServer/GameClient/LobbyManager
     */
    void initNetwork();

    /**
     * @brief 获取本机局域网 IP 地址
     * 用于 PVP 创建房间时显示给对方
     */
    QString getLocalIPAddress() const;
};

#endif // LOBBYPAGE_H
