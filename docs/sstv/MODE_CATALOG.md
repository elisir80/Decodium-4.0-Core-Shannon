# SSTV mode catalogue

Status: discovery catalogue, not a support claim. Snapshot 2026-08-24.

Decodium contained no SSTV implementation at the starting commit. Every row is
therefore initially `catalogued/unimplemented`; `MODE_MATRIX.md` is the
authoritative statement of verified capability. Exact timing and VIS fields
will live in one C++ registry and generated documentation once conflicts below
are resolved. They will not be copied into independent hand-maintained tables.

## Classification and source keys

- `analog`: amateur FM-tone SSTV with VIS or a documented no-VIS structure.
- `related-fax`: image facsimile sharing DSP components but not advertised as
  standard amateur analog SSTV.
- `digital`: HAMDRM or another separately specified digital object protocol.
- Source keys: `Q` QSSTV, `S` current SlowRX, `R` Robot36, `L` libsstv, `P`
  pySSTV. QT6SSTV is the same QSSTV lineage and is not an independent key.
- `path` below means observed implementation path, not executed interoperability.

## Mandatory analog families

| Stable candidate ID | Display name | Class | Observed independent paths | Discovery note/blocker |
|---|---|---|---|---|
| `martin-m1` | Martin M1 | analog | Q RX/TX; S RX; R RX; L/P TX | Common starting mode; exact sync 5 ms versus 4.862 ms conflict must be resolved. |
| `martin-m2` | Martin M2 | analog | Q RX/TX; S/R RX; L/P TX | pySSTV width 160 conflicts with 320 elsewhere. |
| `martin-m3` | Martin M3 | analog | S RX; L TX | SlowRX pixel/line timing is internally inconsistent; independent vector required. |
| `martin-m4` | Martin M4 | analog | S RX; L TX | No Q/Robot36 path; obtain independent capture/specification. |
| `scottie-s1` | Scottie S1 | analog | Q RX/TX; S/R RX; L/P TX | Scottie first-line/sync ordering must be encoded explicitly. |
| `scottie-s2` | Scottie S2 | analog | Q RX/TX; S/R RX; L/P TX | pySSTV width 160 conflicts with 320 elsewhere. |
| `scottie-dx` | Scottie DX | analog | Q RX/TX; S/R RX; L/P TX | Long-duration performance and cancellation required. |
| `scottie-s3` | Scottie S3 | analog | L TX | Specification/vector not yet independently verified. |
| `scottie-s4` | Scottie S4 | analog | L TX | Specification/vector not yet independently verified. |
| `robot-c12` | Robot 12 Colour | analog | L TX | Historical name/VIS/line-pair behaviour need independent documentation; absent from Q/S/R. |
| `robot-c24` | Robot 24 Colour | analog | Q RX/TX; S RX; L TX | Geometry conflict: 160x120, 320x120 and 320x240 across sources. |
| `robot-c36` | Robot 36 Colour | analog | Q RX/TX; S/R RX; L/P TX | Robot alternating chroma and displayed/transmitted rows need exact tests. |
| `robot-c72` | Robot 72 Colour | analog | Q RX/TX; S/R RX; L TX | Robot line-pair chroma requires mode-specific tests. |
| `robot-bw8` | Robot B/W 8 | analog | Q RX/TX; S RX; L/P TX | Current SlowRX timing correction preferred; catalogue R/G/B VIS aliases separately. |
| `robot-bw12` | Robot B/W 12 | analog | Q RX/TX; S RX; L TX | Multiple VIS aliases in libsstv. |
| `robot-bw24` | Robot B/W 24 | analog | S RX; L/P TX | Multiple VIS aliases; absent from QSSTV. |
| `robot-bw36` | Robot B/W 36 | analog | L TX | No audited decoder path; multiple VIS aliases. |
| `wraase-sc2-60` | Wraase SC2-60 | analog | Q RX/TX | No independent audited path/vector yet. |
| `wraase-sc2-120` | Wraase SC2-120 | analog | Q RX/TX; S RX; P TX | pySSTV contains empirical timing workaround; do not treat it as normative. |
| `wraase-sc2-180` | Wraase SC2-180 | analog | Q RX/TX; S/R RX; P TX | Obtain independent vector and authoritative naming/timing. |
| `pasokon-p3` | Pasokon P3 | analog | Q RX/TX; S RX; P TX | Sequential colour ordering requires independent vector. |
| `pasokon-p5` | Pasokon P5 | analog | Q RX/TX; S RX; P TX | Sequential colour ordering requires independent vector. |
| `pasokon-p7` | Pasokon P7 | analog | Q RX/TX; S RX; P TX | Sequential colour ordering requires independent vector. |
| `pd-50` | PD50 | analog | Q RX/TX; S RX; R RX; L TX | pySSTV omits this mode; current SlowRX, not old fork, supplies active PD RX. |
| `pd-90` | PD90 | analog | Q RX/TX; S/R RX; L/P TX | Two-line luminance/chroma structure. |
| `pd-120` | PD120 | analog | Q RX/TX; S/R RX; L/P TX | Two-line luminance/chroma structure. |
| `pd-160` | PD160 | analog | Q RX/TX; S/R RX; L/P TX | Two-line luminance/chroma structure. |
| `pd-180` | PD180 | analog | Q RX/TX; S/R RX; L/P TX | libsstv self-generated WAV exists but is not independent proof. |
| `pd-240` | PD240 | analog | Q RX/TX; S/R RX; L/P TX | Long-duration buffer/performance tests required. |
| `pd-290` | PD290 | analog | Q RX/TX; S/R RX; L/P TX | Long-duration buffer/performance tests required. |
| `avt-24` | AVT24 | analog | Q RX/TX | No independent audited vector; AVT has distinct sync structure. |
| `avt-90` | AVT90 | analog | Q RX/TX | No independent audited vector; special sync required. |
| `avt-94` | AVT94 | analog | Q RX/TX | No independent audited vector; special sync required. |
| `mp-73` | MP73 | analog | Q RX/TX | Extended VIS; independent vector/specification missing. |
| `mp-115` | MP115 | analog | Q RX/TX | Extended VIS; independent vector/specification missing. |
| `mp-140` | MP140 | analog | Q RX/TX | Extended VIS; independent vector/specification missing. |
| `mp-175` | MP175 | analog | Q RX/TX | Extended VIS; independent vector/specification missing. |
| `mr-73` | MR73 | analog | Q RX/TX | Extended VIS; independent vector/specification missing. |
| `mr-90` | MR90 | analog | Q RX/TX | Extended VIS; independent vector/specification missing. |
| `mr-115` | MR115 | analog | Q RX/TX | Extended VIS; independent vector/specification missing. |
| `mr-140` | MR140 | analog | Q RX/TX | QSSTV extended VIS `0x4A23` collides with its MR175 row. Blocked pending independent waveform validation. |
| `mr-175` | MR175 | analog | Q RX/TX | QSSTV says `0x4A23`; SSTV Handbook says `0x4C23`. Likely table correction recorded, but still blocked pending independent waveform validation. |
| `ml-180` | ML180 | analog | Q RX/TX | Extended VIS; independent vector/specification missing. |
| `ml-240` | ML240 | analog | Q RX/TX | Extended VIS; independent vector/specification missing. |
| `ml-280` | ML280 | analog | Q RX/TX | Extended VIS; independent vector/specification missing. |
| `ml-320` | ML320 | analog | Q RX/TX | Extended VIS; independent vector/specification missing. |
| `mp-73-narrow` | MP73-Narrow | analog | Q RX/TX | Narrow/extended VIS; independent vector/specification missing. |
| `mp-110-narrow` | MP110-Narrow | analog | Q RX/TX | Narrow/extended VIS; independent vector/specification missing. |
| `mp-140-narrow` | MP140-Narrow | analog | Q RX/TX | Narrow/extended VIS; independent vector/specification missing. |
| `mc-110-narrow` | MC110-Narrow | analog | Q RX/TX | Narrow/extended VIS; independent vector/specification missing. |
| `mc-140-narrow` | MC140-Narrow | analog | Q RX/TX | Narrow/extended VIS; independent vector/specification missing. |
| `mc-180-narrow` | MC180-Narrow | analog | Q RX/TX | Narrow/extended VIS; independent vector/specification missing. |

Additional Wraase, AVT and MMSSTV variants found later will be added only with
a citable specification and distinct identity. Similar line duration is not
enough to invent a mode.

## Related image modes

| Stable candidate ID | Display name | Class | Observed paths | Discovery note/blocker |
|---|---|---|---|---|
| `fax-480` | FAX480 | related-fax | Q RX/TX; L TX | Geometry conflict: QSSTV 512x500 versus libsstv 512x480. Must not be labelled standard analog SSTV. |
| `hffax` | HFFAX | related-fax | R RX | This is a category until IOC/line-rate variants are authoritatively enumerated. |
| `wefax` | WEFAX | related-fax | none in audited active paths | Research IOC/RPM variants and legal vectors before adding registry rows. |

## Digital protocols

| Stable candidate ID | Display name | Class | Observed paths | Discovery note/blocker |
|---|---|---|---|---|
| `hamdrm` | HAMDRM / digital SSTV | digital | Q RX/TX | Separate subsystem. One lineage only, no audited independent vector, and restricted QSSTV source exclusions require clean-room implementation. |
| `kg-stv` | KG-STV | digital | none in audited open references | Do not equate with HAMDRM. Requires a public authoritative specification and legal validation vectors. |

HAMDRM profile IDs will be catalogued separately by occupied bandwidth,
robustness, constellation, protection, coding, interleaver and source coding.

## VIS and timing normalization rules

The eventual `SstvModeSpec` must store separately:

- seven-bit standard VIS payload, parity bit and bit order;
- extended-VIS bytes/encoding, not an ambiguous packed integer;
- transmitted, sampled and displayed dimensions;
- rational microseconds for every segment and derived line/image duration;
- colour system, component order, subsampling and lines represented per scan;
- provenance/evidence identifiers and explicit RX/TX/auto-detect states.

Nominal leader/header, black/white/sync/separator frequencies are protocol data
in the core registry, never UI literals. A fractional sample accumulator maps
rational durations to integer sample blocks without line-by-line drift.

## Discovery exit criteria

A row may move from discovery to implementation only after its conflicts are
resolved or explicitly represented. A row may move to `verified` only when:

1. timing, VIS and colour/line sequence are documented;
2. RX and/or TX implementation has deterministic tests;
3. an independent implementation or legally usable reference vector was run;
4. the test result and commit are named in `MODE_MATRIX.md`;
5. release/user documentation states only the verified direction/capability.
