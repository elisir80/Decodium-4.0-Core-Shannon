# Decodium native SSTV compatibility matrix

Generated-document precursor, snapshot 2026-08-24.

At the starting commit Decodium has no SSTV code. Consequently every mode is
currently **not implemented and not tested**. This matrix intentionally does
not turn upstream code-path discovery into a Decodium support claim.

Legend: `—` means unavailable/unimplemented; `blocked` means a known protocol
or evidence conflict must be resolved before a truthful implementation claim.
Dimensions, duration and VIS will be generated from the canonical C++ registry
once a row has validated data; they are omitted here rather than guessed.

| Mode | Family/class | RX | TX | Auto detect | Independent Decodium vector/test | Current blocker or next proof |
|---|---|---:|---:|---:|---|---|
| Martin M1 | Martin/analog | — | — | — | none | Resolve sync precision; implement core/common fixture. |
| Martin M2 | Martin/analog | — | — | — | none | Resolve 160/320 geometry conflict. |
| Martin M3 | Martin/analog | blocked | blocked | — | none | Resolve SlowRX pixel/line inconsistency. |
| Martin M4 | Martin/analog | — | — | — | none | Obtain independent specification/vector. |
| Scottie S1 | Scottie/analog | — | — | — | none | Implement first-line ordering and independent fixture. |
| Scottie S2 | Scottie/analog | — | — | — | none | Resolve pySSTV geometry conflict. |
| Scottie DX | Scottie/analog | — | — | — | none | Independent long-duration fixture. |
| Scottie S3 | Scottie/analog | — | — | — | none | libsstv-only path; independent specification/vector. |
| Scottie S4 | Scottie/analog | — | — | — | none | libsstv-only path; independent specification/vector. |
| Robot 12 Colour | Robot colour/analog | — | — | — | none | libsstv-only path; historical spec/vector. |
| Robot 24 Colour | Robot colour/analog | blocked | blocked | — | none | Resolve 160x120/320x120/320x240 geometry. |
| Robot 36 Colour | Robot colour/analog | — | — | — | none | Implement alternating chroma and independent fixture. |
| Robot 72 Colour | Robot colour/analog | — | — | — | none | Implement line-pair chroma and independent fixture. |
| Robot B/W 8 | Robot mono/analog | — | — | — | none | Confirm corrected timing and VIS aliases. |
| Robot B/W 12 | Robot mono/analog | — | — | — | none | Catalogue/test R/G/B aliases. |
| Robot B/W 24 | Robot mono/analog | — | — | — | none | No audited QSSTV path; independent fixture. |
| Robot B/W 36 | Robot mono/analog | — | — | — | none | No audited decoder path; independent fixture. |
| Wraase SC2-60 | Wraase/analog | — | — | — | none | Only QSSTV lineage; independent vector. |
| Wraase SC2-120 | Wraase/analog | — | — | — | none | Resolve normative versus empirical timing. |
| Wraase SC2-180 | Wraase/analog | — | — | — | none | Authoritative timing and vector. |
| Pasokon P3 | Pasokon/analog | — | — | — | none | Independent colour-order fixture. |
| Pasokon P5 | Pasokon/analog | — | — | — | none | Independent colour-order fixture. |
| Pasokon P7 | Pasokon/analog | — | — | — | none | Independent colour-order fixture. |
| PD50 | PD/analog | — | — | — | none | Use current SlowRX RX plus separate TX oracle. |
| PD90 | PD/analog | — | — | — | none | Independent fixture and two-line tests. |
| PD120 | PD/analog | — | — | — | none | Independent fixture and two-line tests. |
| PD160 | PD/analog | — | — | — | none | Independent fixture and two-line tests. |
| PD180 | PD/analog | — | — | — | libsstv self-WAV only; not independent | Secure legal second producer/capture. |
| PD240 | PD/analog | — | — | — | none | Long-duration independent fixture. |
| PD290 | PD/analog | — | — | — | none | Long-duration independent fixture. |
| AVT24 | AVT/analog | — | — | — | none | Special sync plus independent spec/vector. |
| AVT90 | AVT/analog | — | — | — | none | Special sync plus independent spec/vector. |
| AVT94 | AVT/analog | — | — | — | none | Special sync plus independent spec/vector. |
| MP73 | MMSSTV extended/analog | — | — | — | none | Independent extended-VIS vector. |
| MP115 | MMSSTV extended/analog | — | — | — | none | Independent extended-VIS vector. |
| MP140 | MMSSTV extended/analog | — | — | — | none | Independent extended-VIS vector. |
| MP175 | MMSSTV extended/analog | — | — | — | none | Independent extended-VIS vector. |
| MR73 | MMSSTV extended/analog | — | — | — | none | Independent extended-VIS vector. |
| MR90 | MMSSTV extended/analog | — | — | — | none | Independent extended-VIS vector. |
| MR115 | MMSSTV extended/analog | — | — | — | none | Independent extended-VIS vector. |
| MR140 | MMSSTV extended/analog | blocked | blocked | blocked | none | QSSTV extended VIS collides with its MR175 row; independent waveform required. |
| MR175 | MMSSTV extended/analog | blocked | blocked | blocked | none | QSSTV `0x4A23` conflicts with Handbook `0x4C23`; independent waveform required. |
| ML180 | MMSSTV extended/analog | — | — | — | none | Independent extended-VIS vector. |
| ML240 | MMSSTV extended/analog | — | — | — | none | Independent extended-VIS vector. |
| ML280 | MMSSTV extended/analog | — | — | — | none | Independent extended-VIS vector. |
| ML320 | MMSSTV extended/analog | — | — | — | none | Independent extended-VIS vector. |
| MP73-Narrow | MMSSTV narrow/analog | — | — | — | none | Independent narrow/extended-VIS vector. |
| MP110-Narrow | MMSSTV narrow/analog | — | — | — | none | Independent narrow/extended-VIS vector. |
| MP140-Narrow | MMSSTV narrow/analog | — | — | — | none | Independent narrow/extended-VIS vector. |
| MC110-Narrow | MMSSTV narrow/analog | — | — | — | none | Independent narrow/extended-VIS vector. |
| MC140-Narrow | MMSSTV narrow/analog | — | — | — | none | Independent narrow/extended-VIS vector. |
| MC180-Narrow | MMSSTV narrow/analog | — | — | — | none | Independent narrow/extended-VIS vector. |
| FAX480 | related FAX | blocked | blocked | — | none | Resolve 512x500 versus 512x480; label separately. |
| HFFAX variants | related FAX | — | — | — | none | Enumerate authoritative IOC/RPM variants. |
| WEFAX variants | related FAX | — | — | — | none | Enumerate authoritative IOC/RPM variants. |

## FSK ID

| Capability | RX | TX | Evidence | Status |
|---|---:|---:|---|---|
| Six-bit identifier framing | — | — | `test_sstv_fskid`; QSSTV/SlowRX behaviour audit | Native symbol codec implemented and tested; detector, radio TX integration and independent waveform remain pending. |
| Raw symbols/confidence/checksum diagnostics | — | n/a | `test_sstv_fskid` | Native codec diagnostics implemented; checksum has only one complete audited producer lineage. |
| Sanitised callsign/custom text | n/a | — | `test_sstv_fskid` | Native validation/tone plan implemented; no on-air TX support claim until the streaming TX coordinator is proven. |

## Digital compatibility

| Protocol/profile | RX | TX | BSR/resume | Independent interoperability | Status |
|---|---:|---:|---:|---|---|
| HAMDRM profiles | — | — | — | none | Clean-room implementation required; restricted upstream code excluded. |
| KG-STV | — | — | — | none | Public authoritative protocol/vector not yet established. |

## Verification fields for future generated rows

When the registry generator replaces this precursor, every row must include
dimensions, nominal duration, standard/extended VIS, RX, TX, auto-detect,
independent source/vector, test command/result, QSSTV interoperability,
Robot36/SlowRX interoperability where applicable, and precise notes. A state
change requires both the registry change and recorded executable evidence.
