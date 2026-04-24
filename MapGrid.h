#pragma once

// 提前声明 Entity 类，避免头文件循环引用
class Entity; 

// ==========================================
// 枚举：TerrainType (地形类型)
// ==========================================
enum class TerrainType {
    PATH,           // 路径（怪物走，不可部署）
    HIGH_GROUND,    // 高台（部署优势：射程+1，被低处攻击减伤）
    FLAT_LAND,      // 平地（普通可部署区块）
    NO_DEPLOY       // 不可部署区块（障碍物/边界）
};

// ==========================================
// 基础类：MapGrid (地图网格)
// 作用：负责承载具体的地块属性，与成员 B 配合初始化。
// ==========================================
class MapGrid {
private:
    TerrainType terrainType; 
    bool isOccupied;         // 当前格是否已有单位占据
    int heightValue;         // 高度值（0 = 地面，1 = 高台）
    
    // 极其重要的指针：记录当前是哪个实体(塔或怪)占据了这块格子
    // 这对于你实现 PVP "拼点踩死" 和 检查格子是否为空 非常关键
    Entity* occupant;        

public:
    MapGrid(TerrainType type = TerrainType::FLAT_LAND, int height = 0) 
        : terrainType(type), isOccupied(false), heightValue(height), occupant(nullptr) {}

    // --- 给成员 A（你）使用的查询接口 ---
    bool getIsOccupied() const { return isOccupied; }
    int getHeight() const { return heightValue; }
    TerrainType getType() const { return terrainType; }
    Entity* getOccupant() const { return occupant; }

    // --- 状态更新接口（当你瞬移或部署单位时调用） ---
    void setOccupied(bool status, Entity* entity = nullptr) {
        isOccupied = status;
        occupant = entity;
    }
};