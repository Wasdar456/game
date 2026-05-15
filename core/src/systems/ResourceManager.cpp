#include "core/systems/ResourceManager.h"
#include <algorithm>

namespace game::core {

ResourceManager::ResourceManager(int initialResources, int baseHealth)
    : resources_(std::max(0, initialResources)),
      baseHealth_(std::max(0, baseHealth)) {}

void ResourceManager::setResources(int value) {
    resources_ = std::max(0, value);
}

void ResourceManager::addResource(int amount) {
    if (amount > 0) resources_ += amount;
}

bool ResourceManager::consumeResource(int amount) {
    if (amount <= 0) return true;
    if (resources_ < amount) return false;
    resources_ -= amount;
    return true;
}

void ResourceManager::setBaseHealth(int value) {
    baseHealth_ = std::max(0, value);
}

void ResourceManager::damageBase(int damage) {
    if (damage > 0) baseHealth_ = std::max(0, baseHealth_ - damage);
}

void ResourceManager::healBase(int amount) {
    if (amount > 0) baseHealth_ += amount;
}

} // namespace game::core
