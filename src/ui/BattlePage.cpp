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
#include "ui/PvpMapLayout.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFont>
#include <QPen>
#include <QBrush>
#include <QConicalGradient>
#include <QPainterPath>
#include <QRadialGradient>
#include <QLinearGradient>
#include <vector>
#include <QtMath>
#include <QDebug>
#include <QtEndian>
#include <QCoreApplication>
#include <QFileInfo>
#include <QDir>
#include <QKeyEvent>
#include <QResizeEvent>
#include <QSettings>
#include <QPixmapCache>
#include <algorithm>
#include <cmath>

// ========== 引入 MainWindow 头文件以获取 BattleManager ==========
#include "ui/MainWindow.h"
#include "ui/AudioManager.h"
#include "ui/PauseOverlay.h"
#include "ui/TutorialOverlay.h"

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
        if (info.exists() && info.isFile()) return info.absoluteFilePath();
    }
    return {};
}

game::core::TerrainType terrainFromMapTile(const std::string& type)
{
    if (type == "PATH_A" || type == "PATH_B" || type == "PATH_SHARED") {
        return game::core::TerrainType::Path;
    }
    if (type == "SPAWN_A" || type == "SPAWN_B") return game::core::TerrainType::SpawnPoint;
    if (type == "CORE_A") return game::core::TerrainType::CoreA;
    if (type == "CORE_B") return game::core::TerrainType::CoreB;
    if (type == "DEPLOY_A" || type == "DEPLOY_B" || type == "DEPLOY_NEUTRAL") {
        return game::core::TerrainType::FlatLand;
    }
    if (type == "HIGH_GROUND") return game::core::TerrainType::HighGround;
    return game::core::TerrainType::NoDeploy;
}

int terrainHeightFromMapTile(const std::string& type)
{
    return type == "HIGH_GROUND" ? 1 : 0;
}

QString cardDisplayName(game::core::CardKind kind)
{
    switch (kind) {
    case game::core::CardKind::Attack: return "Kiwi Scout";
    case game::core::CardKind::Sniper: return "Sniper Berry";
    case game::core::CardKind::Aoe: return "Orange Bomber";
    case game::core::CardKind::Specialist: return "Berry Tank";
    case game::core::CardKind::Produce: return "Miner Pine";
    case game::core::CardKind::Arsenal: return "Mango Engineer";
    case game::core::CardKind::Heal: return "Peach Healer";
    case game::core::CardKind::HeavyMedic: return "Coco Defender";
    case game::core::CardKind::Attack2: return "Grape Blaster";
    case game::core::CardKind::Heal2: return "Papaya Support";
    }
    return "Unknown";
}

QString mapDisplayName(const QString& mapId)
{
    if (mapId == "lab_map_02") return "Sunny Beach";
    if (mapId == "lab_map_01") return "Office Panic";
    return "Jungle Ruins";
}

QString replayUnitName(const game::core::UnitSnapshot& unit)
{
    return cardDisplayName(unit.kind);
}

QString replayMonsterName(game::core::MonsterKind kind)
{
    switch (kind) {
    case game::core::MonsterKind::ResBasic: return "Juice Thief";
    case game::core::MonsterKind::ResFast: return "Swift Thief";
    case game::core::MonsterKind::ResTank: return "Vault Breaker";
    case game::core::MonsterKind::AtkNormal: return "Beach Raider";
    case game::core::MonsterKind::AtkTank: return "Shell Crusher";
    case game::core::MonsterKind::AtkFast: return "Needle Runner";
    case game::core::MonsterKind::AtkSapper: return "Bomb Sprout";
    case game::core::MonsterKind::AtkBerserk: return "Rage Melon";
    case game::core::MonsterKind::AtkRanged: return "Spore Archer";
    case game::core::MonsterKind::AtkRegen: return "Moss Brute";
    }
    return "Unknown Threat";
}

int heatIndex(int row, int col, int cols)
{
    return row * cols + col;
}

void addHeat(QVector<int>& heat, int rows, int cols, int row, int col)
{
    if (row < 0 || col < 0 || row >= rows || col >= cols || cols <= 0) {
        return;
    }
    const int index = heatIndex(row, col, cols);
    if (index >= 0 && index < heat.size()) {
        heat[index] += 1;
    }
}

QRectF cardSourceRect(game::core::CardKind kind)
{
    switch (kind) {
    case game::core::CardKind::Produce: return {272, 214, 151, 199};
    case game::core::CardKind::Sniper: return {440, 214, 162, 199};
    case game::core::CardKind::Specialist: return {619, 214, 160, 199};
    case game::core::CardKind::Heal: return {798, 214, 161, 199};
    case game::core::CardKind::Attack: return {973, 214, 158, 199};
    case game::core::CardKind::Aoe: return {272, 428, 153, 198};
    case game::core::CardKind::HeavyMedic: return {441, 428, 161, 198};
    case game::core::CardKind::Arsenal: return {619, 428, 160, 198};
    case game::core::CardKind::Attack2: return {795, 423, 160, 201};
    case game::core::CardKind::Heal2: return {968, 423, 164, 201};
    }
    return {440, 214, 162, 199};
}

const QPixmap* unitArtwork(const game::core::UnitSnapshot& unit)
{
    static const QPixmap kiwi(":/images/new_art/unit_kiwi_scout.png");
    static const QPixmap miner(":/images/new_art/unit_miner_pine.png");
    static const QPixmap mango(":/images/new_art/unit_mango_engineer.png");
    static const QPixmap sniper(":/images/new_art/unit_sniper_berry.png");
    static const QPixmap bomber(":/images/new_art/unit_orange_bomber.png");
    static const QPixmap tank(":/images/new_art/unit_berry_tank.png");
    static const QPixmap healer(":/images/new_art/unit_peach_healer.png");
    static const QPixmap defender(":/images/new_art/unit_coco_defender.png");
    static const QPixmap grape(":/images/new_art/unit_grape_blaster.png");
    static const QPixmap papaya(":/images/new_art/unit_papaya_support.png");

    switch (unit.kind) {
    case game::core::CardKind::Attack: return &kiwi;
    case game::core::CardKind::Sniper: return &sniper;
    case game::core::CardKind::Aoe: return &bomber;
    case game::core::CardKind::Specialist: return &tank;
    case game::core::CardKind::Produce: return &miner;
    case game::core::CardKind::Arsenal: return &mango;
    case game::core::CardKind::Heal: return &healer;
    case game::core::CardKind::HeavyMedic: return &defender;
    case game::core::CardKind::Attack2: return &grape;
    case game::core::CardKind::Heal2: return &papaya;
    }
    return nullptr;
}

void drawCachedArtwork(QPainter& painter, const QRectF& target, const QPixmap& source)
{
    const QSize targetSize = target.size().toSize();
    const QString key = QString("unit-%1-%2x%3")
        .arg(source.cacheKey()).arg(targetSize.width()).arg(targetSize.height());
    QPixmap scaled;
    if (!QPixmapCache::find(key, &scaled)) {
        scaled = source.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        QPixmapCache::insert(key, scaled);
    }
    const QPointF topLeft(target.center().x() - scaled.width() * 0.5,
                          target.bottom() - scaled.height());
    painter.drawPixmap(topLeft, scaled);
}

QRectF commandRingRect(const QPointF& center, qreal extent, const QSize& viewport)
{
    const qreal height = extent * 3.2;
    const qreal width = height * 574.0 / 640.0;
    const QPointF shiftedCenter = center + QPointF(extent * 0.24, -extent * 0.22);
    QRectF ring(shiftedCenter.x() - width * 0.48,
                shiftedCenter.y() - height * 0.54,
                width,
                height);
    const qreal margin = 6.0;
    if (ring.left() < margin) ring.moveLeft(margin);
    if (ring.right() > viewport.width() - margin) ring.moveRight(viewport.width() - margin);
    if (ring.top() < margin) ring.moveTop(margin);
    if (ring.bottom() > viewport.height() - margin) ring.moveBottom(viewport.height() - margin);
    return ring;
}

QRect commandButtonRect(const QRectF& ring, const QRectF& normalized)
{
    return QRectF(ring.left() + ring.width() * normalized.x(),
                  ring.top() + ring.height() * normalized.y(),
                  ring.width() * normalized.width(),
                  ring.height() * normalized.height()).toRect();
}

void drawHealthBar(QPainter& painter,
                   const QRectF& rect,
                   int currentHealth,
                   int maxHealth,
                   const QColor& healthyColor)
{
    const qreal ratio = std::clamp(currentHealth / qreal(qMax(1, maxHealth)), 0.0, 1.0);
    const QColor fillColor = ratio <= 0.3
                                 ? QColor(206, 63, 45)
                                 : (ratio <= 0.6 ? QColor(226, 160, 48) : healthyColor);

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(QColor(75, 50, 31, 220), 2));
    painter.setBrush(QColor(64, 47, 34, 225));
    painter.drawRoundedRect(rect, rect.height() / 2.0, rect.height() / 2.0);

    QRectF fillRect = rect.adjusted(2, 2, -2, -2);
    fillRect.setWidth(fillRect.width() * ratio);
    if (fillRect.width() > 0.5) {
        QLinearGradient fill(fillRect.topLeft(), fillRect.bottomLeft());
        fill.setColorAt(0.0, fillColor.lighter(135));
        fill.setColorAt(0.48, fillColor);
        fill.setColorAt(1.0, fillColor.darker(125));
        painter.setPen(Qt::NoPen);
        painter.setBrush(fill);
        painter.drawRoundedRect(fillRect,
                                fillRect.height() / 2.0,
                                fillRect.height() / 2.0);
        painter.setBrush(QColor(255, 255, 255, 75));
        painter.drawRoundedRect(QRectF(fillRect.x() + 2,
                                       fillRect.y() + 1,
                                       qMax(0.0, fillRect.width() - 4),
                                       qMax(1.0, fillRect.height() * 0.24)),
                                2, 2);
    }
    painter.restore();
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
    , m_imageCropX(0)
    , m_imageCropY(0)
    , m_imageCropW(0)
    , m_imageCropH(0)
    , m_imageOffsetX(0)
    , m_imageOffsetY(0)
    , m_artworkOverlayMode(false)
    , m_showGrid(true)
    , m_restrictPvpDeployment(false)
{
    setMapSize(m_mapRows, m_mapCols);

    // ----- 创建环形菜单按钮 -----
    m_btnUpgrade = new QPushButton("Upgrade", this);
    m_btnMove    = new QPushButton("Move", this);
    m_btnRetreat = new QPushButton("Recall", this);

    QString radialStyle =
        "QPushButton {"
        "  background: transparent;"
        "  border: 2px solid transparent; border-radius: 8px;"
        "}"
        "QPushButton:hover { background: rgba(255,245,180,0.18); border-color: rgba(255,235,130,0.85); }"
        "QPushButton:pressed { background: rgba(70,45,20,0.22); }"
        "QPushButton:disabled { background: rgba(35,30,24,0.34); border-color: transparent; }"
    ;
    m_btnUpgrade->setStyleSheet(radialStyle);
    m_btnMove->setStyleSheet(radialStyle);
    m_btnRetreat->setStyleSheet(radialStyle);
    m_btnUpgrade->setToolTip("Upgrade");
    m_btnMove->setToolTip("Move");
    m_btnRetreat->setToolTip("Recall");
    m_btnUpgrade->setCursor(Qt::PointingHandCursor);
    m_btnMove->setCursor(Qt::PointingHandCursor);
    m_btnRetreat->setCursor(Qt::PointingHandCursor);

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
        for (auto &effect : m_effects) {
            effect.life -= 0.05;
        }
        m_effects.erase(std::remove_if(m_effects.begin(), m_effects.end(),
                                       [](const BattleEffect &effect) {
                                           return effect.life <= 0.0;
                                       }),
                        m_effects.end());
        if (!m_effects.isEmpty()
            || !m_snapshot.units.empty()
            || m_mode == InteractionMode::RADIAL_MENU
            || m_mode == InteractionMode::MOVING
            || m_mode == InteractionMode::DEPLOYING) {
            update();
        }
    });
    animTimer->start(50);  // 20 FPS 动画
}

// ========== setMapSize() —— 设置地图大小 ==========
void BattleView::setMapSize(int rows, int cols)
{
    m_mapRows = rows;
    m_mapCols = cols;
    if (m_artworkOverlayMode) {
        update();
        return;
    }
    if (!m_backgroundImage.isNull()) {
        QSize imageSize = m_backgroundImage.size();
        if (m_imageCropW > 0 && m_imageCropH > 0) {
            imageSize = QSize(m_imageCropW, m_imageCropH);
        }
        this->setFixedSize(imageSize.scaled(MaxBattleImageWidth,
                                            MaxBattleImageHeight,
                                            Qt::KeepAspectRatio));
    } else {
        this->setFixedSize(cols * CELL_SIZE, rows * CELL_SIZE);
    }
}

void BattleView::setPvpDeploymentSide(bool enabled, bool isHost)
{
    m_restrictPvpDeployment = enabled;
    m_localIsHost = isHost;
    update();
}

void BattleView::setArtworkOverlayMode(bool enabled)
{
    m_artworkOverlayMode = enabled;
    setAttribute(Qt::WA_TranslucentBackground, enabled);
    setAutoFillBackground(!enabled);
    if (enabled) {
        m_backgroundImage = QPixmap();
        setMinimumSize(0, 0);
        setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
        setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    } else {
        setMapSize(m_mapRows, m_mapCols);
    }
    update();
}

bool BattleView::setBackgroundImage(const QString& path)
{
    QPixmap image(path);
    if (image.isNull()) {
        m_backgroundImage = QPixmap();
        update();
        return false;
    }

    if (m_artworkOverlayMode) {
        m_backgroundImage = QPixmap();
        update();
        return true;
    }

    m_backgroundImage = image;
    if (m_mapRows > 0 && m_mapCols > 0) {
        QSize imageSize = m_backgroundImage.size();
        if (m_imageCropW > 0 && m_imageCropH > 0) {
            imageSize = QSize(m_imageCropW, m_imageCropH);
        }
        this->setFixedSize(imageSize.scaled(MaxBattleImageWidth,
                                            MaxBattleImageHeight,
                                            Qt::KeepAspectRatio));
    }
    update();
    return true;
}

void BattleView::clearBackgroundImage()
{
    m_backgroundImage = QPixmap();
    if (!m_artworkOverlayMode && m_mapRows > 0 && m_mapCols > 0) {
        this->setFixedSize(m_mapCols * CELL_SIZE, m_mapRows * CELL_SIZE);
    }
    update();
}

void BattleView::setImageCrop(int x, int y, int w, int h)
{
    m_imageCropX = x;
    m_imageCropY = y;
    m_imageCropW = w;
    m_imageCropH = h;
}

void BattleView::setImageOffset(int x, int y)
{
    m_imageOffsetX = x;
    m_imageOffsetY = y;
}

double BattleView::cellWidth() const
{
    if (m_mapCols <= 0) return CELL_SIZE;
    if (!m_backgroundImage.isNull() && m_imageCropW > 0 && m_imageCropH > 0) {
        const double imgScale = static_cast<double>(width()) / m_imageCropW;
        return (m_imageCropW * imgScale) / m_mapCols;
    }
    return static_cast<double>(width()) / m_mapCols;
}

double BattleView::cellHeight() const
{
    if (m_mapRows <= 0) return CELL_SIZE;
    if (!m_backgroundImage.isNull() && m_imageCropW > 0 && m_imageCropH > 0) {
        const double imgScale = static_cast<double>(height()) / m_imageCropH;
        return (m_imageCropH * imgScale) / m_mapRows;
    }
    return static_cast<double>(height()) / m_mapRows;
}

double BattleView::cellExtent() const
{
    return std::min(cellWidth(), cellHeight());
}

QRectF BattleView::cellRect(int row, int col) const
{
    const double cw = cellWidth();
    const double ch = cellHeight();
    const double ox = m_backgroundImage.isNull() ? 0.0 : m_imageOffsetX;
    const double oy = m_backgroundImage.isNull() ? 0.0 : m_imageOffsetY;
    return QRectF(ox + col * cw, oy + row * ch, cw, ch);
}

QPointF BattleView::cellCenter(int row, int col) const
{
    return cellRect(row, col).center();
}

int BattleView::rowAtPixel(int y) const
{
    const double ch = cellHeight();
    if (ch <= 0.0) return -1;
    const double oy = m_backgroundImage.isNull() ? 0.0 : m_imageOffsetY;
    return static_cast<int>(std::floor((y - oy) / ch));
}

int BattleView::colAtPixel(int x) const
{
    const double cw = cellWidth();
    if (cw <= 0.0) return -1;
    const double ox = m_backgroundImage.isNull() ? 0.0 : m_imageOffsetX;
    return static_cast<int>(std::floor((x - ox) / cw));
}

// ========== updateFromSnapshot() —— 从快照更新渲染数据 ==========
void BattleView::updateFromSnapshot(const game::core::BattleSnapshot &snapshot)
{
    auto oldUnitHp = [this](int id, int *row, int *col) {
        for (const auto &unit : m_snapshot.units) {
            if (unit.id == id) {
                if (row) *row = unit.row;
                if (col) *col = unit.col;
                return unit.hp;
            }
        }
        return -1;
    };
    auto oldMonsterHp = [this](int id) {
        for (const auto &monster : m_snapshot.monsters) {
            if (monster.id == id) {
                return monster.hp;
            }
        }
        return -1;
    };

    for (const auto &unit : snapshot.units) {
        int oldRow = unit.row;
        int oldCol = unit.col;
        const int previousHp = oldUnitHp(unit.id, &oldRow, &oldCol);
        if (previousHp < 0) {
            addEffect(EffectType::DeployDust, unit.row, unit.col, 0.62);
            AudioManager::instance().playDeploy();
        } else if (unit.hp < previousHp) {
            addEffect(EffectType::HitFlash, unit.row, unit.col, 0.30);
            AudioManager::instance().playHit();
        }
    }

    for (const auto &monster : snapshot.monsters) {
        const int previousHp = oldMonsterHp(monster.id);
        if (previousHp >= 0 && monster.hp < previousHp) {
            addEffect(EffectType::HitFlash, monster.row, monster.col, 0.30);
            AudioManager::instance().playHit();
        }
    }

    if (!m_snapshot.map.grids.empty() && snapshot.baseHealth < m_snapshot.baseHealth) {
        addEffect(EffectType::HitFlash, m_corePos.row, m_corePos.col, 0.42);
        AudioManager::instance().playHit();
    }

    for (const auto &projectile : snapshot.projectiles) {
        bool continuingProjectile = false;
        for (const auto &oldProjectile : m_snapshot.projectiles) {
            if (oldProjectile.sourceId == projectile.sourceId
                && oldProjectile.targetId == projectile.targetId
                && oldProjectile.kind == projectile.kind
                && oldProjectile.progress <= projectile.progress) {
                continuingProjectile = true;
                break;
            }
        }
        if (!continuingProjectile && projectile.progress < 0.22) {
            addEffect(EffectType::AttackFlash,
                      projectile.fromRow, projectile.fromCol, 0.20);
            AudioManager::instance().playAttack();
        }
    }

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
    if (m_showGrid) {
        painter.setPen(QPen(QColor(76, 58, 36, 42), 1));
        painter.setBrush(Qt::NoBrush);
        for (int row = 0; row < m_mapRows; ++row) {
            for (int col = 0; col < m_mapCols; ++col) {
                painter.drawRect(cellRect(row, col));
            }
        }
    }
    drawHoverCell(painter);
    if (!m_artworkOverlayMode) {
        drawSpawnMarker(painter);
        drawCoreMarker(painter);
    }
    drawHighlights(painter);
    drawUnits(painter);
    drawMonsters(painter);
    drawProjectiles(painter);
    drawEffects(painter);

    if (m_mode == InteractionMode::RADIAL_MENU && m_selectedUnitId >= 0) {
        static const QPixmap commandRing(":/images/new_art/command_ring.png");
        for (const auto &unit : m_snapshot.units) {
            if (unit.id != m_selectedUnitId) continue;
            const QRectF ringRect = commandRingRect(cellCenter(unit.row, unit.col),
                                                    cellExtent(), size());
            painter.drawPixmap(ringRect.toRect(), commandRing, commandRing.rect());
            break;
        }
        m_btnUpgrade->raise();
        m_btnMove->raise();
        m_btnRetreat->raise();
    }
}

void BattleView::setShowGrid(bool show)
{
    m_showGrid = show;
    update();
}

// ========== drawTerrain() —— 绘制地形（渐变+纹理感） ==========
void BattleView::drawTerrain(QPainter &painter)
{
    const bool hasImage = !m_backgroundImage.isNull();

    if (m_artworkOverlayMode) {
        return;
    }

    // 绘制背景图片（支持裁剪）
    if (hasImage) {
        if (m_imageCropW > 0 && m_imageCropH > 0) {
            QRect src(m_imageCropX, m_imageCropY, m_imageCropW, m_imageCropH);
            painter.drawPixmap(rect(), m_backgroundImage, src);
        } else {
            painter.drawPixmap(rect(), m_backgroundImage);
        }
    }

    for (const auto &grid : m_snapshot.map.grids) {
        QRectF cellRect = this->cellRect(grid.row, grid.col);

        // 有背景图片时只绘制网格线，跳过不透明的地形填充
        if (hasImage) {
            painter.setPen(QPen(QColor(0, 0, 0, 25), 1));
            painter.setBrush(Qt::NoBrush);
            painter.drawRect(cellRect);
            continue;
        }

        // 无背景图片时绘制地形色块
        switch (grid.terrain) {
        case game::core::TerrainType::Path: {
            QLinearGradient grad(cellRect.topLeft(), cellRect.bottomRight());
            grad.setColorAt(0, QColor(155, 135, 115));
            grad.setColorAt(1, QColor(125, 105, 85));
            painter.fillRect(cellRect, grad);
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
        const QPixmap* sprite = unitArtwork(unit);
        if (sprite && !sprite->isNull()) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(30, 20, 12, 82));
            const qreal phase = (m_animFrame + unit.id * 11) * 0.16;
            const qreal bob = qSin(phase) * cellExtent() * 0.035;
            const qreal scale = 1.0 + qSin(phase + 0.8) * 0.018;
            const QRectF shadowRect(unitRect.left() + unitRect.width() * (0.08 + (1.0 - scale) * 0.2),
                                    unitRect.bottom() - unitRect.height() * 0.18,
                                    unitRect.width() * 0.84 * scale,
                                    unitRect.height() * 0.24);
            painter.drawEllipse(shadowRect);
            const qreal spriteW = unitRect.width() * 1.34 * scale;
            const qreal spriteH = unitRect.height() * 1.40 * scale;
            const QRectF spriteRect(unitRect.center().x() - spriteW * 0.5,
                                    unitRect.bottom() - unitRect.height() * 1.38 + bob,
                                    spriteW,
                                    spriteH);
            painter.save();
            if (isOpponent) painter.setOpacity(0.76);
            drawCachedArtwork(painter, spriteRect, *sprite);
            painter.restore();
        } else {
            QLinearGradient unitGrad(innerRect.topLeft(), innerRect.bottomRight());
            unitGrad.setColorAt(0, unitColor.lighter(120));
            unitGrad.setColorAt(1, unitColor.darker(110));
            painter.setPen(QPen(borderColor, 2));
            painter.setBrush(unitGrad);
            painter.drawRoundedRect(innerRect, 6, 6);
        }

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
        if (!sprite) {
            QFont typeFont("Microsoft YaHei", 10, QFont::Bold);
            painter.setFont(typeFont);
            painter.drawText(innerRect, Qt::AlignCenter, label);
        }

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
    static const QPixmap tomatoA(":/images/characters/tomato_gunner_cutout.png");
    static const QPixmap tomatoB(":/images/characters/tomato_variant_01_cutout.png");
    static const QPixmap tomatoC(":/images/characters/tomato_variant_02_cutout.png");
    const QPixmap* monsterSprites[] = { &tomatoA, &tomatoB, &tomatoC };

    for (const auto &monster : m_snapshot.monsters) {
        QRectF mRect = cellRect(monster.row, monster.col);
        QRectF innerRect = mRect.adjusted(6, 6, -6, -6);
        const qreal bob = qSin((m_animFrame + monster.id * 13) * 0.18) * 2.0;

        // 底部阴影
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0, 0, 0, 50));
        painter.drawRoundedRect(innerRect.adjusted(2, 2, 2, 2), 4, 4);

        // 怪物方块（红色渐变）
        const QPixmap& sprite = *monsterSprites[std::abs(monster.id) % 3];
        if (!sprite.isNull()) {
            painter.setBrush(QColor(42, 24, 18, 85));
            painter.drawEllipse(QRectF(innerRect.left() + innerRect.width() * 0.12,
                                       innerRect.bottom() - innerRect.height() * 0.18,
                                       innerRect.width() * 0.78,
                                       innerRect.height() * 0.26));
            QRectF spriteRect = innerRect.adjusted(-innerRect.width() * 0.35,
                                                   -innerRect.height() * 0.62 + bob,
                                                   innerRect.width() * 0.35,
                                                   innerRect.height() * 0.08 + bob);
            painter.drawPixmap(spriteRect.toRect(), sprite, sprite.rect());
        } else {
            QLinearGradient monsterGrad(innerRect.topLeft(), innerRect.bottomRight());
            monsterGrad.setColorAt(0, QColor(240, 70, 70));
            monsterGrad.setColorAt(1, QColor(180, 30, 30));
            painter.setPen(QPen(QColor(255, 100, 100, 120), 1));
            painter.setBrush(monsterGrad);
            painter.drawRoundedRect(innerRect, 4, 4);
        }

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

// ========== drawProjectiles() —— 绘制投射物特效 ==========
void BattleView::drawProjectiles(QPainter &painter)
{
    for (const auto &projectile : m_snapshot.projectiles) {
        const double progress = std::clamp(projectile.progress, 0.0, 1.0);
        const QPointF start = cellCenter(projectile.fromRow, projectile.fromCol);
        const QPointF end = cellCenter(projectile.toRow, projectile.toCol);
        const QPointF delta = end - start;
        const double distance = std::hypot(delta.x(), delta.y());
        const QPointF normal = distance > 0.001
                                   ? QPointF(-delta.y() / distance, delta.x() / distance)
                                   : QPointF(0.0, -1.0);
        const double arc = qSin(progress * M_PI) * cellExtent() * 0.16;
        const QPointF current = start + delta * progress + normal * arc;

        QColor color(255, 220, 80);
        QColor coreColor(255, 252, 210);
        qreal radius = cellExtent() * 0.085;
        qreal trailWidth = 3.0;
        if (projectile.kind == game::core::ProjectileKind::Sniper) {
            color = QColor(80, 190, 255);
            coreColor = QColor(220, 248, 255);
            radius = cellExtent() * 0.065;
            trailWidth = 2.2;
        } else if (projectile.kind == game::core::ProjectileKind::Aoe) {
            color = QColor(255, 110, 40);
            coreColor = QColor(255, 239, 184);
            radius = cellExtent() * 0.12;
            trailWidth = 4.4;
        } else if (projectile.kind == game::core::ProjectileKind::Monster) {
            color = QColor(210, 70, 255);
            coreColor = QColor(249, 212, 255);
            radius = cellExtent() * 0.09;
            trailWidth = 3.2;
        }

        QPainterPath path;
        path.moveTo(start);
        path.quadTo((start + end) * 0.5 + normal * cellExtent() * 0.18, current);

        painter.save();
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setCompositionMode(QPainter::CompositionMode_Screen);

        QPen wideTrail(QColor(color.red(), color.green(), color.blue(), 42), trailWidth * 3.4,
                       Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        painter.setPen(wideTrail);
        painter.drawPath(path);

        QPen hotTrail(QColor(color.red(), color.green(), color.blue(), 150), trailWidth,
                      Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        painter.setPen(hotTrail);
        painter.drawPath(path);

        QRadialGradient glow(current, radius * 4.2);
        glow.setColorAt(0.0, QColor(coreColor.red(), coreColor.green(), coreColor.blue(), 235));
        glow.setColorAt(0.35, QColor(color.red(), color.green(), color.blue(), 185));
        glow.setColorAt(1.0, QColor(color.red(), color.green(), color.blue(), 0));
        painter.setPen(Qt::NoPen);
        painter.setBrush(glow);
        painter.drawEllipse(current, radius * 4.2, radius * 4.2);

        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
        painter.setBrush(color);
        painter.drawEllipse(current, radius, radius);
        painter.setBrush(coreColor);
        painter.drawEllipse(current, radius * 0.42, radius * 0.42);

        if (projectile.kind == game::core::ProjectileKind::Aoe && projectile.splashRadius > 0) {
            const double splashPixels = projectile.splashRadius * cellExtent();
            const qreal pulse = 0.55 + 0.25 * qSin(m_animFrame * 0.25);
            QColor splashColor(color.red(), color.green(), color.blue(), qRound(28 + pulse * 22));
            painter.setPen(QPen(QColor(color.red(), color.green(), color.blue(), qRound(95 + pulse * 60)),
                                1.5, Qt::DashLine));
            painter.setBrush(splashColor);
            painter.drawEllipse(end, splashPixels, splashPixels);
            painter.setPen(QPen(QColor(255, 238, 180, qRound(80 + pulse * 75)), 1.2));
            painter.drawEllipse(end, splashPixels * (0.68 + pulse * 0.08),
                                splashPixels * (0.68 + pulse * 0.08));
        }
        painter.restore();
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
                if (m_restrictPvpDeployment &&
                    !game::ui::isPvpDeploymentCellForHost(m_localIsHost, {row, col})) {
                    break;
                }
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

    const QRectF ring = commandRingRect(QPointF(pixelX, pixelY), cellExtent(), size());
    m_btnUpgrade->setGeometry(commandButtonRect(ring, QRectF(0.02, 0.02, 0.52, 0.31)));
    m_btnMove->setGeometry(commandButtonRect(ring, QRectF(0.42, 0.20, 0.50, 0.38)));
    m_btnRetreat->setGeometry(commandButtonRect(ring, QRectF(0.65, 0.57, 0.34, 0.42)));

    // 升级按钮状态
    m_btnUpgrade->setEnabled(level < game::core::constants::MaxCardLevel);
    m_btnUpgrade->setText(QString());
    m_btnMove->setText(QString());
    m_btnRetreat->setText(QString());

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

void BattleView::clearEffects()
{
    m_effects.clear();
    m_snapshot = game::core::BattleSnapshot();
}

void BattleView::addEffect(EffectType type, int row, int col, qreal duration)
{
    for (BattleEffect &effect : m_effects) {
        if (effect.type == type && effect.row == row && effect.col == col) {
            effect.life = duration;
            effect.duration = duration;
            return;
        }
    }
    constexpr int MaxEffects = 72;
    if (m_effects.size() >= MaxEffects) {
        m_effects.remove(0, m_effects.size() - MaxEffects + 1);
    }
    m_effects.append({type, row, col, duration, duration});
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
            if (m_restrictPvpDeployment &&
                !game::ui::isPvpDeploymentCellForHost(m_localIsHost, {grid.row, grid.col})) {
                continue;
            }
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
    , m_phaseLabel(nullptr)
    , m_btnPause(nullptr)
    , m_btnSpeed(nullptr)
    , m_btnSkill(nullptr)
    , m_btnExit(nullptr)
    , m_pauseOverlay(nullptr)
    , m_btnGuide(nullptr)
    , m_tutorialOverlay(nullptr)
    , m_isPaused(false)
    , m_speedMultiplier(1.0)
    , m_currentWaveId(0)
    , m_pveFinalWave(10)
    , m_waveTimer(0.0)
    , m_battleManager(nullptr)
    , m_isPvp(false)
    , m_isHost(false)
    , m_inBattlePhase(false)
    , m_waveStarted(false)
    , m_localWaveClear(false)
    , m_peerWaveClear(false)
    , m_stateSyncTimer(0.0)
    , m_displayCoreHealth(game::core::constants::InitialBaseHealth)
    , m_displayOpponentCoreHealth(game::core::constants::InitialBaseHealth)
    , m_opponentLabel(nullptr)
    , m_pvpArtwork(":/images/artwork/battle_pvp.png")
    , m_pvpOfficeMapArtwork(":/images/artwork/battle_pvp_office_map.png")
    , m_pveArtwork(":/images/artwork/battle_pve.png")
    , m_pveUiOverlay(":/images/artwork/battle_pve_ui_overlay_v2.png")
    , m_labMap01(":/images/maps/lab_map_01.png")
    , m_labMap02(":/images/maps/lab_map_02.png")
    , m_deckArtwork(":/images/artwork/deck_atlas.png")
{
    m_deck = {
        game::core::CardKind::Sniper,
        game::core::CardKind::Produce,
        game::core::CardKind::Heal,
        game::core::CardKind::HeavyMedic,
        game::core::CardKind::Aoe
    };
    initUI();
    connectSignals();
}

// ========== initUI() —— 初始化界面 ==========
void BattlePage::initUI()
{
    setFocusPolicy(Qt::StrongFocus);
    m_battleView = new BattleView(this);
    m_battleView->setArtworkOverlayMode(true);

    const QString labelStyle =
        "QLabel { color: #3B2819; background: #F2DCA9;"
        " border-radius: 4px;"
        " font-weight: 700; padding: 2px 6px; }";
    m_waveLabel = new QLabel("Wave 0", this);
    m_phaseLabel = new QLabel("Battle Phase", this);
    m_coreHpLabel = new QLabel("Your Core 10/10", this);
    m_opponentLabel = new QLabel("Enemy Core 10/10", this);
    m_resourceLabel = new QLabel("Resource 0", this);
    for (QLabel *label : {m_waveLabel, m_phaseLabel, m_coreHpLabel,
                          m_opponentLabel, m_resourceLabel}) {
        label->setAlignment(Qt::AlignCenter);
        label->setStyleSheet(labelStyle);
    }

    for (int i = 0; i < 5; ++i) {
        auto *cardBtn = new QPushButton(this);
        cardBtn->setText(QString());
        cardBtn->setCursor(Qt::PointingHandCursor);
        cardBtn->setStyleSheet(
            "QPushButton { background: transparent; border: 3px solid transparent; border-radius: 6px; }"
            "QPushButton:hover { background: rgba(255,245,194,42); border-color: rgba(255,236,139,210); }"
            "QPushButton[selected=\"true\"] { background: rgba(255,232,126,48);"
            " border: 4px solid #F8D77A; }"
            "QPushButton:pressed { background: rgba(111,78,39,70); }"
            "QPushButton:disabled { background: rgba(44,35,28,32); border-color: transparent; }");
        connect(cardBtn, &QPushButton::clicked, this, [this, i]() {
            if (i >= m_deck.size()) return;
            m_selectedCardIndex = i;
            updateCardVisualState(m_displayResources);
            m_battleView->m_mode = BattleView::InteractionMode::DEPLOYING;
            m_battleView->m_selectedCardKind = m_deck[i];
            m_battleView->hideRadialMenu();
            m_battleView->update();
            if (m_tutorialStage == TutorialStage::WaitCard) {
                m_tutorialStage = TutorialStage::DeployPrompt;
                updateTutorialTargets();
                m_tutorialOverlay->showStep(
                    "Captain Pine",
                    "Good choice. The glowing cells are valid ground. Place the defender where it can watch the winding path.",
                    TutorialOverlay::Focus::Battlefield,
                    "Deploy now");
            }
        });
        m_cardButtons.append(cardBtn);

        auto *nameLabel = new QLabel(this);
        nameLabel->setAlignment(Qt::AlignCenter);
        nameLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
        nameLabel->setStyleSheet(
            "QLabel { color: #332216; background: #F1DCA9;"
            " border: 1px solid rgba(104,72,39,120); border-radius: 3px;"
            " font-size: 12px; font-weight: 700; padding: 1px 3px; }");
        m_cardNameLabels.append(nameLabel);

        auto *costLabel = new QLabel(this);
        costLabel->setAlignment(Qt::AlignCenter);
        costLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
        costLabel->setStyleSheet(
            "QLabel { color: #342113; background: #F1DCA9;"
            " border: 2px solid #74502C; border-radius: 10px;"
            " font-size: 15px; font-weight: 800; padding: 1px 5px; }");
        m_cardCostLabels.append(costLabel);
    }

    const QString roundButtonStyle =
        "QPushButton { background: rgba(246,226,176,215); color: #3B2819;"
        " border: 2px solid rgba(89,61,36,190); border-radius: 8px;"
        " font-weight: 700; }"
        "QPushButton:hover { background: rgba(255,239,191,240); border-color: #F8D77A; }"
        "QPushButton:pressed { background: rgba(197,159,94,230); }";

    m_btnPause = new QPushButton(this);
    m_btnPause->setToolTip("暂停 / 设置");
    m_btnPause->setCursor(Qt::PointingHandCursor);
    m_btnPause->setStyleSheet(
        "QPushButton { background: transparent; border: 2px solid transparent; border-radius: 10px; }"
        "QPushButton:hover { background: rgba(255,245,194,38); border-color: rgba(255,236,139,190); }"
        "QPushButton:pressed { background: rgba(111,78,39,65); }");

    m_btnSpeed = new QPushButton("1x", this);
    m_btnSpeed->setToolTip("战斗速度");
    m_btnSpeed->setCursor(Qt::PointingHandCursor);
    m_btnSpeed->setStyleSheet(roundButtonStyle);

    m_btnSkill = new QPushButton("自动技能", this);
    m_btnSkill->setEnabled(false);
    m_btnSkill->setStyleSheet(roundButtonStyle);

    m_btnExit = new QPushButton("Menu", this);
    m_btnExit->setToolTip("打开暂停菜单");
    m_btnExit->setCursor(Qt::PointingHandCursor);
    m_btnExit->setStyleSheet(roundButtonStyle);

    m_btnGuide = new QPushButton("?", this);
    m_btnGuide->setToolTip("Replay story guide");
    m_btnGuide->setCursor(Qt::PointingHandCursor);
    m_btnGuide->setStyleSheet(
        "QPushButton { color:#5b391f; background:rgba(246,226,176,235);"
        " border:2px solid rgba(89,61,36,205); border-radius:22px;"
        " font-size:24px; font-weight:900; }"
        "QPushButton:hover { background:rgba(255,239,191,250); border-color:#F8D77A; }"
        "QPushButton:pressed { background:rgba(197,159,94,235); }");

    setStyleSheet("BattlePage { background-color: #203B35; }");

    // 游戏主循环定时器（约 60FPS）
    m_gameTimer = new QTimer(this);
    connect(m_gameTimer, &QTimer::timeout, this, &BattlePage::onGameTick);

    m_pauseOverlay = new PauseOverlay(this);
    m_pauseOverlay->setGeometry(rect());
    m_pauseOverlay->raise();
    m_tutorialOverlay = new TutorialOverlay(this);
    m_tutorialOverlay->setGeometry(rect());
    connect(m_btnGuide, &QPushButton::clicked,
            this, [this]() { beginTutorial(true); });
    connect(m_tutorialOverlay, &TutorialOverlay::signalAction,
            this, &BattlePage::advanceTutorial);
    connect(m_tutorialOverlay, &TutorialOverlay::signalSkip,
            this, [this]() { finishTutorial(true); });
    refreshCardDisplay();
    layoutArtworkUi();
}

void BattlePage::setDeck(const QVector<game::core::CardKind>& deck)
{
    m_deck = deck.mid(0, 5);
    m_selectedCardIndex = -1;
    refreshCardDisplay();
    update();
}

void BattlePage::refreshCardDisplay()
{
    for (int i = 0; i < m_cardButtons.size(); ++i) {
        const bool hasCard = i < m_deck.size();
        m_cardButtons[i]->setVisible(hasCard);
        m_cardNameLabels[i]->setVisible(hasCard);
        m_cardCostLabels[i]->setVisible(hasCard);
        if (!hasCard) continue;

        const auto kind = m_deck[i];
        const int cost = game::core::CardSystem::deployCost(kind);
        const QString name = cardDisplayName(kind);
        m_cardButtons[i]->setToolTip(QString("%1 - Cost %2").arg(name).arg(cost));
        m_cardNameLabels[i]->setText(name);
        m_cardCostLabels[i]->setText(QString("Juice %1").arg(cost));
    }
}

void BattlePage::updateCardVisualState(int resources)
{
    const QRect canvas = artworkRect();
    const qreal uiScale = std::min(canvas.width() / 1672.0,
                                   canvas.height() / 941.0);
    const int costPx = qMax(10, qRound(15 * uiScale));

    for (int i = 0; i < m_cardButtons.size(); ++i) {
        if (i >= m_deck.size()) continue;

        const int cost = game::core::CardSystem::deployCost(m_deck[i]);
        const bool affordable = !m_isPvp && resources >= cost;
        const bool selected = affordable && i == m_selectedCardIndex;

        m_cardButtons[i]->setEnabled(affordable);
        m_cardButtons[i]->setProperty("selected", selected);
        m_cardButtons[i]->style()->unpolish(m_cardButtons[i]);
        m_cardButtons[i]->style()->polish(m_cardButtons[i]);

        const QColor textColor = affordable ? QColor("#342113") : QColor("#8B3125");
        const QColor background = affordable ? QColor(246, 226, 176, 245)
                                             : QColor(239, 201, 172, 245);
        const QColor border = affordable ? QColor("#74502C") : QColor("#A84D3B");
        m_cardCostLabels[i]->setStyleSheet(QString(
            "QLabel { color: %1; background: rgba(%2,%3,%4,%5);"
            " border: %6px solid %7; border-radius: %8px;"
            " font-size: %9px; font-weight: 800; padding: 0px 2px; }")
            .arg(textColor.name())
            .arg(background.red()).arg(background.green())
            .arg(background.blue()).arg(background.alpha())
            .arg(qMax(1, qRound(2 * uiScale)))
            .arg(border.name())
            .arg(qMax(5, qRound(10 * uiScale)))
            .arg(costPx));
        m_cardCostLabels[i]->setToolTip(
            affordable
                ? QString("Ready - costs %1 Juice").arg(cost)
                : QString("Need %1 more Juice").arg(qMax(0, cost - resources)));
    }
}

void BattlePage::beginTutorial(bool replay)
{
    Q_UNUSED(replay);
    if (m_isPvp || !m_inBattlePhase || !m_tutorialOverlay) {
        return;
    }

    m_tutorialSessionActive = true;
    m_tutorialPaused = true;
    m_coreWarningShown = false;
    m_tutorialStage = TutorialStage::Intro;
    m_selectedCardIndex = -1;
    if (m_battleView) {
        m_battleView->m_mode = BattleView::InteractionMode::NONE;
        m_battleView->hideRadialMenu();
    }
    updateCardVisualState(m_displayResources);
    updateTutorialTargets();
    m_tutorialOverlay->showStep(
        "Captain Pine",
        "The tide has carried raiders to our last safe shore. This small core is home to every fruit still standing.",
        TutorialOverlay::Focus::None,
        "I am ready");
}

void BattlePage::advanceTutorial()
{
    switch (m_tutorialStage) {
    case TutorialStage::Intro:
        m_tutorialStage = TutorialStage::Resources;
        m_tutorialOverlay->showStep(
            "Captain Pine",
            "Juice powers every deployment. Spend it carefully: stronger allies cost more, and empty reserves leave the path exposed.",
            TutorialOverlay::Focus::Resource,
            "Show the squad");
        break;
    case TutorialStage::Resources:
        m_tutorialStage = TutorialStage::CardPrompt;
        m_tutorialOverlay->showStep(
            "Captain Pine",
            "Each card has a role. Miners build the economy, defenders hold the line, and ranged allies cover the bends.",
            TutorialOverlay::Focus::Cards,
            "Choose a card");
        break;
    case TutorialStage::CardPrompt:
        m_tutorialStage = TutorialStage::WaitCard;
        m_tutorialOverlay->closeOverlay();
        break;
    case TutorialStage::DeployPrompt:
        m_tutorialStage = TutorialStage::WaitDeploy;
        m_tutorialOverlay->closeOverlay();
        break;
    case TutorialStage::Tactics:
        m_tutorialStage = TutorialStage::Final;
        m_tutorialOverlay->showStep(
            "Captain Pine",
            "The core is our final line. Field Manual unlocked: use the ? button whenever you want to replay these tactical lessons.",
            TutorialOverlay::Focus::Core,
            "Begin the defense");
        break;
    case TutorialStage::Final:
        finishTutorial(false);
        break;
    case TutorialStage::CoreWarning:
        m_tutorialOverlay->closeOverlay();
        m_tutorialPaused = false;
        m_tutorialStage = TutorialStage::Inactive;
        break;
    case TutorialStage::WaitCard:
    case TutorialStage::WaitDeploy:
    case TutorialStage::Inactive:
        break;
    }
}

void BattlePage::finishTutorial(bool skipped)
{
    if (!m_tutorialOverlay) return;

    m_tutorialOverlay->closeOverlay();
    m_tutorialPaused = false;
    m_tutorialStage = TutorialStage::Inactive;
    m_selectedCardIndex = -1;
    updateCardVisualState(m_displayResources);

    QSettings settings;
    settings.setValue("tutorial/storyGuideV1Complete", true);
    settings.setValue("tutorial/fieldManualUnlocked", true);
    if (!skipped) {
        settings.setValue("tutorial/lastCompletedMap", m_activePveMapId);
    }
}

void BattlePage::updateTutorialTargets()
{
    if (!m_tutorialOverlay) return;

    QRect cards;
    for (QPushButton *button : m_cardButtons) {
        if (button->isVisible()) {
            cards = cards.isNull() ? button->geometry() : cards.united(button->geometry());
        }
    }
    m_tutorialOverlay->setTargets(
        m_resourceLabel ? m_resourceLabel->geometry() : QRect(),
        cards,
        m_battleView ? m_battleView->geometry() : QRect(),
        m_coreHpLabel ? m_coreHpLabel->geometry() : QRect());
}

QRect BattlePage::artworkRect() const
{
    constexpr int DesignWidth = 1672;
    constexpr int DesignHeight = 941;
    QSize fitted(DesignWidth, DesignHeight);
    fitted.scale(size(), Qt::KeepAspectRatio);
    return QRect((width() - fitted.width()) / 2,
                 (height() - fitted.height()) / 2,
                 fitted.width(), fitted.height());
}

void BattlePage::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), QColor(26, 55, 49));

    const QRect canvas = artworkRect();
    auto mapped = [&](const QRectF& designRect) {
        const qreal sx = canvas.width() / 1672.0;
        const qreal sy = canvas.height() / 941.0;
        return QRectF(canvas.x() + designRect.x() * sx,
                      canvas.y() + designRect.y() * sy,
                      designRect.width() * sx,
                      designRect.height() * sy);
    };

    if (m_isPvp && !m_pvpArtwork.isNull()) {
        if (m_netCtx.pvpMapId == "pvp_office_panic" && !m_pvpOfficeMapArtwork.isNull()) {
            painter.drawPixmap(mapped(QRectF(0, 96, 1672, 604)),
                               m_pvpOfficeMapArtwork,
                               QRectF(0, 0, 1672, 604));
        } else {
            painter.drawPixmap(mapped(QRectF(0, 96, 1672, 604)),
                               m_pvpArtwork, QRectF(0, 96, 1672, 604));
        }
    } else if (m_activePveMapId == "island_pve" && !m_pveArtwork.isNull()) {
        painter.drawPixmap(canvas, m_pveArtwork);
    } else {
        const QPixmap& mapArtwork = m_activePveMapId == "lab_map_02"
                                        ? m_labMap02
                                        : m_labMap01;
        if (!mapArtwork.isNull()) {
            painter.drawPixmap(canvas, mapArtwork);
        }
    }

    if (m_isPvp && !m_pvpArtwork.isNull()) {
        painter.drawPixmap(mapped(QRectF(0, 0, 1672, 126)),
                           m_pvpArtwork, QRectF(0, 0, 1672, 126));
        painter.drawPixmap(mapped(QRectF(0, 690, 1672, 251)),
                           m_pvpArtwork, QRectF(0, 690, 1672, 251));
    } else if (!m_pveUiOverlay.isNull()) {
        painter.drawPixmap(canvas, m_pveUiOverlay);
    }

    if (m_isPvp) {
        drawHealthBar(painter, mapped(QRectF(615, 72, 194, 12)),
                      m_displayCoreHealth,
                      game::core::constants::InitialBaseHealth,
                      QColor(62, 157, 203));
        drawHealthBar(painter, mapped(QRectF(918, 72, 194, 12)),
                      m_displayOpponentCoreHealth,
                      game::core::constants::InitialBaseHealth,
                      QColor(205, 83, 61));
    } else {
        drawHealthBar(painter, mapped(QRectF(797, 90, 232, 12)),
                      m_displayCoreHealth,
                      game::core::constants::InitialBaseHealth,
                      QColor(85, 157, 65));
    }

    const QRectF pvpCards[] = {
        {355, 704, 180, 216}, {550, 704, 180, 216},
        {745, 704, 180, 216}, {940, 704, 180, 216},
        {1135, 704, 180, 216}
    };
    const QRectF pveCards[] = {
        {330, 707, 188, 216}, {519, 707, 195, 216},
        {715, 707, 197, 216}, {913, 707, 198, 216},
        {1112, 707, 196, 216}
    };
    const QRectF* cardDestinations = m_isPvp ? pvpCards : pveCards;
    for (int i = 0; i < 5; ++i) {
        if (i < m_deck.size()) {
            painter.drawPixmap(mapped(cardDestinations[i]),
                               m_deckArtwork, cardSourceRect(m_deck[i]));
        }
    }
}

void BattlePage::layoutArtworkUi()
{
    if (!m_battleView) return;

    const QRect canvas = artworkRect();
    const qreal sx = canvas.width() / 1672.0;
    const qreal sy = canvas.height() / 941.0;
    auto mapped = [&](const QRectF &designRect) {
        return QRect(qRound(canvas.x() + designRect.x() * sx),
                     qRound(canvas.y() + designRect.y() * sy),
                     qMax(1, qRound(designRect.width() * sx)),
                     qMax(1, qRound(designRect.height() * sy)));
    };

    const int labelPx = qMax(11, qRound(20 * std::min(sx, sy)));
    const QString labelStyle = QString(
        "QLabel { color: #3B2819; background: #F2DCA9;"
        " border-radius: %1px;"
        " font-size: %2px; font-weight: 700; padding: 1px 4px; }")
        .arg(qMax(3, qRound(5 * sx)))
        .arg(labelPx);
    for (QLabel *label : {m_waveLabel, m_phaseLabel, m_coreHpLabel,
                          m_opponentLabel, m_resourceLabel}) {
        label->setStyleSheet(labelStyle);
    }

    const qreal uiScale = std::min(sx, sy);
    const int namePx = qMax(9, qRound(13 * uiScale));
    const int costPx = qMax(10, qRound(15 * uiScale));
    const QString cardNameStyle = QString(
        "QLabel { color: #332216; background: rgba(241,220,169,235);"
        " border: %1px solid rgba(104,72,39,120); border-radius: %2px;"
        " font-size: %3px; font-weight: 700; padding: 0px 2px; }")
        .arg(qMax(1, qRound(1 * uiScale)))
        .arg(qMax(2, qRound(3 * uiScale)))
        .arg(namePx);
    const QString cardCostStyle = QString(
        "QLabel { color: #342113; background: rgba(246,226,176,245);"
        " border: %1px solid #74502C; border-radius: %2px;"
        " font-size: %3px; font-weight: 850; padding: 0px 2px; }")
        .arg(qMax(1, qRound(2 * uiScale)))
        .arg(qMax(5, qRound(10 * uiScale)))
        .arg(costPx);
    for (QLabel *label : m_cardNameLabels) {
        label->setStyleSheet(cardNameStyle);
    }
    for (QLabel *label : m_cardCostLabels) {
        label->setStyleSheet(cardCostStyle);
    }

    if (m_isPvp) {
        m_battleView->setGeometry(mapped(QRectF(174, 126, 1324, 552)));
        m_waveLabel->setGeometry(mapped(QRectF(135, 35, 126, 50)));
        m_phaseLabel->setGeometry(mapped(QRectF(355, 35, 166, 50)));
        m_coreHpLabel->setGeometry(mapped(QRectF(615, 29, 210, 50)));
        m_opponentLabel->setGeometry(mapped(QRectF(918, 29, 212, 50)));
        m_resourceLabel->setGeometry(mapped(QRectF(1210, 35, 137, 50)));
        m_phaseLabel->show();
        m_opponentLabel->show();
    } else {
        if (m_activePveMapId == "island_pve") {
            m_battleView->setGeometry(mapped(QRectF(238, 164, 1328, 520)));
        } else {
            m_battleView->setGeometry(canvas);
        }
        m_waveLabel->setGeometry(mapped(QRectF(485, 40, 185, 52)));
        m_coreHpLabel->setGeometry(mapped(QRectF(798, 40, 248, 52)));
        m_resourceLabel->setGeometry(mapped(QRectF(1188, 40, 185, 52)));
        m_phaseLabel->hide();
        m_opponentLabel->hide();
    }

    const QRectF pvpCards[] = {
        {355, 704, 180, 216}, {550, 704, 180, 216},
        {745, 704, 180, 216}, {940, 704, 180, 216},
        {1135, 704, 180, 216}
    };
    const QRectF pveCards[] = {
        {330, 707, 188, 216}, {519, 707, 195, 216},
        {715, 707, 197, 216}, {913, 707, 198, 216},
        {1112, 707, 196, 216}
    };
    const QRectF* cards = m_isPvp ? pvpCards : pveCards;
    for (int i = 0; i < m_cardButtons.size(); ++i) {
        const QRect cardRect = mapped(cards[i]);
        m_cardButtons[i]->setGeometry(cardRect);
        m_cardNameLabels[i]->setGeometry(mapped(QRectF(cards[i].x() + 15,
                                                       cards[i].y() + 10,
                                                       cards[i].width() - 30, 30)));
        m_cardCostLabels[i]->setGeometry(mapped(QRectF(cards[i].x() + 28,
                                                       cards[i].y() + 168,
                                                       cards[i].width() - 56, 34)));
    }
    m_btnSpeed->setGeometry(mapped(QRectF(1450, 795, 92, 56)));
    m_btnPause->setGeometry(mapped(QRectF(1538, 23, 98, 88)));
    m_btnGuide->setGeometry(mapped(QRectF(72, 795, 58, 58)));
    m_btnGuide->setVisible(!m_isPvp);
    m_btnExit->hide();
    m_btnSkill->hide();
    m_battleView->lower();
    const QVector<QWidget*> foregroundWidgets = {
        m_waveLabel, m_phaseLabel, m_coreHpLabel, m_opponentLabel,
        m_resourceLabel, m_btnPause, m_btnSpeed, m_btnGuide
    };
    for (QWidget *widget : foregroundWidgets) {
        widget->raise();
    }
    for (auto *button : m_cardButtons) button->raise();
    for (auto *label : m_cardNameLabels) label->raise();
    for (auto *label : m_cardCostLabels) label->raise();
    updateCardVisualState(m_displayResources);
    if (m_pauseOverlay) m_pauseOverlay->raise();
    if (m_tutorialOverlay) {
        m_tutorialOverlay->setGeometry(rect());
        updateTutorialTargets();
        if (m_tutorialOverlay->isVisible()) m_tutorialOverlay->raise();
    }
}

// ========== setNetworkContext() —— 设置网络上下文（PVP 模式） ==========
void BattlePage::setNetworkContext(const NetworkContext& ctx)
{
    m_netCtx = ctx;
    m_isPvp = ctx.isPvp;
    m_isHost = ctx.isHost;
    if (!m_isPvp) {
        m_activePveMapId = ctx.pveMapId.isEmpty() ? QString("island_pve") : ctx.pveMapId;
    }
    if (m_battleView) {
        m_battleView->setPvpDeploymentSide(m_isPvp, !m_isPvp || m_isHost);
    }
    layoutArtworkUi();
    update();
}

// ========== setupPveMap() —— 初始化 PVE 地图 ==========
void BattlePage::setupPveMap()
{
    auto& map = m_battleManager->map();
    m_battleView->clearBackgroundImage();
    m_activePveMapId = m_netCtx.pveMapId.isEmpty()
                           ? QString("island_pve")
                           : m_netCtx.pveMapId;

    if (m_activePveMapId == "island_pve") {
        map.resize(8, 18, game::core::TerrainType::FlatLand, 0);
        const game::core::MapPosition spawnPos(2, 17);
        const game::core::MapPosition corePos(4, 0);

        std::vector<game::core::MapPosition> pathCells = {
            spawnPos, {2,16}, {3,16}, {3,15}, {4,15}, {4,14},
            {4,13}, {4,12}, {4,11}, {4,10}, {4,9}, {4,8},
            {4,7}, {4,6}, {4,5}, {4,4}, {4,3}, {4,2},
            {4,1}, corePos
        };
        for (const auto& pos : pathCells) {
            if (pos != spawnPos && pos != corePos) {
                map.setGrid(pos, game::core::TerrainType::Path, 0);
            }
        }
        map.setGrid(spawnPos, game::core::TerrainType::SpawnPoint, 0);
        map.setGrid(corePos, game::core::TerrainType::CoreA, 0);

        const std::vector<game::core::MapPosition> highGroundCells = {
            {1,3}, {1,6}, {1,10}, {1,13},
            {6,4}, {6,8}, {6,12}, {6,15}
        };
        for (const auto& pos : highGroundCells) {
            map.setGrid(pos, game::core::TerrainType::HighGround, 1);
        }
        m_battleManager->setPath(pathCells);
        m_battleView->m_spawnPos = spawnPos;
        m_battleView->m_corePos = corePos;
    } else {
        const QString safeMapId = m_activePveMapId == "lab_map_02"
                                      ? QString("lab_map_02")
                                      : QString("lab_map_01");
        m_activePveMapId = safeMapId;
        const QString mapPath = findProjectFile(QString("assets/maps/%1.json").arg(safeMapId));
        game::core::LoadedMapConfig config;
        std::string error;
        if (mapPath.isEmpty()
            || !game::core::MapConfigLoader::loadFromJson(mapPath.toStdString(), config, &error)) {
            qWarning() << "[BattlePage] failed to load map config" << mapPath
                       << QString::fromStdString(error);
            m_activePveMapId = "island_pve";
            m_netCtx.pveMapId = m_activePveMapId;
            setupPveMap();
            return;
        }

        map.resize(config.rows, config.cols, game::core::TerrainType::NoDeploy, 0);
        for (const auto& tile : config.tiles) {
            map.setGrid({tile.row, tile.col},
                        terrainFromMapTile(tile.type),
                        terrainHeightFromMapTile(tile.type));
        }
        for (const auto& route : config.routesA) {
            for (const auto& pos : route) {
                const auto *grid = map.gridAt(pos);
                if (grid && grid->terrainType() == game::core::TerrainType::NoDeploy) {
                    map.setGrid(pos, game::core::TerrainType::Path, 0);
                }
            }
        }
        m_battleManager->setPaths(config.routesA);
        m_battleView->m_spawnPos = !config.spawnA.empty()
                                       ? config.spawnA.front()
                                       : config.routesA.front().front();
        m_battleView->m_corePos = !config.coreA.empty()
                                      ? config.coreA.front()
                                      : config.routesA.front().back();
    }

    m_battleView->setMapSize(map.rows(), map.cols());
    layoutArtworkUi();
    update();
}

// ========== setupPvpMap() —— 初始化 PVP 对称地图 ==========
void BattlePage::setupPvpMap()
{
    auto& map = m_battleManager->map();
    m_battleView->clearBackgroundImage();
    const auto layout = game::ui::makePvpMapLayout(m_netCtx.pvpMapId.toStdString());
    game::ui::applyPvpMapLayout(map, layout);

    m_battleManager->rebuildMapOccupancy();
    m_battleManager->setPaths({layout.pathToA, layout.pathToB});
    m_battleView->setMapSize(map.rows(), map.cols());
    m_battleView->m_spawnPos = layout.spawnA;
    m_battleView->m_corePos = m_isHost ? layout.coreA : layout.coreB;
    layoutArtworkUi();
    update();
}

// ========== sendDeployAction() —— 发送部署操作 ==========
void BattlePage::sendDeployAction(game::core::CardKind kind, game::core::MapPosition pos)
{
    // 本地执行
    bool deployed = false;
    if (m_battleManager) {
        if (m_isPvp && !game::ui::isPvpDeploymentCellForHost(m_isHost, pos)) {
            return;
        }
        deployed = static_cast<bool>(m_battleManager->deployCard(kind, pos));
    }
    if (deployed) {
        m_selectedCardIndex = -1;
        m_battleView->m_mode = BattleView::InteractionMode::NONE;
        updateStatusBar(m_battleManager->snapshot());
        if (m_tutorialStage == TutorialStage::WaitDeploy) {
            m_tutorialStage = TutorialStage::Tactics;
            m_tutorialPaused = true;
            updateTutorialTargets();
            m_tutorialOverlay->showStep(
                "Captain Pine",
                "Our first guardian is in position. Click any deployed ally to open its command ring: Upgrade strengthens it, Move repositions it, and Recall returns part of its cost.",
                TutorialOverlay::Focus::Battlefield,
                "Understood");
        }
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

    recordReplaySnapshot(snapshot, 0.10);
    m_battleView->updateFromSnapshot(snapshot);
    updateStatusBar(snapshot);

    if (snapshot.gameOver) {
        finishBattle(snapshot);
        return;
    }

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
    m_renderTick = 0;
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
        m_battleManager->setPveDifficulty(0);
        m_pveFinalWave = 10;

        // 应用同步的随机种子
        m_battleManager->setRandomSeed(m_netCtx.seed);

        // 初始化视野系统
        const auto pvpLayout = game::ui::makePvpMapLayout(m_netCtx.pvpMapId.toStdString());
        auto& vision = m_battleManager->visionManager();
        vision.initDefaultVision(pvpLayout.coreA, pvpLayout.coreB);

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
        m_battleManager->setPvpMode(false);
        m_battleManager->setPveDifficulty(m_netCtx.pveDifficulty);
        switch (std::clamp(m_netCtx.pveDifficulty, 0, 2)) {
        case 1: m_pveFinalWave = 13; break;
        case 2: m_pveFinalWave = 15; break;
        default: m_pveFinalWave = 10; break;
        }

        // 隐藏对手信息
        if (m_opponentLabel) {
            m_opponentLabel->setVisible(false);
        }
    }

    // 重置状态
    m_isPaused = false;
    if (!m_isPvp || m_battleManager->currentWave() <= 0 || m_replayData.frames.isEmpty()) {
        resetReplayRecorder();
    }

    m_btnPause->setText(QString());
    m_pauseOverlay->closeMenu();
    m_pauseOverlay->setPvpMode(m_isPvp);
    m_battleView->clearEffects();
    m_speedMultiplier = 1.0;
    m_waveTimer = 0.0;
    m_stateSyncTimer = 0.0;
    m_inBattlePhase = true;
    m_waveStarted = false;
    m_localWaveClear = false;
    m_peerWaveClear = false;
    m_resultEmitted = false;
    m_selectedCardIndex = -1;
    m_displayResources = -1;
    m_tutorialPaused = false;
    m_tutorialSessionActive = false;
    m_coreWarningShown = false;
    m_tutorialStage = TutorialStage::Inactive;
    if (m_tutorialOverlay) m_tutorialOverlay->closeOverlay();
    m_currentWaveId = (m_isPvp && m_battleManager->currentWave() > 0)
                          ? m_battleManager->currentWave() + 1
                          : 1;

    // PVP 战斗阶段是纯观看，部署/升级/移动/撤回全部放在迷雾部署阶段。
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
    recordReplaySnapshot(snap, 0.0, true);

    if (!m_isPvp) {
        QSettings settings;
        if (!settings.value("tutorial/storyGuideV1Complete", false).toBool()) {
            QTimer::singleShot(120, this, [this]() {
                if (m_inBattlePhase && !m_isPvp) beginTutorial(false);
            });
        }
    }
}

// ========== onGameTick() —— 游戏主循环回调 ==========
void BattlePage::setRevealPaused(bool paused)
{
    if (!m_gameTimer) return;
    if (paused) {
        m_gameTimer->stop();
    } else if (m_inBattlePhase && !m_isPaused && !m_tutorialPaused) {
        m_gameTimer->start(16);
    }
}

void BattlePage::setShowGrid(bool show)
{
    if (m_battleView) m_battleView->setShowGrid(show);
}

void BattlePage::pauseForFocusLoss()
{
    if (m_inBattlePhase && !m_isPvp && !m_isPaused) {
        setPaused(true);
    }
}

void BattlePage::onGameTick()
{
    if (!m_battleManager || m_isPaused || m_tutorialPaused) return;

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
    recordReplaySnapshot(snap, deltaSeconds);

    if (m_isPvp && m_isHost) {
        m_stateSyncTimer += deltaSeconds;
        if (m_stateSyncTimer >= 0.10) {
            m_stateSyncTimer = 0.0;
            sendBattleState(snap);
        }
    }

    if (snap.gameOver) {
        if (m_isPvp && m_isHost) {
            sendBattleState(snap);
        }
        recordReplaySnapshot(snap, 0.0, true);
        m_battleView->updateFromSnapshot(snap);
        updateStatusBar(snap);
        finishBattle(snap);
        return;
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
        recordReplaySnapshot(currentSnap, 0.0, true);
        m_battleView->updateFromSnapshot(currentSnap);
        updateStatusBar(currentSnap);
        return;
    }

    // PVE: 自动下一波
    if (!m_isPvp && !snap.waveActive) {
        if (m_currentWaveId >= m_pveFinalWave) {
            recordReplaySnapshot(snap, 0.0, true);
            m_battleView->updateFromSnapshot(snap);
            updateStatusBar(snap);
            finishBattle(snap, true);
            return;
        }
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
    if ((++m_renderTick & 1) == 0) {
        m_battleView->updateFromSnapshot(snap);
        updateStatusBar(snap);
    }

}

void BattleView::drawEffects(QPainter &painter)
{
    for (const BattleEffect &effect : m_effects) {
        const qreal progress = 1.0 - effect.life / effect.duration;
        const QPointF center = cellCenter(effect.row, effect.col);
        const qreal extent = cellExtent();

        painter.save();
        if (effect.type == EffectType::DeployDust) {
            for (int i = 0; i < 10; ++i) {
                const qreal angle = i * 0.628 + effect.row * 0.19;
                const qreal distance = extent * (0.12 + progress * (0.30 + (i % 3) * 0.04));
                const QPointF pos = center + QPointF(qCos(angle), qSin(angle) * 0.45) * distance;
                const qreal radius = extent * (0.09 - progress * 0.045) * (0.75 + (i % 3) * 0.12);
                painter.setPen(Qt::NoPen);
                painter.setBrush(QColor(151, 119, 73, qRound((1.0 - progress) * 150)));
                painter.drawEllipse(pos, radius, radius * 0.62);
            }
            painter.setPen(QPen(QColor(245, 218, 154,
                                      qRound((1.0 - progress) * 165)), 2));
            painter.setBrush(Qt::NoBrush);
            painter.drawEllipse(center, extent * progress * 0.43,
                                extent * progress * 0.24);
        } else if (effect.type == EffectType::AttackFlash) {
            const qreal alpha = 1.0 - progress;
            QRadialGradient flash(center, extent * 0.48);
            flash.setColorAt(0.0, QColor(255, 252, 212, qRound(alpha * 245)));
            flash.setColorAt(0.35, QColor(255, 193, 72, qRound(alpha * 190)));
            flash.setColorAt(1.0, QColor(255, 133, 32, 0));
            painter.setPen(Qt::NoPen);
            painter.setBrush(flash);
            painter.drawEllipse(center, extent * 0.48, extent * 0.48);

            painter.setCompositionMode(QPainter::CompositionMode_Screen);
            for (int i = 0; i < 8; ++i) {
                const qreal angle = i * M_PI / 4.0 + progress * 1.5;
                const qreal inner = extent * (0.12 + progress * 0.08);
                const qreal outer = extent * (0.34 + progress * 0.24);
                painter.setPen(QPen(QColor(255, 238, 171, qRound(alpha * (190 - i * 7))),
                                    i % 2 ? 1.4 : 2.2, Qt::SolidLine, Qt::RoundCap));
                painter.drawLine(center + QPointF(qCos(angle), qSin(angle)) * inner,
                                 center + QPointF(qCos(angle), qSin(angle)) * outer);
            }
            painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
            painter.setPen(QPen(QColor(92, 58, 26, qRound(alpha * 110)), 1.2));
            painter.setBrush(Qt::NoBrush);
            painter.drawEllipse(center, extent * (0.18 + progress * 0.20),
                                extent * (0.10 + progress * 0.12));
        } else {
            const qreal alpha = 1.0 - progress;
            const qreal radius = extent * (0.16 + progress * 0.42);

            QRadialGradient burst(center, radius * 1.8);
            burst.setColorAt(0.0, QColor(255, 250, 222, qRound(alpha * 180)));
            burst.setColorAt(0.32, QColor(255, 95, 65, qRound(alpha * 95)));
            burst.setColorAt(1.0, QColor(255, 58, 46, 0));
            painter.setPen(Qt::NoPen);
            painter.setBrush(burst);
            painter.drawEllipse(center, radius * 1.8, radius * 1.8);

            painter.setPen(QPen(QColor(255, 244, 218, qRound(alpha * 235)),
                                3.0 - progress * 1.6));
            painter.setBrush(QColor(255, 76, 61, qRound(alpha * 54)));
            painter.drawEllipse(center, radius, radius);

            painter.setCompositionMode(QPainter::CompositionMode_Screen);
            for (int i = 0; i < 7; ++i) {
                const qreal angle = i * 0.897 + effect.col * 0.13;
                const qreal dist = extent * (0.18 + progress * (0.34 + (i % 3) * 0.04));
                const QPointF spark = center + QPointF(qCos(angle), qSin(angle) * 0.72) * dist;
                const qreal sparkRadius = extent * (0.045 + (i % 2) * 0.018) * alpha;
                painter.setPen(Qt::NoPen);
                painter.setBrush(QColor(255, 210, 120, qRound(alpha * 190)));
                painter.drawEllipse(spark, sparkRadius, sparkRadius);
            }
            painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
            painter.setPen(QPen(QColor(255, 91, 67, qRound(alpha * 210)), 2));
            painter.drawLine(center + QPointF(-extent * 0.23, -extent * 0.18),
                             center + QPointF(extent * 0.23, extent * 0.18));
            painter.drawLine(center + QPointF(extent * 0.23, -extent * 0.18),
                             center + QPointF(-extent * 0.23, extent * 0.18));
        }
        painter.restore();
    }
}

void BattlePage::resetReplayRecorder()
{
    m_replayData = BattleReplayData();
    m_replayUnitStats.clear();
    m_replayMonsterStats.clear();
    m_seenReplayUnits.clear();
    m_seenReplayMonsters.clear();
    m_deathHeatCells.clear();
    m_deployHeatCells.clear();
    m_replayClock = 0.0;
    m_replaySampleTimer = 0.0;
    m_previousReplayResources = -1;
    m_hasReplaySnapshot = false;
    m_lastReplaySnapshot = game::core::BattleSnapshot();
}

void BattlePage::recordReplaySnapshot(const game::core::BattleSnapshot &snapshot,
                                      double deltaSeconds,
                                      bool force)
{
    if (snapshot.map.rows <= 0 || snapshot.map.cols <= 0) {
        return;
    }

    if (m_replayData.rows != snapshot.map.rows || m_replayData.cols != snapshot.map.cols) {
        m_replayData.rows = snapshot.map.rows;
        m_replayData.cols = snapshot.map.cols;
        m_deathHeatCells.fill(0, snapshot.map.rows * snapshot.map.cols);
        m_deployHeatCells.fill(0, snapshot.map.rows * snapshot.map.cols);
    }

    auto ensureUnitStat = [this](const game::core::UnitSnapshot& unit) -> BattleStatEntry& {
        BattleStatEntry &entry = m_replayUnitStats[unit.id];
        entry.unitId = unit.id;
        entry.name = replayUnitName(unit);
        entry.type = unit.type;
        entry.row = unit.row;
        entry.col = unit.col;
        entry.level = unit.level;
        return entry;
    };

    QHash<int, game::core::UnitSnapshot> currentUnits;
    QHash<int, game::core::MonsterSnapshot> currentMonsters;
    QHash<int, int> currentTargetSource;
    QHash<int, int> previousTargetSource;

    for (const auto& unit : snapshot.units) {
        currentUnits.insert(unit.id, unit);
        ensureUnitStat(unit);
        if (!m_seenReplayUnits.contains(unit.id)) {
            m_seenReplayUnits.insert(unit.id);
            addHeat(m_deployHeatCells, m_replayData.rows, m_replayData.cols, unit.row, unit.col);
        }
    }
    for (const auto& monster : snapshot.monsters) {
        currentMonsters.insert(monster.id, monster);
        const int kindKey = static_cast<int>(monster.kind);
        BattleMonsterStatEntry &monsterEntry = m_replayMonsterStats[kindKey];
        monsterEntry.kind = monster.kind;
        monsterEntry.name = replayMonsterName(monster.kind);
        monsterEntry.peakHp = qMax(monsterEntry.peakHp, monster.maxHp);
        monsterEntry.threatScore = qMax(monsterEntry.threatScore, monster.maxHp / 10);
        if (!m_seenReplayMonsters.contains(monster.id)) {
            m_seenReplayMonsters.insert(monster.id);
            monsterEntry.seen += 1;
            monsterEntry.threatScore += qMax(1, monster.maxHp / 25);
        }
    }
    for (const auto& projectile : snapshot.projectiles) {
        currentTargetSource.insert(projectile.targetId, projectile.sourceId);
    }
    for (const auto& projectile : m_lastReplaySnapshot.projectiles) {
        previousTargetSource.insert(projectile.targetId, projectile.sourceId);
    }

    if (m_hasReplaySnapshot) {
        QHash<int, game::core::MonsterSnapshot> previousMonsters;
        QHash<int, game::core::UnitSnapshot> previousUnits;
        for (const auto& monster : m_lastReplaySnapshot.monsters) {
            previousMonsters.insert(monster.id, monster);
        }
        for (const auto& unit : m_lastReplaySnapshot.units) {
            previousUnits.insert(unit.id, unit);
        }

        auto nearestAttacker = [&snapshot](const game::core::MonsterSnapshot& monster) {
            int bestId = -1;
            int bestDistance = 9999;
            for (const auto& unit : snapshot.units) {
                if (unit.type != game::core::ObjectType::CardAttack) continue;
                const int distance = qAbs(unit.row - monster.row) + qAbs(unit.col - monster.col);
                if (distance <= qMax(1, unit.range) && distance < bestDistance) {
                    bestDistance = distance;
                    bestId = unit.id;
                }
            }
            return bestId;
        };

        for (const auto& monster : snapshot.monsters) {
            if (!previousMonsters.contains(monster.id)) continue;
            const auto previous = previousMonsters.value(monster.id);
            const int damage = qMax(0, previous.hp - monster.hp);
            if (damage <= 0) continue;
            int sourceId = currentTargetSource.value(monster.id,
                                                     previousTargetSource.value(monster.id, -1));
            if (sourceId < 0) {
                sourceId = nearestAttacker(monster);
            }
            if (sourceId >= 0 && m_replayUnitStats.contains(sourceId)) {
                m_replayUnitStats[sourceId].damage += damage;
            }
            m_replayData.totalDamage += damage;
        }

        for (const auto& previous : m_lastReplaySnapshot.monsters) {
            if (!currentMonsters.contains(previous.id)) {
                BattleMonsterStatEntry &monsterEntry =
                    m_replayMonsterStats[static_cast<int>(previous.kind)];
                monsterEntry.kind = previous.kind;
                monsterEntry.name = replayMonsterName(previous.kind);
                monsterEntry.peakHp = qMax(monsterEntry.peakHp, previous.maxHp);
                if (previous.escaped) {
                    monsterEntry.escaped += 1;
                    monsterEntry.threatScore += previous.maxHp / 5 + 120;
                } else {
                    monsterEntry.defeated += 1;
                    monsterEntry.threatScore += previous.maxHp / 20 + 10;
                    addHeat(m_deathHeatCells, m_replayData.rows, m_replayData.cols,
                            previous.row, previous.col);
                }
            }
        }

        auto nearestHealer = [&snapshot](const game::core::UnitSnapshot& healedUnit) {
            int bestId = -1;
            int bestDistance = 9999;
            for (const auto& unit : snapshot.units) {
                if (unit.type != game::core::ObjectType::CardHeal) continue;
                const int distance = qAbs(unit.row - healedUnit.row) + qAbs(unit.col - healedUnit.col);
                if (distance <= qMax(1, unit.range) && distance < bestDistance) {
                    bestDistance = distance;
                    bestId = unit.id;
                }
            }
            return bestId;
        };

        for (const auto& unit : snapshot.units) {
            if (!previousUnits.contains(unit.id)) continue;
            const int healing = qMax(0, unit.hp - previousUnits.value(unit.id).hp);
            if (healing <= 0) continue;
            const int healerId = nearestHealer(unit);
            const int statId = healerId >= 0 ? healerId : unit.id;
            if (m_replayUnitStats.contains(statId)) {
                m_replayUnitStats[statId].healing += healing;
            }
            m_replayData.totalHealing += healing;
        }
    }

    if (m_previousReplayResources >= 0 && snapshot.resources > m_previousReplayResources) {
        const int gain = snapshot.resources - m_previousReplayResources;
        QVector<int> producers;
        for (const auto& unit : snapshot.units) {
            if (unit.type == game::core::ObjectType::CardProduce) {
                producers.append(unit.id);
            }
        }
        if (!producers.isEmpty()) {
            const int share = qMax(1, gain / producers.size());
            for (int id : producers) {
                if (m_replayUnitStats.contains(id)) {
                    m_replayUnitStats[id].resources += share;
                }
            }
        }
        m_replayData.totalResourceGain += gain;
    }

    m_previousReplayResources = snapshot.resources;
    m_replayClock += qMax(0.0, deltaSeconds);
    m_replaySampleTimer += qMax(0.0, deltaSeconds);
    if (force || m_replayData.frames.isEmpty() || m_replaySampleTimer >= 0.20) {
        m_replaySampleTimer = 0.0;
        BattleReplayFrame frame;
        frame.timeSeconds = m_replayClock;
        frame.snapshot = snapshot;
        m_replayData.frames.append(frame);
        m_replayData.durationSeconds = m_replayClock;
    }

    m_lastReplaySnapshot = snapshot;
    m_hasReplaySnapshot = true;
}

void BattlePage::attachReplayToResult(BattleResult &result)
{
    BattleReplayData replay = m_replayData;
    replay.unitStats.clear();
    replay.unitStats.reserve(m_replayUnitStats.size());
    for (auto it = m_replayUnitStats.constBegin(); it != m_replayUnitStats.constEnd(); ++it) {
        replay.unitStats.append(it.value());
    }
    replay.monsterStats.clear();
    replay.monsterStats.reserve(m_replayMonsterStats.size());
    for (auto it = m_replayMonsterStats.constBegin(); it != m_replayMonsterStats.constEnd(); ++it) {
        replay.monsterStats.append(it.value());
    }
    std::sort(replay.unitStats.begin(), replay.unitStats.end(),
              [](const BattleStatEntry& a, const BattleStatEntry& b) {
                  const int scoreA = a.damage + a.healing + a.resources;
                  const int scoreB = b.damage + b.healing + b.resources;
                  if (scoreA != scoreB) return scoreA > scoreB;
                  return a.unitId < b.unitId;
              });
    std::sort(replay.monsterStats.begin(), replay.monsterStats.end(),
              [](const BattleMonsterStatEntry& a, const BattleMonsterStatEntry& b) {
                  if (a.threatScore != b.threatScore) return a.threatScore > b.threatScore;
                  return a.seen > b.seen;
              });

    auto collectHeat = [](const QVector<int>& cells, int rows, int cols) {
        QVector<BattleHeatPoint> points;
        for (int row = 0; row < rows; ++row) {
            for (int col = 0; col < cols; ++col) {
                const int index = heatIndex(row, col, cols);
                if (index < 0 || index >= cells.size() || cells[index] <= 0) continue;
                points.append({row, col, cells[index]});
            }
        }
        return points;
    };
    replay.deathHeat = collectHeat(m_deathHeatCells, replay.rows, replay.cols);
    replay.deploymentHeat = collectHeat(m_deployHeatCells, replay.rows, replay.cols);
    if (!replay.frames.isEmpty()) {
        replay.durationSeconds = replay.frames.last().timeSeconds;
    }
    result.replay = replay;
}

void BattlePage::finishBattle(const game::core::BattleSnapshot &snapshot, bool pveVictory)
{
    if (m_resultEmitted) {
        return;
    }
    m_resultEmitted = true;
    m_inBattlePhase = false;
    m_waveStarted = false;
    m_gameTimer->stop();

    BattleResult result;
    result.isPvp = m_isPvp;
    result.wave = snapshot.currentWave;
    result.defeatedMonsters = snapshot.defeatedMonsters;
    result.escapedMonsters = snapshot.escapedMonsters;
    result.localCoreHealth = snapshot.baseHealth;
    result.opponentCoreHealth = snapshot.opponentBaseHealth;
    result.mapId = m_isPvp ? m_netCtx.pvpMapId : m_netCtx.pveMapId;
    recordReplaySnapshot(snapshot, 0.0, true);

    if (!m_isPvp) {
        result.outcome = pveVictory ? BattleOutcome::Victory : BattleOutcome::Defeat;
    } else if (snapshot.baseHealth <= 0 && snapshot.opponentBaseHealth <= 0) {
        result.outcome = BattleOutcome::Draw;
    } else if (snapshot.opponentBaseHealth <= 0) {
        result.outcome = BattleOutcome::Victory;
    } else {
        result.outcome = BattleOutcome::Defeat;
    }

    attachReplayToResult(result);
    emit signalBattleFinished(result);
}

// ========== updateStatusBar() —— 更新状态栏 ==========
void BattlePage::updateStatusBar(const game::core::BattleSnapshot &snapshot)
{
    const int previousCoreHealth = m_displayCoreHealth;
    const bool healthChanged =
        m_displayCoreHealth != snapshot.baseHealth
        || m_displayOpponentCoreHealth != snapshot.opponentBaseHealth;
    const bool resourcesChanged = m_displayResources != snapshot.resources;
    m_displayCoreHealth = snapshot.baseHealth;
    m_displayOpponentCoreHealth = snapshot.opponentBaseHealth;
    m_displayResources = snapshot.resources;

    if (!m_isPvp) {
        m_waveLabel->setText(QString("Wave %1/%2").arg(snapshot.currentWave).arg(m_pveFinalWave));
    } else {
        m_waveLabel->setText(QString("Wave %1").arg(snapshot.currentWave));
    }
    m_phaseLabel->setText(m_isPvp
                              ? (snapshot.waveActive ? "Battle Phase" : "Resource Phase")
                              : mapDisplayName(m_activePveMapId));
    m_coreHpLabel->setText(QString("Your Core %1/10").arg(snapshot.baseHealth));
    m_resourceLabel->setText(QString("Resource %1").arg(snapshot.resources));
    if (m_isPvp && m_opponentLabel) {
        m_opponentLabel->setText(QString("Enemy Core %1/10")
                                     .arg(snapshot.opponentBaseHealth));
    }
    if (resourcesChanged) {
        updateCardVisualState(snapshot.resources);
    }
    if (!m_isPvp
        && m_tutorialSessionActive
        && !m_coreWarningShown
        && m_tutorialStage == TutorialStage::Inactive
        && snapshot.baseHealth < previousCoreHealth) {
        m_coreWarningShown = true;
        m_tutorialPaused = true;
        m_tutorialStage = TutorialStage::CoreWarning;
        updateTutorialTargets();
        m_tutorialOverlay->showStep(
            "Peach Healer",
            "The core is hurt, but it is not lost. Reinforce the weak bend and keep enough Juice for an emergency defender.",
            TutorialOverlay::Focus::Core,
            "Hold the line");
    }
    if (healthChanged) {
        update();
    }
}

// ========== connectSignals() —— 连接信号槽 ==========
void BattlePage::connectSignals()
{
    // 暂停按钮
    connect(m_btnPause, &QPushButton::clicked, this, [this]() {
        setPaused(!m_isPaused);
    });

    // 加速按钮
    connect(m_btnSpeed, &QPushButton::clicked, this, [this]() {
        m_speedMultiplier = (m_speedMultiplier == 1.0) ? 2.0 : 1.0;
        m_btnSpeed->setText(m_speedMultiplier == 2.0 ? "2x" : "1x");
    });

    // 退出按钮
    connect(m_btnExit, &QPushButton::clicked, this, [this]() {
        setPaused(true);
    });

    connect(m_pauseOverlay, &PauseOverlay::signalResume,
            this, [this]() { setPaused(false); });
    connect(m_pauseOverlay, &PauseOverlay::signalRestart,
            this, [this]() {
                setPaused(false);
                m_gameTimer->stop();
                m_inBattlePhase = false;
                m_waveStarted = false;
                emit signalBattleRestartRequested();
            });
    connect(m_pauseOverlay, &PauseOverlay::signalExitToLobby,
            this, [this]() {
                setPaused(false);
                m_gameTimer->stop();
                m_inBattlePhase = false;
                m_waveStarted = false;
                if (m_battleManager) {
                    m_battleManager->clearBattle();
                }
                emit signalBattleCancelled();
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

void BattlePage::setPaused(bool paused)
{
    if (!m_inBattlePhase && paused) {
        return;
    }
    m_isPaused = paused;
    m_btnPause->setText(QString());
    if (paused) {
        m_battleView->hideRadialMenu();
        m_pauseOverlay->setPvpMode(m_isPvp);
        m_pauseOverlay->open();
    } else {
        m_pauseOverlay->closeMenu();
        setFocus(Qt::OtherFocusReason);
    }
}

void BattlePage::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape && m_inBattlePhase) {
        setPaused(!m_isPaused);
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

void BattlePage::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    layoutArtworkUi();
    update();
    if (m_pauseOverlay) {
        m_pauseOverlay->setGeometry(rect());
        m_pauseOverlay->raise();
    }
}
