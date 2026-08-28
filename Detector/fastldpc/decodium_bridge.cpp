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
Ft2Decoder& decoder_for_preset (int ndeep) {
    static thread_local std::unique_ptr<Ft2Decoder> ord1, ord2, ord3;

    if (ndeep <= 3) {                       // preset 1..3 -> nord=1 nell'originale
        if (!ord1) {
            Ft2Config c = Ft2Decoder::conservativo();
            // L'originale a ndeep=3 non fa "ordine 1 e basta": fa ordine 1 piu'
            // due passi euristici (npre1, npre2) che cercano le coppie di bit
            // capaci di azzerare i bit di parita' piu' affidabili. Sono molto
            // efficaci, e senza di essi qui si decodificava il 15% in meno.
            // pair_search riproduce quel meccanismo (vedi OsdFast).
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
            c.batch = 16;                   // una parola per chiamata: batch minimo
            ord1.reset (new Ft2Decoder (shared_code(), c));
        }
        return *ord1;
    }
    if (ndeep <= 5) {                       // preset 4..5 -> nord=2
        if (!ord2) {
            Ft2Config c = Ft2Decoder::conservativo();
            c.batch = 16;
            ord2.reset (new Ft2Decoder (shared_code(), c));
        }
        return *ord2;
    }
    // preset >= 6 -> nord=4 nell'originale; qui l'ordine massimo e' 3.
    if (!ord3) {
        Ft2Config c = Ft2Decoder::sensibile();
        c.batch = 16;
        ord3.reset (new Ft2Decoder (shared_code(), c));
    }
    return *ord3;
}

}  // namespace

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
        if (nharderror_out) nharderror_out[w] = nhard;
        if (dmin_out) dmin_out[w] = dmin;
    }
}
