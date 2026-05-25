#ifndef GAMEPROJECT_CORE_SYSTEMS_RESOURCEMANAGER_H
#define GAMEPROJECT_CORE_SYSTEMS_RESOURCEMANAGER_H

#include "core/base/Constants.h"

namespace game::core {

// 管理战斗中的资源和基地血量。
//
// 旧版 PlayerState 是全局单例；正式 core 改为普通对象，
// 由 BattleManager 持有，避免 PVE/PVP、多局游戏或测试之间互相污染。
class ResourceManager {
public:
    ResourceManager(int initialResources = constants::InitialResources,
                    int baseHealth = constants::InitialBaseHealth);

    int resources() const { return resources_; }
    int baseHealth() const { return baseHealth_; }
    bool baseDestroyed() const { return baseHealth_ <= 0; }

    // 资源操作：消费失败时返回 false，调用者据此拒绝部署/升级/移动。
    void setResources(int value);
    void addResource(int amount);
    bool consumeResource(int amount);

    // 基地血量操作。基地归零后 BattleManager::gameOver() 为 true。
    void setBaseHealth(int value);
    void damageBase(int damage);
    void healBase(int amount);

private:
    // 当前可用资源。
    int resources_;
    // 当前基地血量。
    int baseHealth_;
};

} // namespace game::core

#endif // GAMEPROJECT_CORE_SYSTEMS_RESOURCEMANAGER_H
