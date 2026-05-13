#pragma once
#include "Monster.h"

// ==========================================
// 1. 普通步兵 (标准模板)
// 特点：各项属性标准，作为前期波次的主要敌人。
// ==========================================
class AtkNormalMonster : public Monster {
public:
    AtkNormalMonster(int id, int x, int y)
        // 血量100, 攻击10, 移速1.0, 掉落10, 进家伤害1, 走攻击路线
        : Monster(id, x, y, ObjectType::MONSTER_ATK_NORMAL, 100, 10, 1.0f, 10, 1, RouteType::MAIN_ROUTE) {}
        
    void update() override { 
        // TODO: 如果范围内有目标则攻击，否则执行移动
        AStarMove(); 
    }
    
    void onDeath() override { /* 掉落 10 资源 */ }
};

// ==========================================
// 2. 肉盾怪 (重装坦克)
// 特点：高血量，慢速，用于吸收玩家防御塔火力。
// ==========================================
class AtkTankMonster : public Monster {
public:
    AtkTankMonster(int id, int x, int y)
        // 血量400, 移速0.6, 进家伤害2
        : Monster(id, x, y, ObjectType::MONSTER_ATK_TANK, 400, 15, 0.6f, 15, 2, RouteType::MAIN_ROUTE) {}
        
    void update() override { AStarMove(); }
    
    void onDeath() override {}
};

// ==========================================
// 3. 猎犬怪 (极速突破)
// 特点：低血量，高移速，容易突破防线导致漏怪。
// ==========================================
class AtkFastMonster : public Monster {
public:
    AtkFastMonster(int id, int x, int y)
        // 血量60, 移速1.8
        : Monster(id, x, y, ObjectType::MONSTER_ATK_FAST, 60, 5, 1.8f, 10, 1, RouteType::MAIN_ROUTE) {}
        
    void update() override { AStarMove(); }
    
    void onDeath() override {}
};

// ==========================================
// 4. 拆迁怪 / 工兵 (Sapper - 特殊索敌)
// 特点：会偏离主干道，优先寻找并攻击部署在"高台"上的防御单位。
// ==========================================
class AtkSapperMonster : public Monster {
public:
    AtkSapperMonster(int id, int x, int y)
        : Monster(id, x, y, ObjectType::MONSTER_ATK_SAPPER, 120, 25, 0.9f, 20, 1, RouteType::MAIN_ROUTE) {}
        
    void update() override { AStarMove(); }
    
    // [机制重写]：动态改变寻路目标
    void AStarMove() override {
        // TODO: 扫描附近的 MapGrid，寻找 heightValue == 1 的地块。
        // 如果发现高台且上面有 Card，则临时将寻路目标点设为该高台坐标。
        // 否则，正常沿 MAIN_ROUTE 移动。
    }
    void onDeath() override {}
};

// ==========================================
// 5. 狂暴怪 (Berserk - 受击反馈重写)
// 特点：每次受到攻击，都会永久增加一定的移动速度和攻击力。
// ==========================================
class AtkBerserkMonster : public Monster {
public:
    AtkBerserkMonster(int id, int x, int y)
        : Monster(id, x, y, ObjectType::MONSTER_ATK_BERSERK, 200, 10, 0.8f, 20, 1, RouteType::MAIN_ROUTE) {}
    
    // [机制重写]：劫持受击函数
    void takeDamage(int damage) override {
        Entity::takeDamage(damage); // 调用基类函数，先进行实际扣血
        if (!isDead()) {
            moveSpeed += 0.15f; // 每次没被打死，移速加快
            attack += 2;        // 攻击力提升
            // 可以通知 UI 成员在此处播放一个“激怒”特效
        }
    }
    
    void update() override { AStarMove(); }
    
    void onDeath() override {}
};

// ==========================================
// 6. 远程怪 (自带射程)
// 特点：拥有较远的攻击范围，可以在远处站定输出玩家单位。
// ==========================================
class AtkRangedMonster : public Monster {
public:
    AtkRangedMonster(int id, int x, int y)
        // 注意构造函数的最后一个参数（射程 range）设为 3
        : Monster(id, x, y, ObjectType::MONSTER_ATK_RANGED, 80, 15, 0.9f, 15, 1, RouteType::MAIN_ROUTE, 3) {}
    
    // [机制重写]：索敌与移动互斥
    void update() override {
        // TODO: 检查周围 3 格内是否有玩家 Card。
        // 如果有，则停止 AStarMove()，直接原地发动攻击。
        // 如果没有，才执行 AStarMove()。
    }
    
    void onDeath() override {}
};

// ==========================================
// 7. 自愈怪 (Regen - 持续恢复)
// 特点：存活时，随着时间推移自动恢复血量。
// ==========================================
class AtkRegenMonster : public Monster {
private:
    int regenTimer = 0; // 自愈内部计时器

public:
    AtkRegenMonster(int id, int x, int y)
        : Monster(id, x, y, ObjectType::MONSTER_ATK_REGEN, 150, 10, 0.8f, 25, 1, RouteType::MAIN_ROUTE) {}
    
    // [机制重写]：每帧回血逻辑
    void update() override {
        regenTimer++;
        if (regenTimer >= 60) { // 假设游戏运行在 60 帧/秒，即每秒触发一次
            hp += 5;            // 每秒回血 5 点
            if (hp > maxHp) hp = maxHp; // 防止血量溢出
            regenTimer = 0;
        }
        
        AStarMove(); // 回完血继续走
    }
    
    void onDeath() override {}
};