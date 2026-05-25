#include "core/units/Monster.h"
#include "core/systems/ResourceManager.h"
#include <cmath>
#include <utility>

namespace game::core {

Monster::Monster(int id, MapPosition position, MonsterKind kind, int maxHp, int attack,
                 double moveSpeed, int reward, int coreDamage, RouteType routeType,
                 int attackRange)
    : Entity(id, position, toObjectType(kind), Team::Enemy, maxHp, attack),
      kind_(kind),
      moveSpeed_(moveSpeed),
      reward_(reward),
      coreDamage_(coreDamage),
      routeType_(routeType),
      attackRange_(attackRange),
      pathIndex_(0),
      exactRow_(position.row),
      exactCol_(position.col),
      escaped_(false),
      escapePending_(false) {}

void Monster::update(double deltaSeconds) {
    // 死亡或已经逃逸的怪物不再移动。
    if (isDead() || escaped_) return;
    if (escapePending_) {
        escaped_ = true;
        return;
    }
    if (path_.empty() || pathIndex_ >= path_.size()) {
        // 没有路径或路径走完时标记逃逸，扣血由 BattleManager 统一处理。
        escaped_ = true;
        return;
    }

    // 使用浮点坐标朝下一个路径节点平滑移动。
    MapPosition target = path_[pathIndex_];
    double dr = static_cast<double>(target.row) - exactRow_;
    double dc = static_cast<double>(target.col) - exactCol_;
    double distance = std::sqrt(dr * dr + dc * dc);
    double step = moveSpeed_ * deltaSeconds;

    if (distance <= step || distance == 0.0) {
        // 本帧足以到达目标节点时，吸附到节点并进入下一个节点。
        exactRow_ = target.row;
        exactCol_ = target.col;
        setPosition(target);
        ++pathIndex_;
        if (pathIndex_ >= path_.size()) escapePending_ = true;
    } else {
        // 否则沿方向向量前进 step 距离，再同步到整数网格坐标。
        exactRow_ += (dr / distance) * step;
        exactCol_ += (dc / distance) * step;
        setPosition(static_cast<int>(std::round(exactRow_)),
                    static_cast<int>(std::round(exactCol_)));
    }
}

void Monster::onDeath(ResourceManager& resources) {
    // 默认击杀奖励。特殊怪物可以覆盖此函数实现额外效果。
    resources.addResource(reward_);
}

void Monster::setPath(std::vector<MapPosition> path) {
    // 重新设置路径时重置移动状态。
    path_ = std::move(path);
    pathIndex_ = 0;
    escaped_ = false;
    escapePending_ = false;
    exactRow_ = position_.row;
    exactCol_ = position_.col;
    if (!path_.empty() && path_.front() == position_) {
        // 如果路径第一个节点就是当前位置，下一步直接走向第二个节点。
        pathIndex_ = 1;
    }
}

} // namespace game::core
