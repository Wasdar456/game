# network 模块 — 独立测试版

> 本分支包含网络层完整代码骨架 + 可运行的联机测试脚本，供其他模块参考接口、对接开发。
> 分支名：`feature/network-module`
> 负责人：Wasdar456

---

## 一、已完成的模块

### 1.1 protocol/ — 协议定义层

| 文件 | 内容 |
|:---|:---|
| `Packet.h` | 固定包头（3 字节：`msgType[1] + bodyLen[2]`）+ 粘包边界判断 |
| `ProtocolDef.h` | 所有消息类型枚举（MsgType）+ **对接 PVP 用的数据结构**（见下方"对接接口"） |
| `Serializer.h` | struct → QByteArray（大小端转换） |
| `Deserializer.h` | QByteArray → struct（校验 + 解析） |

**消息类型一览（MsgType 枚举）：**

| 消息 | ID | 方向 | 说明 |
|:---|:---:|:---:|:---|
| `JOIN_ROOM` | 0x01 | C→S | Client 加入房间（携带昵称） |
| `JOIN_ACK` | 0x02 | S→C | Host 确认加入（携带房主昵称） |
| `PLAYER_READY` | 0x03 | 双向 | 玩家准备 |
| `PLAYER_UNREADY` | 0x04 | 双向 | 取消准备 |
| `GAME_START` | 0x05 | S→C | 游戏开始（携带 RNG 种子） |
| `SYNC_SEED` | 0x06 | S→C | 种子同步（可合并到 GAME_START） |
| `DEPLOY` | 0x10 | 双向 | 部署单位 |
| `RECALL_UNIT` | 0x11 | 双向 | 撤回单位 |
| `DEPLOYMENT_END` | 0x12 | C→S | 部署阶段结束 |
| `MOVE_UNIT` | 0x20 | 双向 | 移动单位 |
| `UPGRADE_UNIT` | 0x21 | 双向 | 升级单位 |
| `WAVE_START` | 0x22 | S→C | 波次开始（Host 广播 waveId） |
| `WAVE_COMPLETE` | 0x23 | 双向 | 波次完成 |
| `CLASH_RESULT` | 0x24 | 双向 | 拼点踩死结果 |
| `UNIT_HP_SYNC` | 0x30 | S→C | 单位血量同步 |
| `CORE_HP_SYNC` | 0x31 | S→C | 基地血量同步 |
| `RESOURCE_SYNC` | 0x32 | S→C | 资源数量同步 |
| `MONSTER_KILLED` | 0x40 | 双向 | 怪物击杀 |
| `UNIT_DESTROYED` | 0x41 | 双向 | 单位销毁 |
| `GAME_OVER` | 0x50 | S→C | 游戏结束 |
| `ROUND_VALUE` | 0x60 | S→C | Host 广播值（测试用字符串） |
| `ROUND_ACK` | 0x61 | C→S | Client 确认本轮处理完毕 |
| `ROUND_COMPLETE` | 0x62 | S→C | 本轮完成（双方 ack 后广播） |
| `PING` | 0xFE | 双向 | 心跳请求（发 PING 的一方用） |
| `PONG` | 0xFD | 双向 | 心跳响应（收到 PING 后回 PONG） |
| `DISCONNECT` | 0xFF | 双向 | 主动断开 |

---

### 1.2 session/ — 连接管理层

| 文件 | 功能 |
|:---|:---|
| `GameServer.h/cpp` | Host 端：QTcpServer 监听 + 单客户端管理 + 粘包处理 |
| `GameClient.h/cpp` | Client 端：QTcpSocket 连接 + 粘包处理 |
| `LobbyManager.h/cpp` | 大厅握手流程（加入房间→双方准备→游戏开始） |
| `RoundManager.h/cpp` | 广播 + 双方确认机制（Host 广播→双方 ack→下一轮） |
| `NetworkManager.h/cpp` | 网络状态机（Disconnected/Connecting/Connected/Negotiating/Ready/**Reconnecting**/Error） |

---

### 1.3 sync/ — 同步逻辑层

| 文件 | 功能 |
|:---|:---|
| `RandomSynchronizer.h/cpp` | **RNG 种子同步**：Host 生成 seed → 广播 → 双方用相同种子初始化 std::mt19937 |
| `DeploymentSync.h/cpp` | **迷雾部署同步**：Host 分配 unitId，DEPLOYMENT_END 后统一同步 |
| `ClashSync.h/cpp` | **拼点踩死同步**（框架已搭，逻辑待填） |
| `StateValidator.h/cpp` | **状态校验**（框架已搭，逻辑待填） |

---

### 1.4 测试程序（net_test/）

**可独立运行，无需 UI / core 模块（v2 版本）：**

```
net_test/
├── main.cpp              # 完整流程测试脚本（v2）
├── CMakeLists.txt        # 构建配置
└── build/                # 编译产物（Qt Creator Build 后生成）
    └── Qt_6_11_0_for_macOS-Debug/
        └── NetworkTest   # 可执行文件
```

**v2 升级内容**：
- WAVE_START 改用 `WaveStartPayload` 二进制格式（4 字节）
- `Serializer::serialize()` / `Deserializer::decode()` 完整实现
- PING/PONG 心跳逻辑修复（收到 PING → 回 PONG；收到 PONG → 重置计时器）
- 新增 `Reconnecting` 状态 + 断线重连功能
- Lobby 大厅消息辅助函数（`buildJoinRoom` / `buildJoinAck` / `buildGameStart` / `buildSyncSeed`）

---

## 二、核心设计思路

### 2.1 架构分层

```
UI / Core（业务逻辑层）
        ↓ 调用 / ↑ 监听信号
LobbyManager / RoundManager（业务管理层）
        ↓ 发包 / ↑ 收包
NetworkManager（状态机）
        ↓
GameServer / GameClient（连接管理层）
        ↓
QTcpSocket / QTcpServer（Qt 网络层）
```

**关键约束**：
- **网络层不碰 UI**：所有对外接口都是 Qt 信号，网络模块不知道任何 UI 存在
- **业务层不碰 Socket**：业务层只调用 `sendXXX()` 和监听 `xxxReceived` 信号
- 解耦方式：Qt 信号槽（`QObject::connect`）

### 2.2 怪物波次同步方案（种子同步）

```
Host 生成 seed  →  GAME_START 包发给 Client
                        ↓
            双方都用同一个 seed 初始化 std::mt19937

每一波：Host 只广播 waveId（如 waveId=3）
            ↓
双方各自调用 rng().randomInt() 生成完全相同的怪物序列
```

**不需要传输怪物列表**，只传波号 + 种子。`RandomSynchronizer` 类负责这件事。

### 2.3 迷雾部署

```
部署阶段：双方各自调用 addLocalDeploy()，数据不互通
DEPLOYMENT_END：Host 的 DeploymentSync 收集全部部署信息，统一分配 unitId，广播给 Client
```

---

## 三、测试脚本使用方法

### 3.1 编译（在 Qt Creator 中）

1. 打开 Qt Creator
2. 打开项目 → 选择 `net_test/CMakeLists.txt`
3. Kit 选 `Desktop Qt 6.11.0 ...`（macOS ARM）
4. Build（Ctrl+B）
5. 可执行文件路径：
   ```
   net_test/build/Qt_6_11_0_for_macOS-Debug/NetworkTest
   ```

### 3.2 运行（需要两个终端）

```bash
# 终端 A — 先开（Host 端）
cd /Users/wangzihan/Desktop/Study/C++/game/net_test/build/Qt_6_11_0_for_macOS-Debug
./NetworkTest server 房主

# 终端 B — 后开（Client 端）
cd /Users/wangzihan/Desktop/Study/C++/game/net_test/build/Qt_6_11_0_for_macOS-Debug
./NetworkTest client 玩家2 127.0.0.1
```

### 3.3 期望输出

```
[自动流程]
1. Client 连接成功，发送 JOIN_ROOM
2. Host 回复 JOIN_ACK
3. ~1秒后双方自动点"准备"（PLAYER_READY）
4. Host 检测双方都 ready → 广播 GAME_START（含种子）
5. Host 广播第 1 轮 WAVE_START（WaveStartPayload 二进制，4字节）
6. 双方用种子 + waveId 独立生成相同怪物序列
7. 双方处理后各自 localAck()
8. Host 收到双方 ack → 广播 ROUND_COMPLETE → 下一波
9. 无限循环下一波...
10. 心跳：每 2 秒 PING/PONG 自动保活
```

### 3.4 局域网测试（两台电脑）

Host 端会打印本机 IP（局域网 IP），Client 端连接时填那个 IP 即可。

---

## 四、已定义的对接数据结构（ProtocolDef.h 末尾）

这些结构体与 PVP 组（Colewrld815-patch-1 分支）的 `ObjectType` 枚举对齐：

```cpp
// 怪物种类（对应 ObjectType）
enum class MonsterKind : quint8 {
    RES_BASIC=0, RES_FAST=1, RES_TANK=2,
    ATK_NORMAL=3, ATK_TANK=4, ATK_FAST=5,
    ATK_SAPPER=6, ATK_BERSERK=7, ATK_RANGED=8, ATK_REGEN=9
};

// 卡牌种类（对应 CardKind）
enum class CardKind : quint8 {
    ATTACK=0, PRODUCE=1, HEAL=2
};

// 波次开始（Host 广播，4 字节）
struct WaveStartPayload {
    quint8  waveId;       // 波次编号（从 1 开始）
    quint8  reserved[3];   // 保留，填 0
};

// 部署消息（4 字节）
struct DeployPayload {
    quint8 cardKind;  // CardKind
    quint8 row;       // 行（0-based）
    quint8 col;       // 列（0-based）
    quint8 unitId;    // Host 分配的 ID（迷雾同步时用）
};

// 升级消息（2 字节）
struct UpgradePayload {
    quint8 unitId;      // 单位 ID
    quint8 targetLevel; // 目标等级（1~3）
};

// 撤回消息（1 字节）
struct RecallPayload {
    quint8 unitId;
};

// 基地血量同步（4 字节）
struct CoreHpPayload {
    quint32 hp;  // PlayerState::baseHealth
};

// 资源同步（4 字节）
struct ResourcePayload {
    quint32 amount;  // PlayerState::currentResources
};
```

---

## 五、各模块需要做的事情

### 5.1 PVP 组需对接的接口（已简化）

**设计原则**：确定性模拟 + 输入同步。核心假设：
- 双方持有相同 RNG 种子，模拟结果天然一致
- 只需同步**初始状态**（部署）和**玩家操作**（输入），过程结果不广播

| 对接项 | 网络层提供 | PVP 组需要做的 |
|:---|:---|:---|
| **升级** | `sendPacket(UPGRADE_UNIT, payload)` | 玩家主动点时调用 |
| **部署同步** | `DEPLOYMENT_END` 后 Host 广播完整部署 | 迷雾结束触发 |
| **波次触发** | `WAVE_START(waveId)` | PVP 收到 waveId 独立刷怪 |

**不在同步范围内的**（由模拟确定性保证）：
- 基地血量变化、怪物死亡、胜负判定——两边用相同逻辑算出相同结果，不需要网络通知

### 5.2 网络层待完成的工作

| 任务 | 优先级 | 说明 |
|:---|:---:|:---|
| Serializer/Deserializer 实现 | ✅ 完成 | v2：所有 struct 的 serialize/decode 方法已实现 |
| `RoundManager` 正式版替换 | ✅ 完成 | v2：WAVE_START 改用 `WaveStartPayload` 二进制格式 |
| `ClashSync` 拼点逻辑 | 🟡 中 | 框架已搭，需要填入拼点公式和冲突处理 |
| `StateValidator` 状态校验 | 🟡 中 | 防止客户端作弊，当前只有框架 |
| 断线重连 | ✅ 完成 | v2：`Reconnecting` 状态 + `reconnect()` 方法已实现 |
| PING 心跳保活 | ✅ 完成 | v2：Server 发 PING，Client 回 PONG，超时判定断线 |

### 5.3 UI 层对接（交给 UI 组）

网络层暴露给 UI 的接口（Qt 信号）：

```cpp
// LobbyManager 发出的信号（UI 连接这些）
void peerJoined(const QString& name);     // 对方加入了
void peerReady();                        // 对方准备好了
void peerCancelled();                    // 对方取消准备
void gameStarted(quint32 seed);          // 游戏正式开始
void stateChanged(LobbyState newState);   // 大厅状态变化

// RoundManager 发出的信号
void valueReceived(const QByteArray& value);  // 收到本轮数据
void roundComplete();                         // 本轮完成

// NetworkManager 发出的信号
void connectionLost();   // 连接断开
void errorOccurred(const QString& msg);  // 网络错误
```

UI 层需要做的：
1. 大厅页面连 LobbyManager 的信号/槽
2. 战斗页面连 RoundManager 的信号
3. 玩家操作（点部署/升级/撤回）→ 调用 LobbyManager/RoundManager 的公共接口

### 5.4 数据管理层（data_manager）

目前完全未对接。需要约定：
- `level_config.txt` 中的怪物配置是否需要传给网络层？
- 地图数据（`map_data.csv`）是否需要同步？

---

## 六、文件结构

```
network/
├── CMakeLists.txt                        # 编译 game_network 静态库
├── include/network/
│   ├── NetworkModule.h                  # 汇总头文件（#include 所有子模块）
│   ├── protocol/
│   │   ├── Packet.h                     # 包头结构 + 粘包边界
│   │   ├── ProtocolDef.h                # 消息枚举 + 对接数据结构
│   │   ├── Serializer.h                 # struct → QByteArray
│   │   └── Deserializer.h               # QByteArray → struct
│   ├── session/
│   │   ├── GameServer.h/cpp             # Host 连接管理
│   │   ├── GameClient.h/cpp            # Client 连接管理
│   │   ├── LobbyManager.h/cpp          # 大厅握手
│   │   ├── RoundManager.h/cpp          # 广播+确认机制
│   │   ├── NetworkManager.h/cpp         # 网络状态机
│   │   └── NetworkState.h               # 状态枚举
│   └── sync/
│       ├── RandomSynchronizer.h/cpp     # RNG 种子同步
│       ├── DeploymentSync.h/cpp         # 迷雾部署同步
│       ├── ClashSync.h/cpp              # 拼点同步
│       └── StateValidator.h/cpp         # 状态校验

net_test/                                # 独立测试程序
├── CMakeLists.txt
├── main.cpp                             # 完整流程测试
└── build/                               # Qt Creator build 产物
```

---

## 七、踩坑记录

| 问题 | 原因 | 解决方案 |
|:---|:---|:---|
| `readyRead` 触发但数据不完整 | TCP 粘包，不能在 slot 里直接 parse | 用 `m_buffer` 缓冲，按 `bodyLen` 判断完整性再处理 |
| 改了 `ProtocolDef.h` 后测试没变化 | Qt Creator 没有重新 Build | 修改协议头文件后必须 Rebuild（Ctrl+Shift+B） |
| 怪物两边不一致 | 双方 RNG 种子不同步 | GAME_START 时 Host 生成 seed 广播，两边都用同一 seed 初始化 |
| `QByteArray::push_back(3, '\0')` 编译报错 | `QByteArray::push_back(char)` 不接受双参数 | 改用 `append(count, value)` |

---

*本文档随开发进度持续更新。对接接口确认后请同步修改 ProtocolDef.h 并通知全组。*
