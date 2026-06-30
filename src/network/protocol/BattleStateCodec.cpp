#include "network/protocol/BattleStateCodec.h"
#include <QDataStream>
#include <QIODevice>

namespace game::network {

QByteArray BattleStateCodec::encodeHostSnapshot(const game::core::BattleSnapshot& snapshot)
{
    QByteArray body;
    QDataStream out(&body, QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::BigEndian);

    out << static_cast<qint32>(snapshot.currentWave)
        << static_cast<quint8>(snapshot.waveActive ? 1 : 0)
        << static_cast<qint32>(snapshot.resources)
        << static_cast<qint32>(snapshot.baseHealth)
        << static_cast<qint32>(snapshot.opponentResources)
        << static_cast<qint32>(snapshot.opponentBaseHealth)
        << static_cast<qint32>(snapshot.defeatedMonsters)
        << static_cast<qint32>(snapshot.escapedMonsters)
        << static_cast<quint16>(snapshot.units.size());

    for (const auto& unit : snapshot.units) {
        out << static_cast<qint32>(unit.id)
            << static_cast<quint8>(unit.kind)
            << static_cast<quint8>(unit.type)
            << static_cast<qint16>(unit.row)
            << static_cast<qint16>(unit.col)
            << static_cast<qint32>(unit.hp)
            << static_cast<qint32>(unit.maxHp)
            << static_cast<qint32>(unit.attack)
            << static_cast<qint16>(unit.level)
            << static_cast<qint16>(unit.range)
            << static_cast<qint16>(unit.moveLimit)
            << static_cast<qint32>(unit.deployCost);
    }

    out << static_cast<quint16>(snapshot.monsters.size());
    for (const auto& monster : snapshot.monsters) {
        out << static_cast<qint32>(monster.id)
            << static_cast<quint8>(monster.kind)
            << static_cast<qint16>(monster.row)
            << static_cast<qint16>(monster.col)
            << static_cast<qint32>(monster.hp)
            << static_cast<qint32>(monster.maxHp)
            << static_cast<quint8>(monster.escaped ? 1 : 0);
    }

    out << static_cast<quint16>(snapshot.projectiles.size());
    for (const auto& projectile : snapshot.projectiles) {
        out << static_cast<qint32>(projectile.sourceId)
            << static_cast<qint32>(projectile.targetId)
            << static_cast<qint16>(projectile.fromRow)
            << static_cast<qint16>(projectile.fromCol)
            << static_cast<qint16>(projectile.toRow)
            << static_cast<qint16>(projectile.toCol)
            << static_cast<double>(projectile.progress)
            << static_cast<quint8>(projectile.kind)
            << static_cast<qint16>(projectile.splashRadius);
    }

    out << static_cast<quint32>(checksum(snapshot));
    return body;
}

BattleStateDecodeResult BattleStateCodec::decodeHostSnapshot(const QByteArray& body,
                                                             const game::core::MapSnapshot& map)
{
    BattleStateDecodeResult result;
    QDataStream in(body);
    in.setByteOrder(QDataStream::BigEndian);

    qint32 currentWave = 0;
    quint8 waveActive = 0;
    qint32 hostResources = 0;
    qint32 hostBaseHealth = 0;
    qint32 clientResources = 0;
    qint32 clientBaseHealth = 0;
    qint32 defeatedMonsters = 0;
    qint32 escapedMonsters = 0;
    quint16 unitCount = 0;
    in >> currentWave >> waveActive >> hostResources >> hostBaseHealth
       >> clientResources >> clientBaseHealth >> defeatedMonsters >> escapedMonsters
       >> unitCount;
    if (in.status() != QDataStream::Ok) return result;

    auto& snapshot = result.snapshot;
    snapshot.currentWave = currentWave;
    snapshot.waveActive = waveActive != 0;
    snapshot.resources = clientResources;
    snapshot.baseHealth = clientBaseHealth;
    snapshot.opponentResources = hostResources;
    snapshot.opponentBaseHealth = hostBaseHealth;
    snapshot.defeatedMonsters = defeatedMonsters;
    snapshot.escapedMonsters = escapedMonsters;
    snapshot.map = map;

    snapshot.units.reserve(unitCount);
    for (quint16 i = 0; i < unitCount; ++i) {
        qint32 id = 0;
        quint8 kind = 0;
        quint8 type = 0;
        qint16 row = 0;
        qint16 col = 0;
        qint32 hp = 0;
        qint32 maxHp = 0;
        qint32 attack = 0;
        qint16 level = 0;
        qint16 range = 0;
        qint16 moveLimit = 0;
        qint32 deployCost = 0;
        in >> id >> kind >> type >> row >> col >> hp >> maxHp >> attack >> level >> range >> moveLimit
           >> deployCost;
        if (in.status() != QDataStream::Ok) return result;

        game::core::UnitSnapshot unit;
        unit.id = id;
        unit.kind = static_cast<game::core::CardKind>(kind);
        unit.type = static_cast<game::core::ObjectType>(type);
        unit.row = row;
        unit.col = col;
        unit.hp = hp;
        unit.maxHp = maxHp;
        unit.attack = attack;
        unit.level = level;
        unit.range = range;
        unit.moveLimit = moveLimit;
        unit.deployCost = deployCost;
        snapshot.units.push_back(unit);
    }

    quint16 monsterCount = 0;
    in >> monsterCount;
    if (in.status() != QDataStream::Ok) return result;

    snapshot.monsters.reserve(monsterCount);
    for (quint16 i = 0; i < monsterCount; ++i) {
        qint32 id = 0;
        quint8 kind = 0;
        qint16 row = 0;
        qint16 col = 0;
        qint32 hp = 0;
        qint32 maxHp = 0;
        quint8 escaped = 0;
        in >> id >> kind >> row >> col >> hp >> maxHp >> escaped;
        if (in.status() != QDataStream::Ok) return result;

        game::core::MonsterSnapshot monster;
        monster.id = id;
        monster.kind = static_cast<game::core::MonsterKind>(kind);
        monster.row = row;
        monster.col = col;
        monster.hp = hp;
        monster.maxHp = maxHp;
        monster.escaped = escaped != 0;
        snapshot.monsters.push_back(monster);
    }

    quint16 projectileCount = 0;
    in >> projectileCount;
    if (in.status() != QDataStream::Ok) return result;

    snapshot.projectiles.reserve(projectileCount);
    for (quint16 i = 0; i < projectileCount; ++i) {
        qint32 sourceId = 0;
        qint32 targetId = 0;
        qint16 fromRow = 0;
        qint16 fromCol = 0;
        qint16 toRow = 0;
        qint16 toCol = 0;
        double progress = 0.0;
        quint8 kind = 0;
        qint16 splashRadius = 0;
        in >> sourceId >> targetId >> fromRow >> fromCol >> toRow >> toCol
           >> progress >> kind >> splashRadius;
        if (in.status() != QDataStream::Ok) return result;

        game::core::ProjectileSnapshot projectile;
        projectile.sourceId = sourceId;
        projectile.targetId = targetId;
        projectile.fromRow = fromRow;
        projectile.fromCol = fromCol;
        projectile.toRow = toRow;
        projectile.toCol = toCol;
        projectile.progress = progress;
        projectile.kind = static_cast<game::core::ProjectileKind>(kind);
        projectile.splashRadius = splashRadius;
        snapshot.projectiles.push_back(projectile);
    }

    snapshot.gameOver = snapshot.baseHealth <= 0 || snapshot.opponentBaseHealth <= 0;
    result.ok = true;

    if (!in.atEnd()) {
        result.checksumPresent = true;
        in >> result.remoteChecksum;
        if (in.status() != QDataStream::Ok) {
            result.ok = false;
            result.checksumValid = false;
            return result;
        }
        result.localChecksum = checksum(toHostPerspective(snapshot));
        result.checksumValid = result.remoteChecksum == result.localChecksum;
    }

    return result;
}

quint32 BattleStateCodec::checksum(const game::core::BattleSnapshot& snapshot)
{
    quint32 hash = 2166136261u;
    auto mix = [&hash](quint32 value) {
        hash ^= value;
        hash *= 16777619u;
    };

    mix(static_cast<quint32>(snapshot.currentWave));
    mix(static_cast<quint32>(snapshot.waveActive ? 1 : 0));
    mix(static_cast<quint32>(snapshot.resources));
    mix(static_cast<quint32>(snapshot.baseHealth));
    mix(static_cast<quint32>(snapshot.opponentResources));
    mix(static_cast<quint32>(snapshot.opponentBaseHealth));
    mix(static_cast<quint32>(snapshot.defeatedMonsters));
    mix(static_cast<quint32>(snapshot.escapedMonsters));

    for (const auto& unit : snapshot.units) {
        mix(static_cast<quint32>(unit.id));
        mix(static_cast<quint32>(unit.kind));
        mix(static_cast<quint32>(unit.type));
        mix(static_cast<quint32>(unit.row));
        mix(static_cast<quint32>(unit.col));
        mix(static_cast<quint32>(unit.hp));
        mix(static_cast<quint32>(unit.maxHp));
        mix(static_cast<quint32>(unit.deployCost));
    }
    for (const auto& monster : snapshot.monsters) {
        mix(static_cast<quint32>(monster.id));
        mix(static_cast<quint32>(monster.kind));
        mix(static_cast<quint32>(monster.row));
        mix(static_cast<quint32>(monster.col));
        mix(static_cast<quint32>(monster.hp));
        mix(static_cast<quint32>(monster.maxHp));
    }
    for (const auto& projectile : snapshot.projectiles) {
        mix(static_cast<quint32>(projectile.sourceId));
        mix(static_cast<quint32>(projectile.targetId));
        mix(static_cast<quint32>(projectile.fromRow));
        mix(static_cast<quint32>(projectile.fromCol));
        mix(static_cast<quint32>(projectile.toRow));
        mix(static_cast<quint32>(projectile.toCol));
        mix(static_cast<quint32>(projectile.kind));
        mix(static_cast<quint32>(projectile.splashRadius));
    }
    return hash;
}

game::core::BattleSnapshot BattleStateCodec::toHostPerspective(const game::core::BattleSnapshot& clientSnapshot)
{
    game::core::BattleSnapshot hostPerspective = clientSnapshot;
    hostPerspective.resources = clientSnapshot.opponentResources;
    hostPerspective.baseHealth = clientSnapshot.opponentBaseHealth;
    hostPerspective.opponentResources = clientSnapshot.resources;
    hostPerspective.opponentBaseHealth = clientSnapshot.baseHealth;
    return hostPerspective;
}

} // namespace game::network
