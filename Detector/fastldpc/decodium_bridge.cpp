// decodium_bridge.cpp — fastldpc dietro la stessa firma del decoder LDPC di
// Decodium 4, per poterlo sostituire cambiando una riga sola.
//
// In Detector/FtxFt2Stage7.cpp:
//     ftx_decode174_91_c (llr.data (), 91, maxosd, 3, apmask.data (), ...);
// diventa
//     fastldpc_decode174_91_c (llr.data (), 91, maxosd, 3, apmask.data (), ...);
//
// Aggiungere questo file e i cpp/*.hpp di fastldpc alla build. FtxLdpc.cpp
// resta dov'e': serve ancora per encode174_91_nocrc_, le tabelle e le CRC.
//
// COSE DA SAPERE:
//
//  * Convenzione dei segni. Decodium usa LLR positivo = bit 1, fastldpc =
//    bit 0. La conversione la fa questo file, il chiamante non cambia.
//
//  * maxosd e norder sono accettati per compatibilita' di firma ma non hanno
//    lo stesso significato: fastldpc non lavora sugli snapshot del BP, l'OSD
//    parte sempre dai posterior finali. maxosd < 0 disattiva l'OSD (come in
//    Decodium), norder sceglie il preset (<=2 conservativo, >=3 sensibile).
//
//  * Keff diverso da 91 non e' supportato: si ricade sul decoder originale.
//
//  * UNA PAROLA PER CHIAMATA E' LO SPRECO PRINCIPALE. Il min-sum lavora su 16
//    parole per registro AVX2, quindi con una sola parola utile si buttano 15
//    corsie. Se il chiamante puo' raccogliere i candidati e decodificarli
//    insieme (Ft2Decoder::decode_batch), il costo per parola cala di circa un
//    ordine di grandezza. Con kFt2MaxCand = 300 candidati per passata, e' la
//    differenza fra ~100 us e ~14 us a candidato.
//
//  * Un'istanza per thread (thread_local): FT2DecodeWorker decodifica in
//    parallelo e il decoder ha stato interno.
#include "ft2_decoder.hpp"
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>

extern "C" void ftx_ldpc174_91_tables_c (int* Mn_out, int* Nm_out, int* nrw_out, int* ncw_out);

// Il decoder originale di Decodium: e' la via di fuga se la CPU non ha AVX2.
extern "C" void ftx_decode174_91_c (float const*, int, int, int, signed char const*,
                                    signed char*, signed char*, int*, int*, float*);

// Questo file va compilato con -mavx2 anche quando il resto del programma e'
// per x86-64 generico, altrimenti MinSumV3 diventa un alias della versione
// scalare e si perde tutto il guadagno. Il binario resta pero' eseguibile su
// CPU senza AVX2: la scelta si fa a runtime, una volta sola.
// Interruttore a runtime, per confrontare i due decoder sulla stessa banda
// senza ricompilare:
//     DECODIUM_FT2_DISABLE_FASTLDPC=1   -> torna al decoder originale
// Letto una volta sola, come le altre variabili del Detector: per alternare
// va riavviato il programma. All'avvio la scelta viene stampata su stderr,
// cosi' non si resta in dubbio su quale decoder sta girando.
// -1 = nessuna scelta dall'interfaccia, vale la variabile d'ambiente.
// 0/1 = l'utente ha deciso dalle impostazioni. Atomica perche' il decoder gira
// nel thread di FT2DecodeWorker mentre l'interruttore si muove in quello della
// GUI.
static std::atomic<int> g_enabled_from_ui {-1};

// Chiamata da DecodiumBridge quando si tocca l'interruttore nelle impostazioni.
extern "C" void fastldpc_set_enabled_c (int on) {
    g_enabled_from_ui.store (on ? 1 : 0, std::memory_order_relaxed);
    std::fputs (on ? "[fastldpc] decoder FT2: fastldpc (da impostazioni)\n"
                   : "[fastldpc] decoder FT2: originale (da impostazioni)\n", stderr);
}

// Quale decoder e' attivo in questo momento: serve alla GUI per mostrare lo
// stato giusto anche quando la scelta viene dalla variabile d'ambiente.
extern "C" int fastldpc_is_enabled_c ();

static bool fastldpc_disabled () {
    const int ui = g_enabled_from_ui.load (std::memory_order_relaxed);
    if (ui >= 0) return ui == 0;
    static const bool off = [] {
        char const* raw = std::getenv ("DECODIUM_FT2_DISABLE_FASTLDPC");
        const bool v = raw && std::atoi (raw) != 0;
        std::fputs (v ? "[fastldpc] decoder FT2: originale"
                        " (fastldpc disattivato da variabile d'ambiente)\n"
                      : "[fastldpc] decoder FT2: fastldpc\n", stderr);
        return v;
    }();
    return off;
}

static bool cpu_has_avx2 ();

extern "C" int fastldpc_is_enabled_c () {
    return (!fastldpc_disabled () && cpu_has_avx2 ()) ? 1 : 0;
}

static bool cpu_has_avx2 () {
#if (defined(__i386__) || defined(__x86_64__)) \
    && (defined(__GNUC__) || defined(__clang__))
    static const bool ok = __builtin_cpu_supports ("avx2");
#elif defined(_M_IX86) || defined(_M_X64)
    static const bool ok = true;   // MSVC: usare /arch:AVX2 sul solo file
#else
    // Il file viene compilato anche su ARM per mantenere la stessa API
    // pubblica; MinSumV3 e' allora l'alias scalare MinSumV2.
    static const bool ok = false;
#endif
    return ok;
}

namespace {

constexpr int kN = 174;

// La matrice di parita' viene presa dalle tabelle che Decodium ha gia'
// compilate dentro (ftx_ldpc174_91_tables_c), non da un file: niente
// dipendenze esterne a runtime, e la certezza di decodificare con la stessa H
// del resto del programma. Costruita una volta per processo.
const Code& shared_code () {
    static Code code = [] {
        std::vector<int> mn (3 * kN), nm (7 * 83), nrw (83);
        int ncw = 0;
        ftx_ldpc174_91_tables_c (mn.data (), nm.data (), nrw.data (), &ncw);
        Code c;
        c.M = 83; c.N = kN;
        c.row_ptr.push_back (0);
        for (int m = 0; m < c.M; ++m) {
            for (int r = 0; r < nrw[(size_t) m]; ++r)
                c.col_idx.push_back (nm[(size_t) (r + 7 * m)] - 1);   // le tabelle sono 1-based
            c.row_ptr.push_back ((int) c.col_idx.size ());
        }
        return c;
    }();
    return code;
}

// ATTENZIONE al significato di "norder": nel decoder originale NON e' l'ordine
// dell'OSD, e' un INDICE di preset. La tabella in FtxLdpc.cpp (righe 619-649)
// per ndeep=3 -- il valore con cui FT2 chiama -- sceglie nord=1, cioe' l'ordine
// UNO con i due passi di preprocessing, non l'ordine tre.
//
// Leggendolo come ordine si passava a osd_order=3 su span2=91 e span3=48, cioe'
// 4095 coppie + 17296 terne = ~21400 candidati per chiamata invece di qualche
// centinaio. L'unico filtro sui candidati e' il CRC-14, che ne lascia passare
// uno ogni 16384: ~1,3 false decodifiche per chiamata, moltiplicate per le
// centinaia di candidati di un ciclo FT2. Sono le decine di nominativi fantasma
// osservate in FT2 il 27/08/2026, sparite spegnendo fastldpc.
//
// Qui la corrispondenza segue la tabella originale.
// FT8 e FT4 ammettono tipi di messaggio che FT2 non usa: i formati da contest
// i3 = 2, 3 e 5. Con la maschera di FT2 (kSoloUsati) quei messaggi non
// verrebbero decodificati MAI, quindi la modalita' FT8 tiene tutti i tipi
// definiti. Costa meta' del potere filtrante del controllo di plausibilita',
// ed e' il prezzo giusto: un filtro non deve rendere cieco il decoder.
static thread_local bool g_modo_ft8 = false;

extern "C" void fastldpc_set_ft8_mode_c (int on) { g_modo_ft8 = on != 0; }

Ft2Decoder& decoder_for_preset (int ndeep) {
    static thread_local std::unique_ptr<Ft2Decoder> ord1, ord2, ord3;
    static thread_local std::unique_ptr<Ft2Decoder> ord1_ft8, ord2_ft8, ord3_ft8;
    std::unique_ptr<Ft2Decoder>& slot1 = g_modo_ft8 ? ord1_ft8 : ord1;
    std::unique_ptr<Ft2Decoder>& slot2 = g_modo_ft8 ? ord2_ft8 : ord2;
    std::unique_ptr<Ft2Decoder>& slot3 = g_modo_ft8 ? ord3_ft8 : ord3;

    if (ndeep <= 3) {                       // preset 1..3 -> nord=1 nell'originale
        if (!slot1) {
            Ft2Config c = Ft2Decoder::conservativo();
            // L'originale a ndeep=3 non fa "ordine 1 e basta": fa ordine 1 piu'
            // due passi euristici (npre1, npre2) che cercano le coppie di bit
            // capaci di azzerare i bit di parita' piu' affidabili. Sono molto
            // efficaci, e senza di essi qui si decodificava il 15% in meno.
            // pair_search riproduce quel meccanismo (vedi OsdFast).
            // Sulla ricerca larga (ordine 3, span 91/48), provata e ritirata:
            // al banco sembrava vincere su entrambi i fronti -- su 20000 parole
            // a Eb/N0=1 dB e 100000 candidati di rumore, a soglia 0,065, dava
            // 16821 decodifiche e 6 fantasmi contro 16168 e 10 della stretta
            // senza filtro. Sul traffico vero non ha retto: vedi sotto.
            // RICERCA STRETTA. La larga (ordine 3, span 91/48) e' stata
            // provata dal vivo il 28/08/2026 e va tolta: prova ~21400
            // candidati per parola contro ~600, e la CRC-14 ne ammette
            // uno ogni 16384, cioe' ~1,3 falsi attesi per parola. Il
            // controllo di plausibilita' vale 1,9 bit, divide per ~3,7 e
            // ne lascia ~0,35: moltiplicati per le parole che un ciclo
            // FT2 accetta fanno 2,8 nominativi fantasma per ciclo, misurati
            // sul traffico reale (113 decode in 10 minuti, 102 su 106 mai
            // ripetuti). Il filtro paga ~2 bit, l'allargamento ne costa ~5:
            // non lo copre. Il banco non lo vedeva perche' contava i falsi
            // su un numero fisso di candidati di rumore, non sul ritmo con
            // cui FT2 chiama davvero il decoder.
            // Il controllo di plausibilita' RESTA: con la ricerca stretta
            // i suoi bit si sommano a un tasso di falsi gia' basso.
            // RICERCA STRETTA. La larga (ordine 3, span 91/48) e' stata provata
            // due volte il 28/08/2026 e ritirata due volte. La seconda con i
            // controlli strutturali completi: sei minuti a zero fantasmi
            // sembravano assolverla, ma su una banda senza trasmissioni FT2
            // sei minuti non dimostrano niente, e con piu' tempo i fantasmi
            // sono tornati copiosi. Prova ~21400 candidati per parola contro
            // ~600: la CRC-14 ne ammette uno ogni 16384 e i filtri strutturali
            // pagano ~2 bit contro i ~5 che costa l'allargamento.
            c.osd_order = 2;
            c.span2 = 32;
            c.span3 = 0;
            c.pair_search = true;
            c.ntau = 14;                    // come l'originale a ndeep=3
            // Soglia del gate scelta sul RUMORE, non sulle parole vere: e' il
            // caso che domina in FT2, dove la maggior parte dei candidati non
            // contiene alcun segnale e ogni accettazione e' un nominativo
            // fantasma. Su 50000 candidati di puro rumore: 0,12 falsi per mille
            // a 0,065 contro 0,42 a 0,070 e 1,30 a 0,075. Il decoder originale
            // sta a 0,33. Costa il 2,6% di decodifiche senza AP e lo 0,9% con.
            c.nd_max = 0.065f;

            // I tipi di messaggio ammessi dal controllo di plausibilita'.
            // kSoloUsati tiene standard, testo libero e nominativi non
            // standard, e lascia fuori i tre formati da contest (EU VHF, ARRL
            // RTTY): in FT2 non si vedono, e tenerli fuori vale la meta' del
            // filtro. E' una POLITICA, non un test di formato: un messaggio di
            // quei tipi non verrebbe mai decodificato. Con FASTLDPC_TIPI=tutti
            // si torna a non escludere niente, al prezzo di piu' fantasmi.
            {
                char const* env = std::getenv ("FASTLDPC_TIPI");
                c.tipi_ammessi = (g_modo_ft8 || (env && std::string (env) == "tutti"))
                                     ? plaus::kTuttiDefiniti
                                     : plaus::kSoloUsati;
            }
            c.batch = 16;                   // una parola per chiamata: batch minimo
            slot1.reset (new Ft2Decoder (shared_code(), c));
        }
        return *slot1;
    }
    if (ndeep <= 5) {                       // preset 4..5 -> nord=2
        if (!slot2) {
            Ft2Config c = Ft2Decoder::conservativo();
            c.batch = 16;
            slot2.reset (new Ft2Decoder (shared_code(), c));
        }
        return *slot2;
    }
    // preset >= 6 -> nord=4 nell'originale; qui l'ordine massimo e' 3.
    if (!slot3) {
        Ft2Config c = Ft2Decoder::sensibile();
        c.batch = 16;
        slot3.reset (new Ft2Decoder (shared_code(), c));
    }
    return *slot3;
}

}  // namespace

// Soglia sui bit ribaltati, condivisa fra la via singola e quella batch.
// Coerenza con l'ipotesi AP: ACCESO di default. Se una passata AP impone dei
// bit e il decoder li ribalta lo stesso, la parola contraddice l'ipotesi che
// l'ha prodotta. E' un test strutturale, quindi non penalizza i segnali
// deboli come fa una soglia: per questo si tiene insieme al gate sui bit
// ribaltati invece che al suo posto.
// Da verificare con banda aperta: l'AP e' un'ipotesi soft e il decoder ha il
// diritto di contraddirla, quindi in teoria il test puo' scartare decodifiche
// vere. Con DECODIUM_LDPC_AP_CHECK=0 si spegne senza ricompilare.
static bool fastldpc_ap_check () {
    static bool const v = [] {
        char const* raw = std::getenv ("DECODIUM_LDPC_AP_CHECK");
        return !raw || (raw[0] != '0' && raw[0] != 0);
    }();
    return v;
}

// Traccia cosa i gate stanno scartando: DECODIUM_LDPC_GATE_LOG=1.
static bool fastldpc_gate_log () {
    static bool const v = [] {
        char const* raw = std::getenv ("DECODIUM_LDPC_GATE_LOG");
        return raw && raw[0] != '0' && raw[0] != 0;
    }();
    return v;
}

static int fastldpc_max_hard () {
    static int const v = [] {
        char const* raw = std::getenv ("DECODIUM_LDPC_MAX_HARD");
        if (!raw) raw = std::getenv ("DECODIUM_FT2_LDPC_MAX_HARD");
        int const n = raw ? std::atoi (raw) : 0;
        return (n > 0 && n <= kN) ? n : 22;
    }();
    return v;
}

extern "C" void fastldpc_decode174_91_c (float const* llr_in, int Keff, int maxosd, int norder,
                                         signed char const* apmask_in, signed char* message91_out,
                                         signed char* cw_out, int* ntype_out,
                                         int* nharderror_out, float* dmin_out)
{
    if (message91_out) std::memset (message91_out, 0, 91);
    if (cw_out) std::memset (cw_out, 0, kN);
    if (ntype_out) *ntype_out = 0;
    if (nharderror_out) *nharderror_out = -1;
    if (dmin_out) *dmin_out = 0.0f;
    if (!llr_in || !apmask_in) return;

    // Keff diverso da 91, CPU senza AVX2, o interruttore alzato: si torna al
    // decoder originale, che resta linkato. Il chiamante non sa nulla di tutto
    // questo e il comportamento e' identico a prima dell'innesto.
    if (Keff != 91 || !cpu_has_avx2 () || fastldpc_disabled ()) {
        ftx_decode174_91_c (llr_in, Keff, maxosd, norder, apmask_in, message91_out,
                            cw_out, ntype_out, nharderror_out, dmin_out);
        return;
    }

    Ft2Decoder& dec = decoder_for_preset (norder);

    // Decodium: positivo = bit 1. fastldpc: positivo = bit 0.
    float llr[kN];
    uint8_t apmask[kN];
    for (int i = 0; i < kN; ++i) {
        llr[i] = -llr_in[i];
        apmask[i] = apmask_in[i] != 0 ? 1 : 0;
    }

    uint8_t bits[kN], accepted = 0;
    const long bp_before = dec.stats().by_bp;
    if (maxosd < 0) {
        // niente OSD: si passa per il preset veloce, riusando la stessa istanza
        // non si puo', quindi si accetta solo cio' che chiude il min-sum
        Ft2Config c = Ft2Decoder::veloce();
        c.batch = 16;
        static thread_local std::unique_ptr<Ft2Decoder> solo_bp;
        if (!solo_bp) solo_bp.reset (new Ft2Decoder (shared_code(), c));
        solo_bp->decode_batch (llr, 1, bits, &accepted, nullptr, apmask);
        if (!accepted) return;
        if (ntype_out) *ntype_out = 1;
    } else {
        dec.decode_batch (llr, 1, bits, &accepted, nullptr, apmask);
        if (!accepted) return;
        if (ntype_out) *ntype_out = (dec.stats().by_bp > bp_before) ? 1 : 2;
    }

    if (message91_out)
        for (int i = 0; i < 91; ++i) message91_out[i] = (signed char) bits[i];
    if (cw_out)
        for (int i = 0; i < kN; ++i) cw_out[i] = (signed char) bits[i];

    // Stesse metriche di ftx_ldpc174_91_metrics_c, calcolate sugli LLR nella
    // convenzione del chiamante: nharderror = bit in disaccordo con la
    // decisione hard, dmin = somma dei |LLR| corrispondenti.
    int nhard = 0; float dmin = 0.0f;
    for (int i = 0; i < kN; ++i) {
        const int bit = bits[i] ? 1 : 0;
        const int hdec = llr_in[i] >= 0.0f ? 1 : 0;
        if ((hdec ^ bit) != 0) { ++nhard; dmin += std::fabs (llr_in[i]); }
    }
    // Gate sui bit ribaltati. Con fastldpc attivo il percorso NON passa da
    // ftx_decode174_91_c, quindi ldpc174_reject_by_nd non viene mai
    // applicato: senza questo controllo l'unico filtro resta nd, e nd non
    // basta perche' pesa i bit per il loro |LLR| e nel rumore vero gli LLR
    // sono deboli -- ribaltarne quaranta costa poco e nd resta basso.
    //
    // Tarato sul traffico reale del 28/08/2026: su 74 decodifiche di
    // stazioni ripetute (UX5HY, RV3ZN, F5PBG, QSO IK7VKC/F5PBG, da +11 a
    // -26 dB) nharderror aveva mediana 1, p99 16, massimo 20; i fantasmi
    // partivano da 23, con una valle netta fra 19 e 22. Stessa variabile
    // d'ambiente del gate consolidato, cosi' i due percorsi si regolano
    // insieme.
    {
        if (nhard > fastldpc_max_hard ()) {
            if (message91_out) std::memset (message91_out, 0, 91);
            if (cw_out) std::memset (cw_out, 0, kN);
            if (ntype_out) *ntype_out = 0;
            if (nharderror_out) *nharderror_out = -1;
            if (dmin_out) *dmin_out = 0.0f;
            return;
        }
    }

    if (nharderror_out) *nharderror_out = nhard;
    if (dmin_out) *dmin_out = dmin;
}

// ---------------------------------------------------------------------------
// Versione a blocco.
//
// Il min-sum lavora su 16 parole per registro AVX2: con una parola per chiamata
// quindici corsie su sedici restano vuote e si paga il batch intero per una
// parola sola. FT2 prova fino a 6 ipotesi AP sullo stesso candidato, e sono
// indipendenti fra loro (llr e apmask non dipendono dall'esito delle
// precedenti), quindi possono viaggiare insieme.
//
// Il conto: 6 passate in blocco costano quanto UNA chiamata singola di oggi.
// Nel caso migliore -- la prima passata decodifica -- non si perde nulla; nel
// caso peggiore, quando nessuna decodifica e nessuna passata si puo' saltare,
// si guadagna quasi il fattore sei.
//
// llr_in, apmask_in: [n][174] contigui. Le uscite sono [n] o [n][...].
// Il chiamante scorre poi i risultati nell'ordine originale e prende il primo
// valido: la semantica resta quella del ciclo sequenziale.
extern "C" void fastldpc_decode174_91_batch_c (int n, float const* llr_in,
                                               signed char const* apmask_in,
                                               int Keff, int maxosd, int norder,
                                               signed char* message91_out, signed char* cw_out,
                                               int* ntype_out, int* nharderror_out,
                                               float* dmin_out)
{
    if (n <= 0 || !llr_in || !apmask_in) return;

    // Fuori dai casi supportati si ricade sulla via singola, che a sua volta
    // sa tornare al decoder originale.
    if (Keff != 91 || !cpu_has_avx2 () || fastldpc_disabled ()) {
        for (int w = 0; w < n; ++w)
            fastldpc_decode174_91_c (llr_in + (size_t) w * kN, Keff, maxosd, norder,
                                     apmask_in + (size_t) w * kN,
                                     message91_out ? message91_out + (size_t) w * 91 : nullptr,
                                     cw_out ? cw_out + (size_t) w * kN : nullptr,
                                     ntype_out ? ntype_out + w : nullptr,
                                     nharderror_out ? nharderror_out + w : nullptr,
                                     dmin_out ? dmin_out + w : nullptr);
        return;
    }

    Ft2Decoder& dec = decoder_for_preset (norder);

    static thread_local std::vector<float> llr;
    static thread_local std::vector<uint8_t> apmask, bits, accepted;
    llr.resize ((size_t) n * kN);
    apmask.resize ((size_t) n * kN);
    bits.resize ((size_t) n * kN);
    accepted.resize ((size_t) n);

    for (size_t i = 0; i < (size_t) n * kN; ++i) {
        llr[i] = -llr_in[i];                       // Decodium: positivo = bit 1
        apmask[i] = apmask_in[i] != 0 ? 1 : 0;
    }

    const long bp_before = dec.stats ().by_bp;
    dec.decode_batch (llr.data (), n, bits.data (), accepted.data (), nullptr, apmask.data ());
    // by_bp cresce su tutto il blocco: non si puo' attribuire a una singola
    // parola, quindi ntype distingue solo accettata (2) da non accettata (0).
    // Il chiamante di FT2 usa ntype solo per il diario.
    (void) bp_before;

    for (int w = 0; w < n; ++w) {
        signed char* msg = message91_out ? message91_out + (size_t) w * 91 : nullptr;
        signed char* cw  = cw_out ? cw_out + (size_t) w * kN : nullptr;
        if (msg) std::memset (msg, 0, 91);
        if (cw) std::memset (cw, 0, kN);
        if (ntype_out) ntype_out[w] = 0;
        if (nharderror_out) nharderror_out[w] = -1;
        if (dmin_out) dmin_out[w] = 0.0f;
        if (!accepted[(size_t) w]) continue;

        const uint8_t* b = &bits[(size_t) w * kN];
        if (msg) for (int i = 0; i < 91; ++i) msg[i] = (signed char) b[i];
        if (cw)  for (int i = 0; i < kN; ++i) cw[i] = (signed char) b[i];
        if (ntype_out) ntype_out[w] = 2;

        int nhard = 0; float dmin = 0.0f;
        const float* src = llr_in + (size_t) w * kN;
        for (int i = 0; i < kN; ++i) {
            const int hdec = src[i] >= 0.0f ? 1 : 0;
            if ((b[i] ? 1 : 0) != hdec) { ++nhard; dmin += std::fabs (src[i]); }
        }

        // I due controlli qui sotto mancavano sulla via BATCH, che e' quella
        // che FT2 usa davvero: nharderror veniva calcolato e riportato al
        // chiamante, ma non filtrava niente. Il 28/08/2026 arrivavano in lista
        // decode con 31, 36, 38, 40, 41 e 43 bit ribaltati.

        // 1) bit ribaltati. Su 74 decodifiche vere di stazioni ripetute, da
        //    +11 a -26 dB: mediana 1, p99 16, massimo 20. I fantasmi partivano
        //    da 23, con una valle netta fra 19 e 22.
        if (nhard > fastldpc_max_hard ()) {
            if (fastldpc_gate_log ())
                std::fprintf (stderr, "[GATE] scartata: bit ribaltati %d > %d\n",
                              nhard, fastldpc_max_hard ());
            if (msg) std::memset (msg, 0, 91);
            if (cw) std::memset (cw, 0, kN);
            if (ntype_out) ntype_out[w] = 0;
            continue;
        }

        // 2) coerenza con l'ipotesi a priori: se una passata AP ha imposto
        //    dei bit e il decoder li ha ribaltati lo stesso, la parola
        //    contraddice l'ipotesi che l'ha prodotta.
        //
        //    ATTENZIONE, da verificare con banda aperta: l'AP e' un'ipotesi
        //    SOFT e il decoder ha il diritto di contraddirla quando il
        //    segnale lo richiede, quindi questo test puo' scartare decodifiche
        //    vere deboli. Qui tiene fuori molti fantasmi, ma il bilancio sui
        //    segnali veri non e' stato misurato: la banda era ferma.
        if (fastldpc_ap_check ()) {
            const signed char* apm = apmask_in + (size_t) w * kN;
            bool coerente = true;
            for (int i = 0; i < kN && coerente; ++i) {
                if (!apm[i]) continue;
                const int atteso = src[i] >= 0.0f ? 1 : 0;
                if ((b[i] ? 1 : 0) != atteso) coerente = false;
            }
            if (!coerente) {
                if (fastldpc_gate_log ())
                    std::fprintf (stderr, "[GATE] scartata: bit AP contraddetti\n");
                if (msg) std::memset (msg, 0, 91);
                if (cw) std::memset (cw, 0, kN);
                if (ntype_out) ntype_out[w] = 0;
                continue;
            }
        }

        if (nharderror_out) nharderror_out[w] = nhard;
        if (dmin_out) dmin_out[w] = dmin;
    }
}
