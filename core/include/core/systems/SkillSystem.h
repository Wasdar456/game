#ifndef GAMEPROJECT_CORE_SYSTEMS_SKILLSYSTEM_H
#define GAMEPROJECT_CORE_SYSTEMS_SKILLSYSTEM_H

#include "core/units/Card.h"
#include "core/units/Monster.h"
#include <memory>
#include <vector>

namespace game::core {

// 自动技能系统。
//
// README 约束中明确所有技能自动释放，不做手动技能按钮。
// SkillSystem 每帧推进卡牌冷却，并在冷却完成时调用具体卡牌的 autoSkill。
class SkillSystem {
public:
    // cards 会作为友方列表，monsters 会作为敌方列表传入卡牌技能。
    void update(double deltaSeconds,
                std::vector<std::shared_ptr<Card>>& cards,
                std::vector<std::shared_ptr<Monster>>& monsters,
                Map& map,
                ResourceManager& resources);
};

} // namespace game::core

#endif // GAMEPROJECT_CORE_SYSTEMS_SKILLSYSTEM_H
