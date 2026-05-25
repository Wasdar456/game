/**
 * @file SettingsPage.h
 * @brief 设置页面头文件 —— 音量、分辨率等系统设置
 *
 * 布局：
 *   🔊 音量设置 → 背景音乐滑块 + 音效滑块
 *   🖥️ 显示设置 → 分辨率下拉 + 全屏复选框
 *   🎮 游戏设置 → 显示网格 + 自动暂停复选框
 *   [保存设置] 按钮
 *
 * 设计说明：
 *   - 设置页是纯 UI 页面，不直接访问 BattleManager
 *   - 设置值保存使用 QSettings（后续实现）
 *   - 部分设置（如显示网格）需在战斗页生效
 */

#ifndef SETTINGSPAGE_H
#define SETTINGSPAGE_H

#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QSlider>
#include <QComboBox>
#include <QCheckBox>

class SettingsPage : public QWidget
{
    Q_OBJECT

public:
    explicit SettingsPage(QWidget *parent = nullptr);

signals:
    void signalBack();  ///< 返回起始页

    // ========== 设置变更信号 ==========
    // 当用户修改设置时发出，其他页面可监听这些信号
    void signalShowGridChanged(bool show);    ///< 网格显示开关变更
    void signalVolumeChanged(int bgm, int sfx); ///< 音量变更

private:
    // 导航组件
    QPushButton *m_btnBack;
    QLabel      *m_titleLabel;

    // 音量设置
    QSlider *m_bgmSlider;
    QLabel  *m_bgmValueLabel;
    QSlider *m_sfxSlider;
    QLabel  *m_sfxValueLabel;

    // 显示设置
    QComboBox *m_resolutionCombo;
    QCheckBox *m_fullscreenCheck;

    // 游戏设置
    QCheckBox *m_gridCheck;
    QCheckBox *m_autoPauseCheck;

    // 操作按钮
    QPushButton *m_btnSave;

    void initUI();
    void connectSignals();
};

#endif // SETTINGSPAGE_H
