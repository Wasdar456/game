#include "core/units/AttackUnit.h"
#include "core/base/Constants.h"
#include "core/base/DebugConfig.h"
#include "core/combat/DamageCalculator.h"
#include <QDebug>

namespace game::core {

AttackUnit::AttackUnit(int id, MapPosition position)
    : Card(id, position, ObjectType::CardAttack, 100, 20, 3, 2, 1.0,
           constants::DeployCostAttack),
      projectileKind_(ProjectileKind::Bullet),
      splashRadius_(0) {
    priorityList_ = {
        ObjectType::MonsterAtkSapper,
        ObjectType::MonsterAtkRanged,
        ObjectType::MonsterAtkTank,
        ObjectType::MonsterAtkNormal,
        ObjectType::MonsterAtkFast,
        ObjectType::MonsterAtkBerserk,
        ObjectType::MonsterAtkRegen,
        ObjectType::MonsterResTank,
        ObjectType::MonsterResBasic,
        ObjectType::MonsterResFast
    };
}

AttackUnit::AttackUnit(int id,
                       MapPosition position,
                       int maxHp,
                       int attack,
                       int attackRange,
                       int moveLimit,
                       double skillCooldownSeconds,
                       int deployCost,
                       ProjectileKind projectileKind,
                       int splashRadius)
    : Card(id, position, ObjectType::CardAttack, maxHp, attack, attackRange,
           moveLimit, skillCooldownSeconds, deployCost),
      projectileKind_(projectileKind),
      splashRadius_(splashRadius) {
    priorityList_ = {
        ObjectType::MonsterAtkSapper,
        ObjectType::MonsterAtkRanged,
        ObjectType::MonsterAtkTank,
        ObjectType::MonsterAtkNormal,
        ObjectType::MonsterAtkFast,
        ObjectType::MonsterAtkBerserk,
        ObjectType::MonsterAtkRegen,
        ObjectType::MonsterResTank,
        ObjectType::MonsterResBasic,
        ObjectType::MonsterResFast
    };
}

void AttackUnit::autoSkill(std::vector<std::shared_ptr<Entity>>&,
                           std::vector<std::shared_ptr<Entity>>& enemies,
                           Map& map,
                           ResourceManager&,
                           std::vector<Projectile>* projectiles,
                           ProjectileOwner projectileOwner) {
    // 冷却未完成时不做任何事，符合自动技能模型。
    if (!isSkillReady()) return;

    // [DEBUG] 打印目标搜索信息
    if (DebugConfig::DEBUG_ENABLED && DebugConfig::LOG_ATTACK) {
        qDebug() << "[AttackUnit" << id_ << "] skill ready, searching target..."
                 << "pos:(" << row() << "," << col() << ")"
                 << "range:" << attackRange_
                 << "enemies count:" << enemies.size();

        // 打印所有敌人位置和距离
        for (const auto& enemy : enemies) {
            if (enemy && !enemy->isDead()) {
                int dist = position_.manhattanDistanceTo(enemy->position());
                qDebug() << "  Enemy" << enemy->id()
                         << "pos:(" << enemy->row() << "," << enemy->col() << ")"
                         << "distance:" << dist
                         << "inRange:" << (dist <= attackRange_);
            }
        }
    }

    // 按优先级、距离、血量和 id 选择目标。
    auto target = findTarget(enemies, map);
    if (!target) {
        if (DebugConfig::DEBUG_ENABLED && DebugConfig::LOG_ATTACK) {
            qDebug() << "[AttackUnit" << id_ << "] no target found!";
        }
        return;
    }

    // 伤害计算集中交给 DamageCalculator，自动应用高低差惩罚。
    int damage = DamageCalculator::calculateDamage(*this, *target, map);
    if (projectiles) {
        const double speed = projectileKind_ == ProjectileKind::Sniper ? 14.0
                           : projectileKind_ == ProjectileKind::Aoe ? 7.0
                           : 10.0;
        projectiles->emplace_back(id_,
                                  target,
                                  position_,
                                  damage,
                                  projectileKind_,
                                  projectileOwner,
                                  speed,
                                  splashRadius_);
    } else {
        target->takeDamage(damage);
    }

    if (DebugConfig::DEBUG_ENABLED && DebugConfig::LOG_ATTACK) {
        qDebug() << "[AttackUnit" << id_ << "] attacking target" << target->id()
                 << "at (" << target->row() << "," << target->col() << ")"
                 << "fired damage" << damage << "target hp:" << target->hp();
    }

    resetCooldown();
}

} // namespace game::core
