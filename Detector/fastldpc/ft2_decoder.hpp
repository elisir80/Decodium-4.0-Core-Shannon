// ft2_decoder.hpp — l'unico header che serve includere per usare fastldpc.
//
// Incapsula la catena completa: min-sum SIMD (AVX2 o NEON) su un batch di
// candidati, OSD sui non convergenti, CRC-14 e gate sulla distanza soft. Gestisce da solo il
// riempimento del batch, quindi si puo' chiamare con un numero qualsiasi di
// parole.
//
//     Code code = Code::load("data/ldpc_174_91.h.txt");
//     Ft2Decoder dec(code, Ft2Decoder::sensibile());
//     int ok = dec.decode_batch(llr, n, bits, flags);   // llr = [n][174] float
//
// llr: LLR di canale, positivo = bit 0, nello stesso formato del demodulatore.
// bits: [n][174] con i bit decisi. flags[i] = 1 se la parola i e' accettata:
// solo quelle vanno passate al livello superiore. I 77 bit di messaggio sono
// bits[i*174 .. i*174+76].
//
// I tre preset sono tarati su 20 000 parole per punto fra 0,5 e 3 dB
// (vedi README). A parita' di false decodifiche `sensibile` vale circa +0,7 dB
// rispetto alla configurazione OSD-2/span32 da cui il progetto era partito.
#pragma once
#include "minsum_avx2.hpp"
#include "osd_fast.hpp"
#include <cstring>

struct Ft2Config {
    int   batch     = 64;       // parole per chiamata al min-sum, multiplo di 16
    int   max_iter  = 30;       // iterazioni min-sum
    int   osd_order = 3;        // -1 = nessun OSD, 0..3
    int   span2     = 91;       // bit d'informazione esplorati a coppie
    int   span3     = 48;       // ... e a terne
    float nd_max    = 0.075f;   // gate anti-false-decode; 1.0 lo disattiva
    // Tipi di messaggio i3 ammessi dal controllo di plausibilita' dentro
    // l'OSD: 0 lo spegne. Vedi cpp/plausible.hpp.
    uint32_t tipi_ammessi = 0;
    // Bit d'informazione ammessi nelle coppie; 0 = tutti. Vedi OsdFast.
    int pair_span   = 0;
    // Limite sui |LLR| in ingresso, in multipli della media della parola.
    // 0 = disattivato. Un LLR molto piu' grande della media e' quasi sempre un
    // artefatto (interferenza impulsiva) e non informazione: un solo LLR
    // "sicuro e sbagliato" avvelena tutti i check che lo toccano. La soglia e'
    // relativa alla parola, quindi resta invariante di scala come nd.
    float llr_clip  = 2.5f;
    // Ricerca a coppie mirata (vedi OsdFast::pair_search): trova le coppie di
    // bit che azzerano i bit di parita' piu' affidabili, invece di provarle
    // tutte. E' il meccanismo che rende efficace l'OSD di WSJT-X a ordine 1.
    bool  pair_search = false;
    int   ntau        = 14;
    // Fattore di normalizzazione del min-sum, in 1/65536. 49152 = 3/4, la
    // costante classica; 37888 = 0,578 e' quella misurata su QUESTO codice.
    // Vedi lab/README.md: 3/4 e' tarato per far convergere il min-sum, mentre
    // qui il min-sum prepara i posteriori per l'OSD, che e' un altro mestiere.
    unsigned alpha_w  = 49152;
};

class Ft2Decoder {
public:
    // Magnitudine assegnata a un bit noto: il massimo che la quantizzazione
    // interna del min-sum rappresenta (LLR_MAX / LLR_FIX).
    static constexpr float kApMag = 2047.0f / 8.0f;

    // Solo min-sum: ~5 us/parola, nessuna falsa decodifica, meno sensibile.
    static Ft2Config veloce() {
        Ft2Config c; c.osd_order = -1; return c;
    }
    // OSD-2 su span 32: il compromesso classico, tipo WSJT-X.
    static Ft2Config conservativo() {
        Ft2Config c; c.osd_order = 2; c.span2 = 32; c.span3 = 0; c.nd_max = 0.085f; return c;
    }
    // OSD-3: piu' sensibile a parita' di false decodifiche, ~4x piu' lento del
    // conservativo. E' possibile solo perche' il test CRC di un candidato costa
    // uno XOR (vedi osd_fast.hpp) e perche' il gate tiene a freno le false.
    static Ft2Config sensibile() { return Ft2Config{}; }

    Ft2Decoder(const Code& code, const Ft2Config& cfg = Ft2Config{})
        : cfg_(cfg), N_(code.N),
          ms_(code, cfg.batch, cfg.max_iter),
          osd_(code, cfg.osd_order, cfg.span2, cfg.span3),
          bits_((size_t)cfg.batch * code.N), ok_(cfg.batch), iters_(cfg.batch),
          buf_((size_t)cfg.batch * code.N), word_(code.N) {
        osd_.nd_max = cfg.nd_max;
        osd_.tipi_ammessi = cfg.tipi_ammessi;
        osd_.pair_span = cfg.pair_span;
        osd_.pair_search = cfg.pair_search;
        osd_.ntau = cfg.ntau;
        ms_.set_alpha(cfg.alpha_w);
    }

    // Statistiche cumulate dall'ultima reset_stats().
    struct Stats { long words = 0, by_bp = 0, by_osd = 0, osd_tried = 0; };
    const Stats& stats() const { return st_; }
    void reset_stats() { st_ = Stats{}; }

    // Candidati sottoposti alla CRC-14 (con -DOSD_COUNT; senza, resta zero).
    // Diviso per le parole tentate da' i candidati per parola, cioe' il numero
    // da cui dipendono i nominativi fantasma: la CRC ne ammette uno ogni 16384.
    // E' strutturale e deterministico, quindi dice quello che il conteggio dei
    // fantasmi su una piscina di rumore gaussiano non riesce a dire.
    long long crc_tests() const { return osd_.n_crc; }
    void reset_crc_tests() { osd_.n_crc = 0; }

    // apmask (opzionale): [n][174], 1 sui bit gia' noti per ipotesi a priori.
    // Gli LLR di quei bit devono gia' portare il valore noto, come fa FT2
    // (llr[i] = apmag * apbits[i]); qui vengono saturati al massimo
    // rappresentabile, cosi' il min-sum non li ribalta e l'OSD, che ordina per
    // affidabilita', li lascia in fondo al set d'informazione e non li flippa.
    //
    // Ritorna il numero di parole accettate. `nd`, se non nullo, riceve la
    // distanza soft normalizzata delle parole chiuse dall'OSD (0 per le altre).
    // Un'istanza per thread: lo stato interno (batch, matrice ridotta) e'
    // per-istanza e non c'e' niente di condiviso, quindi nessuna sincronizzazione.
    int decode_batch(const float* llr, int n, uint8_t* out, uint8_t* accepted,
                     float* nd = nullptr, const uint8_t* apmask = nullptr) {
        const int B = cfg_.batch, N = N_;
        int total = 0;
        std::memset(accepted, 0, (size_t)n);
        if (nd) std::memset(nd, 0, (size_t)n * sizeof(float));

        for (int off = 0; off < n; off += B) {
            const int cnt = std::min(B, n - off);
            std::memcpy(buf_.data(), &llr[(size_t)off * N], (size_t)cnt * N * sizeof(float));
            // I bit noti per ipotesi a priori vanno bloccati PRIMA del clip,
            // altrimenti il clip stesso ne ridurrebbe la magnitudine.
            if (apmask) {
                for (int b = 0; b < cnt; ++b) {
                    const uint8_t* m = &apmask[(size_t)(off + b) * N];
                    float* w = &buf_[(size_t)b * N];
                    for (int v = 0; v < N; ++v)
                        if (m[v]) w[v] = w[v] >= 0 ? kApMag : -kApMag;
                }
            }
            if (cfg_.llr_clip > 0) {
                for (int b = 0; b < cnt; ++b) {
                    const uint8_t* m = apmask ? &apmask[(size_t)(off + b) * N] : nullptr;
                    float* w = &buf_[(size_t)b * N];
                    float s = 0; int cnt_free = 0;
                    for (int v = 0; v < N; ++v)
                        if (!m || !m[v]) { s += w[v] < 0 ? -w[v] : w[v]; ++cnt_free; }
                    if (cnt_free > 0) {
                        const float lim = cfg_.llr_clip * s / cnt_free;
                        for (int v = 0; v < N; ++v) {
                            if (m && m[v]) continue;                 // i bit AP restano al massimo
                            w[v] = w[v] > lim ? lim : (w[v] < -lim ? -lim : w[v]);
                        }
                    }
                }
            }
            // Le lane in eccesso ripetono la prima parola: il min-sum lavora
            // sempre a batch pieno e il costo per parola resta quello nominale.
            for (int b = cnt; b < B; ++b)
                std::memcpy(&buf_[(size_t)b * N], buf_.data(), (size_t)N * sizeof(float));

            ms_.decode(buf_.data(), bits_.data(), iters_.data(), ok_.data());

            for (int b = 0; b < cnt; ++b) {
                ++st_.words;
                const int i = off + b;
                uint8_t* dst = &out[(size_t)i * N];
                const uint8_t* dec = &bits_[(size_t)b * N];
                if (ok_[b] && crc14_ok(dec)) {          // chiusa dal min-sum
                    std::memcpy(dst, dec, (size_t)N);
                    accepted[i] = 1; ++st_.by_bp; ++total;
                    continue;
                }
                if (cfg_.osd_order < 0) continue;
                ++st_.osd_tried;
                // La soglia del gate si allarga quando ci sono bit noti: lo
                // spazio dei candidati compatibili si riduce di 2^K, quindi i
                // falsi positivi crollano, e allo stesso tempo l'AP fa
                // decodificare parole con piu' errori di canale, che hanno nd
                // piu' alto. Il fattore (N/N_liberi)^2 e' tarato su 0, 14, 29 e
                // 58 bit noti: predice il ginocchio della curva in tutti e
                // quattro i casi (vedi README).
                if (apmask) {
                    int free_bits = 0;
                    const uint8_t* m = &apmask[(size_t)i * N];
                    for (int v = 0; v < N; ++v) free_bits += !m[v];
                    const float r = free_bits > 0 ? (float)N / (float)free_bits : 1.0f;
                    osd_.nd_max = cfg_.nd_max * r * r;
                } else {
                    osd_.nd_max = cfg_.nd_max;
                }
                float d = 0;
                if (osd_.decode(ms_.posterior(b), word_.data(), &buf_[(size_t)b * N], &d,
                                apmask ? &apmask[(size_t)i * N] : nullptr)) {
                    std::memcpy(dst, word_.data(), (size_t)N);
                    accepted[i] = 1; ++st_.by_osd; ++total;
                    if (nd) nd[i] = d;
                }
            }
        }
        return total;
    }

    // Alias breve, per comodita' nei banchi di prova.
    int decode(const float* llr, int n, uint8_t* out, uint8_t* acc,
               float* nd = nullptr, const uint8_t* apmask = nullptr) {
        return decode_batch(llr, n, out, acc, nd, apmask);
    }

    // Check non soddisfatti dall'ultima chiamata al min-sum, per la parola b del
    // batch corrente. Utile in diagnostica; misurato come predittore del
    // successo dell'OSD, NON funziona (vedi README).
    int unsat(int b) const { return ms_.unsat(b); }

private:
    Ft2Config cfg_;
    int N_;
    MinSumV3 ms_;
    OsdFast  osd_;
    std::vector<uint8_t> bits_, ok_;
    std::vector<int> iters_;
    std::vector<float> buf_;
    std::vector<uint8_t> word_;
    Stats st_;
};
