#ifndef GAMEPROJECT_CORE_DATA_CARDSPECS_H
#define GAMEPROJECT_CORE_DATA_CARDSPECS_H

#include "core/base/Constants.h"
#include "core/base/CoreTypes.h"
#include "core/combat/Projectile.h"

#include <array>
#include <cstddef>

namespace game::core {

struct CardSpec {
    CardKind kind = CardKind::Attack;
    const char* key = "attack";
    const char* name = "Kiwi Scout";
    const char* priorityText = "";
    int deployCost = 0;
    int maxHp = 0;
    int attack = 0;
    int attackRange = 0;
    int moveLimit = 0;
    double skillCooldownSeconds = 0.0;
    ProjectileKind projectileKind = ProjectileKind::Bullet;
    int splashRadius = 0;
    int healAmount = 0;
    int resourceYield = 0;
};

inline constexpr int CollectionMaxLevel = 3;

inline constexpr int collectionUpgradeCostForLevel(int currentLevel)
{
    return currentLevel == 1 ? 30
         : currentLevel == 2 ? 60
         : 0;
}

inline constexpr std::array<CardSpec, 10> kCardSpecs = {{
    {
        CardKind::Attack,
        "attack",
        "Kiwi Scout",
        "Sapper > Ranged > Tank > Normal > Fast > Berserk > Regen > Resource",
        constants::DeployCostAttack,
        100,
        20,
        3,
        2,
        1.0,
        ProjectileKind::Bullet,
        0,
        0,
        0
    },
    {
        CardKind::Sniper,
        "sniper",
        "Sniper Berry",
        "Sapper > Ranged > Tank > Normal > Fast > Berserk > Regen > Resource",
        50,
        400,
        35,
        5,
        1,
        2.0,
        ProjectileKind::Sniper,
        0,
        0,
        0
    },
    {
        CardKind::Aoe,
        "aoe",
        "Orange Bomber",
        "Sapper > Ranged > Tank > Normal > Fast > Berserk > Regen > Resource",
        60,
        500,
        18,
        3,
        1,
        1.5,
        ProjectileKind::Aoe,
        1,
        0,
        0
    },
    {
        CardKind::Specialist,
        "specialist",
        "Watermelon Tank",
        "Sapper > Ranged > Tank > Normal > Fast > Berserk > Regen > Resource",
        55,
        450,
        30,
        4,
        3,
        1.2,
        ProjectileKind::Bullet,
        0,
        0,
        0
    },
    {
        CardKind::Produce,
        "produce",
        "Miner Pine",
        "No target. Generates resources whenever cooldown completes.",
        constants::DeployCostProduce,
        80,
        0,
        0,
        1,
        5.0,
        ProjectileKind::Bullet,
        0,
        0,
        25
    },
    {
        CardKind::Arsenal,
        "arsenal",
        "Mango Engineer",
        "No target. Generates resources whenever cooldown completes.",
        80,
        500,
        0,
        0,
        0,
        4.0,
        ProjectileKind::Bullet,
        0,
        0,
        40
    },
    {
        CardKind::Heal,
        "heal",
        "Peach Healer",
        "Lowest HP% injured ally > other injured allies",
        constants::DeployCostHeal,
        60,
        0,
        2,
        2,
        2.0,
        ProjectileKind::Bullet,
        0,
        15,
        0
    },
    {
        CardKind::HeavyMedic,
        "heavy_medic",
        "Coco Defender",
        "Lowest HP% injured ally > other injured allies",
        60,
        600,
        0,
        2,
        1,
        2.5,
        ProjectileKind::Bullet,
        0,
        25,
        0
    },
    {
        CardKind::Attack2,
        "attack2",
        "Grape Blaster",
        "Sapper > Ranged > Tank > Normal > Fast > Berserk > Regen > Resource",
        constants::DeployCostAttack,
        100,
        20,
        3,
        2,
        1.0,
        ProjectileKind::Bullet,
        0,
        0,
        0
    },
    {
        CardKind::Heal2,
        "heal2",
        "Papaya Support",
        "Lowest HP% injured ally > other injured allies",
        constants::DeployCostHeal,
        60,
        0,
        2,
        2,
        2.0,
        ProjectileKind::Bullet,
        0,
        15,
        0
    }
}};

inline const CardSpec& cardSpec(CardKind kind)
{
    for (const auto& spec : kCardSpecs) {
        if (spec.kind == kind) {
            return spec;
        }
    }
    return kCardSpecs.front();
}

inline const char* cardName(CardKind kind)
{
    return cardSpec(kind).name;
}

inline const char* cardKey(CardKind kind)
{
    return cardSpec(kind).key;
}

} // namespace game::core

#endif // GAMEPROJECT_CORE_DATA_CARDSPECS_H
