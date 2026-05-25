#ifndef DEPLOYMENT_SYNC_H
#define DEPLOYMENT_SYNC_H

#include "../protocol/ProtocolDef.h"
#include <QObject>
#include <QVector>

namespace game {
namespace network {

// ═══════════════════════════════════════════════════════════════
// 部署信息结构
// ═══════════════════════════════════════════════════════════════
struct DeployInfo {
    int cardId;       // 卡牌配置ID
    int gridRow;       // 部署行
    int gridCol;       // 部署列
    int unitId;        // 生成的单位ID（Host分配）
};

// ═══════════════════════════════════════════════════════════════
// DeploymentSync - 部署信息同步
//
// 迷雾部署：部署阶段双方数据不互通，
// 游戏正式开始后才同步单位位置。
// ═══════════════════════════════════════════════════════════════
class DeploymentSync : public QObject {
    Q_OBJECT

public:
    explicit DeploymentSync(QObject* parent = nullptr);

    // ═══════════════════════════════════════════════════════════
    // 本地部署操作（存入本地列表）
    // ═══════════════════════════════════════════════════════════
    void addLocalDeploy(const DeployInfo& info);
    void removeLocalDeploy(int unitId);
    void clearLocalDeploys();

    const QVector<DeployInfo>& localDeploys() const { return m_localDeploys; }

    // ═══════════════════════════════════════════════════════════
    // Host 分配 UnitId 并发送部署信息给 Client
    // ═══════════════════════════════════════════════════════════
    QByteArray buildDeploySyncPacket(const QVector<DeployInfo>& deploys);

    // ═══════════════════════════════════════════════════════════
    // 解析收到的部署信息
    // ═══════════════════════════════════════════════════════════
    static QVector<DeployInfo> parseDeploySyncPacket(const QByteArray& body);

signals:
    void localDeployAdded(const DeployInfo& info);
    void localDeployRemoved(int unitId);
    void deploysSynced(const QVector<DeployInfo>& remoteDeploys);

private:
    QVector<DeployInfo> m_localDeploys;
    int                 m_nextUnitId;
};

} // namespace network
} // namespace game

#endif // DEPLOYMENT_SYNC_H
