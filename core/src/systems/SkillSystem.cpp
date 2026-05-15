#include "core/systems/SkillSystem.h"

namespace game::core {

void SkillSystem::update(double deltaSeconds,
                         std::vector<std::shared_ptr<Card>>& cards,
                         std::vector<std::shared_ptr<Monster>>& monsters,
                         Map& map,
                         ResourceManager& resources) {
    // 先构造 Entity 视图，具体卡牌技能只关心“友方”和“敌方”列表。
    std::vector<std::shared_ptr<Entity>> allies;
    std::vector<std::shared_ptr<Entity>> enemies;
    allies.reserve(cards.size());
    enemies.reserve(monsters.size());

    for (auto& card : cards) {
        if (card && !card->isDead()) {
            // 卡牌冷却在技能系统统一推进。
            card->update(deltaSeconds);
            allies.push_back(card);
        }
    }
    for (auto& monster : monsters) {
        if (monster && !monster->isDead() && !monster->escaped()) {
            enemies.push_back(monster);
        }
    }

    for (auto& card : cards) {
        if (card && !card->isDead()) {
            // 自动技能由具体卡牌决定是否释放、如何选目标和产生效果。
            card->autoSkill(allies, enemies, map, resources);
        }
    }
}

} // namespace game::core
