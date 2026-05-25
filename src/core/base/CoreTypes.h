#ifndef GAMEPROJECT_CORE_BASE_CORETYPES_H
#define GAMEPROJECT_CORE_BASE_CORETYPES_H

#include <cstdint>
#include <string>

namespace game::core {

// 游戏内所有对象的粗粒度类型。
//
// 这个枚举承担两个职责：
// 1. 让核心逻辑无需 dynamic_cast 也能判断实体属于哪一类；
// 2. 和网络层的紧凑编号保持可映射，方便 PVP 同步部署、怪物和事件。
enum class ObjectType : std::uint8_t {
    None = 0,

    CardAttack,
    CardProduce,
    CardHeal,

    MonsterResBasic,
    MonsterResFast,
    MonsterResTank,
    MonsterAtkNormal,
    MonsterAtkTank,
    MonsterAtkFast,
    MonsterAtkSapper,
    MonsterAtkBerserk,
    MonsterAtkRanged,
    MonsterAtkRegen,

    Terrain
};

// 阵营标识。当前单机/PVP 核心逻辑主要使用 Player 和 Enemy，
// Neutral 预留给资源怪、公共对象或无阵营地形。
enum class Team : std::uint8_t {
    Neutral = 0,
    Player,
    Enemy
};

// 地图格类型。
// Path 供怪物行走，HighGround/FlatLand 可部署，NoDeploy 表示障碍或边界。
// SpawnPoint 为怪物出生点，CoreA/CoreB 为双方核心。
enum class TerrainType : std::uint8_t {
    Path = 0,
    HighGround,
    FlatLand,
    NoDeploy,
    SpawnPoint,    // 怪物出生点
    CoreA,         // A方核心（左下角）
    CoreB          // B方核心（右下角）
};

// 卡牌分类。UI/网络只需要传这个轻量枚举，core 内部再创建具体单位类。
enum class CardKind : std::uint8_t {
    Attack = 0,
    Sniper,
    Aoe,
    Specialist,
    Produce,
    Arsenal,
    Heal,
    HeavyMedic
};

// 怪物分类。顺序与网络模块 ProtocolDef.h 中的 MonsterKind 设计一致。
enum class MonsterKind : std::uint8_t {
    ResBasic = 0,
    ResFast,
    ResTank,
    AtkNormal,
    AtkTank,
    AtkFast,
    AtkSapper,
    AtkBerserk,
    AtkRanged,
    AtkRegen
};

// 怪物路径类型。主路线会伤害基地，资源路线用于争夺资源单位。
enum class RouteType : std::uint8_t {
    MainRoute = 0,
    ResourceRoute
};

// 判断对象类型是否属于玩家卡牌。
inline bool isCardType(ObjectType type) {
    return type == ObjectType::CardAttack ||
           type == ObjectType::CardProduce ||
           type == ObjectType::CardHeal;
}

// 判断对象类型是否属于怪物。依赖 ObjectType 中怪物枚举连续排列。
inline bool isMonsterType(ObjectType type) {
    return type >= ObjectType::MonsterResBasic &&
           type <= ObjectType::MonsterAtkRegen;
}

// 将卡牌种类转换成具体对象类型，供实体构造和快照使用。
inline ObjectType toObjectType(CardKind kind) {
    switch (kind) {
        case CardKind::Attack:
        case CardKind::Sniper:
        case CardKind::Aoe:
        case CardKind::Specialist:
            return ObjectType::CardAttack;
        case CardKind::Produce:
        case CardKind::Arsenal:
            return ObjectType::CardProduce;
        case CardKind::Heal:
        case CardKind::HeavyMedic:
            return ObjectType::CardHeal;
    }
    return ObjectType::None;
}

inline bool isAttackCardKind(CardKind kind) {
    return kind == CardKind::Attack ||
           kind == CardKind::Sniper ||
           kind == CardKind::Aoe ||
           kind == CardKind::Specialist;
}

inline bool isProduceCardKind(CardKind kind) {
    return kind == CardKind::Produce ||
           kind == CardKind::Arsenal;
}

inline bool isHealCardKind(CardKind kind) {
    return kind == CardKind::Heal ||
           kind == CardKind::HeavyMedic;
}

// 将怪物种类转换成具体对象类型，供 Monster 基类构造使用。
inline ObjectType toObjectType(MonsterKind kind) {
    switch (kind) {
        case MonsterKind::ResBasic: return ObjectType::MonsterResBasic;
        case MonsterKind::ResFast: return ObjectType::MonsterResFast;
        case MonsterKind::ResTank: return ObjectType::MonsterResTank;
        case MonsterKind::AtkNormal: return ObjectType::MonsterAtkNormal;
        case MonsterKind::AtkTank: return ObjectType::MonsterAtkTank;
        case MonsterKind::AtkFast: return ObjectType::MonsterAtkFast;
        case MonsterKind::AtkSapper: return ObjectType::MonsterAtkSapper;
        case MonsterKind::AtkBerserk: return ObjectType::MonsterAtkBerserk;
        case MonsterKind::AtkRanged: return ObjectType::MonsterAtkRanged;
        case MonsterKind::AtkRegen: return ObjectType::MonsterAtkRegen;
    }
    return ObjectType::None;
}

// 调试用字符串转换。实现放在 GameObject.cpp，避免头文件膨胀。
std::string toString(ObjectType type);
std::string toString(TerrainType type);

} // namespace game::core

#endif // GAMEPROJECT_CORE_BASE_CORETYPES_H
