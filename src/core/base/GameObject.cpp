#include "core/base/GameObject.h"

namespace game::core {

GameObject::GameObject(int id, MapPosition position, ObjectType type)
    : id_(id), position_(position), type_(type) {}

void GameObject::update(double) {}

void GameObject::draw() {}

std::string toString(ObjectType type) {
    switch (type) {
        case ObjectType::None: return "None";
        case ObjectType::CardAttack: return "CardAttack";
        case ObjectType::CardProduce: return "CardProduce";
        case ObjectType::CardHeal: return "CardHeal";
        case ObjectType::MonsterResBasic: return "MonsterResBasic";
        case ObjectType::MonsterResFast: return "MonsterResFast";
        case ObjectType::MonsterResTank: return "MonsterResTank";
        case ObjectType::MonsterAtkNormal: return "MonsterAtkNormal";
        case ObjectType::MonsterAtkTank: return "MonsterAtkTank";
        case ObjectType::MonsterAtkFast: return "MonsterAtkFast";
        case ObjectType::MonsterAtkSapper: return "MonsterAtkSapper";
        case ObjectType::MonsterAtkBerserk: return "MonsterAtkBerserk";
        case ObjectType::MonsterAtkRanged: return "MonsterAtkRanged";
        case ObjectType::MonsterAtkRegen: return "MonsterAtkRegen";
        case ObjectType::Terrain: return "Terrain";
    }
    return "Unknown";
}

std::string toString(TerrainType type) {
    switch (type) {
        case TerrainType::Path: return "Path";
        case TerrainType::HighGround: return "HighGround";
        case TerrainType::FlatLand: return "FlatLand";
        case TerrainType::NoDeploy: return "NoDeploy";
    }
    return "Unknown";
}

} // namespace game::core
