#ifndef GAMEPROJECT_CORE_COMBAT_BUFFMANAGER_H
#define GAMEPROJECT_CORE_COMBAT_BUFFMANAGER_H

#include "core/combat/Buff.h"
#include <unordered_map>
#include <vector>

namespace game::core {

// 按实体 id 管理 Buff 的轻量容器。
//
// 它只提供添加、更新、查询和清除能力，不直接修改 Entity。
// 这样以后不同系统可以决定 Slow/Stun/Regeneration 如何生效。
class BuffManager {
public:
    // 给指定实体附加一个 Buff。
    void addBuff(int entityId, const Buff& buff);

    // 每帧推进所有 Buff，并清除过期项。
    void update(double deltaSeconds);
    void clear(int entityId);

    // 查询某实体是否存在指定类型 Buff。
    bool hasBuff(int entityId, BuffType type) const;

    // 累加同类型 Buff 强度，供系统层计算最终属性。
    double totalMagnitude(int entityId, BuffType type) const;
    const std::vector<Buff>& buffsFor(int entityId) const;

private:
    std::unordered_map<int, std::vector<Buff>> buffs_;
    static const std::vector<Buff> empty_;
};

} // namespace game::core

#endif // GAMEPROJECT_CORE_COMBAT_BUFFMANAGER_H
