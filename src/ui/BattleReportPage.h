#ifndef BATTLEREPORTPAGE_H
#define BATTLEREPORTPAGE_H

#include "ui/BattleResult.h"

#include <QPushButton>
#include <QSlider>
#include <QTextEdit>
#include <QTimer>
#include <QWidget>
#include <functional>

class ReplayCanvas : public QWidget
{
public:
    enum class HeatMode {
        Deaths,
        Deployments
    };

    explicit ReplayCanvas(QWidget *parent = nullptr);
    void setData(const BattleReplayData& data);
    void setFrameIndex(int index);
    void setHeatMode(HeatMode mode);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    BattleReplayData m_data;
    int m_frameIndex = 0;
    HeatMode m_heatMode = HeatMode::Deaths;
};

class BattleReportPage : public QWidget
{
public:
    explicit BattleReportPage(QWidget *parent = nullptr);
    void setResult(const BattleResult& result);
    void setNavigationHandlers(std::function<void()> backHandler,
                               std::function<void()> lobbyHandler);

protected:
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    BattleResult m_result;
    ReplayCanvas *m_canvas = nullptr;
    QTextEdit *m_statsText = nullptr;
    QSlider *m_timeline = nullptr;
    QPushButton *m_playButton = nullptr;
    QPushButton *m_speedButton = nullptr;
    QPushButton *m_deathHeatButton = nullptr;
    QPushButton *m_deployHeatButton = nullptr;
    QPushButton *m_exportButton = nullptr;
    QPushButton *m_backButton = nullptr;
    QPushButton *m_lobbyButton = nullptr;
    QTimer m_playTimer;
    std::function<void()> m_backHandler;
    std::function<void()> m_lobbyHandler;
    bool m_playing = false;
    double m_speed = 1.0;
    double m_playbackTime = 0.0;

    void initUI();
    void updateLayout();
    void updateStatsText();
    void setFrameIndex(int index);
    void tickPlayback();
    void exportReport();
    int frameIndexForTime(double seconds) const;
};

#endif // BATTLEREPORTPAGE_H
