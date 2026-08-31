// La fonia che diventa testo.
//
// In SSB Decodium non decodifica niente, perche' non c'e' niente da decodificare
// — c'e' una voce. Questo la trascrive, e il testo entra nella lista dei
// decodificati insieme a tutto il resto. Per chi non sente e' l'unico modo di
// stare in fonia; per gli altri e' comodo quando il segnale e' faticoso.
//
// ─── quello che si puo' e non si puo' aspettare ────────────────────────────
//
// Misurato su parlato radiantistico passato per un filtro SSB, con rumore e
// QSB (whisper.cpp, modelli base e small):
//
//     rapporto S/N      base 142 MB      small 465 MB
//        30 dB           leggibile            —
//        20 dB           leggibile            —
//        15 dB           leggibile        leggibile
//        10 dB           fallisce         leggibile
//         6 dB           fallisce         fallisce
//
// Da qui la scelta di small: quei cinque decibel sono la differenza fra
// "funziona solo sui segnali forti" e "funziona sui collegamenti normali".
// Sotto i sei decibel non trascrive piu' niente, e non e' un limite del
// programma: sotto quella soglia la voce nell'audio non c'e' piu'.
//
// Il testo grezzo passa poi da RadioTranscript, che rimette a posto gergo e
// nominativi: senza quel passaggio il riconoscitore scrive "Chico" per CQ e
// nove parole comuni al posto di un nominativo compitato.
#pragma once

#include <QObject>
#include <QString>
#include <QVector>

#include <atomic>
#include <memory>
#include <vector>

namespace decodium::speech {

class SpeechRecognizer : public QObject
{
    Q_OBJECT

public:
    explicit SpeechRecognizer (QObject* parent = nullptr);
    ~SpeechRecognizer () override;

    // Carica il modello. Va chiamata dal thread su cui gira l'oggetto, e ci
    // mette qualche secondo: il modello small sono 465 MB da leggere.
    bool caricaModello (const QString& percorso, const QString& lingua);

    bool pronto () const { return m_contesto != nullptr; }

public slots:
    // I campioni ricevuti, a 12 kHz come li da' Decodium. Si accumulano finche'
    // non ce n'e' abbastanza per una frase.
    void accumula (const QVector<short>& campioni12k);

    // Butta via quello che c'e' in coda: serve quando si cambia frequenza o si
    // passa in trasmissione, dove il pezzo di frase a meta' non interessa piu'.
    void svuota ();

signals:
    // Una frase trascritta e gia' ricomposta in linguaggio radiantistico.
    // I nominativi sicuri e quelli con una lettera dubbia arrivano separati,
    // perche' vanno trattati in modo diverso: i primi si possono usare, i
    // secondi si mostrano e basta.
    void frase (const QString& testo,
                const QStringList& nominativi,
                const QStringList& nominativiIncerti);

    // Emesso quando il modello finisce di caricarsi, o quando fallisce.
    void modelloCaricato (bool riuscito, const QString& messaggio);

private:
    void elabora ();

    // Il riconoscitore vuole 16 kHz in virgola mobile; Decodium da' 12 kHz in
    // interi. La conversione e' un'interpolazione lineare 3:4 — a 16 kHz un
    // campione su quattro coincide con uno dei 12, gli altri tre stanno in
    // mezzo. Sulla banda della voce, che finisce a 2,7 kHz, l'errore di
    // un'interpolazione lineare e' molto sotto al rumore del segnale.
    void aggiungiConvertiti (const QVector<short>& campioni12k);

    struct Contesto;
    Contesto* m_contesto {nullptr};

    std::vector<float> m_coda;        // audio a 16 kHz in attesa
    QString m_lingua {QStringLiteral("en")};

    // Quanto audio si accumula prima di provare a trascriverlo. Cinque secondi:
    // sotto, il riconoscitore ha troppo poco contesto e sbaglia anche le parole
    // che conosce; sopra, il testo compare troppo tardi per seguire un QSO.
    static constexpr int kSecondiPerBlocco = 5;
    static constexpr int kFrequenzaLavoro = 16000;

    // La coda di un blocco si tiene per il successivo: una frase tagliata a
    // meta' fra due blocchi verrebbe persa da entrambi.
    static constexpr int kCodaRiportata = kFrequenzaLavoro / 2;   // mezzo secondo

    double m_restoInterpolazione {0.0};
    short  m_ultimoCampione {0};
};

} // namespace decodium::speech
