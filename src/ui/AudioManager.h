#ifndef AUDIOMANAGER_H
#define AUDIOMANAGER_H

#include <QAudioFormat>
#include <QObject>
#include <QString>
#include <QVector>

class QAudioOutput;
class QAudioSink;
class QIODevice;
class QMediaPlayer;

class AudioManager final : public QObject
{
    Q_OBJECT

public:
    struct BgmTrack {
        QString id;
        QString displayName;
        QString sourceUrl;
    };

    enum class Scene {
        Menu,
        Battle
    };

    static AudioManager& instance();

    void initialize();
    void setVolumes(int bgmPercent, int sfxPercent);
    void setScene(Scene scene);
    void setBgmTrack(const QString& trackId);
    QString currentBgmTrack() const { return m_bgmTrackId; }
    static QVector<BgmTrack> availableBgmTracks();

    void playWoodClick();
    void playCardSelect();
    void playDeploy();
    void playAttack();
    void playHit();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    explicit AudioManager(QObject *parent = nullptr);
    void playTone(double frequency, int durationMs, double decay,
                  double noiseAmount, double pitchDrop = 0.0);

    QAudioFormat m_format;
    QAudioSink *m_ambientSink;
    QIODevice *m_ambientDevice;
    QMediaPlayer *m_bgmPlayer;
    QAudioOutput *m_bgmOutput;
    QString m_bgmTrackId;
    qreal m_bgmVolume;
    qreal m_sfxVolume;
    Scene m_scene;
    bool m_initialized;
    qint64 m_lastClickMs;
    qint64 m_lastAttackMs;
    qint64 m_lastDeployMs;
    qint64 m_lastHitMs;
};

#endif // AUDIOMANAGER_H
