#ifndef GAMEPROJECT_CORE_SYSTEMS_CARDSYSTEM_H
#define GAMEPROJECT_CORE_SYSTEMS_CARDSYSTEM_H

#include "core/base/CoreTypes.h"
#include "core/map/Map.h"
#include "core/units/Card.h"
#include <memory>
#include <vector>

namespace game::core {

// 管理玩家已部署卡牌。
//
// CardSystem 负责部署、升级、移动、撤回和查找卡牌；
// 资源扣除和地图占用也在这里同步完成，避免 UI 或 BattleManager
// 忘记维护其中一边状态。
class CardSystem {
public:
    explicit CardSystem(int firstUnitId = 1);

    std::shared_ptr<Card> deploy(CardKind kind, MapPosition position,
                                 Map& map, ResourceManager& resources);
    std::shared_ptr<Card> deployWithId(CardKind kind, MapPosition position,
                                       Map& map, ResourceManager& resources,
                                       int unitId);

    // 单位操作入口，返回 false 表示资源不足、目标非法或 id 不存在。
    bool upgrade(int unitId, ResourceManager& resources);
    bool move(int unitId, MapPosition target, Map& map, ResourceManager& resources);
    bool recall(int unitId, Map& map, ResourceManager& resources);
    bool destroy(int unitId, Map& map);
    void reserveUnitId(int unitId);

    // 清空所有卡牌并释放地图占用，切换关卡或重开时使用。
    void clear(Map& map);
    void removeDead(Map& map);

    std::shared_ptr<Card> findCard(int unitId) const;

    // 将卡牌列表转换成 Entity 列表，供索敌系统复用。
    std::vector<std::shared_ptr<Entity>> asEntities() const;

    const std::vector<std::shared_ptr<Card>>& cards() const { return cards_; }
    std::vector<std::shared_ptr<Card>>& cards() { return cards_; }

    // 不同卡牌类型的部署费用集中在这里，便于以后改成配置表。
    static int deployCost(CardKind kind);

private:
    // 下一个可分配单位 id。
    int nextUnitId_;
    int firstUnitId_;
    // 当前场上所有卡牌。
    std::vector<std::shared_ptr<Card>> cards_;
};

} // namespace game::core

#endif // GAMEPROJECT_CORE_SYSTEMS_CARDSYSTEM_H
