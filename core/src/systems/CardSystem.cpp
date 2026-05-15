#include "core/systems/CardSystem.h"
#include "core/base/Constants.h"
#include "core/systems/ResourceManager.h"
#include "core/units/AttackUnit.h"
#include "core/units/HealUnit.h"
#include "core/units/ProduceUnit.h"
#include <algorithm>

namespace game::core {

CardSystem::CardSystem(int firstUnitId)
    : nextUnitId_(firstUnitId) {}

std::shared_ptr<Card> CardSystem::deploy(CardKind kind, MapPosition position,
                                         Map& map, ResourceManager& resources) {
    // 先检查地图，再扣资源，最后创建实体，避免失败时留下半完成状态。
    if (!map.canDeployAt(position)) return nullptr;
    int cost = deployCost(kind);
    if (!resources.consumeResource(cost)) return nullptr;

    // 根据 CardKind 创建具体卡牌。
    std::shared_ptr<Card> card;
    int id = nextUnitId_++;
    switch (kind) {
        case CardKind::Attack:
            card = std::make_shared<AttackUnit>(id, position);
            break;
        case CardKind::Produce:
            card = std::make_shared<ProduceUnit>(id, position);
            break;
        case CardKind::Heal:
            card = std::make_shared<HealUnit>(id, position);
            break;
    }

    // 部署成功后同时更新卡牌列表和地图占用。
    if (!card) return nullptr;
    cards_.push_back(card);
    map.setOccupied(position, true, card->id());
    return card;
}

bool CardSystem::upgrade(int unitId, ResourceManager& resources) {
    auto card = findCard(unitId);
    return card && card->upgrade(resources);
}

bool CardSystem::move(int unitId, MapPosition target, Map& map,
                      ResourceManager& resources) {
    auto card = findCard(unitId);
    return card && card->tryTeleport(target, map, resources);
}

bool CardSystem::recall(int unitId, Map& map, ResourceManager& resources) {
    // 撤回需要找到对应单位；成功后返还部分资源并释放格子。
    auto it = std::find_if(cards_.begin(), cards_.end(),
                           [unitId](const auto& card) { return card->id() == unitId; });
    if (it == cards_.end()) return false;

    resources.addResource((*it)->recallRefund());
    map.clearOccupant((*it)->position());
    cards_.erase(it);
    return true;
}

void CardSystem::clear(Map& map) {
    for (const auto& card : cards_) {
        if (card) map.clearOccupant(card->position());
    }
    cards_.clear();
}

std::shared_ptr<Card> CardSystem::findCard(int unitId) const {
    auto it = std::find_if(cards_.begin(), cards_.end(),
                           [unitId](const auto& card) { return card->id() == unitId; });
    return it == cards_.end() ? nullptr : *it;
}

std::vector<std::shared_ptr<Entity>> CardSystem::asEntities() const {
    // shared_ptr<Card> 可以安全向上转为 shared_ptr<Entity>。
    std::vector<std::shared_ptr<Entity>> result;
    result.reserve(cards_.size());
    for (const auto& card : cards_) result.push_back(card);
    return result;
}

int CardSystem::deployCost(CardKind kind) {
    // 当前费用写在常量中，后续可以替换成 data_manager 配置。
    switch (kind) {
        case CardKind::Attack: return constants::DeployCostAttack;
        case CardKind::Produce: return constants::DeployCostProduce;
        case CardKind::Heal: return constants::DeployCostHeal;
    }
    return 0;
}

} // namespace game::core
