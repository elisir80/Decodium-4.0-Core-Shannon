#include "VoicePack.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QUrl>

namespace decodium::voicekeyer {

namespace {

constexpr auto kPiperZip = "https://github.com/rhasspy/piper/releases/download/2023.11.14-2/piper_windows_amd64.zip";
constexpr auto kVoiceIt = "https://huggingface.co/rhasspy/piper-voices/resolve/main/it/it_IT/paola/medium/";
constexpr auto kVoiceEn = "https://huggingface.co/rhasspy/piper-voices/resolve/main/en/en_US/lessac/medium/";

}  // namespace

const QVector<VoicePack::Item>& VoicePack::manifest()
{
    // Fissati: ogni file viene scaricato da questa sorgente e controllato con
    // SHA256 prima di essere usato. Se un hash non torna, il file viene scartato.
    static const QVector<Item> items {
        {QString::fromLatin1(kPiperZip),
         QStringLiteral("f3c58906402b24f3a96d92145f58acba6d86c9b5db896d207f78dc80811efcea"),
         22477236, QStringLiteral("piper_windows_amd64.zip"), true},
        {QString::fromLatin1(kVoiceIt) + QStringLiteral("it_IT-paola-medium.onnx"),
         QStringLiteral("6fc918b5a0ea6137382833dddfa567bffbe6a5060c02043c87192ee59c04210c"),
         63511038, QStringLiteral("voices/it_IT-paola-medium.onnx"), false},
        {QString::fromLatin1(kVoiceIt) + QStringLiteral("it_IT-paola-medium.onnx.json"),
         QStringLiteral("aea19c0a7fce29fbc359b93f10e7902854401e4c95ae2ea328ae516b15d296cf"),
         7099, QStringLiteral("voices/it_IT-paola-medium.onnx.json"), false},
        {QString::fromLatin1(kVoiceEn) + QStringLiteral("en_US-lessac-medium.onnx"),
         QStringLiteral("5efe09e69902187827af646e1a6e9d269dee769f9877d17b16b1b46eeaaf019f"),
         63201294, QStringLiteral("voices/en_US-lessac-medium.onnx"), false},
        // Riconoscimento vocale (whisper.cpp, MIT): il modello multilingue "small".
        {QStringLiteral("https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-small.bin"),
         QStringLiteral("1be3a9b2063867b937e64e2ec7483364a79917e157fa98c5d94b5c1fffea987b"),
         487601967, QStringLiteral("models/ggml-small.bin"), false},
        {QString::fromLatin1(kVoiceEn) + QStringLiteral("en_US-lessac-medium.onnx.json"),
         QStringLiteral("efe19c417bed055f2d69908248c6ba650fa135bc868b0e6abb3da181dab690a0"),
         4885, QStringLiteral("voices/en_US-lessac-medium.onnx.json"), false},
    };
    return items;
}

VoicePack::VoicePack(const QString& root, QObject* parent)
    : QObject(parent)
    , m_root(root)
    , m_network(new QNetworkAccessManager(this))
{
    m_status = installed() ? tr("Voice pack installed") : tr("Voice pack not installed");
}

QString VoicePack::runtimeDirectory() const
{
    return QDir(m_root).absoluteFilePath(QStringLiteral("piper"));
}

QString VoicePack::piperExecutable() const
{
    return QDir(runtimeDirectory()).absoluteFilePath(QStringLiteral("piper.exe"));
}

QString VoicePack::speechModel() const
{
    return QDir(m_root).absoluteFilePath(QStringLiteral("models/ggml-small.bin"));
}

QString VoicePack::whisperExecutable() const
{
    return QDir(m_root).absoluteFilePath(QStringLiteral("whisper/whisper-cli.exe"));
}

QString VoicePack::voiceModel(const QString& lang) const
{
    const QString name = lang == QLatin1String("en")
        ? QStringLiteral("en_US-lessac-medium.onnx")
        : QStringLiteral("it_IT-paola-medium.onnx");
    return QDir(m_root).absoluteFilePath(QStringLiteral("voices/") + name);
}

bool VoicePack::installed() const
{
    for (const Item& item : manifest()) {
        const QFileInfo fi(QDir(m_root).absoluteFilePath(item.target));
        if (item.archive) {
            if (!QFileInfo::exists(piperExecutable())) return false;
            continue;
        }
        if (!fi.exists() || fi.size() != item.bytes) return false;
    }
    return true;
}

void VoicePack::install()
{
    if (m_busy || installed()) return;
    m_queue = manifest();
    m_index = 0;
    m_doneBytes = 0;
    m_busy = true;
    m_progress = 0.0;
    m_status = tr("Downloading voice pack…");
    QDir().mkpath(m_root);
    emit changed();
    fetchNext();
}

void VoicePack::cancel()
{
    if (!m_busy) return;
    m_busy = false;
    m_status = tr("Download cancelled");
    emit changed();
}

void VoicePack::fetchNext()
{
    if (!m_busy) return;
    if (m_index >= m_queue.size()) {
        done();
        return;
    }
    const Item& item = m_queue.at(m_index);
    m_currentFile = QDir(m_root).absoluteFilePath(item.target);
    QDir().mkpath(QFileInfo(m_currentFile).absolutePath());
    const QString part = m_currentFile + QStringLiteral(".part");

    auto* file = new QFile(part, this);
    if (!file->open(QIODevice::WriteOnly)) {
        fail(tr("Cannot write %1").arg(part));
        return;
    }
    QNetworkRequest request{QUrl(item.url)};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(120000);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Decodium-VoicePack/1.0"));
    QNetworkReply* reply = m_network->get(request);

    connect(reply, &QNetworkReply::readyRead, reply, [reply, file] { file->write(reply->readAll()); });
    connect(reply, &QNetworkReply::downloadProgress, this, [this, item](qint64 got, qint64) {
        const qint64 total = qMax<qint64>(1, [&] { qint64 t = 0; for (const Item& i : m_queue) t += i.bytes; return t; }());
        m_progress = double(m_doneBytes + got) / double(total);
        m_status = tr("Downloading %1…").arg(QFileInfo(item.target).fileName());
        emit changed();
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, file, item, part] {
        const bool ok = reply->error() == QNetworkReply::NoError;
        const QString error = reply->errorString();
        reply->deleteLater();
        file->close();
        file->deleteLater();
        if (!m_busy) { QFile::remove(part); return; }
        if (!ok) {
            QFile::remove(part);
            fail(tr("Download failed: %1").arg(error));
            return;
        }
        QFile check(part);
        if (!check.open(QIODevice::ReadOnly)) { fail(tr("Cannot read %1").arg(part)); return; }
        QCryptographicHash hash(QCryptographicHash::Sha256);
        hash.addData(&check);
        check.close();
        if (hash.result().toHex() != item.sha256.toLatin1()) {
            QFile::remove(part);
            fail(tr("Checksum mismatch for %1: the file was discarded").arg(QFileInfo(item.target).fileName()));
            return;
        }
        QFile::remove(m_currentFile);
        QFile::rename(part, m_currentFile);
        m_doneBytes += item.bytes;
        if (item.archive) {
            m_status = tr("Unpacking the Piper engine…");
            emit changed();
            extractArchive(m_currentFile);
        }
        ++m_index;
        fetchNext();
    });
}

void VoicePack::extractArchive(const QString& file)
{
    // Windows 10 e 11 hanno tar (bsdtar) che legge gli zip.
    QProcess tar;
    tar.setWorkingDirectory(m_root);
    tar.start(QStringLiteral("tar"), {QStringLiteral("-xf"), file});
    if (!tar.waitForFinished(120000) || tar.exitCode() != 0) {
        fail(tr("Cannot unpack %1").arg(QFileInfo(file).fileName()));
        return;
    }
    QFile::remove(file);
}

void VoicePack::done()
{
    m_busy = false;
    m_progress = 1.0;
    if (installed()) {
        m_status = tr("Voice pack installed");
    } else {
        m_status = tr("Voice pack incomplete: try installing again");
    }
    emit changed();
}

void VoicePack::fail(const QString& text)
{
    m_busy = false;
    m_status = text;
    emit changed();
}

}  // namespace decodium::voicekeyer
