/**
 * @file LobbyPage.cpp
 * @brief 大厅/配置页面实现文件
 *
 * 核心逻辑：
 *   1. 根据 Mode 切换显示 PVE 或 PVP 面板
 *   2. PVE：选择地图+难度 → 确认后发 signalConfigDone
 *   3. PVP：创建房间使用 GameServer，加入房间使用 GameClient
 *      大厅状态流转由 LobbyManager 管理：
 *      Idle → Waiting → Connected → LocalReady → AllReady → InGame
 *
 * 与 dev 分支 network 模块的对接：
 *   - GameServer::startListening(port) —— Host 端启动监听
 *   - GameClient 连接后通过 LobbyManager 管理 JOIN/READY/GAME_START 流程
 *   - LobbyManager::gameStarted(seed) 信号触发后，进入选卡页面
 */

#include "ui/LobbyPage.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QRadioButton>
#include <QRegularExpressionValidator>

// ========== 引入网络模块头文件 ==========
// dev 分支的 network 模块，位于 network/include/
#include "network/session/GameServer.h"
#include "network/session/GameClient.h"
#include "network/session/LobbyManager.h"

// ========== 构造函数 ==========
LobbyPage::LobbyPage(QWidget *parent)
    : QWidget(parent)
    , m_currentMode(Mode::PVE)
    , m_server(nullptr)
    , m_client(nullptr)
    , m_lobbyManager(nullptr)
{
    initUI();
    connectSignals();
}

// ========== setMode() —— 切换 PVE/PVP 模式 ==========
void LobbyPage::setMode(Mode mode)
{
    m_currentMode = mode;
    if (mode == Mode::PVE) {
        m_titleLabel->setText("🎮 PVE 配置");
        m_panelStack->setCurrentWidget(m_pvePanel);
    } else {
        m_titleLabel->setText("⚔ PVP 大厅");
        m_panelStack->setCurrentWidget(m_pvpPanel);
    }
}

// ========== initUI() —— 初始化界面 ==========
void LobbyPage::initUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(40, 30, 40, 30);
    mainLayout->setSpacing(20);

    // ----- 顶部导航栏 -----
    QHBoxLayout *topBar = new QHBoxLayout();

    m_btnBack = new QPushButton("← 返回", this);
    m_btnBack->setFixedSize(100, 40);
    m_btnBack->setStyleSheet(
        "QPushButton {"
        "  background-color: rgba(20,40,70,0.70); color: #8AB4F8; border: 2px solid rgba(0,212,255,0.50);"
        "  border-radius: 8px; font-size: 14px;"
        "}"
        "QPushButton:hover { color: #00E5FF; border: 2px solid #00D4FF; }"
    );
    m_btnBack->setCursor(Qt::PointingHandCursor);

    m_titleLabel = new QLabel("🎮 PVE 配置", this);
    m_titleLabel->setStyleSheet("color: #FFFFFF; font-size: 24px; font-weight: bold;");
    m_titleLabel->setAlignment(Qt::AlignCenter);

    topBar->addWidget(m_btnBack);
    topBar->addStretch();
    topBar->addWidget(m_titleLabel);
    topBar->addStretch();
    mainLayout->addLayout(topBar);

    // ----- 内部面板堆叠窗口 -----
    m_panelStack = new QStackedWidget(this);
    createPvePanel();
    createPvpPanel();
    m_panelStack->addWidget(m_pvePanel);   // index 0
    m_panelStack->addWidget(m_pvpPanel);   // index 1
    mainLayout->addWidget(m_panelStack, 1);

    // 页面背景
    this->setStyleSheet(
        "LobbyPage {"
        "  background: qlineargradient("
        "    x1:0, y1:0, x2:0, y2:1,"
        "    stop:0 #0B1622, stop:1 #162544"
        "  );"
        "}"
    );
}

// ========== createPvePanel() —— 创建 PVE 配置面板 ==========
void LobbyPage::createPvePanel()
{
    m_pvePanel = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(m_pvePanel);
    layout->setSpacing(25);
    layout->setContentsMargins(20, 20, 20, 20);

    // ----- 地图选择 -----
    QLabel *mapLabel = new QLabel("📍 选择地图：", m_pvePanel);
    mapLabel->setStyleSheet("color: #FFFFFF; font-size: 18px; font-weight: bold;");
    layout->addWidget(mapLabel);

    m_mapSelector = new QComboBox(m_pvePanel);
    // 地图名称对应 data/maps/ 目录下的 CSV 文件
    m_mapSelector->addItems({"草原平原 (map_01)", "沙漠绿洲 (map_02)", "冰雪峡谷 (map_03)"});
    m_mapSelector->setFixedHeight(45);
    m_mapSelector->setStyleSheet(
        "QComboBox {"
        "  background-color: rgba(22,50,90,0.90); color: #FFFFFF;"
        "  border: 2px solid rgba(0,212,255,0.50); border-radius: 8px;"
        "  padding: 8px 15px; font-size: 16px;"
        "}"
        "QComboBox::drop-down { border: none; width: 30px; }"
        "QComboBox QAbstractItemView {"
        "  background-color: #0F1B2D; color: #FFFFFF;"
        "  selection-background-color: rgba(0,212,255,0.3); border: 1px solid #00D4FF;"
        "}"
    );
    layout->addWidget(m_mapSelector);
    layout->addSpacing(20);

    // ----- 难度选择 -----
    QLabel *diffLabel = new QLabel("⚡ 选择难度：", m_pvePanel);
    diffLabel->setStyleSheet("color: #FFFFFF; font-size: 18px; font-weight: bold;");
    layout->addWidget(diffLabel);

    // QRadioButton + QButtonGroup 实现互斥选择
    m_difficultyGroup = new QButtonGroup(this);
    QHBoxLayout *diffLayout = new QHBoxLayout();
    QStringList difficulties = {"🟢 简单", "🟡 普通", "🔴 困难"};

    for (int i = 0; i < difficulties.size(); ++i) {
        QRadioButton *radio = new QRadioButton(difficulties[i], m_pvePanel);
        radio->setStyleSheet(
            "QRadioButton { color: #FFFFFF; font-size: 16px; spacing: 8px;"
            "  background-color: rgba(20,40,75,0.80); border: 2px solid rgba(0,212,255,0.50);"
            "  border-radius: 8px; padding: 8px 16px; }"
            "QRadioButton:hover { background-color: rgba(0,212,255,0.20); border: 2px solid #00D4FF; }"
            "QRadioButton::indicator { width: 18px; height: 18px; border-radius: 9px;"
            "  border: 2px solid rgba(0,212,255,0.7); }"
            "QRadioButton::indicator:checked { background-color: #00D4FF; border: 2px solid #00D4FF; }"
        );
        m_difficultyGroup->addButton(radio, i);
        diffLayout->addWidget(radio);
    }
    m_difficultyGroup->button(1)->setChecked(true);  // 默认选中"普通"
    layout->addLayout(diffLayout);
    layout->addSpacing(30);

    // ----- 确认按钮 -----
    m_btnPveConfirm = new QPushButton("✓ 确认并选卡", m_pvePanel);
    m_btnPveConfirm->setFixedSize(250, 55);
    m_btnPveConfirm->setStyleSheet(
        "QPushButton {"
        "  background-color: rgba(0,212,255,0.18); color: #00D4FF;"
        "  border: 2px solid rgba(0,212,255,0.6); border-radius: 12px;"
        "  font-size: 18px; font-weight: bold;"
        "}"
        "QPushButton:hover { background-color: rgba(0,212,255,0.32); }"
        "QPushButton:pressed { background-color: rgba(0,212,255,0.48); }"
    );
    m_btnPveConfirm->setCursor(Qt::PointingHandCursor);

    QHBoxLayout *confirmLayout = new QHBoxLayout();
    confirmLayout->addStretch();
    confirmLayout->addWidget(m_btnPveConfirm);
    confirmLayout->addStretch();
    layout->addLayout(confirmLayout);
    layout->addStretch();
}

// ========== createPvpPanel() —— 创建 PVP 大厅面板 ==========
void LobbyPage::createPvpPanel()
{
    m_pvpPanel = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(m_pvpPanel);
    layout->setSpacing(25);
    layout->setContentsMargins(20, 20, 20, 20);

    // ----- 创建房间按钮 -----
    m_btnCreateRoom = new QPushButton("🏠 创建房间 (Host)", m_pvpPanel);
    m_btnCreateRoom->setFixedHeight(60);
    m_btnCreateRoom->setStyleSheet(
        "QPushButton {"
        "  background-color: rgba(0,212,255,0.15); color: #00D4FF;"
        "  border: 2px solid rgba(0,212,255,0.6); border-radius: 12px;"
        "  font-size: 18px; font-weight: bold;"
        "}"
        "QPushButton:hover { background-color: rgba(0,212,255,0.3); }"
        "QPushButton:pressed { background-color: rgba(0,212,255,0.45); }"
    );
    m_btnCreateRoom->setCursor(Qt::PointingHandCursor);
    layout->addWidget(m_btnCreateRoom);
    layout->addSpacing(15);

    // ----- 分隔线 -----
    QLabel *separator = new QLabel("───────── 或 ─────────", m_pvpPanel);
    separator->setAlignment(Qt::AlignCenter);
    separator->setStyleSheet("color: rgba(0,212,255,0.3); font-size: 14px;");
    layout->addWidget(separator);
    layout->addSpacing(15);

    // ----- 加入房间区域 -----
    QLabel *joinLabel = new QLabel("🔗 加入已有房间", m_pvpPanel);
    joinLabel->setStyleSheet("color: #E3F2FD; font-size: 18px; font-weight: bold;");
    layout->addWidget(joinLabel);

    m_ipInput = new QLineEdit(m_pvpPanel);
    m_ipInput->setPlaceholderText("输入 Host 的 IP 地址，例如 192.168.1.100");
    m_ipInput->setFixedHeight(45);
    m_ipInput->setStyleSheet(
        "QLineEdit {"
        "  background-color: rgba(22,37,66,0.75); color: #FFFFFF;"
        "  border: 2px solid rgba(0,212,255,0.3); border-radius: 8px;"
        "  padding: 8px 15px; font-size: 15px;"
        "}"
        "QLineEdit:focus { border: 2px solid #00D4FF; }"
    );
    // IP 地址正则验证
    m_ipInput->setValidator(new QRegularExpressionValidator(
        QRegularExpression("^((25[0-5]|2[0-4]\\d|[01]?\\d\\d?)\\.){3}(25[0-5]|2[0-4]\\d|[01]?\\d\\d?)$"),
        this
    ));

    m_btnJoinRoom = new QPushButton("🔗 加入", m_pvpPanel);
    m_btnJoinRoom->setFixedHeight(50);
    m_btnJoinRoom->setStyleSheet(
        "QPushButton {"
        "  background-color: rgba(0,230,118,0.15); color: #00E676;"
        "  border: 2px solid rgba(0,230,118,0.6); border-radius: 10px;"
        "  font-size: 16px; font-weight: bold;"
        "}"
        "QPushButton:hover { background-color: rgba(0,230,118,0.3); }"
        "QPushButton:pressed { background-color: rgba(0,230,118,0.45); }"
    );
    m_btnJoinRoom->setCursor(Qt::PointingHandCursor);

    QHBoxLayout *joinLayout = new QHBoxLayout();
    joinLayout->addWidget(m_ipInput, 3);
    joinLayout->addSpacing(10);
    joinLayout->addWidget(m_btnJoinRoom, 1);
    layout->addLayout(joinLayout);
    layout->addSpacing(20);

    // ----- 连接状态日志 -----
    QLabel *statusLabel = new QLabel("📋 连接状态：", m_pvpPanel);
    statusLabel->setStyleSheet("color: #8AB4F8; font-size: 14px;");
    layout->addWidget(statusLabel);

    m_statusLog = new QTextEdit(m_pvpPanel);
    m_statusLog->setReadOnly(true);
    m_statusLog->setMinimumHeight(120);
    m_statusLog->setStyleSheet(
        "QTextEdit {"
        "  background-color: rgba(11,22,34,0.8); color: #8AB4F8;"
        "  border: 1px solid rgba(0,212,255,0.2); border-radius: 8px;"
        "  padding: 10px; font-family: Consolas, monospace; font-size: 13px;"
        "}"
    );
    m_statusLog->setPlainText("等待操作...\n请选择创建房间或加入已有房间。");
    layout->addWidget(m_statusLog);
    layout->addStretch();
}

// ========== initNetwork() —— 初始化 PVP 网络模块 ==========
void LobbyPage::initNetwork()
{
    // 如果已经初始化过，直接返回
    if (m_server && m_lobbyManager) return;

    // 创建 Host 端 GameServer
    m_server = new game::network::GameServer(this);

    // 创建 Client 端 GameClient
    m_client = new game::network::GameClient(this);

    // 连接 GameServer 的信号到状态日志
    connect(m_server, &game::network::GameServer::clientConnected,
            this, [this]() {
                m_statusLog->append(">> ✅ 对方已连接！");
            });
    connect(m_server, &game::network::GameServer::errorOccurred,
            this, [this](const QString &msg) {
                m_statusLog->append(">> ❌ 服务器错误: " + msg);
            });

    // 连接 GameClient 的信号到状态日志
    connect(m_client, &game::network::GameClient::connected,
            this, [this]() {
                m_statusLog->append(">> ✅ 已连接到 Host！");
            });
    connect(m_client, &game::network::GameClient::errorOccurred,
            this, [this](const QString &msg) {
                m_statusLog->append(">> ❌ 连接错误: " + msg);
            });
}

// ========== connectSignals() —— 连接信号槽 ==========
void LobbyPage::connectSignals()
{
    // 返回按钮
    connect(m_btnBack, &QPushButton::clicked, this, &LobbyPage::signalBack);

    // PVE 确认按钮 → 发出配置完成信号
    connect(m_btnPveConfirm, &QPushButton::clicked, this, [this]() {
        // 获取选中的地图和难度信息
        // int mapIndex = m_mapSelector->currentIndex();
        // int difficulty = m_difficultyGroup->checkedId();
        // TODO: 根据选择加载对应的地图 CSV 和波次配置
        emit signalConfigDone();
    });

    // PVP 创建房间按钮 → 使用 GameServer 启动监听
    connect(m_btnCreateRoom, &QPushButton::clicked, this, [this]() {
        initNetwork();  // 按需初始化网络模块

        // 启动 GameServer 监听
        bool ok = m_server->startListening(9527);  // 默认端口 9527
        if (ok) {
            m_statusLog->append(">> 🏠 房间已创建，正在等待对方加入...");
            m_statusLog->append(">> 本机 IP 可能在局域网内可达");

            // 创建 LobbyManager（Host 角色）
            m_lobbyManager = new game::network::LobbyManager(
                game::network::LobbyManager::Role::Host, "Host", this);

            // 连接 LobbyManager 的 gameStarted 信号 → 进入选卡页
            connect(m_lobbyManager, &game::network::LobbyManager::gameStarted,
                    this, [this](quint32 seed) {
                        m_statusLog->append(QString(">> 🎮 游戏开始！种子: %1").arg(seed));
                        // TODO: 将 seed 传给 BattleManager::setRandomSeed()
                        emit signalConfigDone();
                    });

            // 连接 LobbyManager 的发包请求到 GameServer
            connect(m_lobbyManager, &game::network::LobbyManager::sendPacketRequested,
                    m_server, &game::network::GameServer::sendPacket);

            // 连接 GameServer 的收包信号到 LobbyManager
            connect(m_server, &game::network::GameServer::packetReceived,
                    m_lobbyManager, &game::network::LobbyManager::onPacketReceived);

            // 连接客户端加入信号
            connect(m_server, &game::network::GameServer::clientConnected,
                    m_lobbyManager, [this]() {
                        // 客户端连接后，LobbyManager 触发 onPeerConnected
                        // 但因为 Host 角色不需要调 onPeerConnected，那是在 Client 端用的
                        m_statusLog->append(">> 对方已进入房间");
                    });
        } else {
            m_statusLog->append(">> ❌ 创建房间失败！请检查端口是否被占用。");
        }
    });

    // PVP 加入房间按钮 → 使用 GameClient 连接到 Host
    connect(m_btnJoinRoom, &QPushButton::clicked, this, [this]() {
        QString ip = m_ipInput->text().trimmed();
        if (ip.isEmpty()) {
            m_statusLog->append(">> ❌ 请输入 IP 地址！");
            return;
        }

        initNetwork();  // 按需初始化网络模块

        // 创建 LobbyManager（Client 角色）
        m_lobbyManager = new game::network::LobbyManager(
            game::network::LobbyManager::Role::Client, "Client", this);

        // 连接 LobbyManager 的发包请求到 GameClient
        connect(m_lobbyManager, &game::network::LobbyManager::sendPacketRequested,
                m_client, &game::network::GameClient::sendPacket);

        // 连接 GameClient 的收包信号到 LobbyManager
        connect(m_client, &game::network::GameClient::packetReceived,
                m_lobbyManager, &game::network::LobbyManager::onPacketReceived);

        // 连接 GameClient 的连接成功信号 → 触发 LobbyManager::onPeerConnected
        connect(m_client, &game::network::GameClient::connected,
                m_lobbyManager, &game::network::LobbyManager::onPeerConnected);

        // 连接 LobbyManager 的 gameStarted 信号 → 进入选卡页
        connect(m_lobbyManager, &game::network::LobbyManager::gameStarted,
                this, [this](quint32 seed) {
                    m_statusLog->append(QString(">> 🎮 游戏开始！种子: %1").arg(seed));
                    emit signalConfigDone();
                });

        // 尝试连接
        m_statusLog->append(">> 正在连接 " + ip + " ...");
        m_client->connectToHost(ip, 9527);
    });
}
