#ifndef GAMEPROJECT_CORE_SNAPSHOT_PROJECTILESNAPSHOT_H
#define GAMEPROJECT_CORE_SNAPSHOT_PROJECTILESNAPSHOT_H

#include "core/combat/Projectile.h"

namespace game::core {

struct ProjectileSnapshot {
    int sourceId = 0;
    int targetId = 0;
    int fromRow = 0;
    int fromCol = 0;
    int toRow = 0;
    int toCol = 0;
    double progress = 0.0;
    ProjectileKind kind = ProjectileKind::Bullet;
    int splashRadius = 0;
};

} // namespace game::core

#endif // GAMEPROJECT_CORE_SNAPSHOT_PROJECTILESNAPSHOT_H
