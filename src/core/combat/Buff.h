#ifndef GAMEPROJECT_CORE_COMBAT_BUFF_H
#define GAMEPROJECT_CORE_COMBAT_BUFF_H

namespace game::core {

// 状态效果类型。当前 BuffManager 只负责存储和计时，
// 具体数值如何影响实体留给系统层扩展。
enum class BuffType {
    Slow,
    Stun,
    AttackBoost,
    SpeedBoost,
    Regeneration
};

// 单个 Buff 实例。
//
// durationSeconds 表示剩余时间，magnitude 表示效果强度，
// 例如减速百分比、攻击加成或每秒回血量。
class Buff {
public:
    Buff(BuffType type = BuffType::Slow, double durationSeconds = 0.0,
         double magnitude = 0.0);

    BuffType type() const { return type_; }
    double remainingSeconds() const { return remainingSeconds_; }
    double magnitude() const { return magnitude_; }
    bool expired() const { return remainingSeconds_ <= 0.0; }

    // 推进 Buff 生命周期，时间归零后由 BuffManager 清理。
    void tick(double deltaSeconds);

private:
    BuffType type_;
    double remainingSeconds_;
    double magnitude_;
};

} // namespace game::core

#endif // GAMEPROJECT_CORE_COMBAT_BUFF_H
