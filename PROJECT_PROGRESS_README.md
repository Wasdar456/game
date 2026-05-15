# 塔防对战游戏项目进度总览

> 用途：快速查看当前各 git 分支、各模块已经完成了什么、还缺什么、下一步应该做什么。
>
> 当前检查分支：`dev`  
> 检查时间：2026-05-13  
> 检查方式：阅读当前工作区源码、README、CMake 配置，并对比本地 git 分支文件内容。

---

## 一句话结论

当前项目完成度最高的是 `feature/network-module` 对应的 PVP 网络模块；`Colewrld815-patch-1` 中有一套旧版核心战斗逻辑雏形；`logic/function1` 提供了正式 `core/` 模块目录骨架但文件内容为空；`main` 主要是完整设计文档和顶层 CMake 规划；当前 `dev` 分支把这些内容合到了一起，但还没有形成一个可完整运行的游戏工程。

---

## Git 分支进度

| 分支 | 当前内容 | 完成度判断 | 主要产物 | 主要问题 |
|---|---|---:|---|---|
| `main` | 项目总设计文档、顶层 `CMakeLists.txt`、战斗界面示意图 | 文档完整，代码未开始 | `README.md`、`CMakeLists.txt`、`image-3.png` | 顶层 CMake 引用了尚不存在的 `data_manager/ui/resources/app` |
| `logic` | 正式 `core/` 目录骨架 | 骨架完成，逻辑未实现 | `core/CMakeLists.txt`、`core/include/core/...`、`core/src/...` | 所有 `core` 头文件和源文件均为 0 行 |
| `function1` | 与 `logic` 指向同一提交 | 同 `logic` | 同 `logic` | 同 `logic` |
| `Colewrld815-patch-1` | 旧版核心逻辑雏形 | 部分实现 | `GameObject.h`、`Entity.h`、`Card.h/cpp`、`Monster*.h`、`MapGrid.h`、`PlayerState.h` | 未并入正式 `core/`；注释编码乱码；部分怪物逻辑仍是 TODO |
| `feature/network-module` | PVP 网络模块和独立测试程序 | 基本可对接 | `network/`、`net_test/`、网络 README、实验流程 | 与真实 core/UI 尚未集成；个别协议实现需要修正 |
| `dev` | 当前整合分支，合并了核心雏形、core 骨架、网络模块和文档 | 集成中 | 以上内容的合集 | 模块之间尚未真正打通，顶层工程暂不可完整构建 |

---

## 模块进度总表

| 模块 | 路径 | 状态 | 已完成 | 待完成 |
|---|---|---|---|---|
| 总设计文档 | `README.md` | 基本完成 | UI 流程、核心架构、PVP 机制、数据格式、分工、阶段计划 | 需要随真实代码进度更新勾选状态 |
| 旧版核心逻辑 | 根目录 `GameObject.h` 等 | 部分完成 | 对象、实体、卡牌、怪物、地图格、玩家状态雏形 | 迁移到 `core/`，修编码，补测试，补完整战斗循环 |
| 正式核心模块 | `core/` | 仅骨架 | 目录结构和 CMake 静态库目标 | 所有头/源文件需要实现 |
| 网络模块 | `network/` | 基本完成 | 协议、连接、心跳、大厅、轮次、RNG、部署、拼点、状态校验 | 修小问题，与 core/UI 对接 |
| 网络测试 | `net_test/` | 已写好 | Host/Client 双终端流程测试脚本 | 需要在配置好 Qt 后实际编译运行 |
| UI 模块 | `ui/` | 未开始 | README 中已规划页面和控件 | 创建目录、页面、战斗视图、交互控件 |
| 数据管理 | `data_manager/` | 未开始 | README 中已规划数据格式 | 实现地图、关卡、卡牌、怪物配置和存档读写 |
| 资源模块 | `resources/`、`assets/` | 未开始 | README 中已规划资源清单 | 建立 qrc、导入图片/音效/字体 |
| 应用集成 | `app/` | 未开始 | README 中已规划控制器和游戏循环 | 创建程序入口、页面导航、core/network/data/ui 装配 |
| 构建系统 | `CMakeLists.txt` | 部分完成 | 顶层和 `core/network/net_test` CMake 已存在 | 补缺失子目录，配置 Qt 路径，修完整构建 |

---

## 已完成内容详表

### 1. 项目文档与总体设计

路径：`README.md`、`game/README.md`

已完成：

- 设计了游戏整体定位：塔防对战游戏。
- 设计了 UI 页面流：起始页、PVE 配置、PVP 大厅、战前选卡、战斗页面、设置、结算。
- 设计了核心类体系：`GameObject`、`Entity`、`Card`、`Monster`、`MapGrid`。
- 设计了卡牌类型：攻击型、生产型、治疗型。
- 设计了怪物类型：资源怪、普通攻击怪、坦克怪、快速怪、工兵、狂暴怪、远程怪、回血怪。
- 设计了地图机制：路径、高台、平地、不可部署区、核心、出生点。
- 设计了战斗机制：自动技能、索敌优先级、高低差、瞬移移动、资源消耗、升级、撤回。
- 设计了 PVP 机制：资源期、拉锯期、决战期、迷雾部署、随机种子同步、拼点踩死。
- 设计了数据格式：`level_config.txt`、`user_profile.dat`、`map_data.csv`。
- 设计了最终多模块目录结构：`core`、`network`、`ui`、`data_manager`、`resources`、`app`。

未完成：

- README 中阶段计划还是静态规划，未同步真实代码完成状态。
- `game/README.md` 与根目录 `README.md` 内容重复，后续可以合并或保留一个权威版本。

---

### 2. 旧版核心逻辑雏形

路径：项目根目录下的 `GameObject.h`、`Entity.h`、`Card.h`、`Card.cpp`、`Card_SubTypes.h`、`Monster.h`、`Monster_Attack.h`、`Monster_Resource.h`、`MapGrid.h`、`PlayerState.h`

已完成：

- `ObjectType` 枚举：定义了卡牌、资源怪、攻击怪等对象类型。
- `GameObject`：包含唯一 ID、地图坐标、对象类型、`update()`、`draw()` 接口。
- `Entity`：包含 HP、最大 HP、攻击力、受伤、治疗、死亡判断、血量百分比。
- `PlayerState`：单例形式保存当前资源和基地血量，支持资源增加、资源消耗、基地扣血。
- `MapGrid`：保存地形类型、占用状态、高度值、占用实体指针。
- `Card`：实现等级、攻击范围、移动限制、技能冷却、索敌优先级。
- `Card::findTarget()`：实现范围筛选、高台射程 +1、优先级排序、距离排序、血量排序、ID 兜底排序。
- `Card::tryTeleport()`：实现瞬移距离检查、地图边界检查、地块合法性检查、占用检查、移动资源扣除、占用状态更新。
- `AttackUnit`：实现攻击单位的自动索敌、攻击、低打高伤害降低。
- `ProduceUnit`：实现定时产出资源。
- `HealUnit`：实现治疗友方单位，优先治疗低血量百分比和距离近的单位。
- `Monster`：实现路径数组、移动速度、精确坐标、沿路径移动、到达终点扣基地血。
- 攻击怪类型：普通怪、坦克怪、快速怪、工兵、狂暴怪、远程怪、回血怪。
- 资源怪类型：普通资源怪、快速资源怪、坦克资源怪。

未完成：

- 这套代码没有接入正式 `core/` 目录和 `game_core` 静态库。
- 注释存在乱码，建议统一转成 UTF-8 并清理。
- `AttackUnit` 依赖全局变量 `g_AllMonsters`、`g_AllCards`、`g_Map`，后续需要改成系统管理器持有状态。
- `Card` 中 `tryTeleport()` 在头文件声明为纯虚函数，但 `Card.cpp` 又提供了实现，需要调整设计。
- 部分怪物逻辑还没有实现：
  - 工兵怪寻找高台单位是 TODO。
  - 远程怪范围攻击是 TODO。
  - 多数 `onDeath()` 只是空实现或注释。
- 没有完整战斗循环、波次系统、A* 寻路、胜负结算、对象生命周期管理。

---

### 3. 正式 core 模块

路径：`core/`

已完成：

- 已建立正式模块目录：
  - `core/include/core/base`
  - `core/include/core/units`
  - `core/include/core/map`
  - `core/include/core/combat`
  - `core/include/core/systems`
  - `core/include/core/snapshot`
  - `core/src/...`
- 已写好 `core/CMakeLists.txt`，目标名为 `game_core`，并提供别名 `GameProject::core`。
- CMake 已列出预期文件和模块边界。

未完成：

- 当前 `core/include` 和 `core/src` 下所有头文件、源文件均为空文件。
- 未实现 `CoreTypes`、`Constants`、`GameObject`、`Entity`。
- 未实现 `Card`、`AttackUnit`、`ProduceUnit`、`HealUnit`、`Monster`。
- 未实现 `Map`、`MapGrid`、`MapPosition`、`AStar`。
- 未实现 `DamageCalculator`、`TargetSelector`、`Projectile`、`Buff`、`BuffManager`。
- 未实现 `BattleManager`、`CardSystem`、`ResourceManager`、`SkillSystem`、`WaveSpawner`。
- 未实现 UI 所需的 `BattleSnapshot`、`MapSnapshot`、`UnitSnapshot`、`MonsterSnapshot`。

建议下一步：

1. 先把根目录旧版核心逻辑迁移进 `core/include/core/...` 和 `core/src/...`。
2. 去掉全局变量依赖，改由 `BattleManager` 或上下文对象统一管理。
3. 先跑通一个命令行 PVE 最小闭环：部署单位、刷怪、移动、攻击、扣基地血。

---

### 4. network 网络模块

路径：`network/`

已完成：

- `ProtocolDef.h`
  - 定义了所有消息类型：大厅、部署、战斗、状态同步、系统心跳。
  - 定义了 PVP 对接结构：`WaveStartPayload`、`DeployPayload`、`UpgradePayload`、`RecallPayload`、`CoreHpPayload`、`ResourcePayload`。
  - 定义了 `MonsterKind`、`CardKind`，用于和核心对象类型对齐。
- `Packet.h`
  - 定义固定 3 字节包头：`msgType[1] + bodyLen[2]`。
- `Serializer.h`
  - 实现基础类型编码：`uint8/uint16/uint32/int32/string`。
  - 实现业务结构体序列化。
  - 提供大厅消息构造函数。
- `Deserializer.h`
  - 实现基础类型解码。
  - 实现包头解析。
  - 实现业务结构体反序列化。
- `GameServer`
  - Host 端监听端口。
  - 只支持单客户端，符合 1v1 设计。
  - 实现收包缓冲和粘包拆包。
  - 实现 PING/PONG 心跳和超时断线。
- `GameClient`
  - Client 端连接 Host。
  - 实现连接超时。
  - 实现收包缓冲和粘包拆包。
  - 实现 PING/PONG 心跳和超时断线。
- `LobbyManager`
  - 实现 JOIN_ROOM、JOIN_ACK。
  - 实现双方准备、取消准备。
  - Host 在双方准备后生成随机种子并广播 `GAME_START`。
  - 双方通过 `gameStarted(seed)` 进入游戏。
- `RoundManager`
  - Host 广播本轮数据。
  - Host 和 Client 各自处理后 ack。
  - 双方 ack 后 Host 广播 `ROUND_COMPLETE`。
- `NetworkManager`
  - 封装 Host/Client 模式。
  - 支持创建房间、加入房间、断开、重连、本机 IP 获取。
- `RandomSynchronizer`
  - Host 生成种子并同步。
  - Client 应用种子。
  - 提供 `std::mt19937` 和 `randomInt()`。
- `DeploymentSync`
  - 保存本地部署。
  - 分配 unitId。
  - 序列化和解析部署列表。
- `ClashSync`
  - 实现拼点战斗值计算：攻击力 + 剩余血量百分比。
  - 实现拼点结果序列化和解析。
- `StateValidator`
  - 实现单位状态快照序列化、解析、差异对比。

待修正：

- `LobbyManager` 当前发送昵称时使用裸 UTF-8；`Serializer::encodeString()` 使用长度前缀，两者协议格式不统一。
- `ClashSync::serializeClashResult()` 实际写入 13 字节，但代码里 `reserve(9)`，`parseClashResult()` 也只检查 `< 9`，应改为 13。
- `ProtocolDef.h` 仍保留 `CORE_HP_SYNC`、`RESOURCE_SYNC` 等消息，但文档后续又说明确定性模拟下这些结果不需要同步，需要统一协议说明。
- 网络模块暂未链接 `game_core`，`network/CMakeLists.txt` 里也注明了 core 暂未实现。
- 还没有和真实 UI、真实战斗循环联调。

---

### 5. net_test 网络测试程序

路径：`net_test/`

已完成：

- `main.cpp` 提供完整的命令行测试流程。
- 支持 Host 模式：
  - 监听 `9527`。
  - 等待 Client 加入。
  - 自动准备。
  - 双方准备后发送 `GAME_START(seed)`。
  - 广播第一波 `WaveStartPayload`。
  - 双方 ack 后循环发送下一波。
- 支持 Client 模式：
  - 连接 Host。
  - 发送 JOIN_ROOM。
  - 收到 JOIN_ACK。
  - 自动准备。
  - 收到 GAME_START 后应用 seed。
  - 收到 WAVE_START 后模拟处理并 ack。
- 演示了 `Serializer::serialize(WaveStartPayload)` 和 `Deserializer::decode(WaveStartPayload)`。

运行方式：

```bash
./NetworkTest server 房主
./NetworkTest client 玩家2 127.0.0.1
```

待完成：

- 当前环境下 CMake 找不到 Qt6，需要配置 `CMAKE_PREFIX_PATH` 或 `Qt6_DIR` 后再编译。
- `net_test/CMakeLists.txt` 中写死了某个用户机器上的 Qt 路径，建议改成可配置方式。

---

### 6. UI 模块

路径：计划中的 `ui/`

状态：未开始。

README 中已规划：

- `MainWindow`
- 起始页面 `StartPage`
- 模式选择 `ModePage`
- PVE 配置 `PveConfigPage`
- PVP 大厅 `PvpLobbyPage`
- 战前选卡 `DeckPage`
- 战斗页 `BattlePage`
- 结算页 `ResultPage`
- 设置页 `SettingsPage`
- 环形菜单 `RadialMenuWidget`
- 顶部状态栏 `StatusBarWidget`
- 底部卡牌栏 `BattleCardBarWidget`
- 地图/实体/特效/高亮渲染器

待完成：

- 创建 `ui/` 目录和 CMake。
- 实现基础页面切换。
- 先接入 `LobbyManager` 跑通 PVP 大厅。
- 再接入 core 快照渲染战斗画面。

---

### 7. data_manager 数据管理模块

路径：计划中的 `data_manager/`

状态：未开始。

README 中已规划：

- `MapLoader`：解析地图 CSV。
- `LevelLoader`：解析波次配置。
- `CardConfigLoader`：读取卡牌配置。
- `MonsterConfigLoader`：读取怪物配置。
- `UserProfileManager`：读写用户存档。
- `SettingsStorage`：保存设置。
- `DataHub`：统一访问静态配置。

待完成：

- 创建目录和 CMake。
- 确定真实配置文件格式。
- 提供 core 可调用的数据结构。
- 补充 `data/maps`、`data/levels`、`data/configs`、`data/save`。

---

### 8. resources / assets 资源模块

路径：计划中的 `resources/`、`assets/`

状态：未开始。

README 中已规划：

- `resources.qrc`
- 卡牌图片
- 怪物图片
- 地形图片
- 特效图片
- UI 图片
- BGM 和音效
- 字体

待完成：

- 创建资源目录。
- 统一命名规范和图片尺寸。
- 建立 Qt 资源文件。
- UI 接入资源加载。

---

### 9. app 应用集成模块

路径：计划中的 `app/`

状态：未开始。

README 中已规划：

- `main.cpp`
- `GameApplication`
- `GameSession`
- `GameController`
- `NavigationController`
- `GameLoop`
- `ViewModelMapper`

待完成：

- 创建程序入口。
- 装配 `core`、`network`、`data_manager`、`ui`。
- 用 `QTimer` 驱动游戏循环。
- 将 core 快照转换为 UI ViewModel。
- 处理 PVE/PVP 模式差异。

---

## 构建状态

当前顶层构建状态：不可完整构建。

已确认的问题：

1. 当前环境 CMake 找不到 Qt6：

```text
Could not find a package configuration file provided by "Qt6"
```

2. 顶层 `CMakeLists.txt` 引用了尚不存在的目录：

```cmake
add_subdirectory(data_manager)
add_subdirectory(ui)
add_subdirectory(resources)
add_subdirectory(app)
```

3. `core/` 虽然有 CMake，但源码为空，无法提供真实核心功能。

4. `network/` 可以作为独立模块继续推进，但需要配置 Qt6。

---

## 推荐下一步路线

### 第一优先级：整理 core

目标：让 `core/` 不再是空壳。

建议动作：

1. 把根目录旧版 `GameObject/Entity/Card/Monster/MapGrid/PlayerState` 迁移进正式 `core/`。
2. 修复乱码注释。
3. 去掉全局变量，建立 `BattleManager` 管理实体、地图、资源。
4. 补一个最小单元测试或命令行 demo。

### 第二优先级：修网络小问题

目标：让网络模块接口更稳定。

建议动作：

1. 统一 Lobby 昵称序列化格式。
2. 修 `ClashSync` 的 13 字节长度检查。
3. 明确哪些同步消息保留，哪些由确定性模拟替代。
4. 把 `net_test/CMakeLists.txt` 的 Qt 路径改成可配置。

### 第三优先级：先做最小 UI

目标：先跑通页面，不追求美术。

建议动作：

1. 建立 `ui/` 和 `app/`。
2. 做一个 `MainWindow + PvpLobbyPage`。
3. 先接 `NetworkManager/LobbyManager`。
4. 再接战斗页占位画面。

### 第四优先级：补数据和资源

目标：把硬编码改成配置驱动。

建议动作：

1. 建立地图和波次配置。
2. 卡牌/怪物属性迁移到配置。
3. 建立资源索引和 qrc。

---

## 快速判断当前项目状态

| 问题 | 答案 |
|---|---|
| 是否有完整游戏可运行？ | 暂时没有 |
| 是否有可复用网络模块？ | 有，基本完整 |
| 是否有核心战斗代码？ | 有旧版雏形，但正式 `core/` 为空 |
| 是否有 UI？ | 没有实际代码 |
| 是否有数据读写？ | 没有实际代码 |
| 是否有资源系统？ | 没有实际代码 |
| 是否能直接顶层 CMake 构建？ | 当前不能 |
| 当前最应该做什么？ | 先把旧版核心逻辑迁移并完善到正式 `core/` |

