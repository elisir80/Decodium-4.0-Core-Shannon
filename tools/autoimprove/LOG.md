# RX self-improvement log

Newest first. Every round: hypothesis, measurement, verdict. Negative results stay here so they
are not tried again. Numbers: `rx_score.py` on build 1.0.708 + the change, same scenes.

## Round 1 (2026-10-22) - candidate search on real noise

Baseline (1.0.708, FT2): ft2real decoded TRAIN 81/200, HELDOUT 35/200 (rheld) and 61/200 (rheld2).
AWGN scenes: 140/240 train, 91/160 heldout, 228 ms CPU per dispatch, 0.40 s latency.

Where the headroom is: forcing the true frequency onto the decoder (genie, `DECODIUM_FT2_GENIE_F`)
gives 81 -> 108 on TRAIN and 35 -> 71 on HELDOUT. So on real band noise the candidate SEARCH loses up
to a third of the decodable frames (AWGN scenes never show it: there the search is already exact).
Candidate trace of the 119 missed TRAIN slots: only 35 had a candidate within +-12 Hz of the truth;
48 had it one tone (41.67 Hz) away, 16 two tones away, 5 had none. The peak of the 15-cell smoothed
spectrum (FtxCandidates.cpp) lands on the wrong tone of the 4-tone FT2 lump when the band is not white.

### KEPT (needs the owner's OK to release): tone-alias candidates
`DECODIUM_FT2_ALIAS=n` / `ftx_ft2_set_tone_alias_c(n)`: for every candidate also try f0 +- k*41.67 Hz,
k = 1..n, as extra candidates appended after the normal ones; first search at double step; an alias
is dropped unless its sync (smax) beats its parent's by 0.10; skipped if the parent already decoded.
Default OFF (bit-identical) unless the app sets it: DecodiumBridge sets 2, and 0 in Low CPU / Slow-PC.

| n | ft2real TRAIN | HELDOUT rheld | HELDOUT rheld2 | AWGN decoded (T/H) | CPU / dispatch | latency |
|---|---|---|---|---|---|---|
| 0 | 81 | 35 | 61 | 140 / 91 | 228 ms | 0.40 s |
| 1 | 89 | 54 | 83 | 140 / 89 | 271 ms (+19 %) | 0.50 s |
| 2 | 96 | 59 | 92 | 141 / 92 | 286 ms (+25 %) | 0.52 s |

No false lines on the injected sets (600 slots) and none on 1000 slots of plain recorded background.
The CPU rule (+10 %) is exceeded on purpose: the gain is real-band only. Slow computers keep it off.

### Rejected / no gain (do not retry)
* `DECODIUM_FONDO=entrambi`: +5 TRAIN, 0 HELDOUT, +17 % CPU. `mediana`: +3 / -2. `0` (off): -10.
* One-at-a-time env switches on ft2real (no gain on both sets): COERENTE (+1/+1), DRIFT_SEARCH (+1/0),
  AP_SOFT (+0, 19 false lines), AP_CQ=0, SYNC_HZ 8 (-3/-5) and 16 (0/+3), LDPC_MAX_HARD 22 (0),
  38 (+1, 85 false lines), 46 (+1, 313 false lines), SUB_NFILT 1000/4000, FONDO_CELLE 9/39,
  STAGE7_LOCAL_RESCUE_BUDGET 64.
* Alias parents limited to the top-N peaks (N = 4, 6, 8): loses most of the gain (the right parent is
  often a weak peak in a busy band).

### Open ideas
* A better lump-centre estimator in getcandidates2 (half-power edges instead of the smoothed peak)
  would give the alias gain without the CPU cost; needs a savg/sbase dump to test offline.
* `ALIAS=3`, and alias only after the parent failed twice (async path).
* Memory-based knobs (ACCUMULO, STORICO) need repeated-station scenes (`mescola_ft2.py --sequenze`).
