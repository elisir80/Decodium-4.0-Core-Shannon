#include "PiperTts.h"

#include "VoiceKeyer.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>

#include <cstring>

namespace decodium::voicekeyer {

namespace {
constexpr int kMaxChars = 300;
constexpr int kTimeoutMs = 30000;
}

int PiperTts::modelSampleRate(const QString& modelPath)
{
    QFile f(modelPath + QStringLiteral(".json"));
    if (!f.open(QIODevice::ReadOnly)) return 22050;
    const QJsonObject root = QJsonDocument::fromJson(f.readAll()).object();
    const int rate = root.value(QStringLiteral("audio")).toObject().value(QStringLiteral("sample_rate")).toInt();
    return rate > 0 ? rate : 22050;
}

std::optional<QVector<short>> PiperTts::synthesize(const QString& piperExe, const QString& runtimeDir,
                                                   const QString& modelPath, const QString& text,
                                                   QString* error)
{
    auto fail = [&](const QString& why) -> std::optional<QVector<short>> {
        if (error) *error = why;
        return std::nullopt;
    };
    // Una sola riga per messaggio: Piper legge una frase per riga.
    QString clean = text;
    clean.replace(QLatin1Char('\n'), QLatin1Char(' ')).replace(QLatin1Char('\r'), QLatin1Char(' '));
    clean = clean.trimmed().left(kMaxChars);
    if (clean.isEmpty()) return fail(QStringLiteral("empty text"));
    if (!QFileInfo::exists(piperExe)) return fail(QStringLiteral("voice engine not installed"));
    if (!QFileInfo::exists(modelPath)) return fail(QStringLiteral("voice model not installed"));

    QProcess piper;
    piper.setWorkingDirectory(runtimeDir);
    piper.start(piperExe, {QStringLiteral("--model"), modelPath, QStringLiteral("--output_raw")});
    if (!piper.waitForStarted(5000)) return fail(QStringLiteral("cannot start the voice engine"));
    piper.write(clean.toUtf8() + "\n");
    piper.closeWriteChannel();
    if (!piper.waitForFinished(kTimeoutMs)) {
        piper.kill();
        piper.waitForFinished(2000);
        return fail(QStringLiteral("voice engine timed out"));
    }
    if (piper.exitCode() != 0) return fail(QStringLiteral("voice engine exited with code %1").arg(piper.exitCode()));

    const QByteArray raw = piper.readAllStandardOutput();
    const int rate = modelSampleRate(modelPath);
    QVector<short> pcm(raw.size() / 2);
    std::memcpy(pcm.data(), raw.constData(), std::size_t(pcm.size()) * 2);
    if (pcm.isEmpty()) return fail(QStringLiteral("no audio produced"));
    return resampleLinear(pcm, rate, kRadioRate);
}

}  // namespace decodium::voicekeyer
