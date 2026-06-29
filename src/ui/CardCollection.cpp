#include "ui/CardCollection.h"
#include "core/data/CardSpecs.h"

#include <QDateTime>
#include <QRandomGenerator>
#include <QSettings>

namespace {

constexpr int kInitialTickets = 30;
constexpr int kDuplicateFragments = 10;
QString ownedKey(game::core::CardKind kind)
{
    return QString("collection/owned/%1").arg(CardCollection::key(kind));
}

QString fragmentKey(game::core::CardKind kind)
{
    return QString("collection/fragments/%1").arg(CardCollection::key(kind));
}

QString levelKey(game::core::CardKind kind)
{
    return QString("collection/level/%1").arg(CardCollection::key(kind));
}

struct DrawPoolEntry {
    game::core::CardKind kind;
    int weight;
};

const QVector<DrawPoolEntry>& drawPool()
{
    static const QVector<DrawPoolEntry> pool = {
        {game::core::CardKind::Attack, 11},
        {game::core::CardKind::Attack2, 11},
        {game::core::CardKind::Produce, 20},
        {game::core::CardKind::Heal, 8},
        {game::core::CardKind::Heal2, 8},
        {game::core::CardKind::Sniper, 14},
        {game::core::CardKind::Specialist, 10},
        {game::core::CardKind::Aoe, 8},
        {game::core::CardKind::HeavyMedic, 6},
        {game::core::CardKind::Arsenal, 4},
    };
    return pool;
}

game::core::CardKind rollKind()
{
    int total = 0;
    for (const auto& entry : drawPool()) {
        total += entry.weight;
    }
    int roll = QRandomGenerator::global()->bounded(qMax(1, total));
    for (const auto& entry : drawPool()) {
        if (roll < entry.weight) {
            return entry.kind;
        }
        roll -= entry.weight;
    }
    return game::core::CardKind::Attack;
}

} // namespace

void CardCollection::initializeDefaults()
{
    QSettings settings;
    if (settings.value("collection/initializedV1", false).toBool()) {
        return;
    }

    settings.setValue("collection/initializedV1", true);
    settings.setValue("collection/tickets", kInitialTickets);
    settings.setValue(ownedKey(game::core::CardKind::Produce), true);
    settings.setValue(ownedKey(game::core::CardKind::Attack), true);
    settings.setValue(levelKey(game::core::CardKind::Produce), 1);
    settings.setValue(levelKey(game::core::CardKind::Attack), 1);
    settings.sync();
}

void CardCollection::resetToDefaults()
{
    QSettings settings;
    settings.beginGroup("collection");
    settings.remove("");
    settings.endGroup();

    settings.setValue("collection/initializedV1", true);
    settings.setValue("collection/tickets", kInitialTickets);
    settings.setValue(ownedKey(game::core::CardKind::Produce), true);
    settings.setValue(ownedKey(game::core::CardKind::Attack), true);
    settings.setValue(levelKey(game::core::CardKind::Produce), 1);
    settings.setValue(levelKey(game::core::CardKind::Attack), 1);
    settings.sync();
}

int CardCollection::tickets()
{
    initializeDefaults();
    return QSettings().value("collection/tickets", 0).toInt();
}

void CardCollection::addTickets(int amount)
{
    initializeDefaults();
    QSettings settings;
    settings.setValue("collection/tickets", qMax(0, tickets() + amount));
}

bool CardCollection::spendTickets(int amount)
{
    initializeDefaults();
    if (tickets() < amount) {
        return false;
    }
    QSettings settings;
    settings.setValue("collection/tickets", tickets() - amount);
    return true;
}

bool CardCollection::isOwned(game::core::CardKind kind)
{
    initializeDefaults();
    return QSettings().value(ownedKey(kind), false).toBool();
}

void CardCollection::unlock(game::core::CardKind kind)
{
    initializeDefaults();
    QSettings settings;
    settings.setValue(ownedKey(kind), true);
    if (settings.value(levelKey(kind), 0).toInt() <= 0) {
        settings.setValue(levelKey(kind), 1);
    }
}

int CardCollection::fragments(game::core::CardKind kind)
{
    initializeDefaults();
    return QSettings().value(fragmentKey(kind), 0).toInt();
}

int CardCollection::level(game::core::CardKind kind)
{
    initializeDefaults();
    return isOwned(kind) ? qMax(1, QSettings().value(levelKey(kind), 1).toInt()) : 0;
}

int CardCollection::upgradeCost(game::core::CardKind kind)
{
    const int current = level(kind);
    if (current <= 0 || current >= game::core::CollectionMaxLevel) {
        return 0;
    }
    return game::core::collectionUpgradeCostForLevel(current);
}

bool CardCollection::canUpgrade(game::core::CardKind kind)
{
    const int cost = upgradeCost(kind);
    return cost > 0 && fragments(kind) >= cost;
}

bool CardCollection::upgrade(game::core::CardKind kind)
{
    if (!canUpgrade(kind)) {
        return false;
    }
    QSettings settings;
    const int cost = upgradeCost(kind);
    settings.setValue(fragmentKey(kind), fragments(kind) - cost);
    settings.setValue(levelKey(kind), level(kind) + 1);
    return true;
}

DrawResult CardCollection::drawOne()
{
    DrawResult result;
    if (!spendTickets(1)) {
        result.ticketsLeft = tickets();
        return result;
    }

    result.kind = rollKind();
    result.isNew = !isOwned(result.kind);
    if (result.isNew) {
        unlock(result.kind);
    } else {
        QSettings settings;
        result.fragmentsGained = kDuplicateFragments;
        settings.setValue(fragmentKey(result.kind),
                          fragments(result.kind) + result.fragmentsGained);
    }
    result.ticketsLeft = tickets();
    return result;
}

QVector<DrawResult> CardCollection::drawMany(int count)
{
    QVector<DrawResult> results;
    results.reserve(count);
    for (int i = 0; i < count && tickets() > 0; ++i) {
        results.append(drawOne());
    }
    return results;
}

QString CardCollection::key(game::core::CardKind kind)
{
    return QString::fromUtf8(game::core::cardKey(kind));
}
