#pragma once
#include "Monster.h"

// ==========================================
// 1. 普通小金猪 (基础资源怪)
// 特点：各项属性均衡，掉落中等资源。
// ==========================================
class ResBasicMonster : public Monster {
public:
    ResBasicMonster(int id, int x, int y)
        // 参数依次为：ID, X, Y, 类型, 血量(100), 攻击(0), 移速(1.0), 掉落(30), 进家伤害(0), 路线
        : Monster(id, x, y, ObjectType::MONSTER_RES_BASIC, 100, 0, 1.0f, 30, 0, RouteType::RESOURCE_ROUTE) {}

    void update() override { AStarMove(); } // 资源怪只需移动，无需索敌
    
    void onDeath() override { /* 通知 PlayerState 增加 30 资源 */ }
};

// ==========================================
// 2. 盗宝飞贼 (高移速资源怪)
// 特点：血量极低，但移动速度极快，考验玩家的爆发输出。
// ==========================================
class ResFastMonster : public Monster {
public:
    ResFastMonster(int id, int x, int y)
        // 血量降至50，移速翻倍至2.0，掉落提升至40
        : Monster(id, x, y, ObjectType::MONSTER_RES_FAST, 50, 0, 2.0f, 40, 0, RouteType::RESOURCE_ROUTE) {}

    void update() override { AStarMove(); }
    
    void onDeath() override { /* 通知 PlayerState 增加 40 资源 */ }
};

// ==========================================
// 3. 移动金库 (重装甲资源怪)
// 特点：血量极厚，移动极慢，击杀后获得海量资源。
// ==========================================
class ResTankMonster : public Monster {
public:
    ResTankMonster(int id, int x, int y)
        // 血量高达500，移速极慢0.4，掉落海量资源150
        : Monster(id, x, y, ObjectType::MONSTER_RES_TANK, 500, 0, 0.4f, 150, 0, RouteType::RESOURCE_ROUTE) {}

    void update() override { AStarMove(); }
    
    void onDeath() override { /* 通知 PlayerState 增加 150 资源 */ }
};