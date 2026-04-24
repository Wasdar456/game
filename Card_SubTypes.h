#pragma once
#include "Card.h"
#include "PlayerState.h"
#include "MapGrid.h"
#include <iostream>
#include <algorithm> // 为了使用 std::sort
#include <cmath>     // 为了使用 std::abs (如果代码中用到了)
// 引用外部地图数组，用于查询高度
// 假设你在外部维护了全场的怪物列表
class Monster; 
extern std::vector<Entity*> g_AllMonsters; 

// ====== 新增下面这一行，告诉编译器 g_AllCards 是存在的 ======
extern std::vector<Entity*> g_AllCards;
extern MapGrid* g_Map[20][30]; 

// 假设你在外部（比如 main.cpp 或 GameManager 中）维护了全场的怪物列表
// 我们需要引入它，以便防御塔能在 update() 中获取所有敌人进行索敌
class Monster; 
extern std::vector<Entity*> g_AllMonsters; 

// ==========================================
// 1. 攻击型单位 (AttackUnit) - 已完整实现高低差减伤
// ==========================================
class AttackUnit : public Card {
public:
    AttackUnit(int id, int x, int y) 
        : Card(id, x, y, ObjectType::CARD_ATTACK, 100, 20, 3, 2, 600) {
        priorityList = { ObjectType::MONSTER_ATK_SAPPER, ObjectType::MONSTER_ATK_TANK, ObjectType::MONSTER_ATK_NORMAL };
    }

    // 重写每帧更新逻辑：负责索敌与攻击
    void update() override {
        tickCooldown();
        
        // 如果攻击冷却/技能冷却完毕，开始寻找目标并攻击
        if (isSkillReady()) {
            // 1. 调用基类的 findTarget (内部已经包含了高台射程+1的逻辑)
            Entity* target = findTarget(g_AllMonsters);
            
            if (target != nullptr) {
                // 2. 基础伤害
                int finalDamage = attack; 
                
                // 3. --- 核心机制：地形高低差伤害惩罚 ---
                // 获取攻击者(塔)和目标(怪)的当前坐标
                int targetX = target->getX();
                int targetY = target->getY();
                
                // 确保坐标在地图范围内，防止数组越界崩溃
                if (posX >= 0 && posX < 30 && posY >= 0 && posY < 20 &&
                    targetX >= 0 && targetX < 30 && targetY >= 0 && targetY < 20) {
                    
                    // 读取 MapGrid 的高度值
                    int attackerHeight = g_Map[posY][posX]->getHeight();
                    int targetHeight = g_Map[targetY][targetX]->getHeight();
                    
                    // 减伤计算：若攻击者高度低于目标高度，则伤害降低 30%
                    if (attackerHeight < targetHeight) {
                        finalDamage = static_cast<int>(finalDamage * 0.7f);
                        std::cout << "触发高低差减伤! 实际造成伤害: " << finalDamage << std::endl;
                    }
                }

                // 4. 执行扣血
                target->takeDamage(finalDamage);
                
                // 5. 重置攻击冷却
                resetCooldown(); 
            }
        }
    }

    void autoSkill() override { 
        // 过载技能等特殊逻辑可以在这里扩充
        std::cout << "AttackUnit " << id << " Overload Skill activated!\n"; 
    }
    
    
};

// ... ProduceUnit 和 HealUnit 保持原样即可 ...

// ==========================================
// 2. 生产型单位 (ProduceUnit)
// ==========================================
class ProduceUnit : public Card {
private:
    int resourceYield; 

public:
    ProduceUnit(int id, int x, int y)
        : Card(id, x, y, ObjectType::CARD_PRODUCE, 80, 0, 0, 1, 300), 
          resourceYield(25) {
        // priorityList 留空，因为它不需要索敌
    }

    void autoSkill() override {
        PlayerState::getInstance()->addResource(resourceYield);
        resetCooldown();
        std::cout << "ProduceUnit " << id << " 产出了 " << resourceYield << " 点资源！" << std::endl;
    }

    void update() override {
        tickCooldown(); 
        if (isSkillReady()) autoSkill();
    }
    
    
};

// ==========================================
// 3. 治疗型单位 (HealUnit)
// ==========================================
class HealUnit : public Card {
private:
    int healAmount; 

public:
    HealUnit(int id, int x, int y)
        : Card(id, x, y, ObjectType::CARD_HEAL, 60, 0, 2, 2, 120), 
          healAmount(15) {
        // 注意：这里的 priorityList 实际上在重写的 findTarget 中被弃用了
    }

    void autoSkill() override {
        resetCooldown();
        std::cout << "HealUnit " << id << " 发动技能：群体治疗！" << std::endl;
    }

    void update() override {
    tickCooldown();
    if (isSkillReady()) {
        // 假设 g_AllCards 是维护的所有玩家单位列表
        Entity* target = findTarget(g_AllCards); 
        if (target != nullptr) {
            target->heal(15); // 给队友回血
            autoSkill();      // 播放技能特效并重置冷却
        }
    }
}

    // ==========================================
    // [核心重写] 治疗单位专属索敌逻辑
    // ==========================================
    Entity* findTarget(const std::vector<Entity*>& targets) override {
        std::vector<Entity*> validTargets;

        // 第一步：严格筛选 (阵营判定 & 血量判定 & 射程判定)
        for (Entity* target : targets) {
            if (target == nullptr || target->isDead()) continue; 

            // 阵营判定：只找自己人（过滤掉所有的 Monster）
            ObjectType t = target->getType();
            if (t != ObjectType::CARD_ATTACK && 
                t != ObjectType::CARD_PRODUCE && 
                t != ObjectType::CARD_HEAL) {
                continue; 
            }

            // 血量判定：满血的单位不需要治疗
            if (target->getHP() >= target->getMaxHP()) {
                continue;
            }

            // 射程判定：计算曼哈顿距离
            int dist = std::abs(target->getX() - this->getX()) + std::abs(target->getY() - this->getY());
            if (dist <= attackRange) {
                validTargets.push_back(target);
            }
        }

        // 如果范围内没有受伤的友方单位，直接返回 nullptr
        if (validTargets.empty()) return nullptr;

        // 第二步：自定义排序 (救死扶伤原则)
        std::sort(validTargets.begin(), validTargets.end(), [this](Entity* a, Entity* b) {
            float hpPercentA = a->getHpPercent();
            float hpPercentB = b->getHpPercent();

            // 规则 1：优先治疗【血量百分比最低】的单位
            if (hpPercentA != hpPercentB) return hpPercentA < hpPercentB; 

            // 规则 2：如果血量百分比一样危急，优先治疗【距离更近】的单位
            int distA = std::abs(a->getX() - this->getX()) + std::abs(a->getY() - this->getY());
            int distB = std::abs(b->getX() - this->getX()) + std::abs(b->getY() - this->getY());
            if (distA != distB) return distA < distB;

            // 规则 3：终极兜底，比较唯一 ID，保证联机计算确定性
            return a->getID() < b->getID();
        });

        // 第三步：返回最需要治疗的单位
        return validTargets.front();
    }
    
   
};