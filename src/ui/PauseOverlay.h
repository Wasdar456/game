#ifndef PAUSEOVERLAY_H
#define PAUSEOVERLAY_H

#include <QWidget>

class QLabel;
class QPushButton;

class PauseOverlay final : public QWidget
{
    Q_OBJECT

public:
    explicit PauseOverlay(QWidget *parent = nullptr);
    void setPvpMode(bool pvp);
    void open();
    void closeMenu();

signals:
    void signalResume();
    void signalSettings();
    void signalRestart();
    void signalExitToLobby();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QWidget *m_panel;
    QLabel *m_modeLabel;
    QPushButton *m_resumeButton;
    QPushButton *m_settingsButton;
    QPushButton *m_restartButton;
    QPushButton *m_exitButton;
    qreal m_phase;
};

#endif // PAUSEOVERLAY_H
