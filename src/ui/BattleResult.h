#ifndef BATTLERESULT_H
#define BATTLERESULT_H

#include <QString>

enum class BattleOutcome {
    Victory,
    Defeat,
    Draw
};

struct BattleResult {
    BattleOutcome outcome = BattleOutcome::Defeat;
    bool isPvp = false;
    int wave = 0;
    int defeatedMonsters = 0;
    int escapedMonsters = 0;
    int localCoreHealth = 0;
    int opponentCoreHealth = 0;
    QString mapId;
};

#endif // BATTLERESULT_H
