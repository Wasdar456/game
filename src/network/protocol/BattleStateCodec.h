#ifndef GAMEPROJECT_NETWORK_PROTOCOL_BATTLESTATECODEC_H
#define GAMEPROJECT_NETWORK_PROTOCOL_BATTLESTATECODEC_H

#include "core/snapshot/BattleSnapshot.h"
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
    static QByteArray encodeHostSnapshot(const game::core::BattleSnapshot& snapshot);
    static BattleStateDecodeResult decodeHostSnapshot(const QByteArray& body,
                                                      const game::core::MapSnapshot& map);
    static quint32 checksum(const game::core::BattleSnapshot& snapshot);
    static game::core::BattleSnapshot toHostPerspective(const game::core::BattleSnapshot& clientSnapshot);
};

} // namespace game::network

#endif // GAMEPROJECT_NETWORK_PROTOCOL_BATTLESTATECODEC_H
