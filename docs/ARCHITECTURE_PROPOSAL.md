# 项目结构重构建议

## 结论

推荐参考 `include/` + `src/` 分离，但不要现在一次性全量搬迁。

当前代码已经能编译、联机流程也在持续修复。此时最重要的是先稳定功能，再逐步拆模块。一次性把 `src/core`、`src/ui`、`src/network` 全部移动到独立库，会同时影响 CMake、Qt AutoMoc、include 路径、网络协议、页面跳转和构建缓存，风险较高。

## 当前更适合的目标结构

先以“最少破坏”为原则，最终可以演进成：

```text
GameProject/
├── CMakeLists.txt
├── README.md
├── docs/
├── data/
├── assets/
├── resources/
│
├── app/
│   ├── CMakeLists.txt
│   └── src/
│       └── main.cpp
│
├── core/
│   ├── CMakeLists.txt
│   ├── include/core/
│   └── src/
│
├── ui/
│   ├── CMakeLists.txt
│   ├── include/ui/
│   └── src/
│
├── network/
│   ├── CMakeLists.txt
│   ├── include/network/
│   └── src/
│
└── data_manager/
    ├── CMakeLists.txt
    ├── include/data_manager/
    └── src/
```

## 和你给的方案的差异

你给的方案方向是对的，但对当前项目来说有几点需要调整：

1. `core` 目前还不是纯 C++。
   - `BattleManager.cpp`、`SkillSystem.cpp`、`AttackUnit.cpp`、`WaveSpawner.cpp` 里还用了 `QDebug`。
   - 如果要让 `core` 真正不依赖 Qt，需要先把调试日志换成标准日志接口或宏。

2. `app` 层现在还没形成。
   - 当前页面流主要在 `MainWindow`、`LobbyPage`、`BattlePage`、`DeployPage` 里。
   - 可以先不急着做 `GameApplication/GameController`，否则会大改 UI 流程。

3. `ui` 当前页面偏大。
   - `BattlePage.cpp` 同时负责页面、绘制、输入、网络快照解析。
   - 已先把 `BATTLE_STATE` 编码/解码/checksum 抽到 `BattleStateCodec`，后续更应该继续拆 `BattleView`、`EffectRenderer`、`RadialMenuWidget`，再迁移目录。

4. `network` 可以较早拆。
   - 网络层已经有 `session/protocol/sync` 目录，边界相对清晰。
   - 适合作为第一批变成静态库的模块。

5. `data_manager` 目前还没真正接入主流程。
   - 可以先定义地图 JSON/CSV 格式和加载器，再接入游戏。

## 推荐迁移顺序

### 第一阶段：只整理文档和忽略规则

目标：不破坏功能。

- 保留当前 `src/` 结构。
- 清理根目录旧工程、备份文件、IDE 文件。
- 写清楚未来目标结构。
- 编译通过后再提交。

当前已经完成到这一阶段。

### 第二阶段：拆出 `network` 静态库

目标：低风险模块化。

- 当前已完成一小步：`BattleStateCodec` 已进入 `src/network/protocol`，`BattlePage` 不再直接维护战斗快照协议。
- 移动 `src/network` 到：
  - `network/include/network`
  - `network/src`
- 顶层 `CMakeLists.txt` 改成 `add_subdirectory(network)`。
- `app` 或主程序链接 `game_network`。

### 第三阶段：拆出 `core` 静态库

目标：核心逻辑独立。

- 先去掉 core 里的 Qt 日志依赖。
- 移动 `src/core` 到：
  - `core/include/core`
  - `core/src`
- 生成 `game_core` 静态库。
- `ui`、`app`、`network` 只通过公开头文件访问 core。

### 第四阶段：拆 UI 大文件

目标：降低 UI 维护成本。

- 从 `BattlePage` 中拆出：
  - `BattleView`
  - `EffectRenderer`
  - `MapRenderer`
  - `EntityRenderer`
  - `BattleInputHandler`
- 从 `DeployPage` 中拆出：
  - `DeployView`
  - `RadialMenuWidget`
- 再迁移到 `ui/include/ui` 和 `ui/src`。

### 第五阶段：补 `app` 控制层

目标：页面不直接承担全局控制职责。

- 引入：
  - `GameSession`
  - `NavigationController`
  - `GameController`
  - `GameLoop`
- `MainWindow` 只管理页面容器，不直接处理大量业务逻辑。

## 当前不要做的事

- 不要现在把所有 `.h` 一次性搬到 `include/`。
- 不要现在把 `BattlePage.cpp` 拆成十几个文件。
- 不要在 `main` 分支做大规模目录迁移。
- 不要边修联机逻辑边改 CMake 模块化，否则出问题很难定位。

## 已完成的低风险解耦

- `BATTLE_STATE` 序列化、反序列化、checksum 已移动到 `src/network/protocol/BattleStateCodec.*`。
- `BattlePage` 只负责发送/接收和页面状态更新，不再直接维护快照包体格式。
- 网络模块无 `Q_OBJECT` 的 `.cpp` 已移除手写 `.moc` include，减少构建噪音。

## 给组员的统一说法

目前可以告诉组员：

> 目录结构后面会参考 `include/ + src/` 分离，但不会现在一次性大搬迁。现在先保证联机功能稳定，短期只清理根目录和文档；后续按 `network -> core -> ui -> app` 的顺序逐步拆模块。这样能保证每一步都能编译、能测试、能回滚。
