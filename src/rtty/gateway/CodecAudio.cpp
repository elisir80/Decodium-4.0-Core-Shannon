#include "gateway/CodecAudio.h"

#include <QAudioSink>
#include <QAudioSource>
#include <QMediaDevices>

#include <algorithm>
#include <cmath>

namespace decortty::gateway {

namespace {

QAudioFormat preferredFormat(int channels)
{
    QAudioFormat format;
    format.setSampleRate(CodecAudio::kDeviceRate);
    format.setChannelCount(channels);
    format.setSampleFormat(QAudioFormat::Int16);
    return format;
}

} // namespace

CodecAudio::CodecAudio(QObject* parent)
    : QObject(parent)
    , m_decimator(kDeviceRate / kWireRate, kDeviceRate)
    , m_interpolator(kDeviceRate / kWireRate, kDeviceRate)
{
}

CodecAudio::~CodecAudio()
{
    stop();
}

QStringList CodecAudio::captureDevices()
{
    QStringList names;
    for (const QAudioDevice& device : QMediaDevices::audioInputs())
        names.append(device.description());
    return names;
}

QStringList CodecAudio::playbackDevices()
{
    QStringList names;
    for (const QAudioDevice& device : QMediaDevices::audioOutputs())
        names.append(device.description());
    return names;
}

QAudioDevice CodecAudio::findDevice(const QList<QAudioDevice>& devices, const QString& hint)
{
    if (hint.isEmpty())
        return {};
    for (const QAudioDevice& device : devices) {
        if (device.description().contains(hint, Qt::CaseInsensitive))
            return device;
    }
    return {};
}

bool CodecAudio::start(const QString& captureHint, const QString& playbackHint)
{
    stop();

    const QAudioDevice capture  = findDevice(QMediaDevices::audioInputs(), captureHint);
    const QAudioDevice playback = findDevice(QMediaDevices::audioOutputs(), playbackHint);

    if (capture.isNull()) {
        emit errorOccurred(tr("No capture device matching '%1'. Available: %2")
                               .arg(captureHint, captureDevices().join(QStringLiteral(", "))));
        return false;
    }

    // Some codecs expose mono capture, some stereo. Take whichever the device
    // prefers rather than forcing a channel count it will then refuse.
    m_captureFormat = preferredFormat(capture.preferredFormat().channelCount() >= 2 ? 2 : 1);
    if (!capture.isFormatSupported(m_captureFormat)) {
        m_captureFormat = capture.preferredFormat();
        if (m_captureFormat.sampleRate() != kDeviceRate) {
            emit errorOccurred(tr("%1 cannot run at %2 Hz")
                                   .arg(capture.description())
                                   .arg(kDeviceRate));
            return false;
        }
    }

    m_captureSink = new CaptureSink(this);
    connect(m_captureSink, &CaptureSink::pcmReady, this, &CodecAudio::onCapturePcm);
    m_captureSink->open(QIODevice::WriteOnly);

    m_source = std::make_unique<QAudioSource>(capture, m_captureFormat);
    // Roughly 40 ms of buffer: enough that a scheduling hiccup does not drop
    // audio, short enough that transmit latency stays under the operator's
    // notice.
    m_source->setBufferSize(m_captureFormat.bytesForDuration(40000));
    m_source->start(m_captureSink);
    m_captureName = capture.description();

    m_decimator.configure(kDeviceRate / kWireRate, kDeviceRate);

    if (!playback.isNull()) {
        m_playbackFormat = preferredFormat(playback.preferredFormat().channelCount() >= 2 ? 2 : 1);
        if (!playback.isFormatSupported(m_playbackFormat))
            m_playbackFormat = playback.preferredFormat();

        m_sink = std::make_unique<QAudioSink>(playback, m_playbackFormat);
        m_sink->setBufferSize(m_playbackFormat.bytesForDuration(80000));
        m_playbackDevice = m_sink->start();
        m_playbackName = playback.description();
        m_interpolator.configure(kDeviceRate / kWireRate, kDeviceRate);
    } else if (!playbackHint.isEmpty()) {
        // Receive still works without it, so this is a warning, not a failure.
        emit errorOccurred(tr("No playback device matching '%1' — transmit is unavailable")
                               .arg(playbackHint));
    }

    m_running = true;
    return true;
}

void CodecAudio::stop()
{
    if (m_source) {
        m_source->stop();
        m_source.reset();
    }
    if (m_sink) {
        m_sink->stop();
        m_sink.reset();
    }
    m_playbackDevice = nullptr;
    if (m_captureSink) {
        m_captureSink->close();
        m_captureSink->deleteLater();
        m_captureSink = nullptr;
    }
    m_running = false;
}

void CodecAudio::onCapturePcm(const QByteArray& pcm)
{
    const int channels = std::max(1, m_captureFormat.channelCount());
    const int bytesPerSample = m_captureFormat.bytesPerSample();
    if (bytesPerSample != 2)
        return;   // only Int16 is negotiated above

    const int frames = static_cast<int>(pcm.size()) / (bytesPerSample * channels);
    if (frames <= 0)
        return;

    const auto* samples = reinterpret_cast<const qint16*>(pcm.constData());
    m_monoIn.resize(static_cast<size_t>(frames));

    if (channels == 1) {
        for (int i = 0; i < frames; ++i)
            m_monoIn[static_cast<size_t>(i)] = static_cast<float>(samples[i]) / 32768.0f;
    } else {
        for (int i = 0; i < frames; ++i) {
            const float left  = static_cast<float>(samples[i * channels]) / 32768.0f;
            const float right = static_cast<float>(samples[i * channels + 1]) / 32768.0f;
            m_monoIn[static_cast<size_t>(i)] = 0.5f * (left + right);
        }
    }

    m_decimated.resize(m_monoIn.size() / static_cast<size_t>(m_decimator.factor()) + 2);
    const int produced = m_decimator.process(m_monoIn.data(), frames, m_decimated.data());
    if (produced <= 0)
        return;

    m_decimated.resize(static_cast<size_t>(produced));
    emit audioReady(m_decimated);
}

void CodecAudio::writeTransmitAudio(const float* samples, int count)
{
    if (!m_playbackDevice || count <= 0)
        return;

    m_txSamplesIn += count;
    const int factor = m_interpolator.factor();
    m_interpolated.resize(static_cast<size_t>(count) * static_cast<size_t>(factor));
    const int produced = m_interpolator.process(samples, count, m_interpolated.data());
    if (produced <= 0)
        return;

    const int channels = std::max(1, m_playbackFormat.channelCount());
    m_pcmOut.resize(static_cast<size_t>(produced) * static_cast<size_t>(channels));

    for (int i = 0; i < produced; ++i) {
        const float clamped = std::clamp(m_interpolated[static_cast<size_t>(i)], -1.0f, 1.0f);
        const qint16 value = static_cast<qint16>(std::lround(clamped * 32767.0f));
        for (int c = 0; c < channels; ++c)
            m_pcmOut[static_cast<size_t>(i) * channels + c] = value;
    }

    // Never write more than the sound card can currently take. QIODevice would
    // otherwise queue the excess in memory without limit, and any small mismatch
    // between the sender's clock and the codec's would build a delay that grows
    // for as long as the transmission lasts — the audio would still be playing
    // seconds after PTT dropped.
    const qint64 wanted = static_cast<qint64>(m_pcmOut.size()) * 2;
    const qint64 room   = m_sink ? m_sink->bytesFree() : wanted;
    const qint64 toWrite = std::min(wanted, room);
    if (toWrite > 0)
        m_txBytesWritten += m_playbackDevice->write(
            reinterpret_cast<const char*>(m_pcmOut.data()), toWrite);
    if (toWrite < wanted) {
        m_droppedBytes += (wanted - toWrite);
        // Report at most once a second: a burst of these would itself stall the
        // audio thread.
        if (!m_dropClock.isValid() || m_dropClock.hasExpired(1000)) {
            emit errorOccurred(tr("Transmit audio overrun — dropped %1 ms")
                                   .arg(m_droppedBytes / 2 / (kDeviceRate / 1000)));
            m_droppedBytes = 0;
            m_dropClock.restart();
        }
    }
}

QString CodecAudio::transmitDiagnostics() const
{
    if (!m_sink)
        return QStringLiteral("nessun dispositivo di uscita aperto");

    static const char* states[] = {"Active", "Suspended", "Stopped", "Idle"};
    const int st = static_cast<int>(m_sink->state());
    return QStringLiteral("uscita '%1'  stato=%2  formato=%3 Hz/%4 ch  "
                          "buffer=%5 B  liberi=%6 B  ricevuti=%7 campioni  scritti=%8 B  persi=%9 B")
        .arg(m_playbackName)
        .arg(st >= 0 && st < 4 ? QLatin1String(states[st]) : QLatin1String("?"))
        .arg(m_playbackFormat.sampleRate())
        .arg(m_playbackFormat.channelCount())
        .arg(m_sink->bufferSize())
        .arg(m_sink->bytesFree())
        .arg(m_txSamplesIn)
        .arg(m_txBytesWritten)
        .arg(m_droppedBytes);
}

void CodecAudio::flushTransmit()
{
    if (!m_sink)
        return;
    // Restarting the sink is the only way to discard what the sound card has
    // already accepted; without it an aborted transmission keeps playing for
    // as long as the buffer holds.
    m_sink->reset();
    m_playbackDevice = m_sink->start();
    m_interpolator.reset();
}

} // namespace decortty::gateway
