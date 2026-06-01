#include "ui/SettingsPage.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QPainter>
#include <QPaintEvent>
#include <QTimer>
#include <QVBoxLayout>

namespace {

QString groupStyle()
{
    return
        "QGroupBox {"
        "  color: #3B281C; font-size: 16px; font-weight: bold;"
        "  border: 2px solid rgba(74,48,32,0.72); border-radius: 8px;"
        "  background-color: rgba(250,225,172,0.90);"
        "  margin-top: 15px; padding-top: 20px;"
        "}"
        "QGroupBox::title { subcontrol-origin: margin; left: 15px; padding: 0 6px; }";
}

QString sliderStyle()
{
    return
        "QSlider::groove:horizontal { height: 6px; background: #7A5A3C; border-radius: 3px; }"
        "QSlider::handle:horizontal { width: 16px; height: 16px; margin: -5px 0;"
        "  background: #FFD27E; border: 1px solid #4C301F; border-radius: 8px; }"
        "QSlider::sub-page:horizontal { background: #9EE0C7; border-radius: 3px; }";
}

QString checkStyle()
{
    return
        "QCheckBox { color: #3B281C; font-size: 14px; spacing: 8px; }"
        "QCheckBox::indicator { width: 20px; height: 20px;"
        "  border: 2px solid rgba(74,48,32,0.55); border-radius: 4px;"
        "  background: rgba(255,244,211,0.80); }"
        "QCheckBox::indicator:checked { background-color: #9EE0C7; border: 2px solid #4C301F; }";
}

QString woodButtonStyle()
{
    return
        "QPushButton { background-color: rgba(225,176,99,0.92); color: #3A2418;"
        "  border: 2px solid rgba(76,48,31,0.82); border-radius: 10px;"
        "  font-size: 16px; font-weight: bold; }"
        "QPushButton:hover { border: 2px solid #FFD27E; }"
        "QPushButton:pressed { background-color: rgba(123,82,50,0.92); }";
}

} // namespace

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

void SettingsPage::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    static QPixmap bg(":/images/ui/scene_lab_04.png");
    if (!bg.isNull()) {
        QSize scaled = bg.size();
        scaled.scale(size(), Qt::KeepAspectRatioByExpanding);
        QRect target(QPoint((width() - scaled.width()) / 2,
                            (height() - scaled.height()) / 2), scaled);
        painter.drawPixmap(target, bg);
    } else {
        painter.fillRect(rect(), QColor(37, 30, 34));
    }
    painter.fillRect(rect(), QColor(36, 25, 22, 142));
}

void SettingsPage::initUI()
{
    setAutoFillBackground(false);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(60, 30, 60, 30);
    mainLayout->setSpacing(20);

    QHBoxLayout *topBar = new QHBoxLayout();
    m_btnBack = new QPushButton("返回", this);
    m_btnBack->setFixedSize(100, 40);
    m_btnBack->setStyleSheet(woodButtonStyle().replace("font-size: 16px", "font-size: 14px"));
    m_btnBack->setCursor(Qt::PointingHandCursor);

    m_titleLabel = new QLabel("游戏设置", this);
    m_titleLabel->setStyleSheet("color: #FFF0C8; font-size: 24px; font-weight: bold;");

    topBar->addWidget(m_btnBack);
    topBar->addStretch();
    topBar->addWidget(m_titleLabel);
    topBar->addStretch();
    mainLayout->addLayout(topBar);

    QGroupBox *audioGroup = new QGroupBox("音量设置", this);
    audioGroup->setStyleSheet(groupStyle());
    QFormLayout *audioLayout = new QFormLayout(audioGroup);
    audioLayout->setSpacing(15);
    audioLayout->setContentsMargins(20, 25, 20, 15);

    m_bgmSlider = new QSlider(Qt::Horizontal, this);
    m_bgmSlider->setRange(0, 100);
    m_bgmSlider->setValue(70);
    m_bgmSlider->setFixedHeight(25);
    m_bgmSlider->setStyleSheet(sliderStyle());

    m_bgmValueLabel = new QLabel("70%", this);
    m_bgmValueLabel->setFixedWidth(50);
    m_bgmValueLabel->setAlignment(Qt::AlignCenter);
    m_bgmValueLabel->setStyleSheet("color: #3B281C; font-size: 14px; font-weight: bold;");

    QHBoxLayout *bgmLayout = new QHBoxLayout();
    bgmLayout->addWidget(m_bgmSlider, 1);
    bgmLayout->addWidget(m_bgmValueLabel);
    audioLayout->addRow("背景音乐:", bgmLayout);

    m_sfxSlider = new QSlider(Qt::Horizontal, this);
    m_sfxSlider->setRange(0, 100);
    m_sfxSlider->setValue(85);
    m_sfxSlider->setFixedHeight(25);
    m_sfxSlider->setStyleSheet(sliderStyle());

    m_sfxValueLabel = new QLabel("85%", this);
    m_sfxValueLabel->setFixedWidth(50);
    m_sfxValueLabel->setAlignment(Qt::AlignCenter);
    m_sfxValueLabel->setStyleSheet("color: #3B281C; font-size: 14px; font-weight: bold;");

    QHBoxLayout *sfxLayout = new QHBoxLayout();
    sfxLayout->addWidget(m_sfxSlider, 1);
    sfxLayout->addWidget(m_sfxValueLabel);
    audioLayout->addRow("音效:", sfxLayout);
    mainLayout->addWidget(audioGroup);

    QGroupBox *displayGroup = new QGroupBox("显示设置", this);
    displayGroup->setStyleSheet(groupStyle());
    QFormLayout *displayLayout = new QFormLayout(displayGroup);
    displayLayout->setSpacing(15);
    displayLayout->setContentsMargins(20, 25, 20, 15);

    m_resolutionCombo = new QComboBox(this);
    m_resolutionCombo->addItems({"1280 x 720 (720p)", "1600 x 900 (900p)", "1920 x 1080 (1080p)", "2560 x 1440 (2K)"});
    m_resolutionCombo->setFixedHeight(38);
    m_resolutionCombo->setStyleSheet(
        "QComboBox { background-color: rgba(255,244,211,0.78); color: #3B281C;"
        "  border: 2px solid rgba(74,48,32,0.55); border-radius: 8px;"
        "  padding: 5px 12px; font-size: 14px; }"
        "QComboBox::drop-down { border: none; width: 25px; }"
        "QComboBox QAbstractItemView { background-color: #F5DCAD; color: #3B281C;"
        "  selection-background-color: rgba(158,224,199,0.55); border: 1px solid #4C301F; }"
    );
    displayLayout->addRow("分辨率:", m_resolutionCombo);

    m_fullscreenCheck = new QCheckBox("启用全屏模式", this);
    m_fullscreenCheck->setStyleSheet(checkStyle());
    displayLayout->addRow("全屏:", m_fullscreenCheck);
    mainLayout->addWidget(displayGroup);

    QGroupBox *gameGroup = new QGroupBox("游戏设置", this);
    gameGroup->setStyleSheet(groupStyle());
    QFormLayout *gameLayout = new QFormLayout(gameGroup);
    gameLayout->setSpacing(15);
    gameLayout->setContentsMargins(20, 25, 20, 15);

    m_gridCheck = new QCheckBox("在战斗中显示地图网格线", this);
    m_gridCheck->setChecked(true);
    m_gridCheck->setStyleSheet(checkStyle());
    gameLayout->addRow("显示网格:", m_gridCheck);

    m_autoPauseCheck = new QCheckBox("失去焦点时自动暂停游戏", this);
    m_autoPauseCheck->setChecked(true);
    m_autoPauseCheck->setStyleSheet(checkStyle());
    gameLayout->addRow("自动暂停:", m_autoPauseCheck);
    mainLayout->addWidget(gameGroup);

    m_btnSave = new QPushButton("保存设置", this);
    m_btnSave->setFixedSize(200, 50);
    m_btnSave->setStyleSheet(woodButtonStyle());
    m_btnSave->setCursor(Qt::PointingHandCursor);

    QHBoxLayout *saveLayout = new QHBoxLayout();
    saveLayout->addStretch();
    saveLayout->addWidget(m_btnSave);
    saveLayout->addStretch();
    mainLayout->addLayout(saveLayout);
    mainLayout->addStretch();

    this->setStyleSheet("SettingsPage { background: transparent; }");
}

void SettingsPage::connectSignals()
{
    connect(m_btnBack, &QPushButton::clicked, this, &SettingsPage::signalBack);

    connect(m_bgmSlider, &QSlider::valueChanged, this, [this](int value) {
        m_bgmValueLabel->setText(QString("%1%").arg(value));
        emit signalVolumeChanged(value, m_sfxSlider->value());
    });

    connect(m_sfxSlider, &QSlider::valueChanged, this, [this](int value) {
        m_sfxValueLabel->setText(QString("%1%").arg(value));
        emit signalVolumeChanged(m_bgmSlider->value(), value);
    });

    connect(m_gridCheck, &QCheckBox::checkStateChanged, this, [this](Qt::CheckState state) {
        emit signalShowGridChanged(state == Qt::Checked);
    });

    connect(m_btnSave, &QPushButton::clicked, this, [this]() {
        m_btnSave->setText("已保存");
        QTimer::singleShot(2000, this, [this]() {
            m_btnSave->setText("保存设置");
        });
    });
}
