#ifndef CARDCOLLECTION_H
#define CARDCOLLECTION_H

#include "core/base/CoreTypes.h"

#include <QString>
#include <QVector>

struct DrawResult {
    game::core::CardKind kind = game::core::CardKind::Attack;
    bool isNew = false;
    int fragmentsGained = 0;
    int ticketsLeft = 0;
};

class CardCollection
{
public:
    static void initializeDefaults();
    static void resetToDefaults();
    static int tickets();
    static void addTickets(int amount);
    static bool spendTickets(int amount);

    static bool isOwned(game::core::CardKind kind);
    static void unlock(game::core::CardKind kind);
    static int fragments(game::core::CardKind kind);
    static int level(game::core::CardKind kind);
    static int upgradeCost(game::core::CardKind kind);
    static bool canUpgrade(game::core::CardKind kind);
    static bool upgrade(game::core::CardKind kind);

    static DrawResult drawOne();
    static QVector<DrawResult> drawMany(int count);

    static QString key(game::core::CardKind kind);
};

#endif // CARDCOLLECTION_H
