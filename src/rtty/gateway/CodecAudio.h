// DecoRTTY gateway — the USB codec side of an FT-991A.
//
// The radio presents itself to the PC as an ordinary USB sound card: receive
// audio on its capture endpoint, transmit audio on its playback endpoint. This
// class turns that into the one format the rest of DecoRTTY speaks — mono float
// at 24 kHz — so the gateway can put it straight into VITA-49 packets and the
// decoder never has to know where the samples came from.
#pragma once

#include "dsp/FirDecimator.h"

#include <QAudioDevice>
#include <QElapsedTimer>
#include <QAudioFormat>
#include <QIODevice>
#include <QObject>
#include <QString>

#include <memory>
#include <vector>

class QAudioSink;
class QAudioSource;

namespace decortty::gateway {

// QAudioSource pushes into a QIODevice; this is the smallest one that forwards
// what it is given.
class CaptureSink : public QIODevice {
    Q_OBJECT
public:
    explicit CaptureSink(QObject* parent = nullptr) : QIODevice(parent) {}
    bool   isSequential() const override { return true; }
    qint64 readData(char*, qint64) override { return 0; }
    qint64 writeData(const char* data, qint64 len) override
    {
        emit pcmReady(QByteArray(data, static_cast<int>(len)));
        return len;
    }
signals:
    void pcmReady(const QByteArray& pcm);
};

class CodecAudio : public QObject {
    Q_OBJECT

public:
    static constexpr int kDeviceRate = 48000;   // what the FT-991A codec runs at
    static constexpr int kWireRate   = 24000;   // what DecoRTTY carries

    explicit CodecAudio(QObject* parent = nullptr);
    ~CodecAudio() override;

    // Substring match against the device description, case-insensitive. The
    // FT-991A shows up as "USB Audio CODEC", which is generic enough that the
    // operator may need to be specific if another USB codec is plugged in.
    bool start(const QString& captureHint, const QString& playbackHint);
    void stop();

    bool isRunning() const { return m_running; }
    QString captureDeviceName() const  { return m_captureName; }
    QString playbackDeviceName() const { return m_playbackName; }

    // Queue transmit audio: mono float at 24 kHz. Interpolated to the codec
    // rate and handed to the sound card.
    void writeTransmitAudio(const float* samples, int count);
    // Drop anything still queued — used when transmit is aborted.
    void flushTransmit();

    // Diagnostica del percorso di trasmissione: quanti campioni sono arrivati,
    // quanti byte la scheda ne ha davvero accettati, e in che stato e' il sink.
    // Senza questi numeri, "non esce il tono" resta un'ipotesi.
    QString transmitDiagnostics() const;

    // List what is available, for the gateway's --list-devices.
    static QStringList captureDevices();
    static QStringList playbackDevices();

signals:
    // Mono float at 24 kHz, ready to become a VITA-49 payload.
    void audioReady(const std::vector<float>& mono);
    void errorOccurred(const QString& message);

private slots:
    void onCapturePcm(const QByteArray& pcm);

private:
    static QAudioDevice findDevice(const QList<QAudioDevice>& devices, const QString& hint);

    std::unique_ptr<QAudioSource> m_source;
    std::unique_ptr<QAudioSink>   m_sink;
    CaptureSink*                  m_captureSink{nullptr};
    QIODevice*                    m_playbackDevice{nullptr};

    QAudioFormat m_captureFormat;
    QAudioFormat m_playbackFormat;
    QString      m_captureName;
    QString      m_playbackName;
    bool         m_running{false};

    dsp::FirDecimator   m_decimator;
    dsp::FirInterpolator m_interpolator;

    std::vector<float> m_monoIn;
    std::vector<float> m_decimated;
    std::vector<float> m_interpolated;
    std::vector<qint16> m_pcmOut;
    qint64              m_droppedBytes{0};
    qint64              m_txSamplesIn{0};
    qint64              m_txBytesWritten{0};
    QElapsedTimer       m_dropClock;
};

} // namespace decortty::gateway
