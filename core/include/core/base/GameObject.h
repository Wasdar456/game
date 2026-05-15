#ifndef GAMEPROJECT_CORE_BASE_GAMEOBJECT_H
#define GAMEPROJECT_CORE_BASE_GAMEOBJECT_H

#include "core/base/CoreTypes.h"
#include "core/map/MapPosition.h"

namespace game::core {

// 所有地图对象的最小公共基类。
//
// GameObject 只保存“身份”和“位置”这类非常稳定的信息。
// 真正的生命值、攻击、技能、路径等行为放在 Entity/Card/Monster 中，
// 这样地形、特效或未来的非战斗对象也可以复用这个根类型。
class GameObject {
public:
    // id 由系统层分配，用于 UI 展示、网络同步和状态校验。
    GameObject(int id, MapPosition position, ObjectType type);
    virtual ~GameObject() = default;

    // 每帧更新入口。基类默认空实现，派生类按需覆盖。
    virtual void update(double deltaSeconds);

    // 渲染入口占位。core 不依赖 Qt，真实绘制由 UI 层根据 snapshot 完成。
    virtual void draw();

    // 现代命名风格访问器。
    int id() const { return id_; }
    int row() const { return position_.row; }
    int col() const { return position_.col; }

    // 兼容旧版核心代码命名，降低迁移和联调成本。
    int getID() const { return id_; }
    int getX() const { return position_.col; }
    int getY() const { return position_.row; }
    MapPosition position() const { return position_; }
    ObjectType type() const { return type_; }
    ObjectType getType() const { return type_; }

    // 坐标采用 row/col 表示。旧代码中的 X/Y 映射为 col/row。
    void setPosition(MapPosition position) { position_ = position; }
    void setPosition(int row, int col) { position_ = MapPosition(row, col); }

protected:
    // 全局唯一对象 id。
    int id_;
    // 网格坐标。
    MapPosition position_;
    // 对象类型，用于索敌和快照展示。
    ObjectType type_;
};

} // namespace game::core

#endif // GAMEPROJECT_CORE_BASE_GAMEOBJECT_H
