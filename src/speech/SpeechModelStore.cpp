#include "speech/SpeechModelStore.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>

namespace decodium::speech {

namespace {

// Il modello "small" multilingua: 465 MB, ed e' quello che regge fino a dieci
// decibel di rapporto segnale rumore. Il "base" da 142 MB si ferma a quindici,
// che su un collegamento normale in fonia non basta.
const char kUrlModello[] =
    "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-small.bin";

const char kNomeFile[] = "ggml-small.bin";

// Sotto i 400 MB il file e' certamente troncato: il modello ne pesa 465.
constexpr qint64 kDimensioneMinima = 400LL * 1024 * 1024;

QString cartella ()
{
    QString const base =
        QStandardPaths::writableLocation (QStandardPaths::AppLocalDataLocation);
    QString const dir = QDir (base).absoluteFilePath (QStringLiteral("speech"));
    QDir().mkpath (dir);
    return dir;
}

QString percorsoParziale ()
{
    return QDir (cartella()).absoluteFilePath (QString::fromLatin1(kNomeFile)
                                               + QStringLiteral(".parziale"));
}

} // namespace

SpeechModelStore::SpeechModelStore (QObject* parent)
    : QObject (parent)
{
}

SpeechModelStore::~SpeechModelStore ()
{
    annulla();
}

QString SpeechModelStore::percorsoModello ()
{
    return QDir (cartella()).absoluteFilePath (QString::fromLatin1(kNomeFile));
}

bool SpeechModelStore::modelloPresente ()
{
    QFileInfo const f (percorsoModello());
    return f.exists() && f.size() >= kDimensioneMinima;
}

void SpeechModelStore::pulisciParziale ()
{
    if (m_file) {
        m_file->close();
        m_file->deleteLater();
        m_file = nullptr;
    }
    QFile::remove (percorsoParziale());
}

void SpeechModelStore::scarica ()
{
    if (modelloPresente()) {
        emit finito (true, tr("il modello c'e' gia'"));
        return;
    }
    if (m_risposta)
        return;   // gia' in corso

    // Si scrive su un file con un altro nome e lo si rinomina alla fine. Se la
    // connessione cade a meta', quello che resta e' un ".parziale" che nessuno
    // scambiera' per un modello buono — mezzo modello si carica e fallisce con
    // un errore che non spiega niente.
    m_file = new QFile (percorsoParziale(), this);
    if (!m_file->open (QIODevice::WriteOnly | QIODevice::Truncate)) {
        QString const perche = m_file->errorString();
        m_file->deleteLater();
        m_file = nullptr;
        emit finito (false, tr("non riesco a scrivere in %1: %2")
                                .arg (cartella(), perche));
        return;
    }

    if (!m_rete)
        m_rete = new QNetworkAccessManager (this);

    QNetworkRequest req { QUrl (QString::fromLatin1(kUrlModello)) };
    req.setAttribute (QNetworkRequest::RedirectPolicyAttribute,
                      QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setHeader (QNetworkRequest::UserAgentHeader,
                   QStringLiteral("Decodium"));

    qInfo().noquote() << "[VOCE] scarico il modello in" << cartella();
    m_risposta = m_rete->get (req);

    connect (m_risposta, &QNetworkReply::readyRead, this, [this]() {
        if (m_file && m_risposta)
            m_file->write (m_risposta->readAll());
    });

    connect (m_risposta, &QNetworkReply::downloadProgress, this,
             [this](qint64 fatti, qint64 totali) {
        int const pc = totali > 0 ? int((fatti * 100) / totali) : 0;
        emit avanzamento (pc, fatti / 1048576, totali / 1048576);
    });

    connect (m_risposta, &QNetworkReply::finished, this, [this]() {
        QNetworkReply* r = m_risposta;
        m_risposta = nullptr;
        bool const bene = (r->error() == QNetworkReply::NoError);
        QString const perche = r->errorString();
        if (m_file) {
            m_file->write (r->readAll());
            m_file->flush();
            m_file->close();
        }
        r->deleteLater();

        if (!bene) {
            pulisciParziale();
            qWarning().noquote() << "[VOCE] scaricamento fallito:" << perche;
            emit finito (false, perche);
            return;
        }

        QFileInfo const parz (percorsoParziale());
        if (parz.size() < kDimensioneMinima) {
            pulisciParziale();
            emit finito (false, tr("il file scaricato e' incompleto (%1 MB)")
                                    .arg (parz.size() / 1048576));
            return;
        }

        if (m_file) { m_file->deleteLater(); m_file = nullptr; }
        QFile::remove (percorsoModello());
        if (!QFile::rename (percorsoParziale(), percorsoModello())) {
            emit finito (false, tr("non riesco a rinominare il file scaricato"));
            return;
        }
        qInfo().noquote() << "[VOCE] modello pronto:" << percorsoModello();
        emit finito (true, percorsoModello());
    });
}

void SpeechModelStore::annulla ()
{
    if (m_risposta) {
        QNetworkReply* r = m_risposta;
        m_risposta = nullptr;
        r->abort();
        r->deleteLater();
    }
    pulisciParziale();
}

} // namespace decodium::speech
