// ═══════════════════════════════════════════════════════════════
// 网络模块完整流程测试
//
// 模拟完整游戏流程：
//   阶段一：建房与加入
//     1. Host 启动，监听端口 9527
//     2. Client 连接 Host，发送 JOIN_ROOM（携带昵称）
//     3. Host 回复 JOIN_ACK（携带房主昵称）
//
//   阶段二：准备阶段
//     4. 双方各自调用 setReady()，发送 PLAYER_READY
//     5. Host 检测到双方都 ready → 生成种子 → 广播 GAME_START
//
//   阶段三：游戏主循环（用 RoundManager 模拟"部署+进攻"轮次）
//     6. Host 广播 ROUND_VALUE（模拟"本轮怪物数据"）
//     7. 双方各自"处理"后调用 localAck()
//     8. Host 收到双方 ack → 广播 ROUND_COMPLETE → 下一轮
//
// 运行方式（两个终端）：
//   终端 A（Host）:   ./NetworkTest server [你的昵称]
//   终端 B（Client）: ./NetworkTest client [你的昵称] [Host的IP]
//
// 局域网测试：
//   Host: ./NetworkTest server
//   Client: ./NetworkTest client 玩家2 192.168.x.x
// ═══════════════════════════════════════════════════════════════

#include <QCoreApplication>
#include <QTimer>
#include <QDebug>
#include <QRandomGenerator>

#include "network/session/GameServer.h"
#include "network/session/GameClient.h"
#include "network/session/LobbyManager.h"
#include "network/session/RoundManager.h"
#include "network/protocol/ProtocolDef.h"

using namespace game::network;

// ═══════════════════════════════════════════════════════════════
// HOST 模式
// ═══════════════════════════════════════════════════════════════
static void runAsHost(const QString& nickname) {
    static GameServer   server;
    static LobbyManager lobby(LobbyManager::Role::Host, nickname);
    static RoundManager round(RoundManager::Role::Host);
    static int          roundNum = 1;

    // ─── 大厅：LobbyManager 发包连接到 GameServer ───
    QObject::connect(&lobby, &LobbyManager::sendPacketRequested,
        [](MsgType type, const QByteArray& body) {
            server.sendPacket(type, body);
        });

    // ─── 轮次：RoundManager 发包连接到 GameServer ───
    QObject::connect(&round, &RoundManager::sendPacketRequested,
        [](MsgType type, const QByteArray& body) {
            server.sendPacket(type, body);
        });

    // ─── 网络包路由：根据消息类型分发给不同 Manager ───
    QObject::connect(&server, &GameServer::packetReceived,
        [](MsgType type, const QByteArray& body) {
            // 大厅阶段消息
            if (type == MsgType::JOIN_ROOM  ||
                type == MsgType::PLAYER_READY ||
                type == MsgType::PLAYER_UNREADY) {
                lobby.onPacketReceived(type, body);
                return;
            }
            // 游戏阶段消息
            round.onPacketReceived(type, body);
        });

    // ─── 大厅事件 ───
    QObject::connect(&lobby, &LobbyManager::peerJoined, [](const QString& name) {
        qInfo() << "\n[Host 大厅] ✓ 玩家加入：" << name;
        qInfo() << "[Host 大厅] 提示：等待双方点击准备...";
        // 模拟 Host 自动准备（1秒后）
        QTimer::singleShot(1000, []() {
            qInfo() << "[Host 大厅] 房主点击准备！";
            lobby.setReady();
        });
    });

    QObject::connect(&lobby, &LobbyManager::peerReady, []() {
        qInfo() << "[Host 大厅] 对方已准备 ✓";
    });

    QObject::connect(&lobby, &LobbyManager::peerCancelled, []() {
        qInfo() << "[Host 大厅] 对方取消准备";
    });

    // ─── 游戏开始（LobbyManager 触发） ───
    QObject::connect(&lobby, &LobbyManager::gameStarted, [](quint32 seed) {
        qInfo() << "\n[Host] ===================================================";
        qInfo() << "[Host] ===== 游戏开始！随机种子 =" << seed << "=====";
        qInfo() << "[Host] ===================================================\n";

        // 500ms 后广播第一轮数据
        QTimer::singleShot(500, []() {
            // 模拟"第1轮怪物出兵数据"
            QString data = QString("wave_%1:monsters=[3,1,2],interval=2000").arg(roundNum);
            qInfo() << "[Host 游戏] 广播第" << roundNum << "轮数据：" << data;
            round.sendRoundValue(data.toUtf8());
        });
    });

    // ─── 轮次：本地处理（模拟耗时） ───
    QObject::connect(&round, &RoundManager::valueReceived, [](const QByteArray& value) {
        qInfo() << "\n[Host 游戏] 处理本轮数据：" << value;
        qInfo() << "[Host 游戏] 模拟处理耗时 500ms...";
        QTimer::singleShot(500, []() {
            qInfo() << "[Host 游戏] 本地处理完成 → localAck()";
            round.localAck();
        });
    });

    // ─── 轮次：双方都 ack → 下一轮 ───
    QObject::connect(&round, &RoundManager::allAcked, []() {
        ++roundNum;
        qInfo() << "\n[Host 游戏] ===== 本轮完成，1.5秒后广播第" << roundNum << "轮 =====";
        QTimer::singleShot(1500, []() {
            QString data = QString("wave_%1:monsters=[2,3,4],interval=1500").arg(roundNum);
            qInfo() << "[Host 游戏] 广播第" << roundNum << "轮数据：" << data;
            round.sendRoundValue(data.toUtf8());
        });
    });

    QObject::connect(&round, &RoundManager::roundComplete, []() {
        qInfo() << "[Host 游戏] 第" << (roundNum - 1) << "轮完成 ✓";
    });

    // ─── 连接/断开事件 ───
    QObject::connect(&server, &GameServer::clientConnected, []() {
        qInfo() << "\n[Host] ─── Client 已连接 ───";
        lobby.onPeerConnected();
    });

    QObject::connect(&server, &GameServer::clientDisconnected, []() {
        qInfo() << "[Host] 客户端断开连接";
    });

    QObject::connect(&server, &GameServer::errorOccurred, [](const QString& msg) {
        qCritical() << "[Host] 错误：" << msg;
    });

    // 启动监听
    if (server.startListening(9527)) {
        qInfo() << "╔════════════════════════════════════════╗";
        qInfo() << "║         HOST 模式启动                  ║";
        qInfo() << "║  昵称：" << nickname;
        qInfo() << "║  监听端口：9527                         ║";
        qInfo() << "║  等待 Client 连接...                   ║";
        qInfo() << "╚════════════════════════════════════════╝\n";
    } else {
        qCritical() << "[Host] 监听失败！端口 9527 可能被占用";
    }
}

// ═══════════════════════════════════════════════════════════════
// CLIENT 模式
// ═══════════════════════════════════════════════════════════════
static void runAsClient(const QString& nickname, const QString& hostIp) {
    static GameClient   client;
    static LobbyManager lobby(LobbyManager::Role::Client, nickname);
    static RoundManager round(RoundManager::Role::Client);

    // ─── 大厅：LobbyManager 发包连接到 GameClient ───
    QObject::connect(&lobby, &LobbyManager::sendPacketRequested,
        [](MsgType type, const QByteArray& body) {
            client.sendPacket(type, body);
        });

    // ─── 轮次：RoundManager 发包连接到 GameClient ───
    QObject::connect(&round, &RoundManager::sendPacketRequested,
        [](MsgType type, const QByteArray& body) {
            client.sendPacket(type, body);
        });

    // ─── 网络包路由 ───
    QObject::connect(&client, &GameClient::packetReceived,
        [](MsgType type, const QByteArray& body) {
            if (type == MsgType::JOIN_ACK   ||
                type == MsgType::PLAYER_READY ||
                type == MsgType::PLAYER_UNREADY ||
                type == MsgType::GAME_START) {
                lobby.onPacketReceived(type, body);
                return;
            }
            round.onPacketReceived(type, body);
        });

    // ─── 大厅事件 ───
    QObject::connect(&lobby, &LobbyManager::peerJoined, [](const QString& name) {
        qInfo() << "\n[Client 大厅] ✓ 进入房间，房主：" << name;
        qInfo() << "[Client 大厅] 提示：等待双方点击准备...";
        // 模拟 Client 自动准备（1.5秒后）
        QTimer::singleShot(1500, []() {
            qInfo() << "[Client 大厅] 玩家点击准备！";
            lobby.setReady();
        });
    });

    QObject::connect(&lobby, &LobbyManager::peerReady, []() {
        qInfo() << "[Client 大厅] 房主已准备 ✓";
    });

    // ─── 游戏开始（收到 Host 广播的 GAME_START） ───
    QObject::connect(&lobby, &LobbyManager::gameStarted, [](quint32 seed) {
        qInfo() << "\n[Client] ===================================================";
        qInfo() << "[Client] ===== 游戏开始！随机种子 =" << seed << "=====";
        qInfo() << "[Client] ===================================================\n";
        qInfo() << "[Client 游戏] 等待 Host 广播轮次数据...";
    });

    // ─── 轮次：收到数据，处理后 ack ───
    QObject::connect(&round, &RoundManager::valueReceived, [](const QByteArray& value) {
        qInfo() << "\n[Client 游戏] 收到本轮数据：" << value;
        qInfo() << "[Client 游戏] 模拟处理耗时 800ms...";
        QTimer::singleShot(800, []() {
            qInfo() << "[Client 游戏] 本地处理完成 → localAck()";
            round.localAck();
        });
    });

    QObject::connect(&round, &RoundManager::roundComplete, []() {
        qInfo() << "[Client 游戏] 本轮完成 ✓，等待下一轮...";
    });

    // ─── 连接成功 → 发送 JOIN_ROOM ───
    QObject::connect(&client, &GameClient::connected, []() {
        qInfo() << "[Client] TCP 连接成功，发送加入请求...";
        lobby.onPeerConnected();
    });

    QObject::connect(&client, &GameClient::disconnected, []() {
        qInfo() << "[Client] 与服务器断开连接";
    });

    QObject::connect(&client, &GameClient::errorOccurred, [](const QString& msg) {
        qCritical() << "[Client] 错误：" << msg;
    });

    // 发起连接
    qInfo() << "╔════════════════════════════════════════╗";
    qInfo() << "║         CLIENT 模式启动                 ║";
    qInfo() << "║  昵称：" << nickname;
    qInfo() << "║  连接目标：" << hostIp << ":9527";
    qInfo() << "╚════════════════════════════════════════╝\n";
    client.connectToHost(hostIp, 9527);
}

// ═══════════════════════════════════════════════════════════════
// main
// ═══════════════════════════════════════════════════════════════
int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);

    if (argc < 2) {
        qInfo() << "用法：";
        qInfo() << "  ./NetworkTest server [昵称]";
        qInfo() << "  ./NetworkTest client [昵称] [Host的IP]";
        qInfo() << "";
        qInfo() << "示例（同一台电脑测试）：";
        qInfo() << "  终端A: ./NetworkTest server 房主";
        qInfo() << "  终端B: ./NetworkTest client 玩家2 127.0.0.1";
        qInfo() << "";
        qInfo() << "示例（局域网两台电脑）：";
        qInfo() << "  Host电脑: ./NetworkTest server 房主";
        qInfo() << "  另一台:   ./NetworkTest client 玩家2 192.168.x.x";
        return 1;
    }

    QString mode = argv[1];

    if (mode == "server") {
        QString name = (argc >= 3) ? QString::fromLocal8Bit(argv[2]) : "房主";
        runAsHost(name);
    } else if (mode == "client") {
        QString name = (argc >= 3) ? QString::fromLocal8Bit(argv[2]) : "玩家2";
        QString ip   = (argc >= 4) ? QString(argv[3]) : "127.0.0.1";
        runAsClient(name, ip);
    } else {
        qCritical() << "未知模式：" << mode << "（应为 server 或 client）";
        return 1;
    }

    return app.exec();
}
