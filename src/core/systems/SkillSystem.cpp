#include "core/systems/SkillSystem.h"
#include "core/base/DebugConfig.h"
#include <QDebug>

namespace game::core {

void SkillSystem::update(double deltaSeconds,
                         std::vector<std::shared_ptr<Card>>& cards,
                         std::vector<std::shared_ptr<Monster>>& monsters,
                         Map& map,
                         ResourceManager& resources,
                         std::vector<Projectile>& projectiles) {
    // 先构造 Entity 视图，具体卡牌技能只关心"友方"和"敌方"列表。
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

    // [DEBUG] 打印战斗状态
    if (DebugConfig::DEBUG_ENABLED && DebugConfig::LOG_SKILL_SYSTEM) {
        static int debugFrame = 0;
        if (++debugFrame % 60 == 0) {  // 每60帧打印一次
            qDebug() << "[SkillSystem] allies:" << allies.size()
                     << "enemies:" << enemies.size();
            for (const auto& card : cards) {
                if (card && !card->isDead()) {
                    qDebug() << "  Card" << card->id()
                             << "pos:(" << card->row() << "," << card->col() << ")"
                             << "range:" << card->attackRange()
                             << "cooldown:" << card->isSkillReady();
                }
            }
            for (const auto& monster : monsters) {
                if (monster && !monster->isDead() && !monster->escaped()) {
                    qDebug() << "  Monster" << monster->id()
                             << "pos:(" << monster->row() << "," << monster->col() << ")"
                             << "hp:" << monster->hp();
                }
            }
        }
    }

    for (auto& card : cards) {
        if (card && !card->isDead()) {
            // 自动技能由具体卡牌决定是否释放、如何选目标和产生效果。
            card->autoSkill(allies, enemies, map, resources,
                            &projectiles, ProjectileOwner::PlayerA);
        }
    }
}

void SkillSystem::updatePvp(double deltaSeconds,
                             std::vector<std::shared_ptr<Card>>& cardsA,
                             std::vector<std::shared_ptr<Card>>& cardsB,
                   std::vector<std::shared_ptr<Monster>>& monsters,
                   Map& map,
                   ResourceManager& resourcesA,
                   ResourceManager& resourcesB,
                   std::vector<Projectile>& projectiles) {
    // 构造实体列表
    std::vector<std::shared_ptr<Entity>> alliesA;  // A方友军
    std::vector<std::shared_ptr<Entity>> alliesB;  // B方友军
    std::vector<std::shared_ptr<Entity>> enemiesForA;  // A方的敌人（怪物+B方单位）
    std::vector<std::shared_ptr<Entity>> enemiesForB;  // B方的敌人（怪物+A方单位）

    // 收集A方单位
    for (auto& card : cardsA) {
        if (card && !card->isDead()) {
            card->update(deltaSeconds);
            alliesA.push_back(card);
            enemiesForB.push_back(card);  // A方单位是B方的敌人
        }
    }

    // 收集B方单位
    for (auto& card : cardsB) {
        if (card && !card->isDead()) {
            card->update(deltaSeconds);
            alliesB.push_back(card);
            enemiesForA.push_back(card);  // B方单位是A方的敌人
        }
    }

    // 收集怪物（双方共同的敌人）
    for (auto& monster : monsters) {
        if (monster && !monster->isDead() && !monster->escaped()) {
            enemiesForA.push_back(monster);
            enemiesForB.push_back(monster);
        }
    }

    // A方单位执行技能
    for (auto& card : cardsA) {
        if (card && !card->isDead()) {
            card->autoSkill(alliesA, enemiesForA, map, resourcesA,
                            &projectiles, ProjectileOwner::PlayerA);
        }
    }

    // B方单位执行技能
    for (auto& card : cardsB) {
        if (card && !card->isDead()) {
            card->autoSkill(alliesB, enemiesForB, map, resourcesB,
                            &projectiles, ProjectileOwner::PlayerB);
        }
    }
}

} // namespace game::core
