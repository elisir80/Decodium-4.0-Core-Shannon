// gate.hpp — gate appreso contro le false decodifiche (FASTLDPC-AI-SPEC-001, strato 2).
// Sostituisce la sola soglia su nd con una regressione logistica su GATE_NF feature
// del candidato che ha passato la CRC-14. Pesi in gate_weights.hpp (appresi offline).
#pragma once
#include <cmath>
#include <cstdint>

static constexpr int GATE_NF = 10;

struct GateFeatures {
    float f[GATE_NF];
    // 0 nd           distanza soft normalizzata (num/den su bit liberi)
    // 1 nhard        frazione di bit del candidato diversi dalla decisione hard di canale
    // 2 nhard_top    idem ma solo sulla meta' di bit con |LLR| piu' alto (i piu' affidabili)
    // 3 min_rel      min|LLR| / media|LLR|
    // 4 mean_abs     media |LLR| di canale (scala)
    // 5 iters        iterazioni min-sum / max_iter
    // 6 unsat        check non soddisfatti all'uscita del min-sum / M
    // 7 score        score OSD (somma |L16| dei flip) / somma |L16|
    // 8 free_ratio   bit liberi (non AP) / N
    // 9 by_osd       1 se il candidato viene dall'OSD, 0 dal min-sum
};

#ifdef FASTLDPC_HAVE_GATE_WEIGHTS
#include "gate_weights.hpp"
inline float gate_logit(const GateFeatures& g) {
    float z = GATE_B;
    for (int k = 0; k < GATE_NF; ++k) z += GATE_W[k] * (g.f[k] - GATE_MU[k]) / GATE_SD[k];
    return z;
}
inline bool gate_accept(const GateFeatures& g) { return gate_logit(g) > GATE_THRESHOLD; }
#else
inline float gate_logit(const GateFeatures&) { return 1.0f; }
inline bool gate_accept(const GateFeatures&) { return true; }
#endif
