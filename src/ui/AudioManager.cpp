#include "ui/AudioManager.h"

#include <QAbstractButton>
#include <QApplication>
#include <QAudioDevice>
#include <QAudioSink>
#include <QDateTime>
#include <QEvent>
#include <QMediaDevices>
#include <QMouseEvent>
#include <QMutex>
#include <QMutexLocker>
#include <QVector>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>

namespace {

constexpr double Pi = 3.14159265358979323846;

void writeSample(char *&cursor, QAudioFormat::SampleFormat format, double value)
{
    value = std::clamp(value, -1.0, 1.0);
    switch (format) {
    case QAudioFormat::UInt8: {
        const quint8 sample = quint8((value * 0.5 + 0.5) * 255.0);
        *cursor++ = char(sample);
        break;
    }
    case QAudioFormat::Int16: {
        const qint16 sample = qint16(value * 32767.0);
        memcpy(cursor, &sample, sizeof(sample));
        cursor += sizeof(sample);
        break;
    }
    case QAudioFormat::Int32: {
        const qint32 sample = qint32(value * 2147483647.0);
        memcpy(cursor, &sample, sizeof(sample));
        cursor += sizeof(sample);
        break;
    }
    case QAudioFormat::Float: {
        const float sample = float(value);
        memcpy(cursor, &sample, sizeof(sample));
        cursor += sizeof(sample);
        break;
    }
    default:
        break;
    }
}

class MixedAudioDevice final : public QIODevice
{
public:
    MixedAudioDevice(const QAudioFormat &format, QObject *parent = nullptr)
        : QIODevice(parent)
        , m_format(format)
        , m_phase(0.0)
        , m_noiseState(0x8a31f2d7u)
    {
    }

    void setBattleMix(bool battle)
    {
        m_battleMix.store(battle, std::memory_order_relaxed);
    }

    void setVolumes(qreal bgm, qreal sfx)
    {
        m_bgmVolume.store(bgm, std::memory_order_relaxed);
        m_sfxVolume.store(sfx, std::memory_order_relaxed);
    }

    void addTone(double frequency, int durationMs, double decay,
                 double noiseAmount, double pitchDrop)
    {
        Voice voice;
        voice.frequency = frequency;
        voice.totalFrames = qMax(1, m_format.sampleRate() * durationMs / 1000);
        voice.decay = decay;
        voice.noiseAmount = noiseAmount;
        voice.pitchDrop = pitchDrop;
        voice.noiseState ^= quint32(frequency * 7919.0);

        QMutexLocker locker(&m_voiceMutex);
        if (m_voices.size() >= MaxVoices) {
            m_voices.removeFirst();
        }
        m_voices.append(voice);
    }

    qint64 readData(char *data, qint64 maxLength) override
    {
        const int channels = qMax(1, m_format.channelCount());
        const int bytesPerFrame = channels * m_format.bytesPerSample();
        const qint64 frames = maxLength / bytesPerFrame;
        char *cursor = data;
        const double sampleRate = qMax(1, m_format.sampleRate());
        const double bgmVolume = m_bgmVolume.load(std::memory_order_relaxed);
        const double sfxVolume = m_sfxVolume.load(std::memory_order_relaxed);
        const bool battleMix = m_battleMix.load(std::memory_order_relaxed);

        QVector<Voice> voices;
        {
            QMutexLocker locker(&m_voiceMutex);
            voices.swap(m_voices);
        }

        for (qint64 frame = 0; frame < frames; ++frame) {
            m_noiseState ^= m_noiseState << 13;
            m_noiseState ^= m_noiseState >> 17;
            m_noiseState ^= m_noiseState << 5;
            const double noise = (double(m_noiseState & 0xffffu) / 32767.5) - 1.0;

            const double swell = 0.5 + 0.5 * std::sin(2.0 * Pi * 0.075 * m_phase);
            m_filteredNoise += (noise - m_filteredNoise) * 0.012;
            const double ocean = (m_filteredNoise * 0.46
                                  + std::sin(2.0 * Pi * 0.18 * m_phase) * 0.10)
                                 * (0.18 + swell * 0.16);

            const double chordStep = std::fmod(m_phase, 16.0);
            const int chordIndex = int(m_phase / 16.0) % 4;
            const double roots[] = {130.81, 110.00, 146.83, 98.00};
            const double root = roots[chordIndex];
            const double fade = std::min(1.0, chordStep / 2.2)
                                * std::min(1.0, (16.0 - chordStep) / 2.5);
            const double melodyStep = std::fmod(m_phase * 1.35, 8.0);
            const int melodyIndex = int(melodyStep) % 8;
            const double menuMelody[] = {523.25, 659.25, 783.99, 659.25,
                                         587.33, 493.88, 587.33, 392.00};
            const double battleMelody[] = {392.00, 523.25, 587.33, 698.46,
                                           783.99, 698.46, 587.33, 523.25};
            const double melodyFreq = battleMix ? battleMelody[melodyIndex]
                                                : menuMelody[melodyIndex];
            const double melodyGate = std::min(1.0, std::fmod(melodyStep, 1.0) / 0.12)
                                      * std::min(1.0, (1.0 - std::fmod(melodyStep, 1.0)) / 0.25);
            const double music =
                (std::sin(2.0 * Pi * root * m_phase)
                 + 0.55 * std::sin(2.0 * Pi * root * 1.25 * m_phase)
                 + 0.38 * std::sin(2.0 * Pi * root * 1.5 * m_phase))
                * 0.070 * fade
                + std::sin(2.0 * Pi * melodyFreq * m_phase)
                * (battleMix ? 0.030 : 0.024) * melodyGate;

            const double pulse = battleMix
                ? std::sin(2.0 * Pi * 0.92 * m_phase) * 0.018
                : 0.0;
            double effects = 0.0;
            for (Voice &voice : voices) {
                if (voice.frame >= voice.totalFrames) {
                    continue;
                }
                const double normalized = double(voice.frame) / voice.totalFrames;
                const double envelope = std::exp(-voice.decay * normalized)
                                        * std::min(1.0, voice.frame / 32.0);
                const double currentFrequency =
                    voice.frequency * (1.0 - voice.pitchDrop * normalized);
                voice.phase += 2.0 * Pi * currentFrequency / sampleRate;
                voice.noiseState ^= voice.noiseState << 13;
                voice.noiseState ^= voice.noiseState >> 17;
                voice.noiseState ^= voice.noiseState << 5;
                const double voiceNoise =
                    (double(voice.noiseState & 0xffffu) / 32767.5) - 1.0;
                const double harmonic =
                    std::sin(voice.phase) + 0.32 * std::sin(voice.phase * 2.03);
                effects += (harmonic * (1.0 - voice.noiseAmount)
                            + voiceNoise * voice.noiseAmount)
                           * envelope * 0.34;
                ++voice.frame;
            }

            const double ambient = (ocean + music + pulse) * bgmVolume * 0.55;
            const double mixed = std::clamp(ambient + effects * sfxVolume * 0.48,
                                            -0.92, 0.92);
            for (int channel = 0; channel < channels; ++channel) {
                writeSample(cursor, m_format.sampleFormat(), mixed);
            }
            m_phase += 1.0 / sampleRate;
        }

        voices.erase(std::remove_if(voices.begin(), voices.end(),
                                    [](const Voice &voice) {
                                        return voice.frame >= voice.totalFrames;
                                    }),
                     voices.end());
        if (!voices.isEmpty()) {
            QMutexLocker locker(&m_voiceMutex);
            const int room = qMax(0, MaxVoices - m_voices.size());
            const int keep = qMin(room, voices.size());
            for (int i = voices.size() - keep; i < voices.size(); ++i) {
                m_voices.prepend(voices[i]);
            }
        }
        return frames * bytesPerFrame;
    }

    bool isSequential() const override
    {
        return true;
    }

    qint64 bytesAvailable() const override
    {
        return m_format.bytesForDuration(500000) + QIODevice::bytesAvailable();
    }

    qint64 writeData(const char *, qint64) override
    {
        return 0;
    }

private:
    struct Voice {
        double frequency = 0.0;
        int totalFrames = 0;
        int frame = 0;
        double decay = 0.0;
        double noiseAmount = 0.0;
        double pitchDrop = 0.0;
        double phase = 0.0;
        quint32 noiseState = 0x1472ab91u;
    };

    static constexpr int MaxVoices = 12;
    QAudioFormat m_format;
    double m_phase;
    double m_filteredNoise = 0.0;
    quint32 m_noiseState;
    std::atomic<bool> m_battleMix{false};
    std::atomic<double> m_bgmVolume{0.70};
    std::atomic<double> m_sfxVolume{0.85};
    QMutex m_voiceMutex;
    QVector<Voice> m_voices;
};

} // namespace

AudioManager& AudioManager::instance()
{
    static AudioManager manager;
    return manager;
}

AudioManager::AudioManager(QObject *parent)
    : QObject(parent)
    , m_ambientSink(nullptr)
    , m_ambientDevice(nullptr)
    , m_bgmVolume(0.70)
    , m_sfxVolume(0.85)
    , m_scene(Scene::Menu)
    , m_initialized(false)
    , m_lastClickMs(0)
    , m_lastAttackMs(0)
    , m_lastDeployMs(0)
    , m_lastHitMs(0)
{
}

void AudioManager::initialize()
{
    if (m_initialized) {
        return;
    }
    m_initialized = true;

    const QAudioDevice output = QMediaDevices::defaultAudioOutput();
    m_format.setSampleRate(48000);
    m_format.setChannelCount(2);
    m_format.setSampleFormat(QAudioFormat::Int16);
    if (!output.isFormatSupported(m_format)) {
        m_format = output.preferredFormat();
    }

    auto *device = new MixedAudioDevice(m_format, this);
    device->open(QIODevice::ReadOnly);
    device->setVolumes(m_bgmVolume, m_sfxVolume);
    m_ambientDevice = device;
    m_ambientSink = new QAudioSink(output, m_format, this);
    m_ambientSink->setBufferSize(m_format.bytesForDuration(160000));
    m_ambientSink->setVolume(1.0);
    m_ambientSink->start(device);

    if (qApp) {
        qApp->installEventFilter(this);
    }
}

void AudioManager::setVolumes(int bgmPercent, int sfxPercent)
{
    m_bgmVolume = std::clamp(bgmPercent / 100.0, 0.0, 1.0);
    m_sfxVolume = std::clamp(sfxPercent / 100.0, 0.0, 1.0);
    if (auto *device = dynamic_cast<MixedAudioDevice *>(m_ambientDevice)) {
        device->setVolumes(m_bgmVolume, m_sfxVolume);
    }
}

void AudioManager::setScene(Scene scene)
{
    m_scene = scene;
    if (auto *device = dynamic_cast<MixedAudioDevice *>(m_ambientDevice)) {
        device->setBattleMix(scene == Scene::Battle);
    }
}

void AudioManager::playWoodClick()
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - m_lastClickMs < 55) {
        return;
    }
    m_lastClickMs = now;
    playTone(145.0, 105, 7.5, 0.34, 0.28);
}

void AudioManager::playCardSelect()
{
    playTone(470.0, 150, 5.8, 0.05, -0.10);
}

void AudioManager::playDeploy()
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - m_lastDeployMs < 90) {
        return;
    }
    m_lastDeployMs = now;
    playTone(105.0, 210, 5.0, 0.48, 0.36);
}

void AudioManager::playAttack()
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - m_lastAttackMs < 55) {
        return;
    }
    m_lastAttackMs = now;
    playTone(620.0, 72, 8.0, 0.12, 0.34);
}

void AudioManager::playHit()
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - m_lastHitMs < 70) {
        return;
    }
    m_lastHitMs = now;
    playTone(92.0, 125, 8.5, 0.42, 0.25);
}

bool AudioManager::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonPress
        && qobject_cast<QAbstractButton *>(watched)) {
        const auto *mouse = static_cast<QMouseEvent *>(event);
        if (mouse->button() == Qt::LeftButton) {
            playWoodClick();
        }
    }
    return QObject::eventFilter(watched, event);
}

void AudioManager::playTone(double frequency, int durationMs, double decay,
                            double noiseAmount, double pitchDrop)
{
    if (!m_initialized || m_sfxVolume <= 0.001) {
        return;
    }

    if (auto *device = dynamic_cast<MixedAudioDevice *>(m_ambientDevice)) {
        device->addTone(frequency, durationMs, decay, noiseAmount, pitchDrop);
    }
}
