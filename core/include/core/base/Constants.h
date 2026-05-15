#ifndef GAMEPROJECT_CORE_BASE_CONSTANTS_H
#define GAMEPROJECT_CORE_BASE_CONSTANTS_H

namespace game::core::constants {

// 默认地图大小，与 README 中的 20 x 30 网格约定一致。
constexpr int DefaultMapRows = 20;
constexpr int DefaultMapCols = 30;

// 默认开局资源和基地血量。后续可由 data_manager 配置覆盖。
constexpr int InitialResources = 100;
constexpr int InitialBaseHealth = 10;

// 卡牌成长与经济参数。
constexpr int MaxCardLevel = 3;
constexpr int DeployCostAttack = 40;
constexpr int DeployCostProduce = 35;
constexpr int DeployCostHeal = 30;
constexpr int UpgradeBaseCost = 30;
constexpr int RecallRefundPercent = 40;

// 瞬移移动消耗：基础消耗 + 曼哈顿距离 * 距离系数。
constexpr int TeleportBaseCost = 10;
constexpr int TeleportDistanceCost = 5;

// 高低差规则：低处攻击高处时伤害衰减，高台单位获得额外射程。
constexpr double LowGroundDamageMultiplier = 0.7;
constexpr int HighGroundRangeBonus = 1;

// 默认逻辑帧长。UI/app 层也可以传入真实 deltaSeconds。
constexpr double DefaultFrameSeconds = 1.0 / 60.0;

} // namespace game::core::constants

#endif // GAMEPROJECT_CORE_BASE_CONSTANTS_H
