#include "speech/SpeechRecognizer.h"
#include "speech/RadioTranscript.h"

#include <QDebug>
#include <QFileInfo>
#include <QThread>

#include <algorithm>
#include <cmath>

#include "whisper.h"

namespace decodium::speech {

// Il contesto di whisper sta qui e non nell'intestazione: cosi' il resto di
// Decodium non deve vedere whisper.h per includere questa classe.
struct SpeechRecognizer::Contesto {
    whisper_context* ctx {nullptr};
};

SpeechRecognizer::SpeechRecognizer (QObject* parent)
    : QObject (parent)
{
}

SpeechRecognizer::~SpeechRecognizer ()
{
    if (m_contesto) {
        if (m_contesto->ctx)
            whisper_free (m_contesto->ctx);
        delete m_contesto;
        m_contesto = nullptr;
    }
}

bool SpeechRecognizer::caricaModello (const QString& percorso, const QString& lingua)
{
    m_lingua = lingua.isEmpty() ? QStringLiteral("en") : lingua;

    QFileInfo const f (percorso);
    if (!f.exists() || f.size() < 1024 * 1024) {
        emit modelloCaricato (false,
            tr("il file del modello non c'e' o e' incompleto: %1").arg(percorso));
        return false;
    }

    whisper_context_params par = whisper_context_default_params();
    // Niente GPU: i backend per acceleratori non sono nemmeno compilati, e su
    // questa macchina il modello gira sulla CPU piu' veloce del tempo reale.
    par.use_gpu = false;

    whisper_context* ctx =
        whisper_init_from_file_with_params (percorso.toUtf8().constData(), par);
    if (!ctx) {
        emit modelloCaricato (false, tr("il modello non si e' caricato: %1").arg(percorso));
        return false;
    }

    if (!m_contesto)
        m_contesto = new Contesto;
    if (m_contesto->ctx)
        whisper_free (m_contesto->ctx);
    m_contesto->ctx = ctx;

    qInfo().noquote() << "[VOCE] modello caricato:" << f.fileName()
                      << QStringLiteral("(%1 MB), lingua %2")
                             .arg (f.size() / 1048576).arg (m_lingua);
    emit modelloCaricato (true, f.fileName());
    return true;
}

void SpeechRecognizer::svuota ()
{
    m_coda.clear();
    m_restoInterpolazione = 0.0;
    m_ultimoCampione = 0;
}

void SpeechRecognizer::aggiungiConvertiti (const QVector<short>& campioni12k)
{
    // Da 12 a 16 kHz: per ogni quattro campioni in uscita se ne consumano tre
    // in ingresso. Si tiene il resto fra una chiamata e l'altra, altrimenti a
    // ogni blocco si perderebbe una frazione di campione e in capo a un minuto
    // il conto sarebbe sbagliato di parecchi millisecondi.
    constexpr double passo = 12000.0 / 16000.0;   // 0,75 campioni in ingresso per uno in uscita

    for (int i = 0; i < campioni12k.size(); ++i) {
        short const attuale = campioni12k[i];
        while (m_restoInterpolazione < 1.0) {
            float const a = static_cast<float>(m_ultimoCampione);
            float const b = static_cast<float>(attuale);
            float const t = static_cast<float>(m_restoInterpolazione);
            m_coda.push_back (((a + (b - a) * t)) / 32768.0f);
            m_restoInterpolazione += passo;
        }
        m_restoInterpolazione -= 1.0;
        m_ultimoCampione = attuale;
    }
}

void SpeechRecognizer::accumula (const QVector<short>& campioni12k)
{
    if (!pronto() || campioni12k.isEmpty())
        return;

    aggiungiConvertiti (campioni12k);

    if (m_coda.size() >= static_cast<size_t>(kSecondiPerBlocco * kFrequenzaLavoro))
        elabora();
}

void SpeechRecognizer::elabora ()
{
    if (!pronto() || m_coda.empty())
        return;

    // Se il blocco e' quasi silenzio non lo si manda al riconoscitore: costa
    // tempo di calcolo e restituisce le frasi che questi modelli inventano sul
    // rumore — "sottotitoli a cura di", "grazie per aver guardato" e simili,
    // che finirebbero nella lista dei decodificati come se qualcuno le avesse
    // dette in aria.
    double energia = 0.0;
    for (float v : m_coda) energia += double(v) * v;
    double const rms = std::sqrt (energia / double(m_coda.size()));
    if (rms < 0.004) {
        m_coda.clear();
        return;
    }

    whisper_full_params par =
        whisper_full_default_params (WHISPER_SAMPLING_GREEDY);
    par.language        = m_lingua.toUtf8().constData();
    par.translate       = false;
    par.print_progress  = false;
    par.print_realtime  = false;
    par.print_timestamps= false;
    par.print_special   = false;
    par.single_segment  = false;
    par.no_context      = true;   // ogni blocco per conto suo: in aria le frasi
                                  // non si concatenano come in un discorso
    par.suppress_nst    = true;   // niente "(musica)", "(rumore)" e simili
    // Meta' dei core: il resto serve ai decodificatori dei modi digitali e
    // all'audio, che non possono permettersi di aspettare.
    par.n_threads = std::max (2, QThread::idealThreadCount() / 2);

    QString testoGrezzo;
    if (whisper_full (m_contesto->ctx, par, m_coda.data(),
                      static_cast<int>(m_coda.size())) == 0) {
        int const n = whisper_full_n_segments (m_contesto->ctx);
        for (int i = 0; i < n; ++i) {
            const char* t = whisper_full_get_segment_text (m_contesto->ctx, i);
            if (t && *t) {
                if (!testoGrezzo.isEmpty()) testoGrezzo += QLatin1Char(' ');
                testoGrezzo += QString::fromUtf8 (t).trimmed();
            }
        }
    }

    // Si tiene la coda dell'audio per il blocco successivo: una frase a cavallo
    // fra due blocchi verrebbe altrimenti persa da entrambi.
    if (m_coda.size() > static_cast<size_t>(kCodaRiportata)) {
        std::vector<float> resto (m_coda.end() - kCodaRiportata, m_coda.end());
        m_coda.swap (resto);
    } else {
        m_coda.clear();
    }

    QString const pulito = testoGrezzo.trimmed();
    if (pulito.isEmpty() || pulito.size() < 3)
        return;

    // E qui il testo diventa radiantistico: CQ al posto di "Chico", i
    // nominativi ricomposti dall'alfabeto fonetico.
    Ricomposizione const r = ricomponi (pulito, m_lingua);
    emit frase (r.testo.isEmpty() ? pulito : r.testo,
                r.nominativi, r.nominativiIncerti);
}

} // namespace decodium::speech
