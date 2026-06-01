/**
 * @file LobbyPage.cpp
 * @brief 澶у巺/閰嶇疆椤甸潰瀹炵幇鏂囦欢
 *
 * 鏍稿績閫昏緫锛?
 *   1. 鏍规嵁 Mode 鍒囨崲鏄剧ず PVE 鎴?PVP 闈㈡澘
 *   2. PVE锛氶€夋嫨鍦板浘+闅惧害 鈫?纭鍚庡彂 signalConfigDone
 *   3. PVP锛氬垱寤烘埧闂翠娇鐢?GameServer锛屽姞鍏ユ埧闂翠娇鐢?GameClient
 *      澶у巺鐘舵€佹祦杞敱 LobbyManager 绠＄悊锛?
 *      Idle 鈫?Waiting 鈫?Connected 鈫?LocalReady 鈫?AllReady 鈫?InGame
 *
 * 涓?dev 鍒嗘敮 network 妯″潡鐨勫鎺ワ細
 *   - GameServer::startListening(port) 鈥斺€?Host 绔惎鍔ㄧ洃鍚?
 *   - GameClient 杩炴帴鍚庨€氳繃 LobbyManager 绠＄悊 JOIN/READY/GAME_START 娴佺▼
 *   - LobbyManager::gameStarted(seed) 淇″彿瑙﹀彂鍚庯紝杩涘叆閫夊崱椤甸潰
 */

#include "ui/LobbyPage.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QRadioButton>
#include <QRegularExpressionValidator>
#include <QNetworkInterface>
#include <QHostAddress>
#include <QDebug>
#include <QPainter>
#include <QPaintEvent>

// ========== 寮曞叆缃戠粶妯″潡澶存枃浠?==========
// dev 鍒嗘敮鐨?network 妯″潡锛屼綅浜?network/include/
#include "network/session/GameServer.h"
#include "network/session/GameClient.h"
#include "network/session/LobbyManager.h"

// ========== 鏋勯€犲嚱鏁?==========
LobbyPage::LobbyPage(QWidget *parent)
    : QWidget(parent)
    , m_currentMode(Mode::PVE)
    , m_btnReady(nullptr)
    , m_readyStatusLabel(nullptr)
    , m_server(nullptr)
    , m_client(nullptr)
    , m_lobbyManager(nullptr)
{
    initUI();
    connectSignals();
}

void LobbyPage::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    static QPixmap bg(":/images/ui/scene_lab_05.png");
    if (!bg.isNull()) {
        QSize scaled = bg.size();
        scaled.scale(size(), Qt::KeepAspectRatioByExpanding);
        QRect target(QPoint((width() - scaled.width()) / 2,
                            (height() - scaled.height()) / 2), scaled);
        painter.drawPixmap(target, bg);
    } else {
        painter.fillRect(rect(), QColor(37, 30, 34));
    }
    painter.fillRect(rect(), QColor(35, 24, 21, 126));
}

// ========== setMode() 鈥斺€?鍒囨崲 PVE/PVP 妯″紡 ==========
void LobbyPage::setMode(Mode mode)
{
    m_currentMode = mode;
    if (mode == Mode::PVE) {
        m_titleLabel->setText("PVE 配置");
        m_panelStack->setCurrentWidget(m_pvePanel);
    } else {
        m_titleLabel->setText("PVP 大厅");
        m_panelStack->setCurrentWidget(m_pvpPanel);
    }
}

// ========== initUI() 鈥斺€?鍒濆鍖栫晫闈?==========
void LobbyPage::initUI()
{
    setAutoFillBackground(false);
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(40, 30, 40, 30);
    mainLayout->setSpacing(20);

    // ----- 椤堕儴瀵艰埅鏍?-----
    QHBoxLayout *topBar = new QHBoxLayout();

    m_btnBack = new QPushButton("返回", this);
    m_btnBack->setFixedSize(100, 40);
    m_btnBack->setStyleSheet(
        "QPushButton {"
        "  background-color: rgba(225,176,99,0.86); color: #3A2418; border: 2px solid rgba(76,48,31,0.82);"
        "  border-radius: 8px; font-size: 14px; font-weight: bold;"
        "}"
        "QPushButton:hover { border: 2px solid #FFD27E; }"
    );
    m_btnBack->setCursor(Qt::PointingHandCursor);

    m_titleLabel = new QLabel("PVE 配置", this);
    m_titleLabel->setStyleSheet("color: #FFF0C8; font-size: 24px; font-weight: bold;");
    m_titleLabel->setAlignment(Qt::AlignCenter);

    topBar->addWidget(m_btnBack);
    topBar->addStretch();
    topBar->addWidget(m_titleLabel);
    topBar->addStretch();
    mainLayout->addLayout(topBar);

    // ----- 鍐呴儴闈㈡澘鍫嗗彔绐楀彛 -----
    m_panelStack = new QStackedWidget(this);
    createPvePanel();
    createPvpPanel();
    m_panelStack->addWidget(m_pvePanel);   // index 0
    m_panelStack->addWidget(m_pvpPanel);   // index 1
    mainLayout->addWidget(m_panelStack, 1);

    // 椤甸潰鑳屾櫙
    this->setStyleSheet("LobbyPage { background: transparent; }");
}

// ========== createPvePanel() 鈥斺€?鍒涘缓 PVE 閰嶇疆闈㈡澘 ==========
void LobbyPage::createPvePanel()
{
    m_pvePanel = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(m_pvePanel);
    layout->setSpacing(25);
    layout->setContentsMargins(20, 20, 20, 20);

    // ----- 鍦板浘閫夋嫨 -----
    QLabel *mapLabel = new QLabel("选择地图：", m_pvePanel);
    mapLabel->setStyleSheet("color: #FFFFFF; font-size: 18px; font-weight: bold;");
    layout->addWidget(mapLabel);

    m_mapSelector = new QComboBox(m_pvePanel);
    m_mapSelector->addItem("实验室通道 (01)", "lab_map_01");
    m_mapSelector->addItem("海滩果汁湾 (06)", "lab_map_02");
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

    // ----- 闅惧害閫夋嫨 -----
    QLabel *diffLabel = new QLabel("选择难度：", m_pvePanel);
    diffLabel->setStyleSheet("color: #FFFFFF; font-size: 18px; font-weight: bold;");
    layout->addWidget(diffLabel);

    // QRadioButton + QButtonGroup 瀹炵幇浜掓枼閫夋嫨
    m_difficultyGroup = new QButtonGroup(this);
    QHBoxLayout *diffLayout = new QHBoxLayout();
    QStringList difficulties = {"简单", "普通", "困难"};

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
    m_difficultyGroup->button(1)->setChecked(true);  // 榛樿閫変腑"鏅€?
    layout->addLayout(diffLayout);
    layout->addSpacing(30);

    // ----- 纭鎸夐挳 -----
    m_btnPveConfirm = new QPushButton("确认并选卡", m_pvePanel);
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

// ========== createPvpPanel() 鈥斺€?鍒涘缓 PVP 澶у巺闈㈡澘 ==========
void LobbyPage::createPvpPanel()
{
    m_pvpPanel = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(m_pvpPanel);
    layout->setSpacing(25);
    layout->setContentsMargins(20, 20, 20, 20);

    // ----- 鍒涘缓鎴块棿鎸夐挳 -----
    m_btnCreateRoom = new QPushButton("创建房间 (Host)", m_pvpPanel);
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

    // ----- 鍒嗛殧绾?-----
    QLabel *separator = new QLabel("或者", m_pvpPanel);
    separator->setAlignment(Qt::AlignCenter);
    separator->setStyleSheet("color: rgba(0,212,255,0.3); font-size: 14px;");
    layout->addWidget(separator);
    layout->addSpacing(15);

    // ----- 鍔犲叆鎴块棿鍖哄煙 -----
    QLabel *joinLabel = new QLabel("加入已有房间", m_pvpPanel);
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
    // IP 鍦板潃姝ｅ垯楠岃瘉
    m_ipInput->setValidator(new QRegularExpressionValidator(
        QRegularExpression("^((25[0-5]|2[0-4]\\d|[01]?\\d\\d?)\\.){3}(25[0-5]|2[0-4]\\d|[01]?\\d\\d?)$"),
        this
    ));

    m_btnJoinRoom = new QPushButton("加入", m_pvpPanel);
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

    // ----- 鍑嗗鎸夐挳鍜岀姸鎬?-----
    m_btnReady = new QPushButton("准备", m_pvpPanel);
    m_btnReady->setFixedHeight(50);
    m_btnReady->setStyleSheet(
        "QPushButton {"
        "  background-color: rgba(255,193,7,0.15); color: #FFC107;"
        "  border: 2px solid rgba(255,193,7,0.6); border-radius: 10px;"
        "  font-size: 18px; font-weight: bold;"
        "}"
        "QPushButton:hover { background-color: rgba(255,193,7,0.3); }"
        "QPushButton:pressed { background-color: rgba(255,193,7,0.45); }"
        "QPushButton:disabled { color: #666; border-color: rgba(255,193,7,0.2); }"
    );
    m_btnReady->setCursor(Qt::PointingHandCursor);
    m_btnReady->setEnabled(false);  // 杩炴帴鎴愬姛鍚庢墠鍚敤
    layout->addWidget(m_btnReady);

    m_readyStatusLabel = new QLabel("等待连接...", m_pvpPanel);
    m_readyStatusLabel->setAlignment(Qt::AlignCenter);
    m_readyStatusLabel->setStyleSheet("color: #8AB4F8; font-size: 14px;");
    layout->addWidget(m_readyStatusLabel);
    layout->addSpacing(10);

    // ----- 杩炴帴鐘舵€佹棩蹇?-----
    QLabel *statusLabel = new QLabel("连接状态：", m_pvpPanel);
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

// ========== getLocalIPAddress() 鈥斺€?鑾峰彇鏈満灞€鍩熺綉 IP 鍦板潃 ==========
QString LobbyPage::getLocalIPAddress() const
{
    QList<QHostAddress> addresses = QNetworkInterface::allAddresses();
    for (const QHostAddress &address : addresses) {
        // 杩囨护鎺?IPv6銆佸洖鐜湴鍧€锛屽彧淇濈暀 IPv4 灞€鍩熺綉鍦板潃
        if (address.protocol() == QAbstractSocket::IPv4Protocol &&
            !address.isLoopback()) {
            QString ip = address.toString();
            // 浼樺厛杩斿洖 192.168.x.x 鎴?10.x.x.x 灞€鍩熺綉鍦板潃
            if (ip.startsWith("192.168.") || ip.startsWith("10.")) {
                return ip;
            }
        }
    }
    // 濡傛灉娌℃壘鍒板眬鍩熺綉鍦板潃锛岃繑鍥炵涓€涓潪鍥炵幆 IPv4 鍦板潃
    for (const QHostAddress &address : addresses) {
        if (address.protocol() == QAbstractSocket::IPv4Protocol &&
            !address.isLoopback()) {
            return address.toString();
        }
    }
    return "127.0.0.1";
}

// ========== initNetwork() 鈥斺€?鍒濆鍖?PVP 缃戠粶妯″潡 ==========
void LobbyPage::initNetwork()
{
    // 濡傛灉宸茬粡鍒濆鍖栬繃锛岀洿鎺ヨ繑鍥?
    if (m_server && m_lobbyManager) return;

    // 鍒涘缓 Host 绔?GameServer
    m_server = new game::network::GameServer(this);

    // 鍒涘缓 Client 绔?GameClient
    m_client = new game::network::GameClient(this);

    // 杩炴帴 GameServer 鐨勪俊鍙峰埌鐘舵€佹棩蹇?
    connect(m_server, &game::network::GameServer::clientConnected,
            this, [this]() {
                m_statusLog->append(">> 对方已连接");
            });
    connect(m_server, &game::network::GameServer::errorOccurred,
            this, [this](const QString &msg) {
                m_statusLog->append(">> 服务器错误: " + msg);
            });

    // 杩炴帴 GameClient 鐨勪俊鍙峰埌鐘舵€佹棩蹇?
    connect(m_client, &game::network::GameClient::connected,
            this, [this]() {
                m_statusLog->append(">> 已连接到 Host");
            });
    connect(m_client, &game::network::GameClient::errorOccurred,
            this, [this](const QString &msg) {
                m_statusLog->append(">> 连接错误: " + msg);
            });
}

// ========== connectSignals() 鈥斺€?杩炴帴淇″彿妲?==========
void LobbyPage::connectSignals()
{
    // 杩斿洖鎸夐挳
    connect(m_btnBack, &QPushButton::clicked, this, &LobbyPage::signalBack);

    // PVE 纭鎸夐挳 鈫?鍙戝嚭閰嶇疆瀹屾垚淇″彿
    connect(m_btnPveConfirm, &QPushButton::clicked, this, [this]() {
        const QString mapId = m_mapSelector->currentData().toString();
        // int difficulty = m_difficultyGroup->checkedId();
        emit signalConfigDone(mapId.isEmpty() ? QString("lab_map_01") : mapId);
    });

    // PVP 鍒涘缓鎴块棿鎸夐挳 鈫?浣跨敤 GameServer 鍚姩鐩戝惉
    connect(m_btnCreateRoom, &QPushButton::clicked, this, [this]() {
        initNetwork();  // 鎸夐渶鍒濆鍖栫綉缁滄ā鍧?

        // 鍚姩 GameServer 鐩戝惉
        bool ok = m_server->startListening(9527);  // 榛樿绔彛 9527
        if (ok) {
            // 鑾峰彇鏈満 IP 鍦板潃
            QString localIP = getLocalIPAddress();
            qDebug() << "[LobbyPage] local IP:" << localIP;

            m_statusLog->append(">> 房间已创建");
            m_statusLog->append(">> ------------------------------");
            m_statusLog->append(">> 请让对方输入以下信息加入房间：");
            m_statusLog->append(QString(">>    IP 地址：%1").arg(localIP));
            m_statusLog->append(">>    端口：9527");
            m_statusLog->append(">> ------------------------------");
            m_statusLog->append(">> 正在等待对方加入...");

            // 鍒涘缓 LobbyManager锛圚ost 瑙掕壊锛?
            m_lobbyManager = new game::network::LobbyManager(
                game::network::LobbyManager::Role::Host, "Host", this);

            // 杩炴帴 LobbyManager 鐨?gameStarted 淇″彿 鈫?杩涘叆閫夊崱椤?
            connect(m_lobbyManager, &game::network::LobbyManager::gameStarted,
                    this, [this](quint32 seed) {
                        m_statusLog->append(QString(">> 双方准备完成，种子: %1").arg(seed));
                        NetworkContext ctx;
                        ctx.isPvp = true;
                        ctx.isHost = true;
                        ctx.seed = seed;
                        ctx.server = m_server;
                        ctx.client = nullptr;
                        emit signalPvpReady(ctx);
                    });

            // 杩炴帴 LobbyManager 鐨勫彂鍖呰姹傚埌 GameServer
            connect(m_lobbyManager, &game::network::LobbyManager::sendPacketRequested,
                    m_server, &game::network::GameServer::sendPacket);

            // 杩炴帴 GameServer 鐨勬敹鍖呬俊鍙峰埌 LobbyManager
            connect(m_server, &game::network::GameServer::packetReceived,
                    m_lobbyManager, &game::network::LobbyManager::onPacketReceived);

            // 杩炴帴瀹㈡埛绔姞鍏ヤ俊鍙?
            connect(m_server, &game::network::GameServer::clientConnected,
                    this, [this]() {
                        m_statusLog->append(">> 对方已进入房间，请点击准备");
                        m_btnReady->setEnabled(true);
                        m_readyStatusLabel->setText("对方已连接，请点击准备");
                        m_readyStatusLabel->setStyleSheet("color: #00E676; font-size: 14px;");
                    });

            // 杩炴帴鍑嗗鐘舵€佷俊鍙?
            connect(m_lobbyManager, &game::network::LobbyManager::peerReady,
                    this, [this]() {
                        m_readyStatusLabel->setText("对方已准备");
                        m_readyStatusLabel->setStyleSheet("color: #00E676; font-size: 14px;");
                    });
        } else {
            m_statusLog->append(">> 创建房间失败，请检查端口是否被占用。");
        }
    });

    // PVP 鍔犲叆鎴块棿鎸夐挳 鈫?浣跨敤 GameClient 杩炴帴鍒?Host
    connect(m_btnJoinRoom, &QPushButton::clicked, this, [this]() {
        QString ip = m_ipInput->text().trimmed();
        if (ip.isEmpty()) {
            m_statusLog->append(">> 请输入 IP 地址。");
            return;
        }

        initNetwork();  // 鎸夐渶鍒濆鍖栫綉缁滄ā鍧?

        // 鍒涘缓 LobbyManager锛圕lient 瑙掕壊锛?
        m_lobbyManager = new game::network::LobbyManager(
            game::network::LobbyManager::Role::Client, "Client", this);

        // 杩炴帴 LobbyManager 鐨勫彂鍖呰姹傚埌 GameClient
        connect(m_lobbyManager, &game::network::LobbyManager::sendPacketRequested,
                m_client, &game::network::GameClient::sendPacket);

        // 杩炴帴 GameClient 鐨勬敹鍖呬俊鍙峰埌 LobbyManager
        connect(m_client, &game::network::GameClient::packetReceived,
                m_lobbyManager, &game::network::LobbyManager::onPacketReceived);

        // 杩炴帴 GameClient 鐨勮繛鎺ユ垚鍔熶俊鍙?鈫?瑙﹀彂 LobbyManager::onPeerConnected
        connect(m_client, &game::network::GameClient::connected,
                m_lobbyManager, &game::network::LobbyManager::onPeerConnected);

        // 杩炴帴杩炴帴鎴愬姛淇″彿 鈫?鍚敤鍑嗗鎸夐挳
        connect(m_client, &game::network::GameClient::connected,
                this, [this]() {
                    m_statusLog->append(">> 已连接到房间，点击准备按钮");
                    m_btnReady->setEnabled(true);
                    m_readyStatusLabel->setText("已连接，请点击准备");
                    m_readyStatusLabel->setStyleSheet("color: #00E676; font-size: 14px;");
                });

        // 杩炴帴鍑嗗鐘舵€佷俊鍙?
        connect(m_lobbyManager, &game::network::LobbyManager::peerReady,
                this, [this]() {
                    m_readyStatusLabel->setText("对方已准备");
                    m_readyStatusLabel->setStyleSheet("color: #00E676; font-size: 14px;");
                });

        // 杩炴帴 LobbyManager 鐨?gameStarted 淇″彿 鈫?杩涘叆閫夊崱椤?
        connect(m_lobbyManager, &game::network::LobbyManager::gameStarted,
                this, [this](quint32 seed) {
                    m_statusLog->append(QString(">> 双方准备完成，种子: %1").arg(seed));
                    NetworkContext ctx;
                    ctx.isPvp = true;
                    ctx.isHost = false;
                    ctx.seed = seed;
                    ctx.server = nullptr;
                    ctx.client = m_client;
                    emit signalPvpReady(ctx);
                });

        // 灏濊瘯杩炴帴
        m_statusLog->append(">> 正在连接 " + ip + " ...");
        m_client->connectToHost(ip, 9527);
    });

    // 鍑嗗鎸夐挳鐐瑰嚮浜嬩欢
    connect(m_btnReady, &QPushButton::clicked, this, [this]() {
        if (m_lobbyManager) {
            m_lobbyManager->setReady();
            m_btnReady->setEnabled(false);
            m_btnReady->setText("已准备");
            m_readyStatusLabel->setText("你已准备，等待对方准备...");
            m_readyStatusLabel->setStyleSheet("color: #FFC107; font-size: 14px;");
            m_statusLog->append(">> 你已准备，等待对方准备...");
        }
    });
}
