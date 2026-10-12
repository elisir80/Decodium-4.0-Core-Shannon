#include "SpeechToText.h"

#include "VoiceKeyer.h"

#include <QAudioDevice>
#include <QAudioFormat>
#include <QAudioSource>
#include <QDir>
#include <QFileInfo>
#include <QMediaDevices>
#include <QRegularExpression>
#include <QStandardPaths>

#include <cstring>

namespace decodium::voicekeyer {

namespace {
constexpr int kWhisperRate = 16000;
constexpr int kListenMaxSeconds = 30;   // un messaggio al microfono, non un discorso
}

QString cleanTranscript(const QString& raw)
{
    QString s = raw;
    static const QRegularExpression marker(QStringLiteral("\\[[^\\]]*\\]"));
    s.remove(marker);
    return s.simplified();
}

SpeechToText::SpeechToText(const QString& engineExe, const QString& modelPath, QObject* parent)
    : QObject(parent)
    , m_engine(engineExe)
    , m_model(modelPath)
{
    m_status = QFileInfo::exists(m_engine) && QFileInfo::exists(m_model)
        ? tr("Hold Listen and speak")
        : tr("Speech recognition not installed");
}

SpeechToText::~SpeechToText()
{
    if (m_source) stopListening();
}

void SpeechToText::setStatus(const QString& text)
{
    m_status = text;
    emit changed();
}

void SpeechToText::startListening(const QString& lang)
{
    if (m_source || m_working) return;
    if (!QFileInfo::exists(m_engine) || !QFileInfo::exists(m_model)) {
        setStatus(tr("Speech recognition not installed: install the voice pack first"));
        return;
    }
    m_lang = (lang == QLatin1String("en")) ? QStringLiteral("en") : QStringLiteral("it");
    const QAudioDevice device = QMediaDevices::defaultAudioInput();
    if (device.isNull()) {
        setStatus(tr("No microphone found"));
        return;
    }
    QAudioFormat fmt;
    fmt.setSampleRate(kCaptureRate);
    fmt.setChannelCount(1);
    fmt.setSampleFormat(QAudioFormat::Int16);
    if (!device.isFormatSupported(fmt)) {
        setStatus(tr("Microphone must support 48 kHz 16-bit PCM"));
        return;
    }
    m_captured.clear();
    m_source = new QAudioSource(device, fmt, this);
    m_source->setBufferSize(fmt.bytesForDuration(400000));
    m_input = m_source->start();
    if (!m_input) {
        delete m_source;
        m_source = nullptr;
        setStatus(tr("Cannot open the microphone"));
        return;
    }
    connect(m_input, &QIODevice::readyRead, this, &SpeechToText::captureReady);
    setStatus(tr("Listening…"));
}

void SpeechToText::captureReady()
{
    if (!m_input) return;
    m_captured += m_input->readAll();
    if (m_captured.size() > kCaptureRate * 2 * kListenMaxSeconds) stopListening();
}

void SpeechToText::stopListening()
{
    if (!m_source) return;
    captureReady();
    m_source->stop();
    delete m_source;
    m_source = nullptr;
    m_input = nullptr;
    transcribe();
}

void SpeechToText::clearTranscript()
{
    m_transcript.clear();
    emit changed();
}

void SpeechToText::transcribe()
{
    QVector<short> pcm(m_captured.size() / 2);
    std::memcpy(pcm.data(), m_captured.constData(), std::size_t(pcm.size()) * 2);
    m_captured.clear();
    if (pcm.size() < kCaptureRate / 4) {
        setStatus(tr("Too short: keep Listen pressed while speaking"));
        return;
    }
    const QVector<short> wav = resampleLinear(pcm, kCaptureRate, kWhisperRate);
    // Un file per ogni ascolto: whisper lo legge mentre il processo gira.
    const QString wavPath = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
                                .absoluteFilePath(QStringLiteral("decodium_listen.wav"));
    QString error;
    if (!writeWav(wavPath, wav, kWhisperRate, &error)) {
        setStatus(tr("Cannot save the recording: %1").arg(error));
        return;
    }

    m_working = true;
    setStatus(tr("Transcribing…"));
    m_process = new QProcess(this);
    connect(m_process, &QProcess::finished, this, [this](int code, QProcess::ExitStatus st) {
        const QByteArray out = m_process->readAllStandardOutput();
        const QString err = QString::fromLocal8Bit(m_process->readAllStandardError()).trimmed();
        m_process->deleteLater();
        m_process = nullptr;
        m_working = false;
        if (st != QProcess::NormalExit || code != 0) {
            setStatus(tr("Recognition failed: %1").arg(err.left(160)));
            return;
        }
        const QString text = cleanTranscript(QString::fromUtf8(out));
        if (text.isEmpty()) {
            setStatus(tr("Nothing recognised"));
            return;
        }
        m_transcript = text;
        setStatus(tr("Recognised"));
    });
    m_process->start(m_engine, {QStringLiteral("-m"), m_model, QStringLiteral("-f"), wavPath,
                                QStringLiteral("-l"), m_lang, QStringLiteral("-nt"), QStringLiteral("-np")});
}

}  // namespace decodium::voicekeyer
