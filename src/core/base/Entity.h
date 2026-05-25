#ifndef GAMEPROJECT_CORE_BASE_ENTITY_H
#define GAMEPROJECT_CORE_BASE_ENTITY_H

#include "core/base/GameObject.h"
#include <algorithm>

namespace game::core {

// 具有生命值、攻击力和阵营的动态实体基类。
//
// 玩家卡牌和怪物都继承 Entity。它只处理通用战斗属性，
// 不知道“部署”“路线”“自动技能”等更高层规则。
class Entity : public GameObject {
public:
    Entity(int id, MapPosition position, ObjectType type, Team team,
           int maxHp, int attack);
    ~Entity() override = default;

    int hp() const { return hp_; }
    int maxHp() const { return maxHp_; }
    int attack() const { return attack_; }
    Team team() const { return team_; }

    // 兼容旧版接口名称，方便旧逻辑迁移。
    int getHP() const { return hp_; }
    int getMaxHP() const { return maxHp_; }
    int getAttack() const { return attack_; }

    // 当前血量百分比，拼点和治疗索敌会使用。
    float getHpPercent() const;

    void setAttack(int attack) { attack_ = attack; }
    void setTeam(Team team) { team_ = team; }
    void setMaxHp(int maxHp);
    void setHp(int hp);

    // 扣血和治疗都在 Entity 中做边界保护，派生类只处理特殊触发。
    virtual void takeDamage(int damage);
    virtual void heal(int amount);
    bool isDead() const { return hp_ <= 0; }

protected:
    // 当前生命值。
    int hp_;
    // 最大生命值。
    int maxHp_;
    // 基础攻击力。
    int attack_;
    // 所属阵营。
    Team team_;
};

} // namespace game::core

#endif // GAMEPROJECT_CORE_BASE_ENTITY_H
