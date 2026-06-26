#include "ui/SettingsPage.h"

#include "ui/CardCollection.h"

#include <QCheckBox>
#include <QComboBox>
#include <QEvent>
#include <QGuiApplication>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QMessageBox>
#include <QPainter>
#include <QPaintEvent>
#include <QScreen>
#include <QSettings>
#include <QSizePolicy>
#include <QTimer>
#include <QVBoxLayout>

namespace {

QString groupStyle()
{
    return
        "QGroupBox {"
        "  color: #2F251A; font-size: 17px; font-weight: 900;"
        "  border: 2px solid rgba(92,68,42,0.78); border-radius: 12px;"
        "  background-color: rgba(248,232,184,0.93);"
        "  margin-top: 12px; padding-top: 14px;"
        "}"
        "QGroupBox::title { subcontrol-origin: margin; left: 18px; padding: 0 8px;"
        "  background-color: rgba(255,236,178,0.92); border-radius: 7px; }";
}

QString sliderStyle()
{
    return
        "QSlider::groove:horizontal { height: 8px; background: rgba(98,75,52,0.74); border-radius: 4px; }"
        "QSlider::handle:horizontal { width: 20px; height: 20px; margin: -6px 0;"
        "  background: #FFE08F; border: 2px solid #4C301F; border-radius: 10px; }"
        "QSlider::handle:horizontal:hover { background: #FFF0B8; }"
        "QSlider::sub-page:horizontal { background: #78C7A6; border-radius: 4px; }";
}

QString checkStyle()
{
    return
        "QCheckBox { color: #3B281C; font-size: 15px; font-weight: 700; spacing: 9px; }"
        "QCheckBox::indicator { width: 20px; height: 20px;"
        "  border: 2px solid rgba(74,48,32,0.55); border-radius: 4px;"
        "  background: rgba(255,244,211,0.80); }"
        "QCheckBox::indicator:checked { background-color: #78C7A6; border: 2px solid #4C301F; }";
}

QString woodButtonStyle()
{
    return
        "QPushButton { background-color: rgba(230,178,88,0.95); color: #321F15;"
        "  border: 2px solid rgba(76,48,31,0.86); border-radius: 12px;"
        "  font-size: 16px; font-weight: bold; }"
        "QPushButton:hover { border: 2px solid #FFF0B8; background-color: rgba(242,194,105,0.98); }"
        "QPushButton:pressed { background-color: rgba(124,86,52,0.94); color: #FFF0C8; }";
}

QString comboStyle()
{
    return
        "QComboBox { background-color: rgba(255,247,219,0.90); color: #3B281C;"
        "  border: 2px solid rgba(74,48,32,0.55); border-radius: 9px;"
        "  padding: 6px 12px; font-size: 14px; font-weight: 700; }"
        "QComboBox:hover { border-color: rgba(120,199,166,0.92); }"
        "QComboBox::drop-down { border: none; width: 26px; }"
        "QComboBox QAbstractItemView { background-color: #F5DCAD; color: #3B281C;"
        "  selection-background-color: rgba(120,199,166,0.62); border: 1px solid #4C301F; }";
}

QString valueLabelStyle()
{
    return
        "QLabel { color: #2E2117; font-size: 14px; font-weight: 900;"
        "  background-color: rgba(255,247,219,0.76); border: 1px solid rgba(74,48,32,0.36);"
        "  border-radius: 8px; padding: 4px 6px; }";
}

QLabel* makeHint(const QString& text, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setWordWrap(true);
    label->setMinimumHeight(22);
    label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    label->setStyleSheet(
        "QLabel { color: #60452A; font-size: 11px; font-weight: 700;"
        "  background: transparent; border: none; padding: 2px 0; }"
    );
    return label;
}

QLabel* makeRowLabel(const QString& text, QWidget *parent, int rowHeight)
{
    auto *label = new QLabel(text, parent);
    label->setFixedSize(132, rowHeight);
    label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    label->setStyleSheet(
        "QLabel { color: #3A2A1D; font-size: 14px; font-weight: 900;"
        "  background: transparent; }"
    );
    return label;
}

QWidget* makeSettingRow(const QString& labelText, QWidget *control, QWidget *parent, QWidget *valueLabel = nullptr, int rowHeight = 34)
{
    auto *rowWidget = new QWidget(parent);
    rowWidget->setFixedHeight(rowHeight);
    rowWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    auto *row = new QHBoxLayout(rowWidget);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(16);
    row->addWidget(makeRowLabel(labelText, rowWidget, rowHeight));
    row->addWidget(control, 0, Qt::AlignVCenter);
    if (valueLabel) {
        row->addWidget(valueLabel, 0, Qt::AlignVCenter);
    }
    row->addStretch();
    return rowWidget;
}

QSize availableScreenSize(const QWidget *widget)
{
    QScreen *screen = widget ? widget->screen() : nullptr;
    if (!screen) {
        screen = QGuiApplication::primaryScreen();
    }
    return screen ? screen->availableGeometry().size() : QSize(2560, 1440);
}

bool fitsAvailableScreen(const QSize& size, const QSize& available)
{
    return size.width() <= available.width() && size.height() <= available.height();
}

QSize clampToAvailableScreen(QSize size, const QSize& available)
{
    const QSize minimum(1280, 720);
    if (!available.isValid()) return size;

    size.setWidth(qMax(minimum.width(), qMin(size.width(), available.width())));
    size.setHeight(qMax(minimum.height(), qMin(size.height(), available.height())));
    return size;
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
    , m_btnResetProgress(nullptr)
    , m_statusLabel(nullptr)
    , m_easterEggLabel(nullptr)
{
    initUI();
    setFocusPolicy(Qt::StrongFocus);

    QSettings settings;
    m_bgmSlider->setValue(settings.value("audio/bgm", 70).toInt());
    m_sfxSlider->setValue(settings.value("audio/sfx", 85).toInt());
    const QSize savedSize = settings.value("display/resolution", QSize(1280, 720)).toSize();
    const int resolutionIndex = m_resolutionCombo->findData(savedSize);
    m_resolutionCombo->setCurrentIndex(resolutionIndex >= 0 ? resolutionIndex : 0);
    m_fullscreenCheck->setChecked(settings.value("display/fullscreen", false).toBool());
    m_gridCheck->setChecked(settings.value("game/showGrid", true).toBool());
    m_autoPauseCheck->setChecked(settings.value("game/autoPause", true).toBool());

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
    painter.fillRect(rect(), QColor(26, 22, 18, 128));

    QLinearGradient glow(0, 0, width(), height());
    glow.setColorAt(0.0, QColor(126, 199, 166, 42));
    glow.setColorAt(0.48, QColor(255, 219, 137, 28));
    glow.setColorAt(1.0, QColor(43, 34, 27, 74));
    painter.fillRect(rect(), glow);
}

void SettingsPage::initUI()
{
    setAutoFillBackground(false);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(60, 18, 60, 18);
    mainLayout->setSpacing(8);

    QHBoxLayout *topBar = new QHBoxLayout();
    m_btnBack = new QPushButton("返回", this);
    m_btnBack->setFixedSize(96, 36);
    m_btnBack->setStyleSheet(woodButtonStyle().replace("font-size: 16px", "font-size: 14px"));
    m_btnBack->setCursor(Qt::PointingHandCursor);

    QVBoxLayout *titleStack = new QVBoxLayout();
    titleStack->setSpacing(4);

    m_titleLabel = new QLabel("Juice Lab Settings", this);
    m_titleLabel->setAlignment(Qt::AlignCenter);
    m_titleLabel->setStyleSheet(
        "QLabel { color: #FFF1C8; font-size: 24px; font-weight: 900;"
        "  letter-spacing: 0px; background: transparent; }"
    );

    QLabel *subtitle = new QLabel("音量和网格立即生效；显示设置点击保存后应用", this);
    subtitle->setAlignment(Qt::AlignCenter);
    subtitle->setStyleSheet(
        "QLabel { color: rgba(255,241,200,0.88); font-size: 13px; font-weight: 700;"
        "  background: transparent; }"
    );
    titleStack->addWidget(m_titleLabel);
    titleStack->addWidget(subtitle);

    topBar->addWidget(m_btnBack);
    topBar->addStretch();
    topBar->addLayout(titleStack);
    topBar->addStretch();
    topBar->addSpacing(96);
    mainLayout->addLayout(topBar);

    QGroupBox *audioGroup = new QGroupBox("Audio Bench", this);
    audioGroup->setStyleSheet(groupStyle());
    QVBoxLayout *audioLayout = new QVBoxLayout(audioGroup);
    audioLayout->setSpacing(5);
    audioLayout->setContentsMargins(22, 14, 22, 10);
    audioLayout->addWidget(makeHint("拖动滑块会立即改变音量；点击保存后下次启动继续使用。", audioGroup));

    m_bgmSlider = new QSlider(Qt::Horizontal, this);
    m_bgmSlider->setRange(0, 100);
    m_bgmSlider->setValue(70);
    m_bgmSlider->setFixedHeight(30);
    m_bgmSlider->setMinimumWidth(320);
    m_bgmSlider->setMaximumWidth(520);
    m_bgmSlider->setStyleSheet(sliderStyle());

    m_bgmValueLabel = new QLabel("70%", this);
    m_bgmValueLabel->setFixedSize(62, 30);
    m_bgmValueLabel->setAlignment(Qt::AlignCenter);
    m_bgmValueLabel->setStyleSheet(valueLabelStyle());

    audioLayout->addWidget(makeSettingRow("BGM:", m_bgmSlider, audioGroup, m_bgmValueLabel));

    m_sfxSlider = new QSlider(Qt::Horizontal, this);
    m_sfxSlider->setRange(0, 100);
    m_sfxSlider->setValue(85);
    m_sfxSlider->setFixedHeight(30);
    m_sfxSlider->setMinimumWidth(320);
    m_sfxSlider->setMaximumWidth(520);
    m_sfxSlider->setStyleSheet(sliderStyle());

    m_sfxValueLabel = new QLabel("85%", this);
    m_sfxValueLabel->setFixedSize(62, 30);
    m_sfxValueLabel->setAlignment(Qt::AlignCenter);
    m_sfxValueLabel->setStyleSheet(valueLabelStyle());

    audioLayout->addWidget(makeSettingRow("SFX:", m_sfxSlider, audioGroup, m_sfxValueLabel));
    mainLayout->addWidget(audioGroup);

    QGroupBox *displayGroup = new QGroupBox("Display Console", this);
    displayGroup->setStyleSheet(groupStyle());
    QVBoxLayout *displayLayout = new QVBoxLayout(displayGroup);
    displayLayout->setSpacing(5);
    displayLayout->setContentsMargins(22, 14, 22, 10);
    displayLayout->addWidget(makeHint("分辨率和全屏会在点击保存后应用。", displayGroup));

    m_resolutionCombo = new QComboBox(this);
    const QSize availableSize = availableScreenSize(this);
    const QVector<QPair<QString, QSize>> resolutions = {
        {"1280 x 720 (720p)", QSize(1280, 720)},
        {"1600 x 900 (900p)", QSize(1600, 900)},
        {"1920 x 1080 (1080p)", QSize(1920, 1080)},
        {"2560 x 1440 (2K)", QSize(2560, 1440)}
    };
    for (const auto& option : resolutions) {
        if (fitsAvailableScreen(option.second, availableSize)) {
            m_resolutionCombo->addItem(option.first, option.second);
        }
    }
    if (m_resolutionCombo->count() == 0) {
        m_resolutionCombo->addItem("1280 x 720 (720p)", QSize(1280, 720));
    }
    m_resolutionCombo->setFixedSize(320, 34);
    m_resolutionCombo->setStyleSheet(comboStyle());
    displayLayout->addWidget(makeSettingRow("分辨率:", m_resolutionCombo, displayGroup));

    m_fullscreenCheck = new QCheckBox("启用全屏模式", this);
    m_fullscreenCheck->setFixedHeight(30);
    m_fullscreenCheck->setMinimumWidth(240);
    m_fullscreenCheck->setStyleSheet(checkStyle());
    displayLayout->addWidget(makeSettingRow("全屏:", m_fullscreenCheck, displayGroup));
    mainLayout->addWidget(displayGroup);

    QGroupBox *gameGroup = new QGroupBox("Field Rules", this);
    gameGroup->setStyleSheet(groupStyle());
    QVBoxLayout *gameLayout = new QVBoxLayout(gameGroup);
    gameLayout->setSpacing(5);
    gameLayout->setContentsMargins(22, 14, 22, 10);
    gameLayout->addWidget(makeHint("网格开关会立即同步到部署和战斗页面；自动暂停保存后生效。", gameGroup));

    m_gridCheck = new QCheckBox("在战斗中显示地图网格线", this);
    m_gridCheck->setChecked(true);
    m_gridCheck->setFixedHeight(30);
    m_gridCheck->setMinimumWidth(300);
    m_gridCheck->setStyleSheet(checkStyle());
    gameLayout->addWidget(makeSettingRow("显示网格:", m_gridCheck, gameGroup));

    m_autoPauseCheck = new QCheckBox("失去焦点时自动暂停游戏", this);
    m_autoPauseCheck->setChecked(true);
    m_autoPauseCheck->setFixedHeight(30);
    m_autoPauseCheck->setMinimumWidth(300);
    m_autoPauseCheck->setStyleSheet(checkStyle());
    gameLayout->addWidget(makeSettingRow("自动暂停:", m_autoPauseCheck, gameGroup));
    mainLayout->addWidget(gameGroup);

    m_btnSave = new QPushButton("保存设置", this);
    m_btnSave->setFixedSize(180, 42);
    m_btnSave->setStyleSheet(woodButtonStyle());
    m_btnSave->setCursor(Qt::PointingHandCursor);

    m_btnResetProgress = new QPushButton("重置教学与抽卡进度", this);
    m_btnResetProgress->setFixedSize(230, 42);
    m_btnResetProgress->setStyleSheet(woodButtonStyle().replace("font-size: 16px", "font-size: 14px"));
    m_btnResetProgress->setCursor(Qt::PointingHandCursor);
    m_btnResetProgress->setToolTip("清空教学进度、抽卡票、已解锁卡、碎片和卡牌等级。");

    m_statusLabel = new QLabel("设置已加载。音量和网格可以即时预览。", this);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setStyleSheet(
        "QLabel { color: #FFF0C8; font-size: 13px; font-weight: 800;"
        "  background-color: rgba(42,31,25,0.54); border: 1px solid rgba(255,224,143,0.38);"
        "  border-radius: 10px; padding: 5px 12px; }"
    );

    m_easterEggLabel = new QLabel("Juice Lab Debug Channel Unlocked", this);
    m_easterEggLabel->setAlignment(Qt::AlignCenter);
    m_easterEggLabel->setStyleSheet(
        "QLabel { color: #1F2D20; font-size: 15px; font-weight: 900;"
        "  background-color: rgba(161,229,186,0.94); border: 2px solid rgba(255,240,184,0.92);"
        "  border-radius: 12px; padding: 10px 18px; }"
    );
    m_easterEggLabel->hide();

    QHBoxLayout *saveLayout = new QHBoxLayout();
    saveLayout->addStretch();
    saveLayout->addWidget(m_btnResetProgress);
    saveLayout->addSpacing(18);
    saveLayout->addWidget(m_btnSave);
    saveLayout->addStretch();
    mainLayout->addLayout(saveLayout);
    mainLayout->addWidget(m_statusLabel);
    mainLayout->addWidget(m_easterEggLabel);

    this->setStyleSheet("SettingsPage { background: transparent; }");

    for (QObject *target : {
             static_cast<QObject*>(m_btnBack),
             static_cast<QObject*>(m_bgmSlider),
             static_cast<QObject*>(m_sfxSlider),
             static_cast<QObject*>(m_resolutionCombo),
             static_cast<QObject*>(m_fullscreenCheck),
             static_cast<QObject*>(m_gridCheck),
             static_cast<QObject*>(m_autoPauseCheck),
             static_cast<QObject*>(m_btnResetProgress),
             static_cast<QObject*>(m_btnSave)
         }) {
        target->installEventFilter(this);
    }
}

void SettingsPage::connectSignals()
{
    connect(m_btnBack, &QPushButton::clicked, this, [this]() {
        emit signalBack();
    });

    connect(m_bgmSlider, &QSlider::valueChanged, this, [this](int value) {
        m_bgmValueLabel->setText(QString("%1%").arg(value));
        emit signalVolumeChanged(value, m_sfxSlider->value());
        showStatusMessage("背景音乐已即时应用，点击保存后持久生效。");
    });

    connect(m_sfxSlider, &QSlider::valueChanged, this, [this](int value) {
        m_sfxValueLabel->setText(QString("%1%").arg(value));
        emit signalVolumeChanged(m_bgmSlider->value(), value);
        showStatusMessage("音效音量已即时应用，点击保存后持久生效。");
    });

    connect(m_gridCheck, &QCheckBox::checkStateChanged, this, [this](Qt::CheckState state) {
        emit signalShowGridChanged(state == Qt::Checked);
        showStatusMessage(state == Qt::Checked
                              ? "地图网格已打开。"
                              : "地图网格已关闭。");
    });

    connect(m_resolutionCombo, &QComboBox::currentIndexChanged, this, [this]() {
        showStatusMessage("显示设置会在点击保存后应用。");
    });

    connect(m_fullscreenCheck, &QCheckBox::checkStateChanged, this, [this]() {
        showStatusMessage("全屏设置会在点击保存后应用。");
    });

    connect(m_autoPauseCheck, &QCheckBox::checkStateChanged, this, [this]() {
        showStatusMessage("自动暂停设置会在点击保存后生效。");
    });

    connect(m_btnResetProgress, &QPushButton::clicked, this, [this]() {
        const auto answer = QMessageBox::question(
            this,
            "确认重置",
            "这会重置教学进度、抽卡票、已解锁卡、碎片和卡牌等级。确定继续吗？",
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);
        if (answer != QMessageBox::Yes) {
            showStatusMessage("已取消重置。");
            return;
        }

        QSettings settings;
        settings.remove("tutorial");
        CardCollection::resetToDefaults();
        settings.sync();

        m_btnResetProgress->setText("重置完成");
        showStatusMessage("教学与抽卡进度已重置。");
        QTimer::singleShot(1800, this, [this]() {
            m_btnResetProgress->setText("重置教学与抽卡进度");
        });
    });

    connect(m_btnSave, &QPushButton::clicked, this, [this]() {
        QSettings settings;
        settings.setValue("audio/bgm", m_bgmSlider->value());
        settings.setValue("audio/sfx", m_sfxSlider->value());
        const QSize requestedSize = m_resolutionCombo->currentData().toSize();
        const QSize safeSize = clampToAvailableScreen(requestedSize, availableScreenSize(this));
        settings.setValue("display/resolution", safeSize);
        settings.setValue("display/fullscreen", m_fullscreenCheck->isChecked());
        settings.setValue("game/showGrid", m_gridCheck->isChecked());
        settings.setValue("game/autoPause", m_autoPauseCheck->isChecked());
        settings.sync();

        emit signalVolumeChanged(m_bgmSlider->value(), m_sfxSlider->value());
        emit signalShowGridChanged(m_gridCheck->isChecked());

        QWidget *top = window();
        if (m_fullscreenCheck->isChecked()) {
            top->showFullScreen();
        } else {
            top->showNormal();
            top->resize(safeSize);
        }
        m_btnSave->setText("已保存");
        showStatusMessage("设置已保存并应用。");
        QTimer::singleShot(2000, this, [this]() {
            m_btnSave->setText("保存设置");
        });
    });
}

void SettingsPage::keyPressEvent(QKeyEvent *event)
{
    handleSecretKey(event->key());
    QWidget::keyPressEvent(event);
}

bool SettingsPage::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::KeyPress) {
        auto *keyEvent = static_cast<QKeyEvent*>(event);
        handleSecretKey(keyEvent->key());
    }
    return QWidget::eventFilter(watched, event);
}

void SettingsPage::showStatusMessage(const QString& message)
{
    if (!m_statusLabel) return;
    m_statusLabel->setText(message);
}

void SettingsPage::showEasterEgg()
{
    if (!m_easterEggLabel) return;
    m_easterEggLabel->show();
    m_easterEggLabel->raise();
    showStatusMessage("隐藏频道已开启：这只是观赏彩蛋，不会改动存档或数值。");
    QTimer::singleShot(4200, this, [this]() {
        if (m_easterEggLabel) m_easterEggLabel->hide();
    });
}

void SettingsPage::handleSecretKey(int key)
{
    static const QVector<int> secret = {
        Qt::Key_Up, Qt::Key_Up, Qt::Key_Down, Qt::Key_Down,
        Qt::Key_Left, Qt::Key_Right, Qt::Key_Left, Qt::Key_Right,
        Qt::Key_B, Qt::Key_A
    };

    m_keySequence.append(key);
    while (!m_keySequence.isEmpty()) {
        bool matches = true;
        for (int i = 0; i < m_keySequence.size(); ++i) {
            if (i >= secret.size() || m_keySequence[i] != secret[i]) {
                matches = false;
                break;
            }
        }
        if (matches) break;
        m_keySequence.removeFirst();
    }

    if (m_keySequence.size() == secret.size()) {
        m_keySequence.clear();
        showEasterEgg();
    }
}
