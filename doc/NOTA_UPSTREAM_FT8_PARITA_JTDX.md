# Nota tecnica upstream — Parità decode FT8 Decodium vs JTDX

**Autore:** IU8LMC (fork iu8lmc) · **Data:** 2026-05-24 · **Destinatario:** Salvatore / upstream elisir80

## Obiettivo dell'indagine

Misura sistematica del divario di decode FT8 tra Decodium 4.0 (1.0.280) e JTDX,
sugli **stessi slot, stessa banda (20m, 14.074), stessa propagazione** (confronto
slot-per-slot sui due `ALL.TXT`). Metodo: conteggio decode unici per slot,
deduplicati, su finestre da 15-23 slot per annullare il rumore di banda.

## Baseline misurato

- Decodium stock 1.0.280: **~80-85% dei decode di JTDX** (band-dependent).
- JTDX gira con: `NDepth=3` (Deep), `FT8Sensitivity=2`, `nPreampass=4`, `Hint=true`,
  `FT8WideDXCallSearch=true`. Media ~33-37 decode/slot su 20m affollato.
- Inoltre: **~15% dei decode FT8 di Decodium mostravano `<...>`** (callsign hashato
  non risolto) contro **~0% di JTDX**.

## Cause-radice identificate

### 1. Soglia di sync troppo alta sulle passate di sottrazione (sensibilità)
La soglia di sync per-passata (`ftx_ft8_prepare_pass_c`, `FtxDecodeBookkeeping.cpp`)
partiva da 1.0 e scendeva a 0.88 (pass 3) / 0.67 (pass 4-5). I segnali deboli
isolati (SNR -16..-23) che JTDX (Sensitivity=2) aggancia restavano sotto soglia.
> **Nota:** `ft8_candidate_sync_threshold` (`FtxFt8Stage4.cpp:2870`) è un path
> secondario (`cq_only_decode`), NON la soglia principale — fuorviante in diagnosi.

### 2. Hash table dei callsign non-standard azzerata per ogni candidato (BUG)
`FtxFt8Stage4.cpp` (funzione di decode del singolo candidato) chiamava
`legacy_pack77_reset_context_c()` **per ogni candidato**, azzerando la hash table
`call→hash` (`g_legacy_pack77_context`, thread_local) prima di ogni decode.
L'unpack auto-salva i call completi (`saveHashCall`, `FtxMessageEncoder.cpp:1367+`),
ma il reset li cancellava subito → nessun accumulo → ~15% di `<...>`.
JTDX mantiene la hash table persistente per ore → risolve tutto.

### 3. (NON risolto) Separazione dei segnali sovrapposti — gap architetturale
Il grosso del divario residuo su banda affollata sono **cluster di segnali
sovrapposti** (es. 1500/1506/1517 Hz). Decodium fa la sottrazione (`lsubtract=1`,
`ftx_subtract_ft8_c`) su 5 passate, ma la qualità/profondità della
subtract-and-research è inferiore a JTDX (`nPreampass=4` + decoder ottimizzato).

## Modifiche applicate sul fork (proposte per review/assorbimento)

| # | File | Modifica | Effetto misurato |
|---|------|----------|------------------|
| 1 | `Detector/FtxDecodeBookkeeping.cpp` (`ftx_ft8_prepare_pass_c`) | per `ndepth>=4`: `local_syncmin *= 0.80` (e `*0.85` su pass≥4) → soglia sync deep più bassa sulle passate di sottrazione | conteggio **80%→~90%**, stabile, 0 stall; **opt-in** (solo con Deep Search) |
| 2 | `DecodiumBridge.cpp` (`maybeDispatchFt8EarlyDecode`) | early preview cap depth 2→3 con Deep Search (resta entro i ~2.2s pre-fine-slot) | **96% dei decode entro +3s** (timely); il fast pass finale resta depth 2 (+1.1s) |
| 3 | `Detector/FtxFt8Stage4.cpp` | rimosso `legacy_pack77_reset_context_c()` per-candidato; il context thread_local accumula come JTDX | **`<...>` 15%→~5%** (cala ancora con l'uptime); universale |

### Vincoli/lezioni dai test (cosa NON fare)
- **Multi-threading non scala**: alzare `ftThreads`/cap OpenMP 4→8→24 non ha dato
  guadagno (CPU <1 core anche con 24 thread richiesti su CPU 32-core). Il decode è
  serial-bound; la velocità di JTDX non è replicabile col solo parallelismo.
- **Final pass "deep"**: rendere il fast pass finale depth 4 lo rende **lento
  (+10.7s) e tardivo** e fa **scendere** il conteggio (82%) — il decode profondo non
  completa entro la finestra timely. Da NON fare senza decode parallelo reale.
- **Over-tuning syncmin** (0.72/0.80) regredisce a 88%: 0.80/0.85 è l'ottimo.
- Misurare sempre su **finestre lunghe** (15+ slot): finestre da 4 slot sono
  dominate dal rumore di propagazione e portano a conclusioni sbagliate.

## Raccomandazioni per il 100% (lavoro core, upstream)

1. **Sottrazione iterativa stile JTDX** (`nPreampass`): più cicli di
   subtract-strongest-and-research sul residuo, per scoprire i segnali sovrapposti.
   È il gap architetturale dominante su banda affollata.
2. **Parallelismo reale del decode** (across-candidate / across-band-segment) per
   permettere un decode profondo *entro la finestra timely* invece del follow-up
   tardivo (+13s). Oggi `run_main_passes` itera candidati in serie.
3. **Hash table persistente condivisa** (oltre al fix #3): un
   `sharedDecode77Context` (già esistente, `FtxMessageEncoder.cpp:3201`) popolato
   da tutti i decode e consultato come fallback, con clear solo su cambio banda —
   per portare i `<...>` verso 0 come JTDX anche subito dopo l'avvio.

## Stato finale del fork (1.0.280 + 3 fix)

~85-93% di JTDX (band-dependent) · 96% timely (≤+3s) · `<...>` ~5% · 0 stall · 0 underrun.
Da ~80% e 15% di call illeggibili. Le modifiche #1/#2 sono opt-in dietro "Deep Search".
