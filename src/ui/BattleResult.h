#ifndef BATTLERESULT_H
#define BATTLERESULT_H

#include "core/base/CoreTypes.h"
#include "core/snapshot/BattleSnapshot.h"

#include <QString>
#include <QVector>

enum class BattleOutcome {
    Victory,
    Defeat,
    Draw
};

struct BattleReplayFrame {
    double timeSeconds = 0.0;
    game::core::BattleSnapshot snapshot;
};

struct BattleStatEntry {
    int unitId = -1;
    QString name;
    game::core::ObjectType type = game::core::ObjectType::None;
    int row = -1;
    int col = -1;
    int level = 1;
    int damage = 0;
    int healing = 0;
    int resources = 0;
};

struct BattleHeatPoint {
    int row = 0;
    int col = 0;
    int count = 0;
};

struct BattleMonsterStatEntry {
    game::core::MonsterKind kind = game::core::MonsterKind::AtkNormal;
    QString name;
    int seen = 0;
    int defeated = 0;
    int escaped = 0;
    int peakHp = 0;
    int threatScore = 0;
};

struct BattleReplayData {
    QVector<BattleReplayFrame> frames;
    QVector<BattleStatEntry> unitStats;
    QVector<BattleMonsterStatEntry> monsterStats;
    QVector<BattleHeatPoint> deathHeat;
    QVector<BattleHeatPoint> deploymentHeat;
    int rows = 0;
    int cols = 0;
    double durationSeconds = 0.0;
    int totalDamage = 0;
    int totalHealing = 0;
    int totalResourceGain = 0;
};

struct BattleResult {
    BattleOutcome outcome = BattleOutcome::Defeat;
    bool isPvp = false;
    int wave = 0;
    int defeatedMonsters = 0;
    int escapedMonsters = 0;
    int localCoreHealth = 0;
    int opponentCoreHealth = 0;
    int supplyTicketsEarned = 0;
    QString mapId;
    BattleReplayData replay;
};

#endif // BATTLERESULT_H
