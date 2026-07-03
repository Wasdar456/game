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
 *   - LobbyManager::gameStarted(seed, mapId) 淇″彿瑙﹀彂鍚庯紝杩涘叆閫夊崱椤甸潰
 */

#include "ui/LobbyPage.h"
#include "ui/ArtHotspot.h"

#include <QRadioButton>
#include <QRegularExpressionValidator>
#include <QNetworkInterface>
#include <QHostAddress>
#include <QDebug>
#include <QLinearGradient>
#include <QPainter>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QShowEvent>
#include <QTimer>
#include <QToolTip>

// ========== 寮曞叆缃戠粶妯″潡澶存枃浠?==========
// dev 鍒嗘敮鐨?network 妯″潡锛屼綅浜?network/include/
#include "network/session/GameServer.h"
#include "network/session/GameClient.h"
#include "network/session/LobbyManager.h"

namespace {

constexpr int kPveWidth = 1609;
constexpr int kPveHeight = 906;
constexpr int kPvpWidth = 1672;
constexpr int kPvpHeight = 941;

const QVector<QRect> kPveRects = {
    {239, 282, 322, 384},
    {583, 282, 314, 384},
    {919, 282, 312, 384},
    {1291, 390, 201, 76},
    {1291, 480, 201, 76},
    {1291, 570, 201, 74},
    {25, 789, 221, 91},
    {1271, 788, 313, 94},
};

const QVector<QRect> kPvpRects = {
    {155, 368, 220, 82},
    {155, 470, 220, 84},
    {428, 292, 286, 388},
    {730, 292, 272, 388},
    {1013, 292, 261, 388},
    {22, 807, 222, 91},
    {1354, 806, 297, 93},
};

QRectF scaledRect(const QRect &source, const QRectF &canvas,
                  int designWidth, int designHeight)
{
    const qreal sx = canvas.width() / designWidth;
    const qreal sy = canvas.height() / designHeight;
    return QRectF(canvas.left() + source.x() * sx,
                  canvas.top() + source.y() * sy,
                  source.width() * sx,
                  source.height() * sy);
}

QString readyStatusStyle(const QString& color)
{
    return QString(
        "QLabel { color:%1; background-color:rgb(242,222,176);"
        " border:1px solid rgba(94,66,37,0.55); border-radius:4px;"
        " padding:5px; font-size:14px; font-weight:800; }").arg(color);
}

QString pvpMapIdForIndex(int index)
{
    switch (index) {
    case 2:
        return "pvp_office_panic";
    case 0:
    default:
        return "pvp_sunny_beach";
    }
}

int pvpMapIndexForId(const QString& mapId)
{
    if (mapId == "pvp_office_panic") {
        return 2;
    }
    return 0;
}

bool pvpMapEnabled(int index)
{
    return index == 0 || index == 2;
}

} // namespace

// ========== 鏋勯€犲嚱鏁?==========
LobbyPage::LobbyPage(QWidget *parent)
    : QWidget(parent)
    , m_currentMode(Mode::PVE)
    , m_titleLabel(nullptr)
    , m_btnBack(nullptr)
    , m_pvePanel(nullptr)
    , m_mapSelector(nullptr)
    , m_difficultyGroup(nullptr)
    , m_btnPveConfirm(nullptr)
    , m_pvpPanel(nullptr)
    , m_btnCreateRoom(nullptr)
    , m_ipInput(nullptr)
    , m_btnJoinRoom(nullptr)
    , m_btnReady(nullptr)
    , m_readyStatusLabel(nullptr)
    , m_statusLog(nullptr)
    , m_server(nullptr)
    , m_client(nullptr)
    , m_lobbyManager(nullptr)
    , m_panelStack(nullptr)
    , m_selectedDifficulty(0)
    , m_selectedPvpMap(0)
{
    initUI();
    connectSignals();
}

void LobbyPage::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.fillRect(rect(), QColor(40, 51, 34));

    static QPixmap pveArtwork(":/images/artwork/pve_setup.jpg");
    static QPixmap pvpArtwork(":/images/artwork/pvp_setup.png");
    const QPixmap &artwork = m_currentMode == Mode::PVE ? pveArtwork : pvpArtwork;
    if (!artwork.isNull()) {
        painter.drawPixmap(m_canvasRect, artwork, QRectF(artwork.rect()));
    }

    QLinearGradient edgeShade(m_canvasRect.topLeft(), m_canvasRect.bottomRight());
    edgeShade.setColorAt(0.0, QColor(32, 46, 27, 18));
    edgeShade.setColorAt(0.55, QColor(32, 46, 27, 0));
    edgeShade.setColorAt(1.0, QColor(24, 29, 18, 28));
    painter.fillRect(m_canvasRect, edgeShade);

    if (m_currentMode == Mode::PVE) {
        const QRectF endlessRect = scaledRect(kPveRects[5], m_canvasRect, kPveWidth, kPveHeight);
        painter.save();
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setPen(QPen(QColor(94, 55, 25), 2.0));
        painter.setBrush(QColor(245, 214, 141, 238));
        painter.drawRoundedRect(endlessRect.adjusted(5, 5, -5, -5), 11, 11);
        QFont font("Microsoft YaHei UI", qMax(13, qRound(endlessRect.height() * 0.28)), QFont::Black);
        painter.setFont(font);
        painter.setPen(QColor(52, 31, 18));
        painter.drawText(endlessRect, Qt::AlignCenter, "无尽模式");
        painter.restore();
    }
}

void LobbyPage::setMode(Mode mode)
{
    if (m_currentMode == Mode::PVP && mode != Mode::PVP) {
        resetPvpSession();
    } else if (m_currentMode == Mode::PVP && mode == Mode::PVP) {
        resetPvpSession();
    }
    m_currentMode = mode;
    m_pvePanel->setVisible(mode == Mode::PVE);
    m_pvpPanel->setVisible(mode == Mode::PVP);
    updateArtworkLayout();
    refreshSelectionVisuals();
    update();
}

void LobbyPage::resetPvpSession()
{
    if (m_lobbyManager) {
        delete m_lobbyManager;
        m_lobbyManager = nullptr;
    }
    if (m_client) {
        m_client->disconnect();
        delete m_client;
        m_client = nullptr;
    }
    if (m_server) {
        m_server->disconnect();
        delete m_server;
        m_server = nullptr;
    }

    if (m_btnReady) {
        m_btnReady->setEnabled(false);
        m_btnReady->setText("Ready");
    }
    if (m_readyStatusLabel) {
        m_readyStatusLabel->setText("等待连接...");
        m_readyStatusLabel->setStyleSheet(readyStatusStyle("#6b5138"));
        m_readyStatusLabel->update();
    }
    if (m_statusLog) {
        m_statusLog->setPlainText("请选择创建房间或输入 IP 加入房间。");
    }
}

void LobbyPage::initUI()
{
    setAutoFillBackground(false);
    createPvePanel();
    createPvpPanel();
    m_pvpPanel->hide();
    updateArtworkLayout();
}

void LobbyPage::createPvePanel()
{
    m_pvePanel = new QWidget(this);
    m_pvePanel->setAttribute(Qt::WA_TranslucentBackground);

    m_mapSelector = new QComboBox(m_pvePanel);
    m_mapSelector->addItem("Sunny Beach", "lab_map_02");
    m_mapSelector->addItem("Jungle Ruins", "island_pve");
    m_mapSelector->addItem("Office Panic", "lab_map_01");
    m_mapSelector->hide();

    m_difficultyGroup = new QButtonGroup(this);
    for (int i = 0; i < 3; ++i) {
        auto *radio = new QRadioButton(m_pvePanel);
        m_difficultyGroup->addButton(radio, i);
        radio->hide();
    }
    m_difficultyGroup->button(0)->setChecked(true);

    m_btnBack = new QPushButton(m_pvePanel);
    m_btnBack->hide();
    m_btnPveConfirm = new QPushButton(m_pvePanel);
    m_btnPveConfirm->hide();

    const QString artwork = ":/images/artwork/pve_setup.jpg";
    for (int i = 0; i < kPveRects.size(); ++i) {
        auto *hotspot = new ArtHotspot(artwork, kPveRects[i], m_pvePanel);
        hotspot->setGlowColor(QColor(255, 220, 128));
        m_pveHotspots.append(hotspot);
    }

    for (int i = 0; i < 3; ++i) {
        m_pveMapHotspots.append(m_pveHotspots[i]);
        m_pveHotspots[i]->setClickHandler([this, i]() {
            m_mapSelector->setCurrentIndex(i);
            refreshSelectionVisuals();
        });
    }
    for (int i = 0; i < 3; ++i) {
        m_pveDifficultyHotspots.append(m_pveHotspots[3 + i]);
        m_pveHotspots[3 + i]->setClickHandler([this, i]() {
            m_selectedDifficulty = i;
            m_difficultyGroup->button(i)->setChecked(true);
            refreshSelectionVisuals();
        });
    }
    m_pveHotspots[6]->setClickHandler([this]() { m_btnBack->click(); });
    m_pveHotspots[7]->setClickHandler([this]() { m_btnPveConfirm->click(); });
}

void LobbyPage::createPvpPanel()
{
    m_pvpPanel = new QWidget(this);
    m_pvpPanel->setAttribute(Qt::WA_TranslucentBackground);

    m_btnCreateRoom = new QPushButton(m_pvpPanel);
    m_btnCreateRoom->hide();
    m_btnJoinRoom = new QPushButton(m_pvpPanel);
    m_btnJoinRoom->hide();
    m_btnReady = new QPushButton(m_pvpPanel);
    m_btnReady->setEnabled(false);
    m_btnReady->hide();

    m_ipInput = new QLineEdit(m_pvpPanel);
    m_ipInput->setPlaceholderText("输入 IP...");
    m_ipInput->setStyleSheet(
        "QLineEdit {"
        " background-color: rgba(238, 218, 171, 0.94);"
        " color: #3d2b1d;"
        " border: 2px solid rgba(95, 67, 38, 0.70);"
        " border-radius: 4px;"
        " padding: 5px 12px;"
        " font-family: 'Microsoft YaHei UI', 'PingFang SC', sans-serif;"
        " font-size: 15px;"
        "}"
        "QLineEdit:focus { border-color: #d18b32; }"
    );
    m_ipInput->setValidator(new QRegularExpressionValidator(
        QRegularExpression("^((25[0-5]|2[0-4]\\d|[01]?\\d\\d?)\\.){3}(25[0-5]|2[0-4]\\d|[01]?\\d\\d?)$"),
        this
    ));

    m_readyStatusLabel = new QLabel("等待连接...", m_pvpPanel);
    m_readyStatusLabel->setAlignment(Qt::AlignCenter);
    m_readyStatusLabel->setStyleSheet(
        "QLabel { color: #3d2b1d; background: rgb(242,222,176);"
        " border: 1px solid rgba(94,66,37,0.45); border-radius: 4px;"
        " padding: 5px; font-size: 13px; font-weight: 700; }"
    );

    m_statusLog = new QTextEdit(m_pvpPanel);
    m_statusLog->setReadOnly(true);
    m_statusLog->setStyleSheet(
        "QTextEdit {"
        " background-color: rgb(239, 218, 171);"
        " color: #39281c;"
        " border: 1px solid rgba(94, 66, 37, 0.55);"
        " border-radius: 4px;"
        " padding: 8px;"
        " font-family: 'Microsoft YaHei UI', 'PingFang SC', sans-serif;"
        " font-size: 12px;"
        "}"
    );
    m_statusLog->setPlainText("请选择创建房间或输入 IP 加入房间。");

    const QString artwork = ":/images/artwork/pvp_setup.png";
    for (int i = 0; i < kPvpRects.size(); ++i) {
        auto *hotspot = new ArtHotspot(artwork, kPvpRects[i], m_pvpPanel);
        hotspot->setGlowColor(QColor(255, 220, 128));
        m_pvpHotspots.append(hotspot);
    }
    m_pvpHotspots[0]->setClickHandler([this]() { m_btnCreateRoom->click(); });
    m_pvpHotspots[1]->setClickHandler([this]() { m_btnJoinRoom->click(); });
    for (int i = 0; i < 3; ++i) {
        m_pvpMapHotspots.append(m_pvpHotspots[2 + i]);
        m_pvpHotspots[2 + i]->setClickHandler([this, i]() {
            if (!pvpMapEnabled(i)) {
                showStatus("Jungle Ruins 暂未开放，请选择 Sunny Beach 或 Office Panic");
                return;
            }
            if (m_lobbyManager
                && m_lobbyManager->role() == game::network::LobbyManager::Role::Client) {
                showStatus("当前地图由房主选择");
                return;
            }
            m_selectedPvpMap = i;
            if (m_lobbyManager && m_lobbyManager->role() == game::network::LobbyManager::Role::Host) {
                m_lobbyManager->setSelectedMapId(pvpMapIdForIndex(m_selectedPvpMap));
                m_statusLog->append(QString(">> 房主切换地图为 %1")
                                        .arg(i == 2 ? "Office Panic" : "Sunny Beach"));
            }
            refreshSelectionVisuals();
        });
    }
    m_pvpHotspots[5]->setClickHandler([this]() {
        resetPvpSession();
        emit signalBack();
    });
    m_pvpHotspots[6]->setClickHandler([this]() {
        if (m_btnReady->isEnabled()) {
            m_btnReady->click();
        } else {
            showStatus("请先创建或加入房间，并等待连接完成");
        }
    });
}

void LobbyPage::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateArtworkLayout();
}

void LobbyPage::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    setGraphicsEffect(nullptr);
    updateArtworkLayout();
    const auto &hotspots = m_currentMode == Mode::PVE ? m_pveHotspots : m_pvpHotspots;
    for (ArtHotspot *hotspot : hotspots) {
        hotspot->refreshVisual();
    }
    refreshSelectionVisuals();
}

void LobbyPage::updateArtworkLayout()
{
    const int designWidth = m_currentMode == Mode::PVE ? kPveWidth : kPvpWidth;
    const int designHeight = m_currentMode == Mode::PVE ? kPveHeight : kPvpHeight;
    const qreal scale = qMin(width() / qreal(designWidth),
                             height() / qreal(designHeight));
    const QSizeF canvasSize(designWidth * scale, designHeight * scale);
    const QPointF topLeft((width() - canvasSize.width()) / 2.0,
                          (height() - canvasSize.height()) / 2.0);
    m_canvasRect = QRectF(topLeft, canvasSize);

    m_pvePanel->setGeometry(rect());
    m_pvpPanel->setGeometry(rect());

    for (int i = 0; i < m_pveHotspots.size(); ++i) {
        m_pveHotspots[i]->setCanvasRect(
            scaledRect(kPveRects[i], m_canvasRect, kPveWidth, kPveHeight));
    }
    for (int i = 0; i < m_pvpHotspots.size(); ++i) {
        m_pvpHotspots[i]->setCanvasRect(
            scaledRect(kPvpRects[i], m_canvasRect, kPvpWidth, kPvpHeight));
    }

    if (m_currentMode == Mode::PVP) {
        m_ipInput->setGeometry(
            scaledRect({160, 607, 212, 53}, m_canvasRect, kPvpWidth, kPvpHeight).toRect());
        m_readyStatusLabel->setGeometry(
            scaledRect({1311, 530, 230, 43}, m_canvasRect, kPvpWidth, kPvpHeight).toRect());
        m_statusLog->setGeometry(
            scaledRect({1310, 575, 232, 112}, m_canvasRect, kPvpWidth, kPvpHeight).toRect());
        m_ipInput->raise();
        m_readyStatusLabel->raise();
        m_statusLog->raise();
    }
    update();
}

void LobbyPage::refreshSelectionVisuals()
{
    for (int i = 0; i < m_pveMapHotspots.size(); ++i) {
        m_pveMapHotspots[i]->setSelected(i == m_mapSelector->currentIndex());
    }
    for (int i = 0; i < m_pveDifficultyHotspots.size(); ++i) {
        m_pveDifficultyHotspots[i]->setSelected(i == m_selectedDifficulty);
    }
    for (int i = 0; i < m_pvpMapHotspots.size(); ++i) {
        m_pvpMapHotspots[i]->setSelected(i == m_selectedPvpMap);
    }
}

void LobbyPage::showStatus(const QString &message)
{
    if (m_currentMode == Mode::PVP) {
        m_readyStatusLabel->setText(message);
        m_readyStatusLabel->show();
        m_readyStatusLabel->raise();
    } else {
        m_btnPveConfirm->setToolTip(message);
        QToolTip::showText(mapToGlobal(m_canvasRect.center().toPoint()), message, this);
    }
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
    bool createdServer = false;
    bool createdClient = false;
    if (!m_server) {
        m_server = new game::network::GameServer(this);
        createdServer = true;
    }
    if (!m_client) {
        m_client = new game::network::GameClient(this);
        createdClient = true;
    }

    if (createdServer) {
        connect(m_server, &game::network::GameServer::clientConnected,
                this, [this]() {
                    m_statusLog->append(">> 对方已连接，等待大厅握手...");
                });
        connect(m_server, &game::network::GameServer::clientDisconnected,
                this, [this]() {
                    m_statusLog->append(">> 对方已断开，准备状态已重置。");
                    if (m_lobbyManager) {
                        delete m_lobbyManager;
                        m_lobbyManager = nullptr;
                    }
                    m_btnReady->setEnabled(false);
                    m_btnReady->setText("Ready");
                    m_readyStatusLabel->setText("连接已断开");
                    m_readyStatusLabel->setStyleSheet(readyStatusStyle("#8a2f24"));
                    m_readyStatusLabel->update();
                });
        connect(m_server, &game::network::GameServer::errorOccurred,
                this, [this](const QString &msg) {
                    m_statusLog->append(">> 服务器错误: " + msg);
                });
    }

    if (createdClient) {
        connect(m_client, &game::network::GameClient::connected,
                this, [this]() {
                    m_statusLog->append(">> 已连接到 Host，等待大厅握手...");
                });
        connect(m_client, &game::network::GameClient::disconnected,
                this, [this]() {
                    m_statusLog->append(">> 已断开连接，准备状态已重置。");
                    if (m_lobbyManager) {
                        delete m_lobbyManager;
                        m_lobbyManager = nullptr;
                    }
                    m_btnReady->setEnabled(false);
                    m_btnReady->setText("Ready");
                    m_readyStatusLabel->setText("连接已断开");
                    m_readyStatusLabel->setStyleSheet(readyStatusStyle("#8a2f24"));
                    m_readyStatusLabel->update();
                });
        connect(m_client, &game::network::GameClient::errorOccurred,
                this, [this](const QString &msg) {
                    m_statusLog->append(">> 连接错误: " + msg);
                });
    }
}

// ========== connectSignals() 鈥斺€?杩炴帴淇″彿妲?==========
void LobbyPage::connectSignals()
{
    // 杩斿洖鎸夐挳
    connect(m_btnBack, &QPushButton::clicked, this, [this]() {
        resetPvpSession();
        emit signalBack();
    });

    // PVE 纭鎸夐挳 鈫?鍙戝嚭閰嶇疆瀹屾垚淇″彿
    connect(m_btnPveConfirm, &QPushButton::clicked, this, [this]() {
        const QString mapId = m_mapSelector->currentData().toString();
        emit signalConfigDone(mapId.isEmpty() ? QString("lab_map_01") : mapId,
                              m_selectedDifficulty);
    });

    // PVP 鍒涘缓鎴块棿鎸夐挳 鈫?浣跨敤 GameServer 鍚姩鐩戝惉
    connect(m_btnCreateRoom, &QPushButton::clicked, this, [this]() {
        if (m_lobbyManager || m_server || m_client) {
            resetPvpSession();
        }
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
            m_lobbyManager->setSelectedMapId(pvpMapIdForIndex(m_selectedPvpMap));

            // 杩炴帴 LobbyManager 鐨?gameStarted 淇″彿 鈫?杩涘叆閫夊崱椤?
            connect(m_lobbyManager, &game::network::LobbyManager::gameStarted,
                    this, [this](quint32 seed, const QString& mapId) {
                        m_statusLog->append(QString(">> 双方准备完成，种子: %1").arg(seed));
                        NetworkContext ctx;
                        ctx.isPvp = true;
                        ctx.isHost = true;
                        ctx.pvpMapId = mapId.isEmpty() ? pvpMapIdForIndex(m_selectedPvpMap) : mapId;
                        ctx.seed = seed;
                        ctx.server = m_server;
                        ctx.client = nullptr;
                        emit signalPvpReady(ctx);
                    });
            connect(m_lobbyManager, &game::network::LobbyManager::mapSelectionChanged,
                    this, [this](const QString& mapId) {
                        m_selectedPvpMap = pvpMapIndexForId(mapId);
                        refreshSelectionVisuals();
                    });
            connect(m_lobbyManager, &game::network::LobbyManager::peerJoined,
                    this, [this](const QString& peerName) {
                        m_statusLog->append(QString(">> %1 已进入房间，请点击准备").arg(peerName));
                        m_btnReady->setEnabled(true);
                        m_readyStatusLabel->setText("对方已进入房间，请点击准备");
                        m_readyStatusLabel->setStyleSheet(readyStatusStyle("#287a43"));
                        m_readyStatusLabel->update();
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
                        m_statusLog->append(">> 对方已连接，等待 JOIN_ROOM...");
                        m_btnReady->setEnabled(false);
                        m_readyStatusLabel->setText("等待对方加入房间...");
                        m_readyStatusLabel->setStyleSheet(readyStatusStyle("#287a43"));
                        m_readyStatusLabel->update();
                    });

            // 杩炴帴鍑嗗鐘舵€佷俊鍙?
            connect(m_lobbyManager, &game::network::LobbyManager::peerReady,
                    this, [this]() {
                        m_readyStatusLabel->setText("对方已准备");
                        m_readyStatusLabel->setStyleSheet(readyStatusStyle("#287a43"));
                        m_readyStatusLabel->update();
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

        if (m_lobbyManager || m_server || m_client) {
            resetPvpSession();
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
                    m_statusLog->append(">> 已连接到房间，等待 JOIN_ACK...");
                    m_btnReady->setEnabled(false);
                    m_readyStatusLabel->setText("等待房主确认...");
                    m_readyStatusLabel->setStyleSheet(readyStatusStyle("#287a43"));
                    m_readyStatusLabel->update();
                });

        // 杩炴帴鍑嗗鐘舵€佷俊鍙?
        connect(m_lobbyManager, &game::network::LobbyManager::peerReady,
                this, [this]() {
                    m_readyStatusLabel->setText("对方已准备");
                    m_readyStatusLabel->setStyleSheet(readyStatusStyle("#287a43"));
                    m_readyStatusLabel->update();
                });
        connect(m_lobbyManager, &game::network::LobbyManager::mapSelectionChanged,
                this, [this](const QString& mapId) {
                    m_selectedPvpMap = pvpMapIndexForId(mapId);
                    refreshSelectionVisuals();
                    m_statusLog->append(QString(">> 房主已切换地图为 %1")
                                            .arg(m_selectedPvpMap == 2 ? "Office Panic"
                                                                      : "Sunny Beach"));
                });
        connect(m_lobbyManager, &game::network::LobbyManager::peerJoined,
                this, [this](const QString& peerName) {
                    m_statusLog->append(QString(">> 已加入 %1 的房间，请点击准备").arg(peerName));
                    m_btnReady->setEnabled(true);
                    m_readyStatusLabel->setText("已进入房间，请点击准备");
                    m_readyStatusLabel->setStyleSheet(readyStatusStyle("#287a43"));
                    m_readyStatusLabel->update();
                });

        // 杩炴帴 LobbyManager 鐨?gameStarted 淇″彿 鈫?杩涘叆閫夊崱椤?
        connect(m_lobbyManager, &game::network::LobbyManager::gameStarted,
                this, [this](quint32 seed, const QString& mapId) {
                    m_statusLog->append(QString(">> 双方准备完成，种子: %1").arg(seed));
                    NetworkContext ctx;
                    ctx.isPvp = true;
                    ctx.isHost = false;
                    ctx.pvpMapId = mapId.isEmpty() ? QString("pvp_sunny_beach") : mapId;
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
            if (!m_lobbyManager->isReady()) {
                m_statusLog->append(">> 当前房间尚未完成握手，暂不能准备。");
                return;
            }
            m_btnReady->setEnabled(false);
            m_btnReady->setText("已准备");
            m_readyStatusLabel->setText("你已准备，等待对方准备...");
            m_readyStatusLabel->setStyleSheet(readyStatusStyle("#9a6418"));
            m_readyStatusLabel->update();
            m_statusLog->append(">> 你已准备，等待对方准备...");
        }
    });
}
