#ifndef GAMEPROJECT_CORE_UNITS_MONSTER_H
#define GAMEPROJECT_CORE_UNITS_MONSTER_H

#include "core/base/Entity.h"
#include <vector>

namespace game::core {

class ResourceManager;

// 怪物基类。
//
// Monster 保存路线、速度、击杀奖励和到达基地后的伤害。
// 具体怪物类型通过 MonsterTypes.h 中的轻量派生类配置数值和特殊行为。
class Monster : public Entity {
public:
    Monster(int id, MapPosition position, MonsterKind kind, int maxHp, int attack,
            double moveSpeed, int reward, int coreDamage, RouteType routeType,
            int attackRange = 1);
    ~Monster() override = default;

    void update(double deltaSeconds) override;

    // 默认死亡奖励：给玩家增加 reward_ 资源。
    virtual void onDeath(ResourceManager& resources);

    MonsterKind kind() const { return kind_; }
    double moveSpeed() const { return moveSpeed_; }
    int reward() const { return reward_; }
    int coreDamage() const { return coreDamage_; }
    int attackRange() const { return attackRange_; }
    RouteType routeType() const { return routeType_; }
    bool escaped() const { return escaped_; }

    void setPath(std::vector<MapPosition> path);
    const std::vector<MapPosition>& path() const { return path_; }

protected:
    // 怪物种类，供快照和网络层识别。
    MonsterKind kind_;
    // 每秒移动的格数。
    double moveSpeed_;
    // 击杀奖励资源。
    int reward_;
    // 到达基地时造成的伤害。
    int coreDamage_;
    // 主路线或资源路线。
    RouteType routeType_;
    // 怪物攻击范围，供未来远程怪/拆迁怪逻辑扩展。
    int attackRange_;
    // 当前路线节点序列。
    std::vector<MapPosition> path_;
    // 下一个目标节点下标。
    std::size_t pathIndex_;
    // 浮点坐标用于平滑移动，整数 position_ 用于网格逻辑和 UI。
    double exactRow_;
    double exactCol_;
    // 标记是否已经走到终点，BattleManager 会据此扣基地血并移除。
    bool escaped_;
};

} // namespace game::core

#endif // GAMEPROJECT_CORE_UNITS_MONSTER_H
