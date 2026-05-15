#include "core/units/Card.h"
#include "core/base/Constants.h"
#include "core/combat/TargetSelector.h"
#include "core/map/Map.h"
#include "core/systems/ResourceManager.h"
#include <algorithm>
#include <utility>

namespace game::core {

Card::Card(int id, MapPosition position, ObjectType type, int maxHp, int attack,
           int attackRange, int moveLimit, double skillCooldownSeconds,
           int deployCost)
    : Entity(id, position, type, Team::Player, maxHp, attack),
      level_(1),
      attackRange_(attackRange),
      moveLimit_(moveLimit),
      skillCooldownSeconds_(skillCooldownSeconds),
      currentCooldownSeconds_(skillCooldownSeconds),
      deployCost_(deployCost) {}

void Card::update(double deltaSeconds) {
    tickCooldown(deltaSeconds);
}

int Card::upgradeCost() const {
    // 升级费用随当前等级线性增长：Lv1->2 为 30，Lv2->3 为 60。
    return constants::UpgradeBaseCost * level_;
}

int Card::recallRefund() const {
    // 撤回只返还部署费用的一部分，避免频繁撤回成为无成本操作。
    return deployCost_ * constants::RecallRefundPercent / 100;
}

bool Card::isSkillReady() const {
    return currentCooldownSeconds_ >= skillCooldownSeconds_;
}

void Card::setPriorityList(std::vector<ObjectType> priorities) {
    priorityList_ = std::move(priorities);
}

void Card::tickCooldown(double deltaSeconds) {
    // 冷却累计到上限即可，不继续增长，便于 isSkillReady 判断。
    currentCooldownSeconds_ = std::min(skillCooldownSeconds_,
                                       currentCooldownSeconds_ + deltaSeconds);
}

void Card::resetCooldown() {
    currentCooldownSeconds_ = 0.0;
}

bool Card::upgrade(ResourceManager& resources) {
    // 达到最高等级后拒绝升级。
    if (level_ >= constants::MaxCardLevel) return false;
    // 资源不足时保持原状态。
    if (!resources.consumeResource(upgradeCost())) return false;

    // 当前基础成长：生命、攻击、射程都提升。
    // 后续接入配置表后可以替换成每张卡独立成长曲线。
    ++level_;
    setMaxHp(maxHp_ + 20);
    heal(20);
    setAttack(attack_ + 5);
    ++attackRange_;
    return true;
}

bool Card::tryTeleport(MapPosition target, Map& map, ResourceManager& resources) {
    // 瞬移距离使用曼哈顿距离，不计算中途路径。
    int distance = position_.manhattanDistanceTo(target);
    if (distance > moveLimit_) return false;

    // 目标必须是可部署且未占用格。
    if (!map.canDeployAt(target)) return false;

    // 移动费用 = 基础费用 + 距离 * 系数。
    int cost = constants::TeleportBaseCost +
               distance * constants::TeleportDistanceCost;
    if (!resources.consumeResource(cost)) return false;

    // 资源扣除成功后再改变地图占用和自身坐标，保证失败无副作用。
    map.clearOccupant(position_);
    position_ = target;
    map.setOccupied(position_, true, id_);
    return true;
}

std::shared_ptr<Entity> Card::findTarget(
    const std::vector<std::shared_ptr<Entity>>& targets,
    const Map& map,
    bool requireDamagedAlly) const {
    // 统一委托给 TargetSelector，保证攻击和治疗使用同一套稳定排序规则。
    return TargetSelector::selectTarget(*this, targets, map, attackRange_,
                                        priorityList_, requireDamagedAlly);
}

} // namespace game::core
