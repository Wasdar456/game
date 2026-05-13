// ═══════════════════════════════════════════════════════════════
// 网络模块完整流程测试 v2
//
// 升级点（相比 v1）：
//   - ROUND_VALUE 不再传字符串，改为 WaveStartPayload 二进制格式
//   - Serializer::serialize(WaveStartPayload) 演示 struct 序列化
//   - Deserializer::decode(WaveStartPayload) 演示 struct 反序列化
//   - PING/PONG 心跳已内置（GameServer/GameClient 自动处理）
//
// 流程：
//   1. Host 启动，监听端口 9527
//   2. Client 连接 → 发送 JOIN_ROOM
//   3. Host 回复 JOIN_ACK
//   4. 双方准备 → GAME_START（含 RNG 种子）
//   5. Host 广播 WAVE_START（waveId），双方用种子独立生成怪物
//   6. 双方处理后 ACK，Host 确认双方完成 → 下一轮
//   7. 心跳每 2 秒一次，自动检测断线
//
// 运行方式（两个终端）：
//   终端 A（Host）:   ./NetworkTest server [昵称]
//   终端 B（Client）: ./NetworkTest client [昵称] [Host的IP]
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
#include "network/protocol/Serializer.h"
#include "network/protocol/Deserializer.h"

using namespace game::network;

// 演示用：双方各自持有一个 RNG（种子相同）
static std::mt19937 g_rng(0);  // 会收到 seed 后重新初始化

// ─── 辅助：打印收到的 WaveStartPayload ───
static void dumpWavePacket(const QByteArray& body) {
    Deserializer d(body);
    WaveStartPayload p;
    if (d.decode(p)) {
        qInfo() << "  └─ WaveStartPayload { waveId =" << p.waveId
                << ", reserved =" << (int)p.reserved[0]
                << (int)p.reserved[1] << (int)p.reserved[2] << "}";
    } else {
        qWarning() << "  └─ 解析 WaveStartPayload 失败（数据不完整）";
    }
}

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

    // ─── 网络包路由 ───
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
        QTimer::singleShot(1000, []() {
            qInfo() << "[Host 大厅] 房主自动准备！";
            lobby.setReady();
        });
    });

    QObject::connect(&lobby, &LobbyManager::peerReady, []() {
        qInfo() << "[Host 大厅] 对方已准备 ✓";
    });

    QObject::connect(&lobby, &LobbyManager::peerCancelled, []() {
        qInfo() << "[Host 大厅] 对方取消准备";
    });

    // ─── 游戏开始 ───
    QObject::connect(&lobby, &LobbyManager::gameStarted, [](quint32 seed) {
        qInfo() << "\n[Host] ═══════════════════════════════════════";
        qInfo() << "[Host] 游戏开始！随机种子 =" << seed;
        qInfo() << "[Host] ═══════════════════════════════════════\n";

        // 用种子初始化本地 RNG（Client 也会用相同 seed 初始化，保证怪物序列一致）
        g_rng.seed(seed);

        // 500ms 后广播第一轮
        QTimer::singleShot(500, []() {
            WaveStartPayload payload;
            payload.waveId = 1;
            payload.reserved[0] = payload.reserved[1] = payload.reserved[2] = 0;

            QByteArray data = Serializer::serialize(payload);
            qInfo() << "[Host 游戏] 广播 WAVE_START（第 1 波）："
                    << "waveId=1, seed 已应用";
            round.sendRoundValue(data);
        });
    });

    // ─── 轮次：收到 WAVE_START，处理 ───
    QObject::connect(&round, &RoundManager::valueReceived,
        [](const QByteArray& value) {
            qInfo() << "\n[Host 游戏] 收到本轮数据：";
            dumpWavePacket(value);

            // 演示：用 seed + waveId 生成一致的怪物序列（Client 侧也这样算）
            // 这里只打印，实际由 PVP 组的逻辑调用
            Deserializer d(value);
            WaveStartPayload p;
            if (d.decode(p)) {
                qInfo() << "[Host 游戏] 用 seed+waveId 生成怪物序列中...";
                // g_rng() 返回相同序列（因为 Client 侧用相同 seed）
            }

            qInfo() << "[Host 游戏] 模拟处理耗时 500ms...";
            QTimer::singleShot(500, []() {
                qInfo() << "[Host 游戏] 本地处理完成 → localAck()";
                round.localAck();
            });
        });

    // ─── 轮次：双方都 ACK → 下一轮 ───
    QObject::connect(&round, &RoundManager::allAcked, []() {
        static int roundNum = 2;
        qInfo() << "\n[Host 游戏] ═══ 本轮完成，准备下一轮 ═══";
        QTimer::singleShot(1500, [=]() {
            WaveStartPayload payload;
            payload.waveId = roundNum;
            payload.reserved[0] = payload.reserved[1] = payload.reserved[2] = 0;

            qInfo() << "[Host 游戏] 广播 WAVE_START（第" << roundNum << "波）";
            round.sendRoundValue(Serializer::serialize(payload));
            roundNum++;
        });
    });

    QObject::connect(&round, &RoundManager::roundComplete, []() {
        qInfo() << "[Host 游戏] 轮次完成 ✓";
    });

    // ─── 心跳日志（收到 PING/PONG 时自动打印）───
    QObject::connect(&server, &GameServer::packetReceived,
        [](MsgType type, const QByteArray&) {
            if (type == MsgType::PONG) {
                qInfo() << "[Host 心跳] ✓ PONG 收到，连接正常";
            }
        });

    // ─── 连接/断开事件 ───
    QObject::connect(&server, &GameServer::clientConnected, []() {
        qInfo() << "\n[Host] ─── Client 已连接 ───";
        lobby.onPeerConnected();
    });

    QObject::connect(&server, &GameServer::clientDisconnected, []() {
        qCritical() << "[Host] ⚠ 客户端断开连接！";
    });

    QObject::connect(&server, &GameServer::errorOccurred, [](const QString& msg) {
        qCritical() << "[Host] 错误：" << msg;
    });

    // 启动监听
    if (server.startListening(9527)) {
        qInfo() << "╔════════════════════════════════════════╗";
        qInfo() << "║         HOST 模式启动                 ║";
        qInfo() << "║  昵称：" << nickname;
        qInfo() << "║  监听端口：9527                        ║";
        qInfo() << "║  心跳：每 2 秒 PING/PONG              ║";
        qInfo() << "║  等待 Client 连接...                  ║";
        qInfo() << "╚════════════════════════════════════════╝\n";
    } else {
        qCritical() << "[Host] 监听失败！端口可能被占用";
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
        QTimer::singleShot(1500, []() {
            qInfo() << "[Client 大厅] 玩家自动准备！";
            lobby.setReady();
        });
    });

    QObject::connect(&lobby, &LobbyManager::peerReady, []() {
        qInfo() << "[Client 大厅] 房主已准备 ✓";
    });

    // ─── 游戏开始 ───
    QObject::connect(&lobby, &LobbyManager::gameStarted, [](quint32 seed) {
        qInfo() << "\n[Client] ═══════════════════════════════════════";
        qInfo() << "[Client] 游戏开始！随机种子 =" << seed;
        qInfo() << "[Client] ═══════════════════════════════════════\n";

        // Client 也用相同 seed 初始化 RNG
        g_rng.seed(seed);
        qInfo() << "[Client] 已用 seed 初始化本地 RNG，等待 WAVE_START...";
    });

    // ─── 轮次：收到 WAVE_START，处理 ───
    QObject::connect(&round, &RoundManager::valueReceived,
        [](const QByteArray& value) {
            qInfo() << "\n[Client 游戏] 收到本轮数据：";
            dumpWavePacket(value);

            Deserializer d(value);
            WaveStartPayload p;
            if (d.decode(p)) {
                qInfo() << "[Client 游戏] 用 seed+waveId 生成怪物序列中...";
                // g_rng() 返回相同序列（因为 Client 侧用相同 seed 初始化）
                // 即便两边分别计算，结果也完全一致
            }

            qInfo() << "[Client 游戏] 模拟处理耗时 800ms...";
            QTimer::singleShot(800, []() {
                qInfo() << "[Client 游戏] 本地处理完成 → localAck()";
                round.localAck();
            });
        });

    QObject::connect(&round, &RoundManager::roundComplete, []() {
        qInfo() << "[Client 游戏] 本轮完成 ✓，等待下一轮...";
    });

    // ─── 心跳日志 ───
    QObject::connect(&client, &GameClient::packetReceived,
        [](MsgType type, const QByteArray&) {
            if (type == MsgType::PONG) {
                qInfo() << "[Client 心跳] ✓ PONG 收到，连接正常";
            }
        });

    // ─── 连接成功 ───
    QObject::connect(&client, &GameClient::connected, []() {
        qInfo() << "[Client] TCP 连接成功，发送加入请求...";
        lobby.onPeerConnected();
    });

    QObject::connect(&client, &GameClient::disconnected, []() {
        qCritical() << "[Client] ⚠ 与服务器断开连接";
    });

    QObject::connect(&client, &GameClient::errorOccurred, [](const QString& msg) {
        qCritical() << "[Client] 错误：" << msg;
    });

    qInfo() << "╔════════════════════════════════════════╗";
    qInfo() << "║         CLIENT 模式启动                ║";
    qInfo() << "║  昵称：" << nickname;
    qInfo() << "║  连接目标：" << hostIp << ":9527";
    qInfo() << "║  心跳：每 2 秒 PING/PONG              ║";
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
        qInfo() << "";
        qInfo() << "测试内容：";
        qInfo() << "  - WAVE_START 用 WaveStartPayload 二进制格式";
        qInfo() << "  - Serializer/Deserializer 完整序列化演示";
        qInfo() << "  - PING/PONG 心跳自动检测断线";
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
