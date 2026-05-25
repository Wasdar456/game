#ifndef NETWORK_MODULE_H
#define NETWORK_MODULE_H

// ═══════════════════════════════════════════════════════════════
// network 模块总头文件
// 其他模块只需 include 这个文件即可使用所有网络功能
// ═══════════════════════════════════════════════════════════════

#include "session/NetworkState.h"
#include "session/NetworkManager.h"
#include "session/GameServer.h"
#include "session/GameClient.h"
#include "session/LobbyManager.h"
#include "session/RoundManager.h"
#include "protocol/ProtocolDef.h"
#include "protocol/Packet.h"
#include "protocol/Serializer.h"
#include "protocol/Deserializer.h"
#include "sync/RandomSynchronizer.h"
#include "sync/DeploymentSync.h"
#include "sync/ClashSync.h"
#include "sync/StateValidator.h"

#endif // NETWORK_MODULE_H
