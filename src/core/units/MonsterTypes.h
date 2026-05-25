#ifndef GAMEPROJECT_CORE_UNITS_MONSTERTYPES_H
#define GAMEPROJECT_CORE_UNITS_MONSTERTYPES_H

#include "core/units/Monster.h"
#include <memory>

namespace game::core {

// 下面这些派生类主要负责“配置怪物数值”。
// 这样 WaveSpawner 可以通过 MonsterKind 创建具体怪物，
// 而 Monster 基类仍然保持通用移动和死亡奖励逻辑。

// 普通资源怪：无攻击、无基地伤害，击杀后给少量资源。
class ResBasicMonster : public Monster {
public:
    ResBasicMonster(int id, MapPosition position)
        : Monster(id, position, MonsterKind::ResBasic, 100, 0, 1.0, 30, 0,
                  RouteType::ResourceRoute) {}
};

// 快速资源怪：血量低、速度快、奖励略高。
class ResFastMonster : public Monster {
public:
    ResFastMonster(int id, MapPosition position)
        : Monster(id, position, MonsterKind::ResFast, 50, 0, 2.0, 40, 0,
                  RouteType::ResourceRoute) {}
};

// 坦克资源怪：血量高、速度慢、奖励高。
class ResTankMonster : public Monster {
public:
    ResTankMonster(int id, MapPosition position)
        : Monster(id, position, MonsterKind::ResTank, 500, 0, 0.4, 150, 0,
                  RouteType::ResourceRoute) {}
};

// 普通攻击怪：标准主路线敌人。
class AtkNormalMonster : public Monster {
public:
    AtkNormalMonster(int id, MapPosition position)
        : Monster(id, position, MonsterKind::AtkNormal, 100, 10, 1.0, 10, 1,
                  RouteType::MainRoute) {}
};

// 坦克攻击怪：高血量、低速度、到达基地伤害更高。
class AtkTankMonster : public Monster {
public:
    AtkTankMonster(int id, MapPosition position)
        : Monster(id, position, MonsterKind::AtkTank, 400, 15, 0.6, 15, 2,
                  RouteType::MainRoute) {}
};

// 快速攻击怪：低血量、高速度，用于突破防线。
class AtkFastMonster : public Monster {
public:
    AtkFastMonster(int id, MapPosition position)
        : Monster(id, position, MonsterKind::AtkFast, 60, 5, 1.8, 10, 1,
                  RouteType::MainRoute) {}
};

// 工兵怪：当前保留数值与类型，未来可扩展为优先攻击高台单位。
class AtkSapperMonster : public Monster {
public:
    AtkSapperMonster(int id, MapPosition position)
        : Monster(id, position, MonsterKind::AtkSapper, 120, 25, 0.9, 20, 1,
                  RouteType::MainRoute) {}
};

// 狂暴怪：每次受伤后提升速度和攻击力。
class AtkBerserkMonster : public Monster {
public:
    AtkBerserkMonster(int id, MapPosition position)
        : Monster(id, position, MonsterKind::AtkBerserk, 200, 10, 0.8, 20, 1,
                  RouteType::MainRoute) {}

    void takeDamage(int damage) override {
        Entity::takeDamage(damage);
        if (!isDead()) {
            moveSpeed_ += 0.15;
            attack_ += 2;
        }
    }
};

// 远程怪：当前保留较大 attackRange，后续可在 BattleManager 中实现停步攻击。
class AtkRangedMonster : public Monster {
public:
    AtkRangedMonster(int id, MapPosition position)
        : Monster(id, position, MonsterKind::AtkRanged, 80, 15, 0.9, 15, 1,
                  RouteType::MainRoute, 3) {}
};

// 回血怪：每秒恢复少量生命。
class AtkRegenMonster : public Monster {
public:
    AtkRegenMonster(int id, MapPosition position)
        : Monster(id, position, MonsterKind::AtkRegen, 150, 10, 0.8, 25, 1,
                  RouteType::MainRoute) {}

    void update(double deltaSeconds) override {
        regenAccumulator_ += deltaSeconds;
        if (regenAccumulator_ >= 1.0) {
            heal(5);
            regenAccumulator_ = 0.0;
        }
        Monster::update(deltaSeconds);
    }

private:
    // 回血计时器，累计到 1 秒触发一次治疗。
    double regenAccumulator_ = 0.0;
};

// 怪物工厂函数。
// 网络层或波次配置只需要传 MonsterKind，core 负责创建具体类型。
inline std::shared_ptr<Monster> createMonster(MonsterKind kind, int id,
                                              MapPosition position) {
    switch (kind) {
        case MonsterKind::ResBasic:
            return std::make_shared<ResBasicMonster>(id, position);
        case MonsterKind::ResFast:
            return std::make_shared<ResFastMonster>(id, position);
        case MonsterKind::ResTank:
            return std::make_shared<ResTankMonster>(id, position);
        case MonsterKind::AtkNormal:
            return std::make_shared<AtkNormalMonster>(id, position);
        case MonsterKind::AtkTank:
            return std::make_shared<AtkTankMonster>(id, position);
        case MonsterKind::AtkFast:
            return std::make_shared<AtkFastMonster>(id, position);
        case MonsterKind::AtkSapper:
            return std::make_shared<AtkSapperMonster>(id, position);
        case MonsterKind::AtkBerserk:
            return std::make_shared<AtkBerserkMonster>(id, position);
        case MonsterKind::AtkRanged:
            return std::make_shared<AtkRangedMonster>(id, position);
        case MonsterKind::AtkRegen:
            return std::make_shared<AtkRegenMonster>(id, position);
    }
    return nullptr;
}

} // namespace game::core

#endif // GAMEPROJECT_CORE_UNITS_MONSTERTYPES_H
