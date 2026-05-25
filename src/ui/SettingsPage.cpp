/**
 * @file SettingsPage.cpp
 * @brief 设置页面实现文件
 *
 * 核心逻辑：
 *   1. 使用 QSlider 控制音量（0~100），实时显示数值
 *   2. 使用 QComboBox 选择分辨率
 *   3. 使用 QCheckBox 切换开关选项
 *   4. 保存按钮 → 将设置写入 QSettings（后续实现完整持久化）
 *
 * 设置与战斗页面的联动：
 *   - signalShowGridChanged(bool) → BattlePage 监听，控制网格线显示
 *   - signalVolumeChanged(int, int) → 音频模块监听，调整音量
 */

#include "ui/SettingsPage.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFormLayout>
#include <QTimer>

// ========== 构造函数 ==========
SettingsPage::SettingsPage(QWidget *parent)
    : QWidget(parent)
    , m_btnBack(nullptr)
    , m_titleLabel(nullptr)
    , m_bgmSlider(nullptr)
    , m_bgmValueLabel(nullptr)
    , m_sfxSlider(nullptr)
    , m_sfxValueLabel(nullptr)
    , m_resolutionCombo(nullptr)
    , m_fullscreenCheck(nullptr)
    , m_gridCheck(nullptr)
    , m_autoPauseCheck(nullptr)
    , m_btnSave(nullptr)
{
    initUI();
    connectSignals();
}

// ========== initUI() —— 初始化界面 ==========
void SettingsPage::initUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(60, 30, 60, 30);
    mainLayout->setSpacing(20);

    // ----- 顶部导航栏 -----
    QHBoxLayout *topBar = new QHBoxLayout();

    m_btnBack = new QPushButton("← 返回", this);
    m_btnBack->setFixedSize(100, 40);
    m_btnBack->setStyleSheet(
        "QPushButton { background-color: transparent; color: #8AB4F8;"
        "  border: 1px solid rgba(0,212,255,0.3); border-radius: 8px; font-size: 14px; }"
        "QPushButton:hover { color: #00D4FF; border: 1px solid #00D4FF; }"
    );
    m_btnBack->setCursor(Qt::PointingHandCursor);

    m_titleLabel = new QLabel("⚙ 游戏设置", this);
    m_titleLabel->setStyleSheet("color: #FFFFFF; font-size: 22px; font-weight: bold;");

    topBar->addWidget(m_btnBack);
    topBar->addStretch();
    topBar->addWidget(m_titleLabel);
    topBar->addStretch();
    mainLayout->addLayout(topBar);

    // ===== 通用分组样式 =====
    QString groupStyle =
        "QGroupBox {"
        "  color: #00D4FF; font-size: 16px; font-weight: bold;"
        "  border: 1px solid rgba(0,212,255,0.25); border-radius: 10px;"
        "  background-color: rgba(15,27,45,0.5);"
        "  margin-top: 15px; padding-top: 20px;"
        "}"
        "QGroupBox::title {"
        "  subcontrol-origin: margin; left: 15px; padding: 0 5px;"
        "}"
    ;

    // ===== 音量设置分组 =====
    QGroupBox *audioGroup = new QGroupBox("🔊 音量设置", this);
    audioGroup->setStyleSheet(groupStyle);

    QFormLayout *audioLayout = new QFormLayout(audioGroup);
    audioLayout->setSpacing(15);
    audioLayout->setContentsMargins(20, 25, 20, 15);

    // 通用滑块样式
    QString sliderStyle =
        "QSlider::groove:horizontal { height: 6px; background: #1A2742; border-radius: 3px; }"
        "QSlider::handle:horizontal { width: 16px; height: 16px; margin: -5px 0;"
        "  background: #00D4FF; border-radius: 8px; }"
        "QSlider::sub-page:horizontal { background: #00D4FF; border-radius: 3px; }"
    ;

    // 背景音乐滑块
    m_bgmSlider = new QSlider(Qt::Horizontal, this);
    m_bgmSlider->setRange(0, 100);
    m_bgmSlider->setValue(70);
    m_bgmSlider->setFixedHeight(25);
    m_bgmSlider->setStyleSheet(sliderStyle);

    m_bgmValueLabel = new QLabel("70%", this);
    m_bgmValueLabel->setFixedWidth(50);
    m_bgmValueLabel->setAlignment(Qt::AlignCenter);
    m_bgmValueLabel->setStyleSheet("color: #00D4FF; font-size: 14px; font-weight: bold;");

    QHBoxLayout *bgmLayout = new QHBoxLayout();
    bgmLayout->addWidget(m_bgmSlider, 1);
    bgmLayout->addWidget(m_bgmValueLabel);
    audioLayout->addRow("背景音乐：", bgmLayout);

    // 音效滑块
    m_sfxSlider = new QSlider(Qt::Horizontal, this);
    m_sfxSlider->setRange(0, 100);
    m_sfxSlider->setValue(85);
    m_sfxSlider->setFixedHeight(25);
    m_sfxSlider->setStyleSheet(sliderStyle);

    m_sfxValueLabel = new QLabel("85%", this);
    m_sfxValueLabel->setFixedWidth(50);
    m_sfxValueLabel->setAlignment(Qt::AlignCenter);
    m_sfxValueLabel->setStyleSheet("color: #00D4FF; font-size: 14px; font-weight: bold;");

    QHBoxLayout *sfxLayout = new QHBoxLayout();
    sfxLayout->addWidget(m_sfxSlider, 1);
    sfxLayout->addWidget(m_sfxValueLabel);
    audioLayout->addRow("音效：", sfxLayout);

    mainLayout->addWidget(audioGroup);

    // ===== 显示设置分组 =====
    QGroupBox *displayGroup = new QGroupBox("🖥️ 显示设置", this);
    displayGroup->setStyleSheet(groupStyle);

    QFormLayout *displayLayout = new QFormLayout(displayGroup);
    displayLayout->setSpacing(15);
    displayLayout->setContentsMargins(20, 25, 20, 15);

    m_resolutionCombo = new QComboBox(this);
    m_resolutionCombo->addItems({"1280 × 720 (720p)", "1600 × 900 (900p)", "1920 × 1080 (1080p)", "2560 × 1440 (2K)"});
    m_resolutionCombo->setFixedHeight(38);
    m_resolutionCombo->setStyleSheet(
        "QComboBox { background-color: rgba(22,37,66,0.75); color: #FFFFFF;"
        "  border: 2px solid rgba(0,212,255,0.3); border-radius: 8px;"
        "  padding: 5px 12px; font-size: 14px; }"
        "QComboBox::drop-down { border: none; width: 25px; }"
        "QComboBox QAbstractItemView { background-color: #0F1B2D; color: #FFFFFF;"
        "  selection-background-color: rgba(0,212,255,0.3); border: 1px solid #00D4FF; }"
    );
    displayLayout->addRow("分辨率：", m_resolutionCombo);

    // 通用复选框样式
    QString checkStyle =
        "QCheckBox { color: #E3F2FD; font-size: 14px; spacing: 8px; }"
        "QCheckBox::indicator { width: 20px; height: 20px; border: 2px solid rgba(0,212,255,0.5); border-radius: 4px; }"
        "QCheckBox::indicator:checked { background-color: #00D4FF; border: 2px solid #00D4FF; }"
    ;

    m_fullscreenCheck = new QCheckBox("启用全屏模式", this);
    m_fullscreenCheck->setStyleSheet(checkStyle);
    displayLayout->addRow("全屏：", m_fullscreenCheck);

    mainLayout->addWidget(displayGroup);

    // ===== 游戏设置分组 =====
    QGroupBox *gameGroup = new QGroupBox("🎮 游戏设置", this);
    gameGroup->setStyleSheet(groupStyle);

    QFormLayout *gameLayout = new QFormLayout(gameGroup);
    gameLayout->setSpacing(15);
    gameLayout->setContentsMargins(20, 25, 20, 15);

    m_gridCheck = new QCheckBox("在战斗中显示地图网格线", this);
    m_gridCheck->setChecked(true);
    m_gridCheck->setStyleSheet(checkStyle);
    gameLayout->addRow("显示网格：", m_gridCheck);

    m_autoPauseCheck = new QCheckBox("失去焦点时自动暂停游戏", this);
    m_autoPauseCheck->setChecked(true);
    m_autoPauseCheck->setStyleSheet(checkStyle);
    gameLayout->addRow("自动暂停：", m_autoPauseCheck);

    mainLayout->addWidget(gameGroup);

    // ----- 保存按钮 -----
    m_btnSave = new QPushButton("💾 保存设置", this);
    m_btnSave->setFixedSize(200, 50);
    m_btnSave->setStyleSheet(
        "QPushButton {"
        "  background-color: rgba(0,212,255,0.15); color: #00D4FF;"
        "  border: 2px solid rgba(0,212,255,0.6); border-radius: 12px;"
        "  font-size: 16px; font-weight: bold;"
        "}"
        "QPushButton:hover { background-color: rgba(0,212,255,0.3); }"
        "QPushButton:pressed { background-color: rgba(0,212,255,0.45); }"
    );
    m_btnSave->setCursor(Qt::PointingHandCursor);

    QHBoxLayout *saveLayout = new QHBoxLayout();
    saveLayout->addStretch();
    saveLayout->addWidget(m_btnSave);
    saveLayout->addStretch();
    mainLayout->addLayout(saveLayout);
    mainLayout->addStretch();

    // 页面背景
    this->setStyleSheet(
        "SettingsPage {"
        "  background: qlineargradient("
        "    x1:0, y1:0, x2:0, y2:1,"
        "    stop:0 #0B1622, stop:1 #162544"
        "  );"
        "}"
    );
}

// ========== connectSignals() —— 连接信号槽 ==========
void SettingsPage::connectSignals()
{
    // 返回按钮
    connect(m_btnBack, &QPushButton::clicked, this, &SettingsPage::signalBack);

    // 音量滑块实时更新数值标签
    connect(m_bgmSlider, &QSlider::valueChanged, this, [this](int value) {
        m_bgmValueLabel->setText(QString("%1%").arg(value));
        emit signalVolumeChanged(value, m_sfxSlider->value());
    });

    connect(m_sfxSlider, &QSlider::valueChanged, this, [this](int value) {
        m_sfxValueLabel->setText(QString("%1%").arg(value));
        emit signalVolumeChanged(m_bgmSlider->value(), value);
    });

    // 网格显示开关
    connect(m_gridCheck, &QCheckBox::checkStateChanged, this, [this](Qt::CheckState state) {
        emit signalShowGridChanged(state == Qt::Checked);
    });

    // 保存按钮
    connect(m_btnSave, &QPushButton::clicked, this, [this]() {
        // TODO: 使用 QSettings 持久化保存
        // QSettings settings("GameTeam", "TowerDefense");
        // settings.setValue("audio/bgm", m_bgmSlider->value());
        // settings.setValue("audio/sfx", m_sfxSlider->value());
        // settings.setValue("display/resolution", m_resolutionCombo->currentIndex());
        // settings.setValue("display/fullscreen", m_fullscreenCheck->isChecked());
        // settings.setValue("game/showGrid", m_gridCheck->isChecked());
        // settings.setValue("game/autoPause", m_autoPauseCheck->isChecked());

        // 保存成功提示
        m_btnSave->setText("✅ 已保存！");
        QTimer::singleShot(2000, this, [this]() {
            m_btnSave->setText("💾 保存设置");
        });
    });
}
