#ifndef RESULTPAGE_H
#define RESULTPAGE_H

#include "ui/BattleResult.h"

#include <QRectF>
#include <QTimer>
#include <QWidget>

class QLabel;
class QPushButton;
class QResizeEvent;
class QShowEvent;

class ResultPage : public QWidget
{
    Q_OBJECT

public:
    explicit ResultPage(QWidget *parent = nullptr);
    void setResult(const BattleResult &result);
    const BattleResult& result() const { return m_result; }

signals:
    void signalReplay();
    void signalReport();
    void signalReturnToLobby();

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    BattleResult m_result;
    QRectF m_boardRect;
    QLabel *m_resultLabel;
    QLabel *m_modeLabel;
    QLabel *m_sideLabel;
    QLabel *m_waveValue;
    QLabel *m_killValue;
    QLabel *m_coreValue;
    QLabel *m_escapeValue;
    QLabel *m_messageLabel;
    QPushButton *m_replayButton;
    QPushButton *m_lobbyButton;
    QPushButton *m_reportButton;
    QTimer m_animationTimer;
    qreal m_phase;

    void initUI();
    void updateContent();
    void updateLayout();
    QString buttonStyle(bool primary) const;
};

#endif // RESULTPAGE_H
