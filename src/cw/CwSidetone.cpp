#include "CwSidetone.h"

#include <QAudioDevice>
#include <QAudioFormat>
#include <QDebug>
#include <QMediaDevices>

#include <cmath>
#include <algorithm>

namespace decodium::cw {

namespace {
constexpr int kDefaultRate = 48000;
constexpr double kRampMs = 5.0;

QAudioFormat makeFormat(int sampleRate, int channels)
{
    QAudioFormat format;
    format.setSampleRate(sampleRate);
    format.setChannelCount(channels);
    if (channels == 1)
        format.setChannelConfig(QAudioFormat::ChannelConfigMono);
    else if (channels == 2)
        format.setChannelConfig(QAudioFormat::ChannelConfigStereo);
    format.setSampleFormat(QAudioFormat::Int16);
    return format;
}
}

// Genera i campioni su richiesta della scheda audio: la coda dei segmenti
// (tono si'/no, durata in campioni) e' l'unica cosa condivisa col thread GUI.
class SidetoneSource : public QIODevice {
public:
    SidetoneSource() { open(QIODevice::ReadOnly); }

    void push(qint64 samples, bool on)
    {
        if (samples <= 0)
            return;
        QMutexLocker l(&m_lock);
        m_queue.append({samples, on});
    }
    void setTone(int hz)
    {
        QMutexLocker l(&m_lock);
        m_toneHz = hz > 0 ? hz : 700;
        m_step = 2.0 * M_PI * m_toneHz / m_rate;
    }
    void setFormat(int sampleRate, int channels)
    {
        QMutexLocker l(&m_lock);
        m_rate = qMax(1, sampleRate);
        m_channels = qBound(1, channels, 2);
        m_step = 2.0 * M_PI * m_toneHz / m_rate;
    }
    void setGain(double g) { QMutexLocker l(&m_lock); m_gain = g; }
    void flush() { QMutexLocker l(&m_lock); m_queue.clear(); }
    bool idle() const { QMutexLocker l(&m_lock); return m_queue.isEmpty() && m_env < 0.001; }
    bool isSequential() const override { return true; }
    qint64 bytesAvailable() const override { return 1 << 20; }

protected:
    qint64 readData(char* data, qint64 maxLen) override
    {
        auto* out = reinterpret_cast<qint16*>(data);
        const qint64 bytesPerFrame = 2 * m_channels;
        const qint64 frames = maxLen / bytesPerFrame;
        QMutexLocker l(&m_lock);
        const double rampStep = 1.0 / (kRampMs * m_rate / 1000.0);
        for (qint64 i = 0; i < frames; ++i) {
            bool on = false;
            if (!m_queue.isEmpty()) {
                on = m_queue.first().on;
                if (--m_queue.first().left <= 0)
                    m_queue.removeFirst();
            }
            m_env += on ? rampStep : -rampStep;
            m_env = std::clamp(m_env, 0.0, 1.0);
            m_phase += m_step;
            if (m_phase > 2.0 * M_PI)
                m_phase -= 2.0 * M_PI;
            // rampa a coseno rialzato: niente colpi di tasto
            const double e = 0.5 - 0.5 * std::cos(M_PI * m_env);
            const qint16 sample = static_cast<qint16>(std::sin(m_phase) * e * m_gain * 32000.0);
            for (int channel = 0; channel < m_channels; ++channel)
                *out++ = sample;
        }
        return frames * bytesPerFrame;
    }
    qint64 writeData(const char*, qint64) override { return -1; }

private:
    struct Seg { qint64 left; bool on; };
    mutable QMutex m_lock;
    QList<Seg> m_queue;
    double m_phase {0.0};
    int m_rate {kDefaultRate};
    int m_channels {1};
    int m_toneHz {700};
    double m_step {2.0 * M_PI * 700 / kDefaultRate};
    double m_env {0.0};
    double m_gain {0.35};
};

CwSidetone::CwSidetone(QObject* parent)
    : QObject(parent)
    , m_source(std::make_unique<SidetoneSource>())
{
    m_idle.setInterval(3000);
    connect(&m_idle, &QTimer::timeout, this, &CwSidetone::idleCheck);
}

CwSidetone::~CwSidetone()
{
    if (m_sink) {
        m_sink->stop();
        delete m_sink;
    }
}

bool CwSidetone::ensureSink()
{
    if (m_sink && m_sink->state() != QAudio::StoppedState)
        return true;
    if (m_sink)
        delete m_sink;
    QAudioDevice dev = QMediaDevices::defaultAudioOutput();
    if (!m_deviceId.isEmpty()) {
        dev = QAudioDevice();
        for (const auto& candidate : QMediaDevices::audioOutputs())
            if (candidate.id() == m_deviceId) { dev = candidate; break; }
    }
    if (dev.isNull()) {
        emit failed(tr("CW listening output is unavailable. Select speakers or headphones and try again."));
        return false;
    }

    const QAudioFormat preferred = dev.preferredFormat();
    QList<QAudioFormat> candidates;
    candidates.append(makeFormat(kDefaultRate, 1));
    candidates.append(makeFormat(kDefaultRate, 2));
    if (preferred.isValid() && preferred.sampleRate() > 0) {
        candidates.append(makeFormat(preferred.sampleRate(), 1));
        candidates.append(makeFormat(preferred.sampleRate(), 2));
    }

    QAudioFormat fmt;
    for (const QAudioFormat& candidate : candidates) {
        if (dev.isFormatSupported(candidate)) {
            fmt = candidate;
            break;
        }
    }
    if (!fmt.isValid()) {
        emit failed(tr("CW listening output %1 does not support a compatible PCM audio format (preferred: %2 Hz, %3 channels).")
                       .arg(dev.description())
                       .arg(preferred.sampleRate())
                       .arg(preferred.channelCount()));
        return false;
    }

    m_sampleRate = fmt.sampleRate();
    m_source->setFormat(fmt.sampleRate(), fmt.channelCount());
    qInfo().noquote() << "[CW-SIDETONE] output=" << dev.description()
                      << "format=" << fmt.sampleRate() << "Hz"
                      << fmt.channelCount() << "ch Int16";
    m_sink = new QAudioSink(dev, fmt, this);
    m_sink->setBufferSize(m_sampleRate / 10 * fmt.bytesPerFrame()); // ~100 ms
    m_sink->start(m_source.get());
    if (m_sink->error() != QAudio::NoError) {
        const auto error = m_sink->error();
        qWarning().noquote() << "[CW-SIDETONE] start failed output=" << dev.description()
                             << "error=" << static_cast<int>(error);
        emit failed(tr("Cannot start the CW listening output %1 (audio error %2).")
                        .arg(dev.description())
                        .arg(static_cast<int>(error)));
        return false;
    }
    m_idle.start();
    return true;
}

bool CwSidetone::enqueue(const QList<int>& deltasMs, const QList<bool>& down, int toneHz)
{
    m_source->setGain(m_volume);
    if (!ensureSink()) { m_source->flush(); return false; }
    m_source->setTone(toneHz > 0 ? toneHz : 700);
    for (int i = 0; i < deltasMs.size() && i < down.size(); ++i) {
        // lo stato che finisce con questo istante e' l'opposto di quello nuovo
        m_source->push(static_cast<qint64>(deltasMs.at(i)) * m_sampleRate / 1000, !down.at(i));
    }
    return true;
}

void CwSidetone::clear()
{
    if (m_sink) m_sink->stop();
    m_idle.stop();
    m_source->flush();
}

void CwSidetone::setDeviceId(const QByteArray& id)
{
    if (m_deviceId == id) return;
    clear();
    m_deviceId = id;
}

void CwSidetone::setVolume(double volume)
{
    m_volume = std::clamp(volume, 0.0, 1.0);
    m_source->setGain(m_volume);
}

void CwSidetone::idleCheck()
{
    if (m_source->idle() && m_sink) {
        m_sink->stop();
        m_idle.stop();
    }
}

}  // namespace decodium::cw
