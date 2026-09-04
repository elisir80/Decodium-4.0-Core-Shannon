// gate_weights.hpp — pesi del gate appreso (strato 2, FASTLDPC-AI-SPEC-001 §2).
//
// ATTENZIONE, DA LEGGERE PRIMA DI ACCENDERE DECODIUM_LDPC_GATE=1: questi pesi
// vengono dal pacchetto di ricerca originale (2 settembre 2026), addestrati sul
// canale FT2 SINTETICO di train/ft2chan.py con la configurazione del decoder di
// allora. Da allora il decoder di produzione e' cambiato sotto ai piedi di questi
// pesi: alpha 3/4 -> 0,578 (scala degli LLR), ntau 14 -> 13 e pair_span 0 -> 64
// (candidati alla CRC), span2 32 -> 64. Le feature "mean_abs" e "score" dipendono
// dalla scala degli LLR, quindi la loro distribuzione sui candidati veri e falsi
// NON e' garantita identica a quella su cui questi pesi sono stati tarati.
//
// Il meccanismo (gate.hpp, il codice che li usa) e' collaudato: a flag spento il
// comportamento resta bit-identico, verificato. Questi pesi NON sono stati
// riaddestrati ne' rimisurati su questo decoder, ne' sul traffico vero — esattamente
// il passo che FASTLDPC-AI-SPEC-001 §2 elenca come necessario prima del rilascio
// ("prima della release: rifare dataset e soglia sui LLR REALI esportati da
// DECODIUM"). Trattare DECODIUM_LDPC_GATE=1 come un banco di prova da rimisurare,
// non come una soglia pronta all'uso. Vedi Detector/fastldpc/lab/gate/README.md e
// lab/gate/train_gate.py per riaddestrarli.
static const float GATE_W[10] = {-4.291915f, -1.574422f, -1.256663f, 0.078694f, 1.136246f, -0.020515f, -0.174598f, -1.268278f, 0.000000f, 0.000000f};
static const float GATE_B = -5.989304f;
static const float GATE_MU[10] = {0.094402f, 0.197569f, 0.028379f, 0.014154f, 2.271920f, 0.999885f, 0.156348f, 0.070599f, 1.000000f, 1.000000f};
static const float GATE_SD[10] = {0.030958f, 0.048061f, 0.015475f, 0.014063f, 0.085713f, 0.009374f, 0.062497f, 0.030065f, 0.000001f, 0.000001f};
static const float GATE_THRESHOLD = 0.581900f;
