#ifndef GAMEPROJECT_CORE_COMBAT_PROJECTILE_H
#define GAMEPROJECT_CORE_COMBAT_PROJECTILE_H

#include "core/map/MapPosition.h"

namespace game::core {

// 投射物数据模型。
//
// 当前只保存源目标、起终点、伤害和进度。UI 可以用 progress 插值绘制弹道，
// BattleManager 未来也可以在 reached() 后统一结算命中。
class Projectile {
public:
    Projectile(int sourceId, int targetId, MapPosition from,
               MapPosition to, int damage, double speed = 8.0);

    int sourceId() const { return sourceId_; }
    int targetId() const { return targetId_; }
    int damage() const { return damage_; }
    bool reached() const { return reached_; }
    double progress() const { return progress_; }
    MapPosition from() const { return from_; }
    MapPosition to() const { return to_; }

    // 根据速度推进飞行进度，progress 到 1.0 时视为到达目标。
    void update(double deltaSeconds);

private:
    int sourceId_;
    int targetId_;
    MapPosition from_;
    MapPosition to_;
    int damage_;
    double speed_;
    double progress_;
    bool reached_;
};

} // namespace game::core

#endif // GAMEPROJECT_CORE_COMBAT_PROJECTILE_H
