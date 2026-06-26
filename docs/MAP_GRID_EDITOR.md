# 地图网格标注工具

`tools/map_grid_editor.py` 用于编辑 `assets/maps/*.json`。游戏运行时会读取这些 JSON 来生成地图、部署区和怪物路线。

核心规则只有一条：怪物移动看 `routes.A/B[*].path`，不是看 `tiles` 里画了什么颜色。

## 启动方式

在项目根目录运行：

```bash
python3 tools/map_grid_editor.py --load assets/maps/lab_map_01.json
```

新建地图时可以指定底图、行列和默认输出：

```bash
python3 tools/map_grid_editor.py \
  --image assets/maps/lab_map_01.png \
  --rows 17 \
  --cols 28 \
  --cell-size 19 \
  --output assets/maps/lab_map_01.json
```

没有底图时也可以打开空白网格：

```bash
python3 tools/map_grid_editor.py --rows 17 --cols 28 --cell-size 19
```

## 坐标规则

JSON 使用 0 基坐标：

| 视觉行列 | JSON 坐标 |
| --- | --- |
| 第 1 行，第 1 列 | `{ "row": 0, "col": 0 }` |
| 第 10 行，第 17 列 | `{ "row": 9, "col": 16 }` |
| 第 8 行，第 25 列 | `{ "row": 7, "col": 24 }` |

编辑器右侧会显示鼠标当前格子的两套坐标：

```text
JSON: row=9, col=16 | 视觉: 第 10 行，第 17 列
```

写 JSON、看代码、查运行逻辑时用 `row/col`。和策划、美术沟通时通常说“第几行第几列”。

## routes 和 tiles

`routes` 是怪物实际移动路线。每只怪物出生时会拿到其中一条 path，然后按 path 的点逐格移动。

```json
"routes": {
  "A": [
    {
      "id": "A2",
      "path": [
        { "row": 9, "col": 16 },
        { "row": 8, "col": 16 },
        { "row": 7, "col": 16 },
        { "row": 7, "col": 17 }
      ]
    }
  ]
}
```

`tiles` 是地块标注。它决定格子在游戏里是路径、部署区、出生点、核心、高台或障碍，也决定编辑器里的颜色和 route label 显示。

```json
"tiles": [
  {
    "row": 9,
    "col": 16,
    "type": "PATH_A",
    "routeIndexA": 19,
    "routeLabelA": "A2:19"
  }
]
```

只把一个格子画成 `PATH_A` 不会改变怪物路线。必须在 `记录 A 路顺序` 或 `记录 B 路顺序` 模式下点击格子，或直接修改 `routes.A/B[*].path`。

## 基本操作

- `普通地块标注（只改 tiles）`：用来画部署区、障碍、高台、路径颜色等地块类型。
- `记录 A 路顺序（改怪物路线）`：按怪物移动顺序点击格子，写入 `routes.A`。
- `记录 B 路顺序（改怪物路线）`：按怪物移动顺序点击格子，写入 `routes.B`。
- `A路线号` / `B路线号`：选择当前正在编辑第几条路线。
- `新增 A 路线` / `新增 B 路线`：给多出生点地图增加独立路线。
- `清空当前 A/B 路线`：只清空当前路线顺序，不清空其他地块。
- `校验地图`：检查出生点、核心、路线连续性，以及 `tiles` 和 `routes` 是否不一致。
- `保存 JSON`：导出游戏可读取的地图配置。

左键点击会按当前模式绘制。左键拖拽只在普通地块标注模式下批量绘制。右键会擦除当前格子，并从路线中移除这个点。

## A2 拐弯示例

需求：A2 从视觉上的第 10 行第 17 列向上走两格，然后向右拐。

对应 JSON path 应该是：

```json
[
  { "row": 9, "col": 16 },
  { "row": 8, "col": 16 },
  { "row": 7, "col": 16 },
  { "row": 7, "col": 17 },
  { "row": 7, "col": 18 }
]
```

不要写成：

```json
[
  { "row": 9, "col": 17 },
  { "row": 8, "col": 17 },
  { "row": 7, "col": 17 }
]
```

这会让怪物在视觉上的第 18 列向上走。

## JSON 结构

地图 JSON 的主要字段：

```json
{
  "schemaVersion": 1,
  "name": "lab_map_01",
  "mode": "PVE",
  "image": "lab_map_01.png",
  "grid": {
    "rows": 17,
    "cols": 28,
    "cellSize": 19,
    "cellSizeX": 19,
    "cellSizeY": 19
  },
  "routes": {
    "A": [
      {
        "id": "A1",
        "spawn": { "row": 3, "col": 16 },
        "core": { "row": 7, "col": 24 },
        "path": [
          { "row": 3, "col": 16 },
          { "row": 4, "col": 16 }
        ]
      }
    ],
    "B": []
  },
  "points": {
    "SPAWN_A": [{ "row": 3, "col": 16 }],
    "SPAWN_B": [],
    "CORE_A": [{ "row": 7, "col": 24 }],
    "CORE_B": [],
    "RESOURCE": []
  },
  "tiles": [
    {
      "row": 3,
      "col": 16,
      "type": "SPAWN_A",
      "routeIndexA": 0,
      "routeLabelA": "A1:0"
    }
  ]
}
```

## 地块类型

| 类型 | 游戏含义 |
| --- | --- |
| `EMPTY` | 未标注，运行时通常不会导出 |
| `BLOCKED` | 障碍，不可走、不可部署 |
| `PATH_A` | A 路怪物路径地块 |
| `PATH_B` | B 路怪物路径地块 |
| `PATH_SHARED` | 公共路径地块 |
| `SPAWN_A` | A 路出生点 |
| `SPAWN_B` | B 路出生点 |
| `CORE_A` | A 方核心 |
| `CORE_B` | B 方核心 |
| `DEPLOY_A` | A 方可部署地块 |
| `DEPLOY_B` | B 方可部署地块 |
| `DEPLOY_NEUTRAL` | 双方/中立可部署地块 |
| `VISION_BLOCK` | 视野阻挡，当前主要是预留标注 |
| `HIGH_GROUND` | 高台，运行时会转成高度 1 |
| `RESOURCE` | 资源点，当前主要是预留标注 |

## 接入游戏的运行逻辑

单机 PVE 读取 `assets/maps/<mapId>.json`。读取成功后：

- `grid.rows` / `grid.cols` 重建地图尺寸。
- `tiles` 决定格子类型，未标注格子默认是 `NoDeploy`。
- `DEPLOY_A` / `DEPLOY_NEUTRAL` 会变成可部署普通地块。
- `PATH_A` / `PATH_SHARED` 会变成路径地块。
- `SPAWN_A` 会变成出生点。
- `CORE_A` 会变成核心。
- `routes.A` 会传给 `BattleManager::setPaths()`，刷怪时按多条路线分配。

如果 JSON 不存在或解析失败，游戏会回退到旧的硬编码测试地图。

## 常见问题

**为什么我把格子画成 `PATH_A`，怪物还是不走那里？**

因为你只改了 `tiles`。怪物移动只看 `routes.A/B[*].path`。切到 `记录 A 路顺序` 后按顺序点路径，或者直接改 path。

**为什么怪物看起来从第 18 列走，而我 JSON 写的是 `col:17`？**

JSON 是 0 基坐标，`col:17` 是视觉第 18 列。视觉第 17 列要写 `col:16`。

**为什么 `routeLabelA` 要同步？**

`routeLabelA` 不直接驱动怪物移动，但它让编辑器显示路径序号。如果 label 和 `routes` 不一致，你看到的路径标号会误导后续编辑。

**为什么保存时出现 PATH 地块不在 routes 中的警告？**

这说明某个格子只是被画成路径颜色，但没有加入实际路线。它可以作为视觉标记存在，但怪物不会按这个格子走。

**为什么 route 点的 tile 是 `DEPLOY_A` 会被提醒？**

这说明实际路线经过了一个部署地块。游戏仍会按 route 移动，但地图显示、部署逻辑和调试判断会混乱，建议改成 `PATH_A`、出生点或核心。
