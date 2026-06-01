/**
 * @file StartPage.h
 * @brief 起始页面头文件 —— 游戏的主菜单入口
 */

#ifndef STARTPAGE_H
#define STARTPAGE_H

#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QRectF>
#include <QTimer>
#include <QVector>

#include "ui/TechButton.h"

class ParticleWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ParticleWidget(QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    struct Particle {
        qreal x, y;
        qreal vx, vy;
        qreal size;
        qreal opacity;
        QColor color;
    };

    QVector<Particle> m_particles;
    QTimer *m_timer;
    int m_frame;

    void initParticles();
};

class StartImageButton;

class StartPage : public QWidget
{
    Q_OBJECT

public:
    explicit StartPage(QWidget *parent = nullptr);
    void playEnterAnimation();

signals:
    void signalPveClicked();
    void signalPvpClicked();
    void signalAtlasClicked();
    void signalSettingsClicked();
    void signalExitClicked();

protected:
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    bool        m_splashActive;
    QWidget    *m_menuLayer;
    ParticleWidget *m_particles;
    QLabel      *m_pressHint;
    QLabel      *m_clickHint;
    QRectF      m_demoCanvasRect;
    QVector<StartImageButton*> m_demoButtons;
    QLabel      *m_titleLabel;
    QLabel      *m_subtitleLabel;
    TechButton  *m_btnPve;
    TechButton  *m_btnPvp;
    TechButton  *m_btnAtlas;
    TechButton  *m_btnSettings;
    TechButton  *m_btnExit;

    void initUI();
    void revealMenu();
    void updateDemoLayout();
    void handleDemoButton(const QString &id, const QString &title);
    void showDemoHint(const QString &text);
    TechButton* createMenuButton(const QString &text, const QString &icon,
                                  const QColor &accent = QColor(0, 212, 255));
};

#endif // STARTPAGE_H
