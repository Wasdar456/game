#include "ui/BattleReportPage.h"

#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QPainter>
#include <QResizeEvent>
#include <QScrollBar>
#include <QShowEvent>
#include <QTextStream>
#include <QVBoxLayout>
#include <algorithm>
#include <utility>

namespace {

QString shortTime(double seconds)
{
    const int total = qMax(0, qRound(seconds));
    return QString("%1:%2").arg(total / 60).arg(total % 60, 2, 10, QLatin1Char('0'));
}

QColor terrainColor(game::core::TerrainType terrain)
{
    switch (terrain) {
    case game::core::TerrainType::Path: return QColor(177, 147, 99);
    case game::core::TerrainType::HighGround: return QColor(103, 137, 83);
    case game::core::TerrainType::FlatLand: return QColor(89, 126, 78);
    case game::core::TerrainType::NoDeploy: return QColor(64, 72, 65);
    case game::core::TerrainType::SpawnPoint: return QColor(181, 92, 66);
    case game::core::TerrainType::CoreA: return QColor(92, 151, 204);
    case game::core::TerrainType::CoreB: return QColor(197, 98, 78);
    }
    return QColor(80, 96, 76);
}

QString outcomeText(BattleOutcome outcome)
{
    switch (outcome) {
    case BattleOutcome::Victory: return "Victory";
    case BattleOutcome::Defeat: return "Defeat";
    case BattleOutcome::Draw: return "Draw";
    }
    return "Result";
}

QString typeName(game::core::ObjectType type)
{
    switch (type) {
    case game::core::ObjectType::CardAttack: return "Damage";
    case game::core::ObjectType::CardProduce: return "Resource";
    case game::core::ObjectType::CardHeal: return "Support";
    default: return "Unit";
    }
}

QString unitImagePath(const BattleStatEntry& entry)
{
    if (entry.name.contains("Kiwi")) return ":/images/new_art/unit_kiwi_scout.png";
    if (entry.name.contains("Miner")) return ":/images/new_art/unit_miner_pine.png";
    if (entry.name.contains("Mango")) return ":/images/new_art/unit_mango_engineer.png";
    if (entry.name.contains("Sniper")) return ":/images/new_art/unit_sniper_berry.png";
    if (entry.name.contains("Orange")) return ":/images/new_art/unit_orange_bomber.png";
    if (entry.name.contains("Berry Tank")) return ":/images/new_art/unit_berry_tank.png";
    if (entry.name.contains("Grape")) return ":/images/new_art/unit_grape_blaster.png";
    if (entry.name.contains("Peach")) return ":/images/new_art/unit_peach_healer.png";
    if (entry.name.contains("Papaya")) return ":/images/new_art/unit_papaya_support.png";
    if (entry.name.contains("Coco")) return ":/images/new_art/unit_coco_defender.png";
    return ":/images/new_art/unit_orange_bomber.png";
}

QString monsterImagePath(const BattleMonsterStatEntry& entry)
{
    switch (entry.kind) {
    case game::core::MonsterKind::AtkFast:
    case game::core::MonsterKind::AtkSapper:
        return ":/images/characters/tomato_variant_01_cutout.png";
    case game::core::MonsterKind::AtkTank:
    case game::core::MonsterKind::AtkBerserk:
    case game::core::MonsterKind::AtkRegen:
        return ":/images/characters/tomato_variant_02_cutout.png";
    default:
        return ":/images/characters/tomato_gunner_cutout.png";
    }
}

BattleHeatPoint hottestPoint(const QVector<BattleHeatPoint>& points)
{
    BattleHeatPoint best;
    for (const auto& point : points) {
        if (point.count > best.count) best = point;
    }
    return best;
}

BattleStatEntry bestContributor(const QVector<BattleStatEntry>& entries)
{
    BattleStatEntry best;
    int bestScore = -1;
    for (const auto& entry : entries) {
        const int score = entry.damage + entry.healing + entry.resources;
        if (score > bestScore) {
            bestScore = score;
            best = entry;
        }
    }
    return best;
}

QVector<BattleStatEntry> sortedBy(const QVector<BattleStatEntry>& source, int BattleStatEntry::*field)
{
    QVector<BattleStatEntry> result = source;
    std::sort(result.begin(), result.end(),
              [field](const BattleStatEntry& a, const BattleStatEntry& b) {
                  if (a.*field != b.*field) return a.*field > b.*field;
                  return a.unitId < b.unitId;
              });
    return result;
}

QString rankingHtml(const QString& title,
                    const QVector<BattleStatEntry>& entries,
                    int BattleStatEntry::*field,
                    const QString& suffix)
{
    QString html = QString("<h3 style='margin-bottom:4px'>%1</h3><table width='100%' cellspacing='0' cellpadding='4'>")
                       .arg(title);
    int shown = 0;
    for (const auto& entry : entries) {
        const int value = entry.*field;
        if (value <= 0 && shown > 0) continue;
        if (shown >= 5) break;
        html += QString("<tr><td><b>%1</b><br><span style='color:#806341'>%2 Lv%3 (%4,%5)</span></td>"
                        "<td align='right' style='font-size:16px'><b>%6</b> %7</td></tr>")
                    .arg(entry.name.toHtmlEscaped())
                    .arg(typeName(entry.type))
                    .arg(entry.level)
                    .arg(entry.row)
                    .arg(entry.col)
                    .arg(value)
                    .arg(suffix);
        ++shown;
    }
    if (shown == 0) {
        html += "<tr><td style='color:#806341'>No data captured yet.</td></tr>";
    }
    html += "</table>";
    return html;
}

} // namespace

ReplayCanvas::ReplayCanvas(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(720, 420);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void ReplayCanvas::setData(const BattleReplayData& data)
{
    m_data = data;
    m_frameIndex = 0;
    update();
}

void ReplayCanvas::setFrameIndex(int index)
{
    const int maxIndex = qMax(0, static_cast<int>(m_data.frames.size()) - 1);
    m_frameIndex = std::clamp(index, 0, maxIndex);
    update();
}

void ReplayCanvas::setHeatMode(HeatMode mode)
{
    m_heatMode = mode;
    update();
}

void ReplayCanvas::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(rect(), QColor(236, 221, 186));

    QRectF panel = rect().adjusted(8, 8, -8, -8);
    QLinearGradient paper(panel.topLeft(), panel.bottomRight());
    paper.setColorAt(0.0, QColor(250, 230, 179));
    paper.setColorAt(1.0, QColor(232, 203, 142));
    painter.setPen(QPen(QColor(103, 73, 41), 3));
    painter.setBrush(paper);
    painter.drawRoundedRect(panel, 12, 12);

    if (m_data.frames.isEmpty()) {
        painter.setPen(QColor(62, 42, 27));
        painter.setFont(QFont("Segoe UI", 20, QFont::Bold));
        painter.drawText(panel, Qt::AlignCenter, "No replay frames recorded");
        return;
    }

    const auto& frame = m_data.frames[m_frameIndex];
    const auto& snapshot = frame.snapshot;
    const int rows = qMax(1, snapshot.map.rows);
    const int cols = qMax(1, snapshot.map.cols);
    const qreal cell = std::min((panel.width() - 52) / cols, (panel.height() - 84) / rows);
    QRectF grid(panel.center().x() - cell * cols * 0.5,
                panel.top() + 56,
                cell * cols,
                cell * rows);

    painter.setPen(QPen(QColor(70, 48, 30, 110), 1));
    for (const auto& tile : snapshot.map.grids) {
        QRectF r(grid.left() + tile.col * cell, grid.top() + tile.row * cell, cell, cell);
        painter.fillRect(r, terrainColor(tile.terrain));
        painter.drawRect(r);
    }

    const QVector<BattleHeatPoint>& heat = m_heatMode == HeatMode::Deaths
                                               ? m_data.deathHeat
                                               : m_data.deploymentHeat;
    int maxHeat = 1;
    for (const auto& point : heat) maxHeat = qMax(maxHeat, point.count);
    for (const auto& point : heat) {
        QRectF r(grid.left() + point.col * cell, grid.top() + point.row * cell, cell, cell);
        const qreal ratio = point.count / qreal(maxHeat);
        const QColor color = m_heatMode == HeatMode::Deaths
                                 ? QColor(229, 74, 45, qRound(70 + 150 * ratio))
                                 : QColor(71, 165, 231, qRound(65 + 145 * ratio));
        painter.setPen(Qt::NoPen);
        painter.setBrush(color);
        painter.drawEllipse(r.adjusted(cell * 0.12, cell * 0.12, -cell * 0.12, -cell * 0.12));
    }

    auto drawHpBar = [&painter](const QRectF& rect, int hp, int maxHp) {
        const qreal ratio = std::clamp(hp / qreal(qMax(1, maxHp)), 0.0, 1.0);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(48, 36, 27, 220));
        painter.drawRoundedRect(rect, 2, 2);
        QRectF fill = rect.adjusted(1, 1, -1, -1);
        fill.setWidth(fill.width() * ratio);
        painter.setBrush(ratio > 0.45 ? QColor(91, 182, 80) : QColor(218, 86, 62));
        painter.drawRoundedRect(fill, 2, 2);
    };

    for (const auto& unit : snapshot.units) {
        QPointF c(grid.left() + (unit.col + 0.5) * cell, grid.top() + (unit.row + 0.5) * cell);
        painter.setPen(QPen(QColor(46, 33, 23), 2));
        painter.setBrush(unit.id >= 1000 ? QColor(198, 93, 76) : QColor(86, 158, 111));
        painter.drawEllipse(c, cell * 0.32, cell * 0.32);
        painter.setPen(QColor(255, 247, 220));
        painter.setFont(QFont("Segoe UI", qMax(8, qRound(cell * 0.22)), QFont::Bold));
        painter.drawText(QRectF(c.x() - cell * 0.34, c.y() - cell * 0.30, cell * 0.68, cell * 0.42),
                         Qt::AlignCenter,
                         QString::number(unit.level));
        drawHpBar(QRectF(c.x() - cell * 0.34, c.y() + cell * 0.26, cell * 0.68, cell * 0.08),
                  unit.hp, unit.maxHp);
    }

    for (const auto& monster : snapshot.monsters) {
        QPointF c(grid.left() + (monster.col + 0.5) * cell, grid.top() + (monster.row + 0.5) * cell);
        painter.setPen(QPen(QColor(82, 42, 33), 2));
        painter.setBrush(QColor(103, 83, 178));
        painter.drawRoundedRect(QRectF(c.x() - cell * 0.28, c.y() - cell * 0.28,
                                       cell * 0.56, cell * 0.56),
                                cell * 0.12, cell * 0.12);
        drawHpBar(QRectF(c.x() - cell * 0.30, c.y() + cell * 0.30, cell * 0.60, cell * 0.08),
                  monster.hp, monster.maxHp);
    }

    painter.setPen(QColor(55, 37, 24));
    painter.setFont(QFont("Segoe UI", 18, QFont::Bold));
    painter.drawText(QRectF(panel.left() + 24, panel.top() + 14, panel.width() - 48, 32),
                     Qt::AlignLeft | Qt::AlignVCenter,
                     QString("Replay %1 / %2   %3")
                         .arg(m_frameIndex + 1)
                         .arg(m_data.frames.size())
                         .arg(shortTime(frame.timeSeconds)));
    painter.setFont(QFont("Segoe UI", 11, QFont::DemiBold));
    painter.drawText(QRectF(panel.left() + 24, panel.bottom() - 30, panel.width() - 48, 22),
                     Qt::AlignRight | Qt::AlignVCenter,
                     m_heatMode == HeatMode::Deaths ? "Monster death heatmap" : "Tower deployment heatmap");
}

BattleReportPage::BattleReportPage(QWidget *parent)
    : QWidget(parent)
{
    initUI();
}

void BattleReportPage::setResult(const BattleResult& result)
{
    m_result = result;
    m_canvas->setData(m_result.replay);
    m_timeline->setRange(0, qMax(0, m_result.replay.frames.size() - 1));
    m_timeline->setValue(0);
    m_playbackTime = 0.0;
    m_playing = false;
    m_playButton->setText("Play");
    updateStatsText();
    updateLayout();
}

void BattleReportPage::setNavigationHandlers(std::function<void()> backHandler,
                                             std::function<void()> lobbyHandler)
{
    m_backHandler = std::move(backHandler);
    m_lobbyHandler = std::move(lobbyHandler);
}

void BattleReportPage::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateLayout();
}

void BattleReportPage::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    updateLayout();
}

void BattleReportPage::initUI()
{
    setStyleSheet(
        "BattleReportPage { background:#E8D6AC; }"
        "QPushButton { background:#F0D8A3; color:#352314; border:2px solid #674725;"
        " border-radius:8px; padding:6px 8px; font-weight:800; }"
        "QPushButton:hover { background:#FFE7AD; border-color:#D6A952; }"
        "QPushButton:pressed { background:#C89C63; }"
        "QSlider::groove:horizontal { height:10px; border-radius:5px; background:#735331; }"
        "QSlider::handle:horizontal { width:22px; margin:-7px 0; border-radius:11px;"
        " background:#F6D673; border:2px solid #513619; }"
        "QTextEdit { background:#F1DCA9; color:#332216; border:3px solid #674725;"
        " border-radius:10px; padding:12px; }");

    m_canvas = new ReplayCanvas(this);
    m_statsText = new QTextEdit(this);
    m_statsText->setReadOnly(true);
    m_statsText->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    m_timeline = new QSlider(Qt::Horizontal, this);
    m_playButton = new QPushButton("Play", this);
    m_speedButton = new QPushButton("1x", this);
    m_deathHeatButton = new QPushButton("Deaths", this);
    m_deployHeatButton = new QPushButton("Deploy", this);
    m_exportButton = new QPushButton("Export", this);
    m_backButton = new QPushButton("Back", this);
    m_lobbyButton = new QPushButton("Lobby", this);
    m_deathHeatButton->setToolTip("Show monster death heatmap");
    m_deployHeatButton->setToolTip("Show tower deployment heatmap");
    m_exportButton->setToolTip("Export battle report");

    connect(m_playButton, &QPushButton::clicked, this, [this]() {
        m_playing = !m_playing;
        m_playButton->setText(m_playing ? "Pause" : "Play");
    });
    connect(m_speedButton, &QPushButton::clicked, this, [this]() {
        m_speed = m_speed >= 4.0 ? 1.0 : m_speed * 2.0;
        m_speedButton->setText(QString("%1x").arg(qRound(m_speed)));
    });
    connect(m_deathHeatButton, &QPushButton::clicked, this, [this]() {
        m_canvas->setHeatMode(ReplayCanvas::HeatMode::Deaths);
    });
    connect(m_deployHeatButton, &QPushButton::clicked, this, [this]() {
        m_canvas->setHeatMode(ReplayCanvas::HeatMode::Deployments);
    });
    connect(m_exportButton, &QPushButton::clicked, this, &BattleReportPage::exportReport);
    connect(m_backButton, &QPushButton::clicked, this, [this]() {
        if (m_backHandler) m_backHandler();
    });
    connect(m_lobbyButton, &QPushButton::clicked, this, [this]() {
        if (m_lobbyHandler) m_lobbyHandler();
    });
    connect(m_timeline, &QSlider::valueChanged, this, &BattleReportPage::setFrameIndex);
    connect(&m_playTimer, &QTimer::timeout, this, &BattleReportPage::tickPlayback);
    m_playTimer.start(33);
}

void BattleReportPage::updateLayout()
{
    const QRect area = rect().adjusted(28, 24, -28, -24);
    const int gap = 18;
    const int rightW = qBound(330, area.width() / 3, 430);
    const int controlsH = 72;

    m_canvas->setGeometry(area.left(), area.top(),
                          area.width() - rightW - gap,
                          area.height() - controlsH - gap);
    m_statsText->setGeometry(area.right() - rightW + 1, area.top(),
                             rightW, area.height() - controlsH - gap);

    QRect controls(area.left(), area.bottom() - controlsH + 1, area.width(), controlsH);
    const int smallGap = 8;
    const int playW = 78;
    const int speedW = 58;
    const int actionW = qBound(92, controls.width() / 12, 124);
    const int actionTotal = actionW * 5 + smallGap * 4;
    const int fixedW = playW + speedW + actionTotal + smallGap * 4;
    const int timelineW = qMax(180, controls.width() - fixedW);
    m_playButton->setGeometry(controls.left(), controls.top() + 12, playW, 44);
    m_speedButton->setGeometry(m_playButton->geometry().right() + smallGap, controls.top() + 12, speedW, 44);
    m_timeline->setGeometry(m_speedButton->geometry().right() + smallGap, controls.top() + 12,
                            timelineW, 44);
    int x = m_timeline->geometry().right() + smallGap;
    for (QPushButton *button : {m_deathHeatButton, m_deployHeatButton,
                                m_exportButton, m_backButton, m_lobbyButton}) {
        button->setGeometry(x, controls.top() + 12, actionW, 44);
        x += actionW + smallGap;
    }
}

void BattleReportPage::updateStatsText()
{
    const auto& replay = m_result.replay;
    const QVector<BattleStatEntry> byDamage = sortedBy(replay.unitStats, &BattleStatEntry::damage);
    const QVector<BattleStatEntry> byHealing = sortedBy(replay.unitStats, &BattleStatEntry::healing);
    const QVector<BattleStatEntry> byResources = sortedBy(replay.unitStats, &BattleStatEntry::resources);
    const BattleStatEntry mvp = bestContributor(replay.unitStats);
    const BattleMonsterStatEntry danger = replay.monsterStats.isEmpty()
                                              ? BattleMonsterStatEntry()
                                              : replay.monsterStats.first();
    const BattleHeatPoint hotDeath = hottestPoint(replay.deathHeat);
    const BattleHeatPoint hotDeploy = hottestPoint(replay.deploymentHeat);
    const int dangerPressure = m_result.escapedMonsters * 120
                               + qMax(0, 10 - m_result.localCoreHealth) * 45
                               + hotDeath.count * 8;
    const int totalThreats = m_result.defeatedMonsters + m_result.escapedMonsters;
    const QString mvpName = mvp.unitId >= 0 ? mvp.name : QString("No MVP yet");
    const QString dangerName = danger.seen > 0 ? danger.name : QString("Unknown Threat");

    QString html;
    html += QString("<body style='font-family:Segoe UI; font-size:12px'>"
                    "<h1 style='margin-top:0'>Battle Report</h1>"
                    "<p><b>%1</b> | %2 | Wave %3 | Duration %4</p>"
                    "<table width='100%' cellspacing='0' cellpadding='6'>"
                    "<tr>"
                    "<td bgcolor='#F7E5B8'><img src='%5' width='58' height='58'></td>"
                    "<td bgcolor='#F7E5B8'><b>最大功臣 MVP</b><br>%6<br>"
                    "<span style='color:#785838'>贡献 %7</span></td>"
                    "</tr>"
                    "<tr>"
                    "<td bgcolor='#EEC7A5'><img src='%8' width='58' height='58'></td>"
                    "<td bgcolor='#EEC7A5'><b>最大危险怪物</b><br>%9<br>"
                    "<span style='color:#785838'>出现 %10 | 威胁 %11</span></td>"
                    "</tr>"
                    "</table>"
                    "<p><b>防线压力</b>: %12 &nbsp; <b>总威胁</b>: %13<br>"
                    "<b>死亡热点</b>: (%14,%15) x%16 &nbsp; <b>部署热点</b>: (%17,%18) x%19</p>"
                    "<p>Frames: <b>%20</b> &nbsp; Damage: <b>%21</b> &nbsp; Healing: <b>%22</b><br>"
                    "Resource Gain: <b>%23</b> &nbsp; Core Lost: <b>%24</b></p><hr>")
                .arg(outcomeText(m_result.outcome))
                .arg(m_result.isPvp ? "PVP" : "PVE")
                .arg(m_result.wave)
                .arg(shortTime(replay.durationSeconds))
                .arg(unitImagePath(mvp))
                .arg(mvpName.toHtmlEscaped())
                .arg(qMax(0, mvp.damage + mvp.healing + mvp.resources))
                .arg(monsterImagePath(danger))
                .arg(dangerName.toHtmlEscaped())
                .arg(danger.seen)
                .arg(danger.threatScore)
                .arg(dangerPressure)
                .arg(totalThreats)
                .arg(hotDeath.row)
                .arg(hotDeath.col)
                .arg(hotDeath.count)
                .arg(hotDeploy.row)
                .arg(hotDeploy.col)
                .arg(hotDeploy.count)
                .arg(replay.frames.size())
                .arg(replay.totalDamage)
                .arg(replay.totalHealing)
                .arg(replay.totalResourceGain)
                .arg(qMax(0, 10 - m_result.localCoreHealth));
    html += rankingHtml("Damage Ranking", byDamage, &BattleStatEntry::damage, "DMG");
    html += rankingHtml("Healing Ranking", byHealing, &BattleStatEntry::healing, "HP");
    html += rankingHtml("Resource Ranking", byResources, &BattleStatEntry::resources, "Juice");
    html += "<h3 style='margin-bottom:4px'>Threat Mix</h3><table width='100%' cellspacing='0' cellpadding='4'>";
    int monsterRows = 0;
    for (const auto& entry : replay.monsterStats) {
        if (monsterRows >= 4) break;
        html += QString("<tr><td><b>%1</b><br><span style='color:#806341'>seen %2, defeated %3, escaped %4</span></td>"
                        "<td align='right'><b>%5</b></td></tr>")
                    .arg(entry.name.toHtmlEscaped())
                    .arg(entry.seen)
                    .arg(entry.defeated)
                    .arg(entry.escaped)
                    .arg(entry.threatScore);
        ++monsterRows;
    }
    if (monsterRows == 0) {
        html += "<tr><td style='color:#806341'>No monster data captured.</td></tr>";
    }
    html += "</table>";
    html += QString("<hr><p>Death heat points: <b>%1</b><br>Deployment heat points: <b>%2</b></p></body>")
                .arg(replay.deathHeat.size())
                .arg(replay.deploymentHeat.size());
    m_statsText->setHtml(html);
    m_statsText->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
}

void BattleReportPage::setFrameIndex(int index)
{
    if (m_result.replay.frames.isEmpty()) return;
    const int maxIndex = static_cast<int>(m_result.replay.frames.size()) - 1;
    const int safeIndex = std::clamp(index, 0, maxIndex);
    m_canvas->setFrameIndex(safeIndex);
    m_playbackTime = m_result.replay.frames[safeIndex].timeSeconds;
}

int BattleReportPage::frameIndexForTime(double seconds) const
{
    const auto& frames = m_result.replay.frames;
    if (frames.isEmpty()) return 0;
    for (int i = 0; i < frames.size(); ++i) {
        if (frames[i].timeSeconds >= seconds) {
            return i;
        }
    }
    return frames.size() - 1;
}

void BattleReportPage::tickPlayback()
{
    if (!m_playing || m_result.replay.frames.isEmpty()) return;
    m_playbackTime += 0.033 * m_speed;
    if (m_playbackTime >= m_result.replay.durationSeconds) {
        m_playbackTime = m_result.replay.durationSeconds;
        m_playing = false;
        m_playButton->setText("Play");
    }
    const int index = frameIndexForTime(m_playbackTime);
    if (m_timeline->value() != index) {
        m_timeline->setValue(index);
    } else {
        m_canvas->setFrameIndex(index);
    }
}

void BattleReportPage::exportReport()
{
    const QString defaultPath = QDir::home().filePath("Desktop/battle_report.md");
    const QString path = QFileDialog::getSaveFileName(this, "Export Battle Report",
                                                      defaultPath,
                                                      "Markdown (*.md);;Text (*.txt)");
    if (path.isEmpty()) return;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return;
    }

    QTextStream out(&file);
    out << "# Battle Report\n\n";
    out << "- Outcome: " << outcomeText(m_result.outcome) << "\n";
    out << "- Mode: " << (m_result.isPvp ? "PVP" : "PVE") << "\n";
    out << "- Wave: " << m_result.wave << "\n";
    out << "- Duration: " << shortTime(m_result.replay.durationSeconds) << "\n";
    out << "- Frames: " << m_result.replay.frames.size() << "\n";
    out << "- Core HP: " << m_result.localCoreHealth << "\n";
    if (m_result.isPvp) out << "- Enemy Core HP: " << m_result.opponentCoreHealth << "\n";
    out << "\n## Totals\n\n";
    out << "- Damage: " << m_result.replay.totalDamage << "\n";
    out << "- Healing: " << m_result.replay.totalHealing << "\n";
    out << "- Resource Gain: " << m_result.replay.totalResourceGain << "\n";
    out << "- Monster Death Heat Points: " << m_result.replay.deathHeat.size() << "\n";
    out << "- Deployment Heat Points: " << m_result.replay.deploymentHeat.size() << "\n";
    const BattleStatEntry mvp = bestContributor(m_result.replay.unitStats);
    const BattleMonsterStatEntry danger = m_result.replay.monsterStats.isEmpty()
                                              ? BattleMonsterStatEntry()
                                              : m_result.replay.monsterStats.first();
    const BattleHeatPoint hotDeath = hottestPoint(m_result.replay.deathHeat);
    const BattleHeatPoint hotDeploy = hottestPoint(m_result.replay.deploymentHeat);
    out << "- MVP: " << (mvp.unitId >= 0 ? mvp.name : QString("None"))
        << " (" << qMax(0, mvp.damage + mvp.healing + mvp.resources) << " contribution)\n";
    out << "- Most Dangerous Monster: " << (danger.seen > 0 ? danger.name : QString("Unknown"))
        << " (" << danger.threatScore << " threat)\n";
    out << "- Hottest Death Cell: (" << hotDeath.row << "," << hotDeath.col
        << ") x" << hotDeath.count << "\n";
    out << "- Hottest Deployment Cell: (" << hotDeploy.row << "," << hotDeploy.col
        << ") x" << hotDeploy.count << "\n";
    out << "\n## Unit Ranking\n\n";
    out << "| Unit | Type | Pos | Damage | Healing | Resource |\n";
    out << "| --- | --- | --- | ---: | ---: | ---: |\n";
    for (const auto& entry : m_result.replay.unitStats) {
        out << "| " << entry.name << " #" << entry.unitId
            << " | " << typeName(entry.type)
            << " | (" << entry.row << "," << entry.col << ")"
            << " | " << entry.damage
            << " | " << entry.healing
            << " | " << entry.resources << " |\n";
    }
    out << "\n## Threat Mix\n\n";
    out << "| Monster | Seen | Defeated | Escaped | Peak HP | Threat |\n";
    out << "| --- | ---: | ---: | ---: | ---: | ---: |\n";
    for (const auto& entry : m_result.replay.monsterStats) {
        out << "| " << entry.name
            << " | " << entry.seen
            << " | " << entry.defeated
            << " | " << entry.escaped
            << " | " << entry.peakHp
            << " | " << entry.threatScore << " |\n";
    }
}
