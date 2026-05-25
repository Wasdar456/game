#ifndef GAMEPROJECT_CORE_SYSTEMS_SKILLSYSTEM_H
#define GAMEPROJECT_CORE_SYSTEMS_SKILLSYSTEM_H

#include "core/units/Card.h"
#include "core/units/Monster.h"
#include "core/combat/Projectile.h"
#include <memory>
#include <vector>

namespace game::core {

// 自动技能系统。
//
// README 约束中明确所有技能自动释放，不做手动技能按钮。
// SkillSystem 每帧推进卡牌冷却，并在冷却完成时调用具体卡牌的 autoSkill。
class SkillSystem {
public:
    // 单机模式：cards 会作为友方列表，monsters 会作为敌方列表传入卡牌技能。
    void update(double deltaSeconds,
                std::vector<std::shared_ptr<Card>>& cards,
                std::vector<std::shared_ptr<Monster>>& monsters,
                Map& map,
                ResourceManager& resources,
                std::vector<Projectile>& projectiles);

    // PVP 模式：双方卡牌互相攻击，同时攻击怪物
    void updatePvp(double deltaSeconds,
                   std::vector<std::shared_ptr<Card>>& cardsA,
                   std::vector<std::shared_ptr<Card>>& cardsB,
                   std::vector<std::shared_ptr<Monster>>& monsters,
                   Map& map,
                   ResourceManager& resourcesA,
                   ResourceManager& resourcesB,
                   std::vector<Projectile>& projectiles);
};

} // namespace game::core

#endif // GAMEPROJECT_CORE_SYSTEMS_SKILLSYSTEM_H
