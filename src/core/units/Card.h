#ifndef GAMEPROJECT_CORE_UNITS_CARD_H
#define GAMEPROJECT_CORE_UNITS_CARD_H

#include "core/base/Entity.h"
#include <memory>
#include <vector>

namespace game::core {

class Map;
class ResourceManager;

// 玩家防御单位基类。
//
// Card 封装所有卡牌共同能力：等级、射程、瞬移、技能冷却、升级、
// 召回退款和通用索敌。具体技能效果由 AttackUnit/ProduceUnit/HealUnit 实现。
class Card : public Entity {
public:
    Card(int id, MapPosition position, ObjectType type, int maxHp, int attack,
         int attackRange, int moveLimit, double skillCooldownSeconds,
         int deployCost);
    ~Card() override = default;

    void update(double deltaSeconds) override;

    int level() const { return level_; }
    int attackRange() const { return attackRange_; }
    int moveLimit() const { return moveLimit_; }
    int deployCost() const { return deployCost_; }
    int upgradeCost() const;
    int recallRefund() const;
    bool isSkillReady() const;

    // 索敌优先级列表，越靠前优先级越高。
    const std::vector<ObjectType>& priorityList() const { return priorityList_; }
    void setPriorityList(std::vector<ObjectType> priorities);

    // 冷却推进和重置。BattleManager 每帧经 SkillSystem 调用。
    void tickCooldown(double deltaSeconds);
    void resetCooldown();

    // 升级会消耗资源并提升生命、攻击和射程。
    bool upgrade(ResourceManager& resources);

    // 瞬移移动：检查距离、目标格、资源，并更新地图占用。
    bool tryTeleport(MapPosition target, Map& map, ResourceManager& resources);

    // 通用索敌入口。治疗单位可传 requireDamagedAlly=true。
    std::shared_ptr<Entity> findTarget(
        const std::vector<std::shared_ptr<Entity>>& targets,
        const Map& map,
        bool requireDamagedAlly = false) const;

    // 自动技能。所有单位技能都是自动释放，符合 README 的约束。
    virtual void autoSkill(std::vector<std::shared_ptr<Entity>>& allies,
                           std::vector<std::shared_ptr<Entity>>& enemies,
                           Map& map,
                           ResourceManager& resources) = 0;

protected:
    // 当前等级，范围 1..MaxCardLevel。
    int level_;
    // 基础攻击/治疗范围。
    int attackRange_;
    // 单次瞬移最大曼哈顿距离。
    int moveLimit_;
    // 完整技能冷却时间。
    double skillCooldownSeconds_;
    // 当前已累计冷却时间。
    double currentCooldownSeconds_;
    // 部署费用，用于召回退款。
    int deployCost_;
    // 目标类型优先级。
    std::vector<ObjectType> priorityList_;
};

} // namespace game::core

#endif // GAMEPROJECT_CORE_UNITS_CARD_H
