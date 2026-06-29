#include "core/systems/CardSystem.h"
#include "core/base/Constants.h"
#include "core/data/CardSpecs.h"
#include "core/systems/ResourceManager.h"
#include "core/units/AttackUnit.h"
#include "core/units/HealUnit.h"
#include "core/units/ProduceUnit.h"
#include <QSettings>
#include <algorithm>

namespace game::core {

namespace {

QString collectionKey(CardKind kind)
{
    return QString::fromUtf8(cardKey(kind));
}

int collectionLevel(CardKind kind)
{
    QSettings settings;
    return qBound(1, settings.value(QString("collection/level/%1").arg(collectionKey(kind)), 1).toInt(), 3);
}

void applyCollectionBonus(const std::shared_ptr<Card>& card, CardKind kind)
{
    if (!card) return;
    const int bonusSteps = qMax(0, collectionLevel(kind) - 1);
    if (bonusSteps <= 0) return;

    const int hpBonus = qRound(card->maxHp() * 0.03 * bonusSteps);
    if (hpBonus > 0) {
        card->setMaxHp(card->maxHp() + hpBonus);
        card->heal(hpBonus);
    }
    if (card->attack() > 0) {
        const int attackBonus = qMax(1, qRound(card->attack() * 0.03 * bonusSteps));
        card->setAttack(card->attack() + attackBonus);
    }
}

} // namespace

CardSystem::CardSystem(int firstUnitId)
    : nextUnitId_(firstUnitId) {}

std::shared_ptr<Card> CardSystem::deploy(CardKind kind, MapPosition position,
                                         Map& map, ResourceManager& resources) {
    // 先检查地图，再扣资源，最后创建实体，避免失败时留下半完成状态。
    if (!map.canDeployAt(position)) return nullptr;
    const CardSpec& spec = cardSpec(kind);
    int cost = spec.deployCost;
    if (!resources.consumeResource(cost)) return nullptr;

    // 根据 CardKind 创建具体卡牌。
    std::shared_ptr<Card> card;
    int id = nextUnitId_++;
    switch (kind) {
        case CardKind::Attack:
        case CardKind::Sniper:
        case CardKind::Aoe:
        case CardKind::Specialist:
        case CardKind::Attack2:
            card = std::make_shared<AttackUnit>(id, position, spec.maxHp, spec.attack,
                                                spec.attackRange, spec.moveLimit,
                                                spec.skillCooldownSeconds, spec.deployCost,
                                                spec.kind, spec.projectileKind,
                                                spec.splashRadius);
            break;
        case CardKind::Produce:
        case CardKind::Arsenal:
            card = std::make_shared<ProduceUnit>(id, position, spec.maxHp, spec.moveLimit,
                                                 spec.skillCooldownSeconds, spec.deployCost,
                                                 spec.kind, spec.resourceYield);
            break;
        case CardKind::Heal:
        case CardKind::HeavyMedic:
        case CardKind::Heal2:
            card = std::make_shared<HealUnit>(id, position, spec.maxHp, spec.attackRange,
                                              spec.moveLimit, spec.skillCooldownSeconds,
                                              spec.deployCost, spec.kind, spec.healAmount);
            break;
    }

    // 部署成功后同时更新卡牌列表和地图占用。
    if (!card) return nullptr;
    applyCollectionBonus(card, kind);
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

void CardSystem::removeDead(Map& map) {
    cards_.erase(std::remove_if(cards_.begin(), cards_.end(),
        [&map](const std::shared_ptr<Card>& card) {
            if (!card || !card->isDead()) return false;
            map.clearOccupant(card->position());
            return true;
        }),
        cards_.end());
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
    return cardSpec(kind).deployCost;
}

} // namespace game::core
