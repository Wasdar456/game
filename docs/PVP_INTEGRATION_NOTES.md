# PVP 战斗集成说明

## 本次已接入

- 卡牌攻击改为投射物结算：单位开火后生成子弹，子弹抵达目标后才扣血。
- 投射物支持占位表现：普通子弹、狙击弹、AOE 火球、怪物弹。
- AOE 卡牌具备 1 格溅射半径，UI 会显示占位范围圈。
- 攻击型怪物会在射程内攻击双方防御塔，目前表现为紫色投射物。
- PVP 战斗阶段禁用部署、移动、升级、撤回，战斗只观看。
- `BATTLE_STATE` 已从 `BattlePage` 抽离到 `BattleStateCodec`，联机快照编码/解码/checksum 不再混在页面里。
- 清理了网络 `.cpp` 中无效的手写 `.moc` include，构建时不再刷 AutoMoc warning。

## 测试方法

1. 单机快速看弹道：
   - 进入普通战斗。
   - 部署攻击单位。
   - 等怪物进入射程，观察黄色/蓝色/橙色弹道。
   - 观察怪物血量应在弹道抵达后下降，不应在开火瞬间下降。

2. AOE 测试：
   - 选 AOE 炮塔。
   - 等多个怪物靠近。
   - 观察橙色火球和虚线溅射圈。
   - 命中后目标附近 1 格内怪物应一起掉血。

3. PVP 战斗阶段测试：
   - Host/Client 完成迷雾部署。
   - 双方点击开战。
   - 战斗阶段底部卡牌应不可用，点击地图单位不应弹出操作菜单。
   - 子弹、怪物血量、双方单位血量应通过 Host 快照同步显示。
   - 控制台不应再出现 `includes the moc file ... but does not contain Q_OBJECT` 这类 AutoMoc warning。

4. 怪物攻击塔测试：
   - 把塔部署在怪物路径附近 1 格内。
   - 怪物进入射程后应发射紫色弹道。
   - 紫色弹道命中后塔血量下降。

## 给单机部分的接口约定

- 单机仍然使用 `BattleManager::startWave(waveId)` 和 `BattleManager::update(deltaSeconds)`。
- 波次生成已经是 `seed + waveId` 确定性规则；单机关卡如果需要固定体验，应调用 `BattleManager::setRandomSeed(seed)`。
- UI 不应直接改怪物/单位血量，只读 `BattleSnapshot`。
- 投射物展示读取 `BattleSnapshot::projectiles`，不需要单机 UI 自己计算弹道。
- 联机快照协议由 `src/network/protocol/BattleStateCodec.*` 维护，后续改字段优先改这里，不要再把序列化逻辑塞回页面。

## 给美术部分的资源约定

当前没有正式图片，已使用 QPainter 占位。后续替换图片时建议提供：

- `projectile_bullet.png`：普通子弹，建议 32x32，透明背景。
- `projectile_sniper.png`：狙击弹/光束，建议 48x16 或 32x32，透明背景。
- `projectile_aoe.png`：火球，建议 32x32，透明背景。
- `projectile_monster.png`：怪物远程弹，建议 32x32，透明背景。
- `impact_aoe.png`：AOE 命中爆炸，建议 64x64，透明背景。

地图资源后续应按统一网格输出底图。程序侧需要知道：

- 地图图片尺寸。
- 行列数。
- 每格类型：路径、出生点、核心、可部署区、高台、阻挡区。
- A/B 双方初始部署区域。

## 当前仍需继续完善

- 怪物攻击目前是“边走边打”，还没有停步攻击/拆塔 AI。
- 投射物命中没有正式爆炸动画，只是占位圆点和范围圈。
- 地图仍是代码硬编码测试图，还未接入配置文件。
- PVP 仍是 Host 权威快照；当前已有帧快照 checksum，后续还应增加阶段首尾 checksum 校验。
