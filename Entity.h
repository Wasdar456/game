#pragma once
#include "GameObject.h"

// ==========================================
// 基类：Entity (继承自 GameObject)
// ==========================================
class Entity : public GameObject {
protected:
    int hp;         
    int maxHp;      
    int attack;     

public:
    Entity(int id, int x, int y, ObjectType t, int max_hp, int atk) 
        : GameObject(id, x, y, t), hp(max_hp), maxHp(max_hp), attack(atk) {}

    virtual ~Entity() = default;

    // ==========================================
    // 提供默认的 draw() 实现
    // ==========================================
    void draw() override {
        // TODO: Phase 4 时，交由 Qt QPainter 进行具体绘制
    }

    // 基础受击逻辑 (注意：整个文件里只能有这一个 takeDamage)
    virtual void takeDamage(int damage) {
        hp -= damage;
        if (hp < 0) hp = 0;
    }

    // ====== 新增：安全的治疗逻辑 ======
    virtual void heal(int amount) {
        hp += amount;
        if (hp > maxHp) hp = maxHp; // 防止加血超过上限
    }

    // 死亡状态判定
    bool isDead() const { return hp <= 0; }

    // 基础数据 Get 接口
    int getHP() const { return hp; }
    int getMaxHP() const { return maxHp; }
    int getAttack() const { return attack; }
    
    // 获取血量百分比 (用于 PVP 拼点)
    float getHpPercent() const { return (float)hp / maxHp; } 
};