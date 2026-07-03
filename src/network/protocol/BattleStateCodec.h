#ifndef GAMEPROJECT_NETWORK_PROTOCOL_BATTLESTATECODEC_H
#define GAMEPROJECT_NETWORK_PROTOCOL_BATTLESTATECODEC_H

#include "core/snapshot/BattleSnapshot.h"
#include "network/protocol/ProtocolDef.h"
#include <QByteArray>
#include <QtGlobal>

namespace game::network {

struct BattleStateDecodeResult {
    bool ok = false;
    bool checksumPresent = false;
    bool checksumValid = true;
    quint32 remoteChecksum = 0;
    quint32 localChecksum = 0;
    game::core::BattleSnapshot snapshot;
};

class BattleStateCodec {
public:
    struct DeployAction {
        game::core::CardKind cardKind = game::core::CardKind::Attack;
        game::core::MapPosition position;
        int unitId = 0;
        int roundId = 0;
    };

    struct UnitAction {
        int unitId = 0;
        game::core::MapPosition position;
        int targetLevel = 0;
        int roundId = 0;
    };

    static QByteArray encodeHostSnapshot(const game::core::BattleSnapshot& snapshot);
    static BattleStateDecodeResult decodeHostSnapshot(const QByteArray& body,
                                                      const game::core::MapSnapshot& map);
    static QByteArray encodeDeployAction(game::core::CardKind kind,
                                         game::core::MapPosition position,
                                         int unitId = 0,
                                         int roundId = 0);
    static bool decodeDeployAction(const QByteArray& body, DeployAction& action);
    static QByteArray encodeUpgradeAction(int unitId, int targetLevel = 0, int roundId = 0);
    static bool decodeUpgradeAction(const QByteArray& body, UnitAction& action);
    static QByteArray encodeMoveAction(int unitId, game::core::MapPosition position, int roundId = 0);
    static bool decodeMoveAction(const QByteArray& body, UnitAction& action);
    static QByteArray encodeRecallAction(int unitId, int roundId = 0);
    static bool decodeRecallAction(const QByteArray& body, UnitAction& action);
    static QByteArray encodeWaveId(int waveId);
    static bool decodeWaveId(const QByteArray& body, int& waveId);
    static QByteArray encodeDeploymentRound(int roundId);
    static bool decodeDeploymentRound(const QByteArray& body, int& roundId);
    static quint32 checksum(const game::core::BattleSnapshot& snapshot);
    static game::core::BattleSnapshot toHostPerspective(const game::core::BattleSnapshot& clientSnapshot);
};

} // namespace game::network

#endif // GAMEPROJECT_NETWORK_PROTOCOL_BATTLESTATECODEC_H
