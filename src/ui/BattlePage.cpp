/**
 * @file BattlePage.cpp
 * @brief 战斗主页面实现文件
 *
 * 核心架构说明：
 *   本文件是 UI 层与 core 层的桥梁，遵循"快照驱动渲染"的设计：
 *
 *   每帧执行流程：
 *   1. QTimer 触发 onGameTick()
 *   2. 调用 BattleManager::update(deltaSeconds) 推进游戏逻辑
 *   3. 调用 BattleManager::snapshot() 获取只读快照
 *   4. 将快照传给 BattleView::updateFromSnapshot() 渲染界面
 *   5. 更新状态栏数据
 *
 *   用户操作流程：
 *   1. 用户在 BattleView 上点击 → BattleView 发出信号
 *   2. BattlePage 的槽函数接收信号
 *   3. 调用 BattleManager 对应接口（deployCard/upgradeCard/moveCard/recallCard）
 *   4. 下一帧自动刷新界面
 */

#include "ui/BattlePage.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFont>
#include <QPen>
#include <QBrush>
#include <QConicalGradient>
#include <QRadialGradient>
#include <QLinearGradient>
#include <vector>
#include <QtMath>
#include <QMessageBox>
#include <QDebug>
#include <QtEndian>
#include <QCoreApplication>
#include <QFileInfo>
#include <QDir>
#include <algorithm>
#include <cmath>

// ========== 引入 MainWindow 头文件以获取 BattleManager ==========
#include "ui/MainWindow.h"

// ========== 引入核心层头文件 ==========
#include "core/systems/ResourceManager.h"  // 资源管理
#include "network/protocol/BattleStateCodec.h"
#include "core/base/Constants.h"           // 游戏常量
#include "core/map/MapConfigLoader.h"

// ========== 引入网络模块头文件 ==========
#include "network/session/GameServer.h"
#include "network/session/GameClient.h"

namespace {

constexpr int MaxBattleImageWidth = 1180;
constexpr int MaxBattleImageHeight = 560;

QString findProjectFile(const QString& relativePath)
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString cwd = QDir::currentPath();
    const QStringList candidates = {
        QDir(cwd).filePath(relativePath),
        QDir(appDir).filePath(relativePath),
        QDir(appDir).filePath("../" + relativePath),
        QDir(appDir).filePath("../../" + relativePath),
        QDir(appDir).filePath("../../../" + relativePath)
    };

    for (const QString& candidate : candidates) {
        QFileInfo info(candidate);
        if (info.exists() && info.isFile()) {
            return info.absoluteFilePath();
        }
    }
    return {};
}

game::core::TerrainType terrainFromMapTile(const std::string& type)
{
    if (type == "PATH_A" || type == "PATH_B" || type == "PATH_SHARED") {
        return game::core::TerrainType::Path;
    }
    if (type == "SPAWN_A" || type == "SPAWN_B") {
        return game::core::TerrainType::SpawnPoint;
    }
    if (type == "CORE_A") {
        return game::core::TerrainType::CoreA;
    }
    if (type == "CORE_B") {
        return game::core::TerrainType::CoreB;
    }
    if (type == "DEPLOY_A" || type == "DEPLOY_B" || type == "DEPLOY_NEUTRAL") {
        return game::core::TerrainType::FlatLand;
    }
    if (type == "HIGH_GROUND") {
        return game::core::TerrainType::HighGround;
    }
    return game::core::TerrainType::NoDeploy;
}

int terrainHeightFromMapTile(const std::string& type)
{
    return type == "HIGH_GROUND" ? 1 : 0;
}

} // namespace

// ============================================================================
// BattleView 实现
// ============================================================================

BattleView::BattleView(QWidget *parent)
    : QWidget(parent)
    , m_mode(InteractionMode::NONE)
    , m_selectedCardKind(game::core::CardKind::Attack)
    , m_selectedUnitId(-1)
    , m_moveRange(0)
    , m_spawnPos(1, 1)
    , m_corePos(10, 16)
    , m_localIsHost(true)
    , m_interactionEnabled(true)
    , m_btnUpgrade(nullptr)
    , m_btnMove(nullptr)
    , m_btnRetreat(nullptr)
    , m_animFrame(0)
    , m_hoverRow(-1)
    , m_hoverCol(-1)
    , m_mapRows(game::core::constants::DefaultMapRows)
    , m_mapCols(game::core::constants::DefaultMapCols)
{
    setMapSize(m_mapRows, m_mapCols);

    // ----- 创建环形菜单按钮 -----
    m_btnUpgrade = new QPushButton("⬆ 升级", this);
    m_btnMove    = new QPushButton("🏃 移动", this);
    m_btnRetreat = new QPushButton("↩ 撤回", this);

    QString radialStyle =
        "QPushButton {"
        "  background-color: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
        "    stop:0 rgba(22,50,90,0.95), stop:1 rgba(12,28,50,0.95));"
        "  color: #00E5FF;"
        "  border: 2px solid rgba(0,212,255,0.65); border-radius: 18px;"
        "  font-size: 13px; font-weight: bold; padding: 8px 14px;"
        "}"
        "QPushButton:hover { background-color: rgba(0,212,255,0.25); border: 2px solid #00D4FF; }"
        "QPushButton:disabled { color: #667788; border: 2px solid rgba(0,212,255,0.25); }"
    ;
    m_btnUpgrade->setStyleSheet(radialStyle);
    m_btnMove->setStyleSheet(radialStyle);
    m_btnRetreat->setStyleSheet(radialStyle);

    m_btnUpgrade->hide();
    m_btnMove->hide();
    m_btnRetreat->hide();

    // 连接环形菜单按钮信号
    connect(m_btnUpgrade, &QPushButton::clicked, this, [this]() {
        if (m_selectedUnitId >= 0) {
            emit signalUpgradeCard(m_selectedUnitId);
        }
        hideRadialMenu();
        m_mode = InteractionMode::NONE;
    });

    connect(m_btnMove, &QPushButton::clicked, this, [this]() {
        if (m_selectedUnitId >= 0) {
            m_mode = InteractionMode::MOVING;
            for (const auto &unit : m_snapshot.units) {
                if (unit.id == m_selectedUnitId) {
                    m_moveRange = unit.moveLimit;
                    break;
                }
            }
            hideRadialMenu();
            update();
        }
    });

    connect(m_btnRetreat, &QPushButton::clicked, this, [this]() {
        if (m_selectedUnitId >= 0) {
            emit signalRecallCard(m_selectedUnitId);
        }
        hideRadialMenu();
        m_mode = InteractionMode::NONE;
    });

    this->setMouseTracking(true);

    // 动画帧定时器（用于脉冲/旋转效果，独立于游戏逻辑）
    QTimer *animTimer = new QTimer(this);
    connect(animTimer, &QTimer::timeout, this, [this]() {
        m_animFrame++;
        update();
    });
    animTimer->start(50);  // 20 FPS 动画
}

// ========== setMapSize() —— 设置地图大小 ==========
void BattleView::setMapSize(int rows, int cols)
{
    m_mapRows = rows;
    m_mapCols = cols;
    if (!m_backgroundImage.isNull()) {
        this->setFixedSize(m_backgroundImage.size().scaled(MaxBattleImageWidth,
                                                           MaxBattleImageHeight,
                                                           Qt::KeepAspectRatio));
    } else {
        this->setFixedSize(cols * CELL_SIZE, rows * CELL_SIZE);
    }
}

bool BattleView::setBackgroundImage(const QString& path)
{
    QPixmap image(path);
    if (image.isNull()) {
        m_backgroundImage = QPixmap();
        update();
        return false;
    }

    m_backgroundImage = image;
    if (m_mapRows > 0 && m_mapCols > 0) {
        this->setFixedSize(m_backgroundImage.size().scaled(MaxBattleImageWidth,
                                                           MaxBattleImageHeight,
                                                           Qt::KeepAspectRatio));
    }
    update();
    return true;
}

void BattleView::clearBackgroundImage()
{
    m_backgroundImage = QPixmap();
    if (m_mapRows > 0 && m_mapCols > 0) {
        this->setFixedSize(m_mapCols * CELL_SIZE, m_mapRows * CELL_SIZE);
    }
    update();
}

double BattleView::cellWidth() const
{
    return m_mapCols > 0 ? static_cast<double>(width()) / m_mapCols : CELL_SIZE;
}

double BattleView::cellHeight() const
{
    return m_mapRows > 0 ? static_cast<double>(height()) / m_mapRows : CELL_SIZE;
}

double BattleView::cellExtent() const
{
    return std::min(cellWidth(), cellHeight());
}

QRectF BattleView::cellRect(int row, int col) const
{
    const double cw = cellWidth();
    const double ch = cellHeight();
    return QRectF(col * cw, row * ch, cw, ch);
}

QPointF BattleView::cellCenter(int row, int col) const
{
    return cellRect(row, col).center();
}

int BattleView::rowAtPixel(int y) const
{
    const double ch = cellHeight();
    return ch > 0.0 ? static_cast<int>(std::floor(y / ch)) : -1;
}

int BattleView::colAtPixel(int x) const
{
    const double cw = cellWidth();
    return cw > 0.0 ? static_cast<int>(std::floor(x / cw)) : -1;
}

// ========== updateFromSnapshot() —— 从快照更新渲染数据 ==========
void BattleView::updateFromSnapshot(const game::core::BattleSnapshot &snapshot)
{
    m_snapshot = snapshot;

    // 如果快照中有地图数据，更新地图大小
    if (snapshot.map.rows > 0 && snapshot.map.cols > 0) {
        if (m_mapRows != snapshot.map.rows || m_mapCols != snapshot.map.cols) {
            setMapSize(snapshot.map.rows, snapshot.map.cols);
        }
    }

    // 触发重绘
    update();
}

// ========== paintEvent() —— 绘制地图和所有元素 ==========
void BattleView::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    drawTerrain(painter);
    drawHoverCell(painter);
    drawSpawnMarker(painter);
    drawCoreMarker(painter);
    drawHighlights(painter);
    drawUnits(painter);
    drawMonsters(painter);
    drawProjectiles(painter);
}

// ========== drawTerrain() —— 绘制地形（渐变+纹理感） ==========
void BattleView::drawTerrain(QPainter &painter)
{
    if (!m_backgroundImage.isNull()) {
        painter.drawPixmap(rect(), m_backgroundImage);
        return;
    }

    for (const auto &grid : m_snapshot.map.grids) {
        QRectF cellRect = this->cellRect(grid.row, grid.col);

        // 每种地形用渐变填充，增加质感
        switch (grid.terrain) {
        case game::core::TerrainType::Path: {
            QLinearGradient grad(cellRect.topLeft(), cellRect.bottomRight());
            grad.setColorAt(0, QColor(155, 135, 115));
            grad.setColorAt(1, QColor(125, 105, 85));
            painter.fillRect(cellRect, grad);
            // 路径纹理：小点
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(100, 80, 60, 40));
            for (int dx = 8; dx < CELL_SIZE; dx += 12) {
                for (int dy = 8; dy < CELL_SIZE; dy += 12) {
                    painter.drawEllipse(cellRect.x() + dx, cellRect.y() + dy, 2, 2);
                }
            }
            break;
        }
        case game::core::TerrainType::FlatLand: {
            QLinearGradient grad(cellRect.topLeft(), cellRect.bottomRight());
            grad.setColorAt(0, QColor(62, 95, 40));
            grad.setColorAt(1, QColor(48, 78, 28));
            painter.fillRect(cellRect, grad);
            // 草地纹理：随机小线条
            painter.setPen(QPen(QColor(80, 120, 50, 50), 1));
            for (int i = 0; i < 3; ++i) {
                int sx = static_cast<int>(cellRect.x()) + 10 + i * 12;
                int sy = static_cast<int>(cellRect.bottom()) - 8;
                painter.drawLine(sx, sy, sx - 2, sy - 8);
            }
            break;
        }
        case game::core::TerrainType::HighGround: {
            QLinearGradient grad(cellRect.topLeft(), cellRect.bottomRight());
            grad.setColorAt(0, QColor(95, 117, 57));
            grad.setColorAt(1, QColor(75, 97, 37));
            painter.fillRect(cellRect, grad);
            // 高台标记：右上角三角 + 箭头
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(255, 255, 255, 50));
            QPolygon tri;
            int x = static_cast<int>(cellRect.right()), y = static_cast<int>(cellRect.top());
            tri << QPoint(x, y) << QPoint(x - 10, y) << QPoint(x, y + 10);
            painter.drawPolygon(tri);
            break;
        }
        case game::core::TerrainType::NoDeploy: {
            QLinearGradient grad(cellRect.topLeft(), cellRect.bottomRight());
            grad.setColorAt(0, QColor(55, 55, 60));
            grad.setColorAt(1, QColor(40, 40, 45));
            painter.fillRect(cellRect, grad);
            break;
        }
        case game::core::TerrainType::SpawnPoint: {
            QLinearGradient grad(cellRect.topLeft(), cellRect.bottomRight());
            grad.setColorAt(0, QColor(180, 60, 60));
            grad.setColorAt(1, QColor(140, 40, 40));
            painter.fillRect(cellRect, grad);
            // 绘制 S 标记
            painter.setPen(QColor(255, 255, 255));
            QFont font("Microsoft YaHei", 14, QFont::Bold);
            painter.setFont(font);
            painter.drawText(cellRect, Qt::AlignCenter, "S");
            break;
        }
        case game::core::TerrainType::CoreA: {
            QLinearGradient grad(cellRect.topLeft(), cellRect.bottomRight());
            grad.setColorAt(0, QColor(60, 100, 220));
            grad.setColorAt(1, QColor(40, 70, 180));
            painter.fillRect(cellRect, grad);
            // 绘制 A 标记
            painter.setPen(QColor(255, 255, 255));
            QFont font("Microsoft YaHei", 14, QFont::Bold);
            painter.setFont(font);
            painter.drawText(cellRect, Qt::AlignCenter, "A");
            break;
        }
        case game::core::TerrainType::CoreB: {
            QLinearGradient grad(cellRect.topLeft(), cellRect.bottomRight());
            grad.setColorAt(0, QColor(220, 60, 60));
            grad.setColorAt(1, QColor(180, 40, 40));
            painter.fillRect(cellRect, grad);
            // 绘制 B 标记
            painter.setPen(QColor(255, 255, 255));
            QFont font("Microsoft YaHei", 14, QFont::Bold);
            painter.setFont(font);
            painter.drawText(cellRect, Qt::AlignCenter, "B");
            break;
        }
        }

        // 网格线（淡色）
        painter.setPen(QPen(QColor(0, 0, 0, 25), 1));
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(cellRect);
    }
}

// ========== drawSpawnMarker() —— 绘制出生点（旋转传送门） ==========
void BattleView::drawSpawnMarker(QPainter &painter)
{
    QPointF center = cellCenter(m_spawnPos.row, m_spawnPos.col);
    int r = static_cast<int>(cellExtent() / 2.0) - 2;

    painter.save();
    painter.translate(center);

    // 旋转的锥形渐变（传送门效果）
    qreal angle = m_animFrame * 6.0;  // 每帧旋转6度
    QConicalGradient conGrad(0, 0, angle);
    conGrad.setColorAt(0, QColor(0, 180, 255, 120));
    conGrad.setColorAt(0.25, QColor(0, 120, 255, 40));
    conGrad.setColorAt(0.5, QColor(0, 180, 255, 120));
    conGrad.setColorAt(0.75, QColor(0, 120, 255, 40));
    conGrad.setColorAt(1, QColor(0, 180, 255, 120));

    painter.setPen(QPen(QColor(0, 200, 255, 180), 2));
    painter.setBrush(conGrad);
    painter.drawEllipse(-r, -r, r * 2, r * 2);

    // 内环
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 150, 255, 30));
    painter.drawEllipse(-r + 5, -r + 5, (r - 5) * 2, (r - 5) * 2);

    // "S" 文字
    painter.setPen(QColor(0, 220, 255));
    QFont spawnFont("Arial", 13, QFont::Bold);
    painter.setFont(spawnFont);
    QRect textRect(-r, -r, r * 2, r * 2);
    painter.drawText(textRect, Qt::AlignCenter, "S");

    // 外围发光
    QRadialGradient glow(0, 0, r + 6);
    qreal pulse = 0.3 + 0.2 * qSin(m_animFrame * 0.15);
    glow.setColorAt(0, QColor(0, 180, 255, 0));
    glow.setColorAt(0.7, QColor(0, 180, 255, 0));
    glow.setColorAt(1, QColor(0, 180, 255, int(pulse * 80)));
    painter.setPen(Qt::NoPen);
    painter.setBrush(glow);
    painter.drawEllipse(-r - 6, -r - 6, (r + 6) * 2, (r + 6) * 2);

    painter.restore();
}

// ========== drawCoreMarker() —— 绘制核心（脉冲发光） ==========
void BattleView::drawCoreMarker(QPainter &painter)
{
    QPointF center = cellCenter(m_corePos.row, m_corePos.col);
    const double cx = center.x();
    const double cy = center.y();
    int r = static_cast<int>(cellExtent() / 2.0) - 2;

    // 脉冲大小
    qreal pulseScale = 1.0 + 0.08 * qSin(m_animFrame * 0.12);
    int pr = static_cast<int>(r * pulseScale);

    // 外围脉冲光晕
    QRadialGradient outerGlow(cx, cy, pr + 10);
    qreal pulseAlpha = 0.3 + 0.3 * qSin(m_animFrame * 0.12);
    outerGlow.setColorAt(0, QColor(0, 212, 255, int(pulseAlpha * 100)));
    outerGlow.setColorAt(0.5, QColor(0, 180, 255, int(pulseAlpha * 40)));
    outerGlow.setColorAt(1, QColor(0, 150, 255, 0));
    painter.setPen(Qt::NoPen);
    painter.setBrush(outerGlow);
    painter.drawEllipse(cx - pr - 10, cy - pr - 10, (pr + 10) * 2, (pr + 10) * 2);

    // 蓝白圆环
    QLinearGradient ringGrad(cx - pr, cy - pr, cx + pr, cy + pr);
    ringGrad.setColorAt(0, QColor(144, 202, 249));
    ringGrad.setColorAt(0.5, QColor(66, 165, 245));
    ringGrad.setColorAt(1, QColor(30, 136, 229));
    painter.setPen(QPen(QBrush(ringGrad), 3));
    painter.setBrush(QColor(0, 180, 255, 40));
    painter.drawEllipse(cx - pr, cy - pr, pr * 2, pr * 2);

    // "C" 文字
    painter.setPen(QColor(200, 230, 255));
    QFont coreFont("Arial", 13, QFont::Bold);
    painter.setFont(coreFont);
    QRect textRect(cx - r, cy - r, r * 2, r * 2);
    painter.drawText(textRect, Qt::AlignCenter, "C");
}

// ========== drawHighlights() —— 绘制部署/移动高亮 ==========
void BattleView::drawHighlights(QPainter &painter)
{
    if (m_mode == InteractionMode::DEPLOYING) {
        QVector<game::core::MapPosition> deployable = getDeployableCells();
        qreal pulse = 0.5 + 0.3 * qSin(m_animFrame * 0.15);
        for (const auto &pos : deployable) {
            QRectF r = cellRect(pos.row, pos.col);
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(0, 255, 100, int(pulse * 60)));
            painter.fillRect(r, painter.brush());
            // 边框
            painter.setPen(QPen(QColor(0, 255, 100, int(pulse * 120)), 1));
            painter.setBrush(Qt::NoBrush);
            painter.drawRect(r.adjusted(1, 1, -1, -1));
        }
    } else if (m_mode == InteractionMode::MOVING) {
        QVector<game::core::MapPosition> movable = getMovableCells(m_selectedUnitId);
        qreal pulse = 0.5 + 0.3 * qSin(m_animFrame * 0.15);
        for (const auto &pos : movable) {
            QRectF r = cellRect(pos.row, pos.col);
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(0, 150, 255, int(pulse * 60)));
            painter.fillRect(r, painter.brush());
            painter.setPen(QPen(QColor(0, 150, 255, int(pulse * 120)), 1));
            painter.setBrush(Qt::NoBrush);
            painter.drawRect(r.adjusted(1, 1, -1, -1));
        }
    }
}

// ========== drawUnits() —— 绘制单位（阴影+发光） ==========
void BattleView::drawUnits(QPainter &painter)
{
    for (const auto &unit : m_snapshot.units) {
        QRectF unitRect = cellRect(unit.row, unit.col);
        QRectF innerRect = unitRect.adjusted(4, 4, -4, -4);

        // Host 快照中 Host 单位为 1-999，Client 单位为 1000+。
        // Client 观看 Host 权威快照时需要反过来判断敌我。
        bool isOpponent = m_localIsHost ? (unit.id >= 1000) : (unit.id < 1000);

        // 根据阵营和类型选择颜色
        QColor unitColor;
        QColor borderColor;
        QString label;

        if (isOpponent) {
            // 对方单位：红色系
            borderColor = QColor(255, 80, 80);
            switch (unit.type) {
            case game::core::ObjectType::CardAttack:
                unitColor = QColor(200, 60, 60);
                label = "敌攻";
                break;
            case game::core::ObjectType::CardProduce:
                unitColor = QColor(60, 200, 60);
                label = "敌产";
                break;
            case game::core::ObjectType::CardHeal:
                unitColor = QColor(60, 60, 200);
                label = "敌医";
                break;
            default:
                unitColor = QColor(150, 150, 150);
                label = "敌?";
            }
        } else {
            // 己方单位：蓝色/绿色系
            borderColor = QColor(80, 200, 255);
            switch (unit.type) {
            case game::core::ObjectType::CardAttack:
                unitColor = QColor(100, 150, 255);
                label = "攻";
                break;
            case game::core::ObjectType::CardProduce:
                unitColor = QColor(100, 255, 100);
                label = "产";
                break;
            case game::core::ObjectType::CardHeal:
                unitColor = QColor(100, 200, 255);
                label = "医";
                break;
            default:
                unitColor = QColor(200, 200, 200);
                label = "?";
            }
        }

        // 底部阴影
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0, 0, 0, 40));
        painter.drawRoundedRect(innerRect.adjusted(2, 3, 2, 3), 6, 6);

        // 单位方块（渐变填充）
        QLinearGradient unitGrad(innerRect.topLeft(), innerRect.bottomRight());
        unitGrad.setColorAt(0, unitColor.lighter(120));
        unitGrad.setColorAt(1, unitColor.darker(110));
        painter.setPen(QPen(borderColor, 2));
        painter.setBrush(unitGrad);
        painter.drawRoundedRect(innerRect, 6, 6);

        // 选中发光效果
        if (unit.id == m_selectedUnitId && m_mode == InteractionMode::RADIAL_MENU) {
            qreal pulse = 0.6 + 0.4 * qSin(m_animFrame * 0.2);
            painter.setPen(QPen(QColor(0, 212, 255, int(pulse * 200)), 2));
            painter.setBrush(Qt::NoBrush);
            painter.drawRoundedRect(innerRect.adjusted(-1, -1, 1, 1), 7, 7);

            // 发光光晕
            QRadialGradient glowGrad(innerRect.center(), cellExtent() / 2.0);
            glowGrad.setColorAt(0, QColor(0, 212, 255, int(pulse * 50)));
            glowGrad.setColorAt(1, QColor(0, 212, 255, 0));
            painter.setPen(Qt::NoPen);
            painter.setBrush(glowGrad);
            painter.drawEllipse(innerRect.center(), cellExtent() / 2.0, cellExtent() / 2.0);
        }

        // 单位标签和等级
        painter.setPen(QColor(255, 255, 255, 220));
        QFont unitFont("Microsoft YaHei", 9, QFont::Bold);
        painter.setFont(unitFont);
        painter.drawText(innerRect.adjusted(2, 1, 0, 0),
                         Qt::AlignTop | Qt::AlignLeft,
                         QString("Lv%1").arg(unit.level));

        // 单位类型标签
        QFont typeFont("Microsoft YaHei", 10, QFont::Bold);
        painter.setFont(typeFont);
        painter.drawText(innerRect, Qt::AlignCenter, label);

        // 血量条（渐变）
        int barWidth = std::max(1, static_cast<int>(innerRect.width()) - 2);
        int barHeight = 4;
        int barX = static_cast<int>(innerRect.x()) + 1;
        int barY = static_cast<int>(innerRect.bottom()) - 7;

        // 血量条背景
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(30, 30, 30, 180));
        painter.drawRoundedRect(barX, barY, barWidth, barHeight, 2, 2);

        // 血量条填充
        double hpRatio = (unit.maxHp > 0) ? static_cast<double>(unit.hp) / unit.maxHp : 0.0;
        int hpWidth = static_cast<int>(barWidth * hpRatio);
        if (hpWidth > 0) {
            QLinearGradient hpGrad(barX, barY, barX + hpWidth, barY);
            if (hpRatio > 0.5) {
                hpGrad.setColorAt(0, QColor(80, 220, 80));
                hpGrad.setColorAt(1, QColor(50, 180, 50));
            } else if (hpRatio > 0.25) {
                hpGrad.setColorAt(0, QColor(255, 200, 50));
                hpGrad.setColorAt(1, QColor(220, 160, 30));
            } else {
                hpGrad.setColorAt(0, QColor(255, 80, 80));
                hpGrad.setColorAt(1, QColor(200, 40, 40));
            }
            painter.setBrush(hpGrad);
            painter.drawRoundedRect(barX, barY, hpWidth, barHeight, 2, 2);
        }

        // 攻击范围指示（选中时）
        if (unit.id == m_selectedUnitId) {
            QPointF center = cellCenter(unit.row, unit.col);
            int rangeRadius = static_cast<int>(unit.range * cellExtent());
            qreal pulse = 0.5 + 0.3 * qSin(m_animFrame * 0.1);
            painter.setPen(QPen(QColor(0, 212, 255, int(pulse * 100)), 1, Qt::DashLine));
            painter.setBrush(QColor(0, 212, 255, int(pulse * 12)));
            painter.drawEllipse(center, rangeRadius, rangeRadius);
        }
    }
}

// ========== drawMonsters() —— 绘制怪物 ==========
void BattleView::drawMonsters(QPainter &painter)
{
    for (const auto &monster : m_snapshot.monsters) {
        QRectF mRect = cellRect(monster.row, monster.col);
        QRectF innerRect = mRect.adjusted(6, 6, -6, -6);

        // 底部阴影
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0, 0, 0, 50));
        painter.drawRoundedRect(innerRect.adjusted(2, 2, 2, 2), 4, 4);

        // 怪物方块（红色渐变）
        QLinearGradient monsterGrad(innerRect.topLeft(), innerRect.bottomRight());
        monsterGrad.setColorAt(0, QColor(240, 70, 70));
        monsterGrad.setColorAt(1, QColor(180, 30, 30));
        painter.setPen(QPen(QColor(255, 100, 100, 120), 1));
        painter.setBrush(monsterGrad);
        painter.drawRoundedRect(innerRect, 4, 4);

        // 怪物血量条
        int barWidth = std::max(1, static_cast<int>(innerRect.width()));
        int barHeight = 3;
        int barX = static_cast<int>(innerRect.x());
        int barY = static_cast<int>(innerRect.bottom()) - 5;

        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(40, 10, 10, 180));
        painter.drawRoundedRect(barX, barY, barWidth, barHeight, 1, 1);

        double hpRatio = (monster.maxHp > 0) ? static_cast<double>(monster.hp) / monster.maxHp : 0.0;
        int hpWidth = static_cast<int>(barWidth * hpRatio);
        if (hpWidth > 0) {
            QLinearGradient hpGrad(barX, barY, barX + hpWidth, barY);
            hpGrad.setColorAt(0, QColor(255, 100, 100));
            hpGrad.setColorAt(1, QColor(200, 50, 50));
            painter.setBrush(hpGrad);
            painter.drawRoundedRect(barX, barY, hpWidth, barHeight, 1, 1);
        }
    }
}

// ========== drawProjectiles() —— 绘制投射物占位特效 ==========
void BattleView::drawProjectiles(QPainter &painter)
{
    for (const auto &projectile : m_snapshot.projectiles) {
        const double progress = std::clamp(projectile.progress, 0.0, 1.0);
        const QPointF start = cellCenter(projectile.fromRow, projectile.fromCol);
        const QPointF end = cellCenter(projectile.toRow, projectile.toCol);
        const double startX = start.x();
        const double startY = start.y();
        const double endX = end.x();
        const double endY = end.y();
        const double x = startX + (endX - startX) * progress;
        const double y = startY + (endY - startY) * progress;

        QColor color(255, 220, 80);
        int radius = 5;
        if (projectile.kind == game::core::ProjectileKind::Sniper) {
            color = QColor(80, 190, 255);
            radius = 4;
        } else if (projectile.kind == game::core::ProjectileKind::Aoe) {
            color = QColor(255, 110, 40);
            radius = 7;
        } else if (projectile.kind == game::core::ProjectileKind::Monster) {
            color = QColor(210, 70, 255);
            radius = 5;
        }

        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setPen(QPen(QColor(color.red(), color.green(), color.blue(), 110), 2));
        painter.drawLine(QPointF(startX, startY), QPointF(x, y));

        QRadialGradient glow(QPointF(x, y), radius * 2.5);
        glow.setColorAt(0, QColor(color.red(), color.green(), color.blue(), 220));
        glow.setColorAt(1, QColor(color.red(), color.green(), color.blue(), 0));
        painter.setPen(Qt::NoPen);
        painter.setBrush(glow);
        painter.drawEllipse(QPointF(x, y), radius * 2.5, radius * 2.5);

        painter.setBrush(color);
        painter.drawEllipse(QPointF(x, y), radius, radius);

        if (projectile.kind == game::core::ProjectileKind::Aoe && projectile.splashRadius > 0) {
            const double splashPixels = projectile.splashRadius * cellExtent();
            QColor splashColor(color.red(), color.green(), color.blue(), 35);
            painter.setPen(QPen(QColor(color.red(), color.green(), color.blue(), 80), 1, Qt::DashLine));
            painter.setBrush(splashColor);
            painter.drawEllipse(QPointF(endX, endY), splashPixels, splashPixels);
        }
    }
}

// ========== drawHoverCell() —— 绘制悬停格子高亮 ==========
void BattleView::drawHoverCell(QPainter &painter)
{
    if (m_hoverRow < 0 || m_hoverCol < 0) return;
    if (m_hoverRow >= m_mapRows || m_hoverCol >= m_mapCols) return;

    QRectF hoverRect = cellRect(m_hoverRow, m_hoverCol);
    painter.setPen(QPen(QColor(0, 212, 255, 70), 2));
    painter.setBrush(QColor(0, 212, 255, 18));
    painter.drawRect(hoverRect);
}

// ========== mouseMoveEvent() —— 鼠标移动追踪 ==========
void BattleView::mouseMoveEvent(QMouseEvent *event)
{
    int col = colAtPixel(event->pos().x());
    int row = rowAtPixel(event->pos().y());

    if (row != m_hoverRow || col != m_hoverCol) {
        m_hoverRow = row;
        m_hoverCol = col;
        update();
    }
}

// ========== mousePressEvent() —— 处理鼠标点击 ==========
void BattleView::mousePressEvent(QMouseEvent *event)
{
    if (!m_interactionEnabled) {
        hideRadialMenu();
        m_mode = InteractionMode::NONE;
        return;
    }

    // 像素坐标 → 网格坐标
    int col = colAtPixel(event->pos().x());
    int row = rowAtPixel(event->pos().y());

    // 边界检查
    if (row < 0 || row >= m_mapRows || col < 0 || col >= m_mapCols) return;

    switch (m_mode) {
    case InteractionMode::NONE: {
        // 检查是否点击了己方单位
        int unitId = findUnitAt(row, col);
        if (unitId >= 0) {
            m_selectedUnitId = unitId;
            m_mode = InteractionMode::RADIAL_MENU;
            const QPointF center = cellCenter(row, col);
            showRadialMenu(unitId, static_cast<int>(center.x()), static_cast<int>(center.y()));
            update();
        }
        break;
    }

    case InteractionMode::DEPLOYING: {
        // 检查目标格是否可部署
        bool canDeploy = false;
        for (const auto &grid : m_snapshot.map.grids) {
            if (grid.row == row && grid.col == col &&
                !grid.occupied &&
                (grid.terrain == game::core::TerrainType::FlatLand ||
                 grid.terrain == game::core::TerrainType::HighGround)) {
                canDeploy = true;
                break;
            }
        }
        if (canDeploy) {
            // 发出部署信号 → BattlePage 调用 BattleManager::deployCard
            emit signalDeployCard(m_selectedCardKind, game::core::MapPosition(row, col));
            m_mode = InteractionMode::NONE;
            update();
        }
        // 右键取消部署
        if (event->button() == Qt::RightButton) {
            m_mode = InteractionMode::NONE;
            update();
        }
        break;
    }

    case InteractionMode::MOVING: {
        QVector<game::core::MapPosition> movable = getMovableCells(m_selectedUnitId);
        bool isValid = false;
        for (const auto &pos : movable) {
            if (pos.row == row && pos.col == col) { isValid = true; break; }
        }
        if (isValid) {
            emit signalMoveCard(m_selectedUnitId, game::core::MapPosition(row, col));
            m_mode = InteractionMode::NONE;
            m_selectedUnitId = -1;
            update();
        } else if (event->button() == Qt::RightButton) {
            m_mode = InteractionMode::NONE;
            m_selectedUnitId = -1;
            update();
        }
        break;
    }

    case InteractionMode::RADIAL_MENU: {
        // 点击非菜单区域 → 关闭菜单
        if (!m_btnUpgrade->geometry().contains(event->pos()) &&
            !m_btnMove->geometry().contains(event->pos()) &&
            !m_btnRetreat->geometry().contains(event->pos())) {
            hideRadialMenu();
            m_mode = InteractionMode::NONE;
            m_selectedUnitId = -1;
            update();
        }
        break;
    }
    }
}

// ========== 辅助方法实现 ==========

void BattleView::showRadialMenu(int unitId, int pixelX, int pixelY)
{
    // 从快照查找单位信息
    int level = 1;
    for (const auto &unit : m_snapshot.units) {
        if (unit.id == unitId) { level = unit.level; break; }
    }

    int btnWidth = 70, btnHeight = 36;

    const int menuOffset = static_cast<int>(cellExtent());
    m_btnUpgrade->setGeometry(pixelX - btnWidth / 2, pixelY - menuOffset - btnHeight - 5, btnWidth, btnHeight);
    m_btnMove->setGeometry(pixelX - menuOffset - btnWidth - 5, pixelY + 10, btnWidth, btnHeight);
    m_btnRetreat->setGeometry(pixelX + menuOffset + 5, pixelY + 10, btnWidth, btnHeight);

    // 升级按钮状态
    m_btnUpgrade->setEnabled(level < game::core::constants::MaxCardLevel);
    m_btnUpgrade->setText(level >= game::core::constants::MaxCardLevel ?
                              "⬆ 满级" : QString("⬆ Lv%1→%2").arg(level).arg(level + 1));

    m_btnUpgrade->show();
    m_btnMove->show();
    m_btnRetreat->show();
}

void BattleView::hideRadialMenu()
{
    m_btnUpgrade->hide();
    m_btnMove->hide();
    m_btnRetreat->hide();
}

int BattleView::findUnitAt(int row, int col) const
{
    for (const auto &unit : m_snapshot.units) {
        if (unit.row == row && unit.col == col) return unit.id;
    }
    return -1;
}

QVector<game::core::MapPosition> BattleView::getDeployableCells() const
{
    QVector<game::core::MapPosition> result;
    for (const auto &grid : m_snapshot.map.grids) {
        if (!grid.occupied &&
            (grid.terrain == game::core::TerrainType::FlatLand ||
             grid.terrain == game::core::TerrainType::HighGround)) {
            result.append(game::core::MapPosition(grid.row, grid.col));
        }
    }
    return result;
}

QVector<game::core::MapPosition> BattleView::getMovableCells(int unitId) const
{
    QVector<game::core::MapPosition> result;

    // 从快照查找单位的当前位置和移动范围
    game::core::MapPosition unitPos;
    bool found = false;
    for (const auto &unit : m_snapshot.units) {
        if (unit.id == unitId) {
            unitPos = game::core::MapPosition(unit.row, unit.col);
            found = true;
            break;
        }
    }
    if (!found) return result;

    // 曼哈顿距离不超过 moveRange 的可部署空格
    for (const auto &grid : m_snapshot.map.grids) {
        game::core::MapPosition gridPos(grid.row, grid.col);
        int dist = unitPos.manhattanDistanceTo(gridPos);
        if (dist > 0 && dist <= m_moveRange && !grid.occupied &&
            (grid.terrain == game::core::TerrainType::FlatLand ||
             grid.terrain == game::core::TerrainType::HighGround)) {
            result.append(gridPos);
        }
    }
    return result;
}


// ============================================================================
// BattlePage 实现
// ============================================================================

BattlePage::BattlePage(QWidget *parent)
    : QWidget(parent)
    , m_battleView(nullptr)
    , m_gameTimer(nullptr)
    , m_waveLabel(nullptr)
    , m_coreHpLabel(nullptr)
    , m_resourceLabel(nullptr)
    , m_btnPause(nullptr)
    , m_btnSpeed(nullptr)
    , m_btnSkill(nullptr)
    , m_btnExit(nullptr)
    , m_isPaused(false)
    , m_speedMultiplier(1.0)
    , m_battleManager(nullptr)
    , m_isPvp(false)
    , m_isHost(false)
    , m_inBattlePhase(false)
    , m_waveStarted(false)
    , m_localWaveClear(false)
    , m_peerWaveClear(false)
    , m_stateSyncTimer(0.0)
    , m_opponentLabel(nullptr)
{
    initUI();
    connectSignals();
}

// ========== initUI() —— 初始化界面 ==========
void BattlePage::initUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // ===== 顶部状态栏 =====
    {
        QWidget *barContainer = new QWidget(this);
        barContainer->setFixedHeight(50);
        barContainer->setStyleSheet(
            "QWidget {"
            "  background: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
            "    stop:0 rgba(10,20,38,0.96), stop:1 rgba(14,28,52,0.92));"
            "  border-bottom: 2px solid rgba(0,212,255,0.40);"
            "}"
        );
        QHBoxLayout *layout = new QHBoxLayout(barContainer);
        layout->setContentsMargins(20, 0, 20, 0);

        m_waveLabel = new QLabel("🌊 波次: 0", barContainer);
        m_waveLabel->setStyleSheet("color: #FFFFFF; font-size: 16px; font-weight: bold; background: transparent;");

        m_coreHpLabel = new QLabel("🏰 核心: 10", barContainer);
        m_coreHpLabel->setStyleSheet("color: #00E5FF; font-size: 16px; font-weight: bold; background: transparent;");

        m_resourceLabel = new QLabel("💰 资源: 100", barContainer);
        m_resourceLabel->setStyleSheet("color: #FFD54F; font-size: 16px; font-weight: bold; background: transparent;");

        // 对手信息标签（PVP 模式显示）
        m_opponentLabel = new QLabel("对手资源: --", barContainer);
        m_opponentLabel->setStyleSheet("color: #FF8A80; font-size: 16px; font-weight: bold; background: transparent;");
        m_opponentLabel->setVisible(false);  // 默认隐藏，PVP 模式下显示

        // 退出按钮
        m_btnExit = new QPushButton("✕ 退出", barContainer);
        m_btnExit->setFixedSize(80, 36);
        m_btnExit->setStyleSheet(
            "QPushButton {"
            "  background-color: rgba(255,82,82,0.2); color: #FF5252;"
            "  border: 2px solid rgba(255,82,82,0.5); border-radius: 8px;"
            "  font-size: 14px; font-weight: bold;"
            "}"
            "QPushButton:hover {"
            "  background-color: rgba(255,82,82,0.4);"
            "  border: 2px solid #FF5252;"
            "}"
        );
        m_btnExit->setCursor(Qt::PointingHandCursor);

        layout->addWidget(m_waveLabel);
        layout->addStretch();
        layout->addWidget(m_coreHpLabel);
        layout->addStretch();
        layout->addWidget(m_resourceLabel);
        layout->addSpacing(15);
        layout->addWidget(m_opponentLabel);
        layout->addSpacing(15);
        layout->addWidget(m_btnExit);
        mainLayout->addWidget(barContainer);
    }

    // ===== 中央战斗视口 =====
    m_battleView = new BattleView(this);
    QHBoxLayout *viewLayout = new QHBoxLayout();
    viewLayout->addStretch();
    viewLayout->addWidget(m_battleView);
    viewLayout->addStretch();
    mainLayout->addLayout(viewLayout, 1);

    // ===== 底部操作栏 =====
    {
        QWidget *barContainer = new QWidget(this);
        barContainer->setFixedHeight(90);
        barContainer->setStyleSheet(
            "QWidget {"
            "  background: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
            "    stop:0 rgba(14,28,52,0.92), stop:1 rgba(10,20,38,0.96));"
            "  border-top: 2px solid rgba(0,212,255,0.40);"
            "}"
        );
        QHBoxLayout *layout = new QHBoxLayout(barContainer);
        layout->setContentsMargins(15, 10, 15, 10);

        // 卡牌按钮
        QStringList cardNames = {"突击手", "采矿工", "医生", "狙击手", "AOE炮塔"};
        QVector<game::core::CardKind> cardKinds = {
            game::core::CardKind::Attack,
            game::core::CardKind::Produce,
            game::core::CardKind::Heal,
            game::core::CardKind::Sniper,
            game::core::CardKind::Aoe
        };
        QStringList cardCosts = {"40", "35", "30", "50", "60"};
        QStringList cardIcons = {"⚔", "⛏", "❤", "🎯", "💥"};

        for (int i = 0; i < cardNames.size(); ++i) {
            QPushButton *cardBtn = new QPushButton(barContainer);
            cardBtn->setFixedSize(90, 70);
            cardBtn->setText(QString("%1\n%2\n💰%3")
                                 .arg(cardIcons[i])
                                 .arg(cardNames[i])
                                 .arg(cardCosts[i]));
            cardBtn->setStyleSheet(
                "QPushButton {"
                "  background-color: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
                "    stop:0 rgba(22,50,90,0.90), stop:1 rgba(12,30,55,0.85));"
                "  color: #FFFFFF;"
                "  border: 2px solid rgba(0,212,255,0.55); border-radius: 10px;"
                "  font-size: 11px; font-weight: bold;"
                "}"
                "QPushButton:hover {"
                "  border: 2px solid #00D4FF;"
                "  background-color: rgba(0,212,255,0.18);"
                "  color: #00E5FF;"
                "}"
                "QPushButton:pressed { background-color: rgba(0,212,255,0.30); }"
            );
            cardBtn->setCursor(Qt::PointingHandCursor);

            game::core::CardKind kind = cardKinds[i];
            connect(cardBtn, &QPushButton::clicked, this, [this, kind]() {
                m_battleView->m_mode = BattleView::InteractionMode::DEPLOYING;
                m_battleView->m_selectedCardKind = kind;
                m_battleView->hideRadialMenu();
                m_battleView->update();
            });

            m_cardButtons.append(cardBtn);
            layout->addWidget(cardBtn);
        }

        layout->addSpacing(20);

        // 暂停按钮
        m_btnPause = new QPushButton("⏸", barContainer);
        m_btnPause->setFixedSize(50, 45);
        m_btnPause->setStyleSheet(
            "QPushButton { background-color: rgba(22,50,90,0.85); color: #FFFFFF;"
            "  border: 2px solid rgba(0,212,255,0.50); border-radius: 10px; font-size: 16px; }"
            "QPushButton:hover { color: #00E5FF; border: 2px solid #00D4FF; }"
        );
        layout->addWidget(m_btnPause);

        // 加速按钮
        m_btnSpeed = new QPushButton("1x", barContainer);
        m_btnSpeed->setFixedSize(50, 45);
        m_btnSpeed->setStyleSheet(m_btnPause->styleSheet());
        layout->addWidget(m_btnSpeed);

        // 技能按钮
        m_btnSkill = new QPushButton("⚡", barContainer);
        m_btnSkill->setFixedSize(50, 45);
        m_btnSkill->setEnabled(false);
        m_btnSkill->setStyleSheet(
            "QPushButton { background-color: rgba(22,40,70,0.60); color: #7AACCC;"
            "  border: 2px solid rgba(0,212,255,0.25); border-radius: 10px; font-size: 16px; }"
        );
        m_btnSkill->setToolTip("技能为自动释放");
        layout->addWidget(m_btnSkill);

        mainLayout->addWidget(barContainer);
    }

    // 页面背景
    this->setStyleSheet("BattlePage { background-color: #0B1622; }");

    // 游戏主循环定时器（约 60FPS）
    m_gameTimer = new QTimer(this);
    connect(m_gameTimer, &QTimer::timeout, this, &BattlePage::onGameTick);
}

// ========== setNetworkContext() —— 设置网络上下文（PVP 模式） ==========
void BattlePage::setNetworkContext(const NetworkContext& ctx)
{
    m_netCtx = ctx;
    m_isPvp = ctx.isPvp;
    m_isHost = ctx.isHost;
    if (m_battleView) {
        m_battleView->m_localIsHost = !m_isPvp || m_isHost;
    }
}

// ========== setupPveMap() —— 初始化 PVE 地图 ==========
void BattlePage::setupPveMap()
{
    auto& map = m_battleManager->map();

    game::core::LoadedMapConfig mapConfig;
    std::string loadError;
    const QString mapPath = findProjectFile("assets/maps/lab_map_01.json");
    if (!mapPath.isEmpty() &&
        game::core::MapConfigLoader::loadFromJson(mapPath.toStdString(), mapConfig, &loadError)) {
        map.resize(mapConfig.rows, mapConfig.cols, game::core::TerrainType::NoDeploy, 0);

        for (const auto& tile : mapConfig.tiles) {
            map.setGrid({tile.row, tile.col},
                        terrainFromMapTile(tile.type),
                        terrainHeightFromMapTile(tile.type));
        }

        for (const auto& route : mapConfig.routesA) {
            for (const auto& pos : route) {
                const game::core::MapGrid* grid = map.gridAt(pos);
                if (grid && grid->terrainType() == game::core::TerrainType::NoDeploy) {
                    map.setGrid(pos, game::core::TerrainType::Path, 0);
                }
            }
        }

        if (!mapConfig.routesA.empty()) {
            m_battleManager->setPaths(mapConfig.routesA);
        } else if (!mapConfig.spawnA.empty()) {
            m_battleManager->setSpawnPoint(mapConfig.spawnA.front());
        }

        const game::core::MapPosition spawnPos = !mapConfig.spawnA.empty()
                                                    ? mapConfig.spawnA.front()
                                                    : (!mapConfig.routesA.empty() && !mapConfig.routesA.front().empty()
                                                           ? mapConfig.routesA.front().front()
                                                           : game::core::MapPosition(1, 1));
        const game::core::MapPosition corePos = !mapConfig.coreA.empty()
                                                   ? mapConfig.coreA.front()
                                                   : (!mapConfig.routesA.empty() && !mapConfig.routesA.front().empty()
                                                          ? mapConfig.routesA.front().back()
                                                          : game::core::MapPosition(map.rows() - 2, map.cols() - 2));

        m_battleView->setMapSize(map.rows(), map.cols());
        m_battleView->m_spawnPos = spawnPos;
        m_battleView->m_corePos = corePos;

        if (!mapConfig.image.empty()) {
            QString imagePath = QFileInfo(mapPath).dir().filePath(QString::fromStdString(mapConfig.image));
            if (!m_battleView->setBackgroundImage(imagePath)) {
                qWarning() << "[BattlePage] failed to load PVE map image:" << imagePath;
            }
        } else {
            m_battleView->clearBackgroundImage();
        }

        qDebug() << "[BattlePage] loaded PVE map from" << mapPath
                 << "size" << map.rows() << "x" << map.cols()
                 << "routesA" << static_cast<int>(mapConfig.routesA.size());
        return;
    }

    if (!mapPath.isEmpty()) {
        qWarning() << "[BattlePage] failed to load PVE map:" << QString::fromStdString(loadError)
                   << "path:" << mapPath;
    } else {
        qWarning() << "[BattlePage] PVE map JSON not found, using fallback hardcoded map.";
    }

    map.resize(game::core::constants::DefaultMapRows,
               game::core::constants::DefaultMapCols,
               game::core::TerrainType::FlatLand,
               0);
    m_battleView->clearBackgroundImage();

    game::core::MapPosition spawnPos(1, 1);
    game::core::MapPosition corePos(10, 16);

    // 设置 NoDeploy 边框
    for (int r = 0; r < map.rows(); ++r) {
        map.setGrid({r, 0}, game::core::TerrainType::NoDeploy, 0);
        map.setGrid({r, map.cols() - 1}, game::core::TerrainType::NoDeploy, 0);
    }
    for (int c = 0; c < map.cols(); ++c) {
        map.setGrid({0, c}, game::core::TerrainType::NoDeploy, 0);
        map.setGrid({map.rows() - 1, c}, game::core::TerrainType::NoDeploy, 0);
    }

    // S 型路径
    std::vector<game::core::MapPosition> pathCells = {
        {1,1}, {1,2}, {1,3}, {1,4}, {1,5}, {1,6},
        {2,6}, {3,6},
        {4,6}, {4,7}, {4,8}, {4,9}, {4,10}, {4,11}, {4,12}, {4,13},
        {5,13}, {6,13},
        {7,13}, {7,12}, {7,11}, {7,10}, {7,9}, {7,8}, {7,7}, {7,6},
        {8,6}, {9,6},
        {10,6}, {10,7}, {10,8}, {10,9}, {10,10}, {10,11},
        {10,12}, {10,13}, {10,14}, {10,15}, {10,16}
    };

    for (const auto& pos : pathCells) {
        map.setGrid(pos, game::core::TerrainType::Path, 0);
    }

    // 高台
    std::vector<game::core::MapPosition> highGroundCells = {
        {2,2}, {2,3}, {3,2}, {3,3},
        {2,8}, {2,9}, {3,8}, {3,9},
        {5,5}, {5,6}, {6,5}, {6,6},
        {5,10}, {5,11}, {6,10}, {6,11},
        {8,9}, {8,10}, {9,9}, {9,10},
        {8,14}, {8,15}, {9,14}, {9,15}
    };
    for (const auto& pos : highGroundCells) {
        map.setGrid(pos, game::core::TerrainType::HighGround, 1);
    }

    m_battleManager->setSpawnPoint(spawnPos);
    m_battleManager->setPath(pathCells);

    m_battleView->m_spawnPos = spawnPos;
    m_battleView->m_corePos = corePos;
}

// ========== setupPvpMap() —— 初始化 PVP 对称地图 ==========
void BattlePage::setupPvpMap()
{
    auto& map = m_battleManager->map();
    m_battleView->clearBackgroundImage();

    // 新 PVP 地图布局（12x18）
    // 出怪口在上方 (1,6) 和 (1,11)
    // A方核心在左下角 (10,1)，B方核心在右下角 (10,16)

    // 1) 设置 NoDeploy 边框
    for (int r = 0; r < map.rows(); ++r) {
        map.setGrid({r, 0}, game::core::TerrainType::NoDeploy, 0);
        map.setGrid({r, map.cols() - 1}, game::core::TerrainType::NoDeploy, 0);
    }
    for (int c = 0; c < map.cols(); ++c) {
        map.setGrid({0, c}, game::core::TerrainType::NoDeploy, 0);
        map.setGrid({map.rows() - 1, c}, game::core::TerrainType::NoDeploy, 0);
    }

    // 2) 设置出生点
    game::core::MapPosition spawn1(1, 6);   // 上方出生点
    game::core::MapPosition spawn2(1, 11);  // 左上角出生点
    map.setGrid(spawn1, game::core::TerrainType::SpawnPoint, 0);
    map.setGrid(spawn2, game::core::TerrainType::SpawnPoint, 0);

    // 3) 设置核心
    game::core::MapPosition coreA(10, 1);   // A方核心（左下角）
    game::core::MapPosition coreB(10, 16);  // B方核心（右下角）
    map.setGrid(coreA, game::core::TerrainType::CoreA, 0);
    map.setGrid(coreB, game::core::TerrainType::CoreB, 0);

    // 4) 设置路径
    // 从出生点1向下到分叉点
    std::vector<game::core::MapPosition> path1_down = {
        {2,6}, {3,6}, {4,6}, {5,6}, {6,6}, {7,6}, {8,6}
    };
    // 从出生点2向下到分叉点
    std::vector<game::core::MapPosition> path2_down = {
        {2,11}, {3,11}, {4,11}, {5,11}, {6,11}, {7,11}, {8,11}
    };
    // 横向连接路径
    std::vector<game::core::MapPosition> path_horizontal = {
        {8,7}, {8,8}, {8,9}, {8,10}
    };
    // 向左下到A方核心
    std::vector<game::core::MapPosition> path_toA = {
        {9,6}, {9,5}, {9,4}, {9,3}, {9,2}, {9,1}
    };
    // 向右下到B方核心
    std::vector<game::core::MapPosition> path_toB = {
        {9,11}, {9,12}, {9,13}, {9,14}, {9,15}, {9,16}
    };

    // 标记所有路径
    for (const auto& pos : path1_down)
        map.setGrid(pos, game::core::TerrainType::Path, 0);
    for (const auto& pos : path2_down)
        map.setGrid(pos, game::core::TerrainType::Path, 0);
    for (const auto& pos : path_horizontal)
        map.setGrid(pos, game::core::TerrainType::Path, 0);
    for (const auto& pos : path_toA)
        map.setGrid(pos, game::core::TerrainType::Path, 0);
    for (const auto& pos : path_toB)
        map.setGrid(pos, game::core::TerrainType::Path, 0);

    // 5) 设置高台（战略要地）
    std::vector<game::core::MapPosition> highGround = {
        // A方区域的高台
        {3,2}, {3,3}, {5,2}, {5,3}, {7,2}, {7,3},
        // B方区域的高台
        {3,14}, {3,15}, {5,14}, {5,15}, {7,14}, {7,15},
        // 中间区域的高台
        {4,8}, {4,9}, {6,8}, {6,9}
    };
    for (const auto& pos : highGround)
        map.setGrid(pos, game::core::TerrainType::HighGround, 1);

    // 6) 设置双核心路线。WaveSpawner 会按怪物序号轮流分配，保证两边压力接近。
    std::vector<game::core::MapPosition> pathToA;
    pathToA.push_back(spawn1);
    for (const auto& pos : path1_down) pathToA.push_back(pos);
    for (const auto& pos : path_toA) pathToA.push_back(pos);
    pathToA.push_back(coreA);

    std::vector<game::core::MapPosition> pathToB;
    pathToB.push_back(spawn2);
    for (const auto& pos : path2_down) pathToB.push_back(pos);
    for (const auto& pos : path_toB) pathToB.push_back(pos);
    pathToB.push_back(coreB);

    m_battleManager->setPaths({pathToA, pathToB});

    // 根据玩家角色设置显示标记
    m_battleView->m_spawnPos = spawn1;  // 所有玩家看到相同出生点
    m_battleView->m_corePos = m_isHost ? coreA : coreB;  // 各自看到自己的核心
}

// ========== sendDeployAction() —— 发送部署操作 ==========
void BattlePage::sendDeployAction(game::core::CardKind kind, game::core::MapPosition pos)
{
    // 本地执行
    if (m_battleManager) {
        m_battleManager->deployCard(kind, pos);
    }

    // 网络发送
    if (m_isPvp) {
        game::network::DeployPayload payload;
        payload.cardKind = static_cast<quint8>(kind);
        payload.row = static_cast<quint8>(pos.row);
        payload.col = static_cast<quint8>(pos.col);
        payload.unitId = 0;

        QByteArray body(reinterpret_cast<const char*>(&payload), sizeof(payload));

        if (m_isHost && m_netCtx.server) {
            m_netCtx.server->sendPacket(game::network::MsgType::DEPLOY, body);
        } else if (!m_isHost && m_netCtx.client) {
            m_netCtx.client->sendPacket(game::network::MsgType::DEPLOY, body);
        }
    }
}

// ========== sendUpgradeAction() —— 发送升级操作 ==========
void BattlePage::sendUpgradeAction(int unitId)
{
    if (m_battleManager) {
        m_battleManager->upgradeCard(unitId);
    }

    if (m_isPvp) {
        game::network::UpgradePayload payload;
        payload.unitId = static_cast<quint8>(unitId);
        payload.targetLevel = 0;

        QByteArray body(reinterpret_cast<const char*>(&payload), sizeof(payload));

        if (m_isHost && m_netCtx.server) {
            m_netCtx.server->sendPacket(game::network::MsgType::UPGRADE_UNIT, body);
        } else if (!m_isHost && m_netCtx.client) {
            m_netCtx.client->sendPacket(game::network::MsgType::UPGRADE_UNIT, body);
        }
    }
}

// ========== sendMoveAction() —— 发送移动操作 ==========
void BattlePage::sendMoveAction(int unitId, game::core::MapPosition target)
{
    if (m_battleManager) {
        m_battleManager->moveCard(unitId, target);
    }

    if (m_isPvp) {
        QByteArray body;
        body.append(static_cast<char>(unitId));
        body.append(static_cast<char>(target.row));
        body.append(static_cast<char>(target.col));

        if (m_isHost && m_netCtx.server) {
            m_netCtx.server->sendPacket(game::network::MsgType::MOVE_UNIT, body);
        } else if (!m_isHost && m_netCtx.client) {
            m_netCtx.client->sendPacket(game::network::MsgType::MOVE_UNIT, body);
        }
    }
}

// ========== sendRecallAction() —— 发送撤回操作 ==========
void BattlePage::sendRecallAction(int unitId)
{
    if (m_battleManager) {
        m_battleManager->recallCard(unitId);
    }

    if (m_isPvp) {
        game::network::RecallPayload payload;
        payload.unitId = static_cast<quint8>(unitId);

        QByteArray body(reinterpret_cast<const char*>(&payload), sizeof(payload));

        if (m_isHost && m_netCtx.server) {
            m_netCtx.server->sendPacket(game::network::MsgType::RECALL_UNIT, body);
        } else if (!m_isHost && m_netCtx.client) {
            m_netCtx.client->sendPacket(game::network::MsgType::RECALL_UNIT, body);
        }
    }
}

void BattlePage::completePvpWave()
{
    if (!m_inBattlePhase) return;

    qDebug() << "[BattlePage] wave complete, back to deploy";
    m_inBattlePhase = false;
    m_waveStarted = false;
    m_localWaveClear = false;
    m_peerWaveClear = false;
    if (m_battleManager) {
        m_battleManager->clearMonsters();
    }
    m_gameTimer->stop();
    emit signalBackToDeploy();
}

void BattlePage::sendBattleState(const game::core::BattleSnapshot& snapshot)
{
    if (!m_isPvp || !m_isHost || !m_netCtx.server) return;
    m_netCtx.server->sendPacket(game::network::MsgType::BATTLE_STATE,
                                game::network::BattleStateCodec::encodeHostSnapshot(snapshot));
}

void BattlePage::handleRemoteBattleState(const game::core::BattleSnapshot& snapshot)
{
    if (m_battleManager) {
        m_battleManager->resources().setResources(snapshot.resources);
        m_battleManager->resources().setBaseHealth(snapshot.baseHealth);
        m_battleManager->opponentResources().setResources(snapshot.opponentResources);
        m_battleManager->opponentResources().setBaseHealth(snapshot.opponentBaseHealth);
        m_battleManager->syncPvpUnitsFromHostSnapshot(snapshot, m_isHost);
    }

    m_battleView->updateFromSnapshot(snapshot);
    updateStatusBar(snapshot);

    if (m_isPvp && !m_isHost && m_waveStarted && !snapshot.waveActive && !m_localWaveClear) {
        m_localWaveClear = true;
        qDebug() << "[BattlePage] remote state shows WAVE_CLEAR for wave" << m_currentWaveId;
        if (m_netCtx.client) {
            QByteArray body;
            body.append(static_cast<char>(m_currentWaveId));
            m_netCtx.client->sendPacket(game::network::MsgType::WAVE_CLEAR, body);
        }
    }
}

// ========== onNetworkPacket() —— 处理网络包 ==========
void BattlePage::onNetworkPacket(game::network::MsgType type, const QByteArray& body)
{
    if (!m_battleManager) return;

    switch (type) {
    case game::network::MsgType::DEPLOY: {
        if (!m_inBattlePhase) break;
        if (body.size() >= 4) {
            auto kind = static_cast<game::core::CardKind>(static_cast<quint8>(body[0]));
            int row = static_cast<quint8>(body[1]);
            int col = static_cast<quint8>(body[2]);
            // 对方部署，使用 deployOpponentCard
            m_battleManager->deployOpponentCard(kind, game::core::MapPosition(row, col));
            qDebug() << "[BattlePage] received opponent DEPLOY:" << (int)kind << "at" << row << col;
        }
        break;
    }
    case game::network::MsgType::UPGRADE_UNIT: {
        if (!m_inBattlePhase) break;
        if (body.size() >= 2) {
            int unitId = static_cast<quint8>(body[0]);
            // 对方升级，需要加上偏移量
            m_battleManager->upgradeOpponentCard(unitId);
            qDebug() << "[BattlePage] received opponent UPGRADE unit" << unitId;
        }
        break;
    }
    case game::network::MsgType::MOVE_UNIT: {
        if (!m_inBattlePhase) break;
        if (body.size() >= 3) {
            int unitId = static_cast<quint8>(body[0]);
            int row = static_cast<quint8>(body[1]);
            int col = static_cast<quint8>(body[2]);
            // 对方移动
            m_battleManager->moveOpponentCard(unitId, game::core::MapPosition(row, col));
            qDebug() << "[BattlePage] received opponent MOVE unit" << unitId << "to" << row << col;
        }
        break;
    }
    case game::network::MsgType::RECALL_UNIT: {
        if (!m_inBattlePhase) break;
        if (body.size() >= 1) {
            int unitId = static_cast<quint8>(body[0]);
            // 对方撤回
            m_battleManager->recallOpponentCard(unitId);
            qDebug() << "[BattlePage] received opponent RECALL unit" << unitId;
        }
        break;
    }
    case game::network::MsgType::WAVE_START: {
        if (body.size() >= 1) {
            int waveId = static_cast<quint8>(body[0]);
            if (m_waveStarted && m_currentWaveId == waveId) {
                qDebug() << "[BattlePage] duplicated WAVE_START ignored:" << waveId;
                break;
            }
            m_currentWaveId = waveId;
            m_waveStarted = true;
            m_battleManager->startWave(waveId);
            qDebug() << "[BattlePage] received WAVE_START:" << waveId;
        }
        break;
    }
    case game::network::MsgType::WAVE_COMPLETE: {
        if (!m_inBattlePhase) break;
        qDebug() << "[BattlePage] received WAVE_COMPLETE, back to deploy";
        completePvpWave();
        break;
    }
    case game::network::MsgType::WAVE_CLEAR: {
        if (!m_inBattlePhase || !m_isHost) break;
        m_peerWaveClear = true;
        qDebug() << "[BattlePage] received peer WAVE_CLEAR for wave" << m_currentWaveId;
        if (m_localWaveClear) {
            if (m_netCtx.server) {
                QByteArray body;
                body.append(static_cast<char>(m_currentWaveId));
                m_netCtx.server->sendPacket(game::network::MsgType::WAVE_COMPLETE, body);
            }
            completePvpWave();
        }
        break;
    }
    case game::network::MsgType::RESOURCE_SYNC: {
        if (m_isPvp && m_inBattlePhase) {
            break;
        }
        if (body.size() >= 4) {
            quint32 amount;
            memcpy(&amount, body.constData(), 4);
            amount = qFromBigEndian(amount);
            if (m_opponentLabel) {
                m_opponentLabel->setText(QString("对手资源: %1").arg(amount));
            }
        }
        break;
    }
    case game::network::MsgType::BATTLE_STATE: {
        if (!m_inBattlePhase || m_isHost) break;
        const game::core::MapSnapshot map = m_battleManager
                                                ? m_battleManager->snapshot().map
                                                : game::core::MapSnapshot();
        auto decode = game::network::BattleStateCodec::decodeHostSnapshot(body, map);
        if (decode.ok) {
            if (decode.checksumPresent && !decode.checksumValid) {
                qDebug() << "[BattlePage] BATTLE_STATE checksum mismatch, remote:"
                         << decode.remoteChecksum << "local:" << decode.localChecksum;
            }
            handleRemoteBattleState(decode.snapshot);
        } else {
            qDebug() << "[BattlePage] failed to parse BATTLE_STATE, size:" << body.size();
        }
        break;
    }
    default:
        break;
    }
}

// ========== startBattle() —— 开始战斗 ==========
void BattlePage::startBattle()
{
    // 获取 BattleManager 引用（通过 MainWindow）
    MainWindow *mainWin = qobject_cast<MainWindow*>(window());
    if (mainWin) {
        m_battleManager = mainWin->battleManager();
    }

    if (!m_battleManager) return;

    // PVE：清理之前的状态；PVP：保留 DeployPage 的部署数据
    if (!m_isPvp) {
        m_battleManager->clearBattle();
    }

    // 根据 PVP/PVE 模式初始化地图
    if (m_isPvp) {
        setupPvpMap();

        // 启用 PVP 模式
        m_battleManager->setPvpMode(true);

        // 应用同步的随机种子
        m_battleManager->setRandomSeed(m_netCtx.seed);

        // 初始化视野系统
        auto& vision = m_battleManager->visionManager();
        vision.initDefaultVision(game::core::MapPosition(10, 1), game::core::MapPosition(10, 16));

        // 连接网络信号
        if (m_isHost && m_netCtx.server) {
            connect(m_netCtx.server, &game::network::GameServer::packetReceived,
                    this, &BattlePage::onNetworkPacket, Qt::UniqueConnection);
        } else if (!m_isHost && m_netCtx.client) {
            connect(m_netCtx.client, &game::network::GameClient::packetReceived,
                    this, &BattlePage::onNetworkPacket, Qt::UniqueConnection);
        }

        // 显示对手信息
        if (m_opponentLabel) {
            m_opponentLabel->setVisible(true);
            m_opponentLabel->setText("对手核心: --  资源: --");
        }

        qDebug() << "[BattlePage] PVP mode started, isHost:" << m_isHost << "seed:" << m_netCtx.seed;
    } else {
        setupPveMap();

        // 隐藏对手信息
        if (m_opponentLabel) {
            m_opponentLabel->setVisible(false);
        }
    }

    // 重置状态
    m_isPaused = false;
    m_speedMultiplier = 1.0;
    m_waveTimer = 0.0;
    m_stateSyncTimer = 0.0;
    m_inBattlePhase = true;
    m_waveStarted = false;
    m_localWaveClear = false;
    m_peerWaveClear = false;
    m_currentWaveId = (m_isPvp && m_battleManager->currentWave() > 0)
                          ? m_battleManager->currentWave() + 1
                          : 1;

    // PVP 战斗阶段是纯观看，部署/升级/移动/撤回全部放在迷雾部署阶段。
    for (auto* btn : m_cardButtons) {
        btn->setEnabled(!m_isPvp);
    }
    if (m_battleView) {
        m_battleView->m_interactionEnabled = !m_isPvp;
        m_battleView->hideRadialMenu();
        m_battleView->m_mode = BattleView::InteractionMode::NONE;
    }

    // 启动波次：Host 驱动 + 广播，Client 等待 WAVE_START
    if (m_isPvp) {
        if (m_isHost) {
            qDebug() << "[BattlePage] Host starting wave" << m_currentWaveId
                     << "seed:" << m_netCtx.seed;
            m_waveStarted = true;
            m_battleManager->startWave(m_currentWaveId);

            QByteArray body;
            body.append(static_cast<char>(m_currentWaveId));
            m_netCtx.server->sendPacket(game::network::MsgType::WAVE_START, body);
        } else {
            qDebug() << "[BattlePage] Client waiting for WAVE_START, seed:" << m_netCtx.seed
                     << "expected wave:" << m_currentWaveId;
        }
    } else {
        m_battleManager->startWave(m_currentWaveId);
    }

    // 启动游戏主循环
    m_gameTimer->start(16);

    // 立即渲染一帧
    game::core::BattleSnapshot snap = m_battleManager->snapshot();
    m_battleView->updateFromSnapshot(snap);
    updateStatusBar(snap);
}

// ========== onGameTick() —— 游戏主循环回调 ==========
void BattlePage::onGameTick()
{
    if (!m_battleManager || m_isPaused) return;

    double deltaSeconds = game::core::constants::DefaultFrameSeconds * m_speedMultiplier;

    // Client: 还没收到 WAVE_START 时不做逻辑
    if (m_isPvp && !m_waveStarted) {
        // 还没收到波次，只渲染
        game::core::BattleSnapshot snap = m_battleManager->snapshot();
        m_battleView->updateFromSnapshot(snap);
        updateStatusBar(snap);
        return;
    }

    // PVP Client 不再本地推进战斗，避免血量/死亡状态和 Host 分叉。
    // 战斗画面完全由 Host 的 BATTLE_STATE 快照驱动。
    if (m_isPvp && !m_isHost) {
        return;
    }

    m_battleManager->update(deltaSeconds);
    game::core::BattleSnapshot snap = m_battleManager->snapshot();

    if (m_isPvp && m_isHost) {
        m_stateSyncTimer += deltaSeconds;
        if (m_stateSyncTimer >= 0.10) {
            m_stateSyncTimer = 0.0;
            sendBattleState(snap);
        }
    }

    // PVP: 本端怪物清空后只发送/记录确认，Host 等双方都确认后统一回部署。
    if (m_isPvp && !snap.waveActive) {
        if (!m_localWaveClear) {
            m_localWaveClear = true;
            qDebug() << "[BattlePage] local WAVE_CLEAR for wave" << m_currentWaveId;

            if (m_isHost) {
                if (m_peerWaveClear) {
                    if (m_netCtx.server) {
                        QByteArray body;
                        body.append(static_cast<char>(m_currentWaveId));
                        m_netCtx.server->sendPacket(game::network::MsgType::WAVE_COMPLETE, body);
                    }
                    completePvpWave();
                }
            } else if (m_netCtx.client) {
                QByteArray body;
                body.append(static_cast<char>(m_currentWaveId));
                m_netCtx.client->sendPacket(game::network::MsgType::WAVE_CLEAR, body);
            }
        }

        game::core::BattleSnapshot currentSnap = m_battleManager->snapshot();
        m_battleView->updateFromSnapshot(currentSnap);
        updateStatusBar(currentSnap);
        return;
    }

    // PVE: 自动下一波
    if (!m_isPvp && !snap.waveActive) {
        m_waveTimer += deltaSeconds;
        if (m_waveTimer >= WAVE_INTERVAL) {
            m_waveTimer = 0.0;
            m_currentWaveId++;
            m_battleManager->startWave(m_currentWaveId);
            snap = m_battleManager->snapshot();
        }
    } else {
        m_waveTimer = 0.0;
    }

    // PVP: 定期同步资源
    m_battleView->updateFromSnapshot(snap);
    updateStatusBar(snap);

    if (snap.gameOver) {
        m_inBattlePhase = false;
        m_waveStarted = false;
        m_gameTimer->stop();
        emit signalBattleEnd();
    }
}

// ========== updateStatusBar() —— 更新状态栏 ==========
void BattlePage::updateStatusBar(const game::core::BattleSnapshot &snapshot)
{
    m_waveLabel->setText(QString("🌊 波次: %1  场上: %2  %3")
                             .arg(snapshot.currentWave)
                             .arg(snapshot.monsters.size())
                             .arg(snapshot.waveActive ? "出怪中" : "已清空"));
    m_coreHpLabel->setText(QString("🏰 核心: %1").arg(snapshot.baseHealth));
    m_resourceLabel->setText(QString("💰 资源: %1").arg(snapshot.resources));
    if (m_isPvp && m_opponentLabel) {
        m_opponentLabel->setText(QString("对手核心: %1  资源: %2")
                                     .arg(snapshot.opponentBaseHealth)
                                     .arg(snapshot.opponentResources));
    }
}

// ========== connectSignals() —— 连接信号槽 ==========
void BattlePage::connectSignals()
{
    // 暂停按钮
    connect(m_btnPause, &QPushButton::clicked, this, [this]() {
        m_isPaused = !m_isPaused;
        m_btnPause->setText(m_isPaused ? "▶" : "⏸");
    });

    // 加速按钮
    connect(m_btnSpeed, &QPushButton::clicked, this, [this]() {
        m_speedMultiplier = (m_speedMultiplier == 1.0) ? 2.0 : 1.0;
        m_btnSpeed->setText(m_speedMultiplier == 2.0 ? "2x" : "1x");
    });

    // 退出按钮
    connect(m_btnExit, &QPushButton::clicked, this, [this]() {
        qDebug() << "[BattlePage] exit button clicked";
        QMessageBox::StandardButton reply = QMessageBox::question(
            this,
            "确认退出",
            "确定要退出当前战斗吗？\n未保存的进度将会丢失。",
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No
        );

        if (reply == QMessageBox::Yes) {
            qDebug() << "[BattlePage] user confirmed exit";
            // 停止游戏循环
            if (m_gameTimer) {
                m_gameTimer->stop();
            }
            m_inBattlePhase = false;
            m_waveStarted = false;
            // 清理战斗状态
            if (m_battleManager) {
                m_battleManager->clearBattle();
            }
            // 发出退出信号
            emit signalBattleEnd();
        }
    });

    // ===== BattleView 的操作信号 → 调用 send 方法（本地 + 网络） =====

    // 部署卡牌
    connect(m_battleView, &BattleView::signalDeployCard,
            this, [this](game::core::CardKind kind, game::core::MapPosition pos) {
        sendDeployAction(kind, pos);
    });

    // 升级单位
    connect(m_battleView, &BattleView::signalUpgradeCard,
            this, [this](int unitId) {
        sendUpgradeAction(unitId);
    });

    // 移动单位
    connect(m_battleView, &BattleView::signalMoveCard,
            this, [this](int unitId, game::core::MapPosition target) {
        sendMoveAction(unitId, target);
    });

    // 撤回单位
    connect(m_battleView, &BattleView::signalRecallCard,
            this, [this](int unitId) {
        sendRecallAction(unitId);
    });
}
