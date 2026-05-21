# 开发日志 & 协作指南

> 本文档记录 UI 层从零到当前的完整开发历程、架构决策、配色规范和踩坑记录，供后续修改与团队协作参考。

---

## 一、项目概况

| 项 | 值 |
|:---|:---|
| 项目名 | DffenseAndAttack（塔防对战游戏） |
| 技术栈 | Qt 6.11.0 + C++17 + CMake + MinGW 13.1 + Ninja |
| 目标平台 | Windows（当前开发机） |
| 构建产物 | `build/DffenseAndAttack.exe` |
| 命名空间 | 核心层 `game::core`，网络层 `game::network` |

---

## 二、构建与运行

### 构建命令

PowerShell 中执行（需设置 PATH 以找到 MinGW/CMake/Qt/Ninja）：

```powershell
$env:PATH = 'D:\Qt\Tools\mingw1310_64\bin;D:\Qt\Tools\CMake_64\bin;D:\Qt\6.11.0\mingw_64\bin;D:\Qt\Tools\Ninja;' + $env:PATH
cmake --build 'c:/Users/dfaf1234/Desktop/game/build' 2>&1
```

### 运行

```powershell
Start-Process 'c:/Users/dfaf1234/Desktop/game/build/DffenseAndAttack.exe'
```

### 注意事项

- MinGW/CMake 不在系统 PATH 中，**必须**在 PowerShell 中手动设置 PATH 后才能构建
- 如果新增了 `.h` / `.cpp` 文件，需要重新 `cmake ..` 让 GLOB_RECURSE 重新扫描
- AutoMoc 警告（GameClient/GameServer 等 `Q_OBJECT` 缺失）可忽略，不影响编译

---

## 三、架构设计

### 3.1 核心架构：快照驱动渲染

```
用户操作 → BattleManager 接口 → 核心逻辑更新
                                    ↓
                            BattleManager::snapshot()
                                    ↓
                            BattleSnapshot（只读）
                                    ↓
                            BattleView::updateFromSnapshot() → QPainter 渲染
```

- **UI 层不直接操作游戏实体**，所有渲染数据来自 `BattleSnapshot`
- 用户操作（部署/升级/移动/撤回）通过 `BattleManager` 的公开接口完成
- 每帧由 `QTimer(16ms)` 触发 `onGameTick()`，推进逻辑 → 获取快照 → 渲染

### 3.2 页面导航

```
StartPage → LobbyPage(PVE) → DeckPage → BattlePage
         → LobbyPage(PVP) → DeckPage → BattlePage
         → DeckPage(图鉴)
         → SettingsPage
```

- `MainWindow` 使用 `QStackedWidget` 管理所有页面
- 页面间通过 Qt 信号槽通信，不直接持有对方引用
- `MainWindow` 持有唯一的 `BattleManager` 实例，子页面通过 `battleManager()` 获取

### 3.3 页面索引

| Index | 页面 | 头文件 | 说明 |
|:---:|:---|:---|:---|
| 0 | StartPage | `ui/StartPage.h` | 主菜单，含粒子动画背景 |
| 1 | LobbyPage | `ui/LobbyPage.h` | PVE配置/PVP大厅 |
| 2 | DeckPage | `ui/DeckPage.h` | 战前选卡 & 图鉴 |
| 3 | BattlePage | `ui/BattlePage.h` | 战斗主界面 |
| 4 | SettingsPage | `ui/SettingsPage.h` | 系统设置 |

---

## 四、UI 开发历程

### 阶段一：基础 UI 搭建

- 创建了所有 5 个页面（StartPage / LobbyPage / DeckPage / BattlePage / SettingsPage）
- `MainWindow` 用 `QStackedWidget` 管理页面切换，带淡入淡出动画
- BattlePage 内嵌 `BattleView`（自定义 QWidget），使用 QPainter 自绘地图/单位/怪物
- 底部操作栏 5 张卡牌按钮 + 暂停/加速/技能按钮
- 点击已部署单位弹出环形菜单（升级/移动/撤回）
- DeckPage 上半区卡池（QScrollArea + QGridLayout）+ 右侧详情面板，下半区 5 个出战卡槽

### 阶段二：蓝白科技风配色重构

原始配色为金色/暗紫色主题，存在以下问题：
- UI 元素仅 hover 时可见
- 字体颜色太暗，费眼
- 缺少 hover 动效

**配色方案**（参考明日方舟风格）：

| 用途 | 色值 | 说明 |
|:---|:---|:---|
| 页面背景 | `#0B1622` → `#162544` | 深蓝渐变 |
| 主强调色 | `#00D4FF` / `#00E5FF` | 青色，用于边框、高亮、核心信息 |
| 次强调色 | `#8AB4F8` | 浅蓝，用于辅助信息 |
| 正文文字 | `#FFFFFF` | 纯白，最大对比度 |
| 辅助文字 | `#E3F2FD` / `#7AB8DD` | 浅蓝白，次要信息 |
| 资源数字 | `#FFD54F` | 金色，资源/费用类 |
| 攻击型卡牌 | `#FF5252` | 红色主题 |
| 生产型卡牌 | `#00E676` | 绿色主题 |
| 治疗型卡牌 | `#448AFF` | 蓝色主题 |
| 退出/危险 | `#FF5252` | 红色 |

**关键改动**：
- 创建 `TechButton` 自定义按钮类，支持 `QPropertyAnimation` 驱动的 hover 发光/缩放效果
- 所有页面的 QPushButton 替换为 TechButton（StartPage 菜单按钮）
- BattlePage / DeckPage / LobbyPage / SettingsPage 全面重写 QSS 配色

### 阶段三：对比度与可见性修复

**核心问题**：半透明 alpha 背景在深色底上几乎看不见，必须 hover 才能看到 UI 元素。

**解决方案**：
1. **卡池卡牌**（DeckPage）：从 alpha 通道背景（如 `#FF525240`）改为**不透明实色背景**（如 `rgb(92,30,30)`），取主题色的暗色版本，确保默认状态就清晰可见
2. **空卡槽**：文字色 `#5580AA` → `#7AB8DD`，边框 `dashed 0.2` → `dashed 0.45`
3. **战斗页卡牌按钮**：背景 alpha 0.75 → 0.90，边框 1px → 2px
4. **状态栏/操作栏**：边框 1px → 2px，alpha 0.25 → 0.40
5. **LobbyPage 难度选项**：QRadioButton 增加实色背景 + 边框，文字始终纯白
6. **禁用状态按钮**：文字从 `#445` → `#7AACCC`，保持可读性

**经验教训**：深色背景上，**绝对不要用低 alpha 半透明色作为 UI 元素背景**，至少 80% opacity 或直接用不透明实色。

---

## 五、关键类与文件说明

### 5.1 TechButton（自定义按钮）

**文件**：`src/ui/TechButton.h` / `TechButton.cpp`

- 继承 `QPushButton`，支持 hover 动画（发光扩展 + 背景变亮）
- 通过 `QPropertyAnimation` 驱动 `hoverProgress` 属性（0.0 → 1.0）
- 默认状态：白色文字 + 青色边框 + 深色背景
- Hover：边框发光扩展、背景变亮、顶部高光线
- 可通过 `setAccentColor()` 自定义强调色（退出按钮用红色 `#FF5252`）

### 5.2 BattleView（战斗视口）

**文件**：`src/ui/BattlePage.h` / `BattlePage.cpp`（内嵌类）

- 自绘 QWidget，负责地图渲染和鼠标交互
- `CELL_SIZE = 48` 像素/格
- 交互模式：`NONE` / `DEPLOYING` / `MOVING` / `RADIAL_MENU`
- 绘制方法：`drawTerrain()` / `drawSpawnMarker()` / `drawCoreMarker()` / `drawHighlights()` / `drawUnits()` / `drawMonsters()` / `drawHoverCell()`
- 出生点（S）：旋转锥形渐变传送门效果
- 核心（C）：脉冲发光蓝白圆环
- 单位：渐变方块 + 血量条 + 选中发光光晕
- 怪物：红色渐变方块 + 血量条

### 5.3 BattlePage（战斗页面）

- 顶部状态栏：波次/核心HP/资源
- 中央：BattleView
- 底部操作栏：5张卡牌按钮 + 暂停/加速/技能
- 游戏主循环：`QTimer(16ms)` → `onGameTick()` → `BattleManager::update()` → `snapshot()` → 渲染
- 自动波次推进：场上无怪后 15 秒自动出下一波

### 5.4 DeckPage（选卡页面）

- 上半区：卡池（QScrollArea + QGridLayout，每行4张）+ 右侧详情面板（HTML 格式化）
- 下半区：5个出战卡槽 + 开始战斗按钮
- 卡池数据从 `Card` 派生类属性中提取（临时构造实例读取）
- 8 张卡牌：突击手/狙击手/AOE炮塔/特种兵（攻击）、采矿工/兵工厂（生产）、医生/重装医生（治疗）
- 点击卡池 → 填入第一个空槽位 + 显示详情；点击已填卡槽 → 移除

### 5.5 LobbyPage（大厅页面）

- PVE 模式：地图选择（QComboBox）+ 难度选择（QRadioButton 互斥）+ 确认按钮
- PVP 模式：创建房间（GameServer）+ 加入房间（IP输入 + GameClient）+ 状态日志
- 通过 `QStackedWidget` 在 PVE/PVP 面板间切换
- 网络模块按需初始化（`initNetwork()`）

### 5.6 StartPage（起始页面）

- 粒子动画背景（`ParticleWidget`，QPainter 绘制蓝白色粒子）
- 5 个 TechButton 菜单项：PVE / PVP / 图鉴 / 设置 / 退出
- 进入时播放渐入动画

### 5.7 SettingsPage（设置页面）

- 音量：BGM/音效滑块
- 显示：分辨率下拉 + 全屏复选框
- 游戏：显示网格 + 自动暂停复选框
- 保存按钮（QSettings 持久化待实现）

---

## 六、核心层对接说明

### BattleManager（核心入口）

UI 层通过 `MainWindow::battleManager()` 获取，主要接口：

| 方法 | 说明 |
|:---|:---|
| `deployCard(CardKind, MapPosition)` | 部署卡牌 |
| `upgradeCard(unitId)` | 升级单位 |
| `moveCard(unitId, MapPosition)` | 瞬移单位 |
| `recallCard(unitId)` | 撤回单位 |
| `startWave(waveId)` | 开始指定波次 |
| `setSpawnPoint(MapPosition)` | 设置出生点 |
| `setPath(vector<MapPosition>)` | 设置怪物路径 |
| `update(deltaSeconds)` | 推进游戏逻辑 |
| `snapshot()` | 获取只读快照 |

### BattleSnapshot（渲染数据）

```
BattleSnapshot
├── currentWave: int
├── baseHealth: int
├── resources: int
├── gameOver: bool
├── map: MapSnapshot
│   ├── rows, cols: int
│   └── grids: QVector<MapGridSnapshot>
│       ├── row, col, terrain, occupied, height
├── units: QVector<UnitSnapshot>
│   ├── id, row, col, level, hp, maxHp, range, moveLimit
└── monsters: QVector<MonsterSnapshot>
    ├── row, col, hp, maxHp
```

### CardKind 枚举

```cpp
enum class CardKind { Attack, Produce, Heal };
```

---

## 七、配色规范速查

### QSS 模板

**卡片按钮（默认可见 + hover 变亮）**：
```css
QPushButton {
  background: qlineargradient(x1:0,y1:0,x2:0,y2:1,
    stop:0 rgb(暗色版), stop:1 rgb(更暗色版));  /* 不透明实色！*/
  color: #FFFFFF;
  border: 2px solid <主题色>; border-radius: 10px;
  font-size: 13px; font-weight: bold;
}
QPushButton:hover {
  background: qlineargradient(x1:0,y1:0,x2:0,y2:1,
    stop:0 rgb(亮色版), stop:1 rgb(稍亮色版));
  border: 2px solid <主题色>;
}
```

**暗色背景上的文字**：始终用 `#FFFFFF`（纯白）或 `#E3F2FD`（浅蓝白），**不要用灰色**。

**边框**：深色背景上至少 `2px solid rgba(0,212,255,0.50)` 或更亮，1px 在暗底上几乎不可见。

**禁用状态**：文字至少 `#7AACCC`，不要用 `#445` 这种深灰。

---

## 八、已知问题 & TODO

| 优先级 | 问题 | 说明 |
|:---:|:---|:---|
| 高 | 设置页 QSettings 持久化未实现 | `m_btnSave` 点击后无实际保存 |
| 高 | BattlePage 地图数据硬编码 | `startBattle()` 中路径/地形是写死的，应从 CSV 加载 |
| 中 | 卡牌属性取值方式不优雅 | DeckPage 临时构造 Card 实例读取属性，应有统一数据表 |
| 中 | 战斗结束无结算页面 | `signalBattleEnd` 发出后无对应处理 |
| 低 | 粒子动画可优化 | StartPage 粒子数量和性能可调整 |
| 低 | 音效系统未接入 | SettingsPage 滑块无实际作用 |
| 低 | PVP 迷雾部署未实现 | 网络模块已搭好框架，但 UI 未实现部署阶段隐藏 |

---

## 九、目录结构（当前实际）

```
game/
├── CMakeLists.txt                  # CMake 构建配置
├── README.md                       # 原始项目设计文档
├── DEVLOG.md                       # 本文档
├── image-3.png                     # 战斗界面参考图
│
├── src/
│   ├── main.cpp                    # 程序入口
│   ├── PlayerState.h               # 玩家状态（遗留）
│   │
│   ├── ui/                         # UI 层
│   │   ├── MainWindow.h/.cpp       # 主窗口，页面导航
│   │   ├── StartPage.h/.cpp        # 起始页（粒子动画 + TechButton 菜单）
│   │   ├── LobbyPage.h/.cpp        # PVE配置 / PVP大厅
│   │   ├── DeckPage.h/.cpp         # 战前选卡 & 图鉴
│   │   ├── BattlePage.h/.cpp       # 战斗页面（含 BattleView 自绘视口）
│   │   ├── SettingsPage.h/.cpp     # 系统设置
│   │   └── TechButton.h/.cpp       # 自定义按钮（hover 动画）
│   │
│   ├── core/                       # 核心游戏逻辑
│   │   ├── base/                   # 基类 & 常量
│   │   │   ├── Constants.h         # 游戏常量
│   │   │   ├── CoreTypes.h         # 枚举定义
│   │   │   ├── Entity.h/.cpp       # 实体基类
│   │   │   └── GameObject.h/.cpp   # 对象基类
│   │   ├── combat/                 # 战斗系统
│   │   │   ├── Buff.h/.cpp         # Buff 效果
│   │   │   ├── BuffManager.h/.cpp  # Buff 管理
│   │   │   ├── DamageCalculator.h/.cpp  # 伤害计算
│   │   │   ├── Projectile.h/.cpp   # 投射物
│   │   │   └── TargetSelector.h/.cpp    # 索敌逻辑
│   │   ├── map/                    # 地图模块
│   │   │   ├── AStar.h/.cpp        # A* 寻路
│   │   │   ├── Map.h/.cpp          # 地图容器
│   │   │   ├── MapGrid.h/.cpp      # 地块类
│   │   │   └── MapPosition.h/.cpp  # 网格坐标
│   │   ├── snapshot/               # 快照（只读渲染数据）
│   │   │   ├── BattleSnapshot.h/.cpp
│   │   │   ├── MapSnapshot.h/.cpp
│   │   │   ├── MonsterSnapshot.h/.cpp
│   │   │   └── UnitSnapshot.h/.cpp
│   │   ├── systems/                # 管理器
│   │   │   ├── BattleManager.h/.cpp  # 战斗总管
│   │   │   ├── CardSystem.h/.cpp     # 卡牌系统
│   │   │   ├── ResourceManager.h/.cpp # 资源管理
│   │   │   ├── SkillSystem.h/.cpp     # 技能系统
│   │   │   └── WaveSpawner.h/.cpp     # 波次生成
│   │   └── units/                  # 单位实体
│   │       ├── Card.h/.cpp         # 卡牌基类
│   │       ├── AttackUnit.h/.cpp   # 攻击型
│   │       ├── ProduceUnit.h/.cpp  # 生产型
│   │       ├── HealUnit.h/.cpp     # 治疗型
│   │       ├── Monster.h/.cpp      # 怪物基类
│   │       └── MonsterTypes.h      # 怪物类型定义
│   │
│   └── network/                    # PVP 网络模块
│       ├── NetworkModule.h         # 模块入口
│       ├── protocol/               # 通信协议
│       │   ├── Packet.h            # 数据包定义
│       │   ├── ProtocolDef.h       # 协议常量
│       │   ├── Serializer.h        # 序列化
│       │   └── Deserializer.h      # 反序列化
│       ├── session/                # 会话管理
│       │   ├── GameServer.h/.cpp   # 服务端
│       │   ├── GameClient.h/.cpp   # 客户端
│       │   ├── LobbyManager.h/.cpp # 大厅状态机
│       │   ├── NetworkManager.h/.cpp # 网络管理
│       │   ├── NetworkState.h      # 网络状态枚举
│       │   └── RoundManager.h/.cpp # 回合管理
│       └── sync/                   # 同步模块
│
├── build/                          # 构建产物（gitignore）
└── .vscode/                        # VS Code 配置
```

---

*最后更新：2026-05-19*
