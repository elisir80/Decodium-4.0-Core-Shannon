# SSTV upstream provenance

Audit snapshot: 2026-08-23/24. All repositories were inspected in temporary
directories outside the Decodium checkout. No upstream source has been copied
or adapted at this stage.

## Audited revisions

| Project | Audited revision | Licence finding | Relevant paths and permitted use |
|---|---|---|---|
| QSSTV | `ON4QZ/QSSTV@8c27d6d169d8c6c197eb47c2089870e39bc06a02` | Root GPL-3.0; many analog files GPL-2.0-or-later; project notice requires attribution for work based in whole or part on QSSTV | `src/sstv/sstvparam.*`, `src/sstv/modes/*`, `src/sstv/{sstvrx,sstvtx,syncprocessor,visfskid}.*`, `src/dsp/*`, `src/drmrx/*`, `src/drmtx/*`. Behaviour/tables may be audited; any adaptation needs file-level review, SPDX/notice and attribution. Do not transplant the application architecture. |
| SlowRX mission fork | `dnet/slowrx@a50a4e2c291d852a950f25e77d411e77efd9cd89` | ISC, compatible when notice is retained | `modespec.c`, `common.h`, `vis.c`, `fsk.c`, `sync.c`, `video.c`, `pcm.c`. RX robustness/behaviour reference. The fork stopped in 2013. |
| SlowRX current upstream | `windytan/slowrx@ca6d7012ae788b5057646170bd86590a7f68bd69` | ISC | Same paths. Unlike the old fork, current upstream contains active PD decoding and a corrected Robot BW8 timing. Prefer this revision while preserving the mission fork in the comparison. |
| Robot36 | `xdsopl/robot36@75146a5342bf27a165f8790bcb33b56a6d96a2f8` (`v2`) | 0BSD | `Decoder.java`, `Mode.java`, `BaseMode.java`, `RGBModes.java`, Robot/PD/HFFax classes and streaming DSP classes. Behaviour-only/clean-room C++ reference; no Java runtime. |
| libsstv | `rimio/libsstv@193157a993ac34bfa074074004c9ddadcfe6fd15` | MIT | `src/libsstv.template.h`, `src/sstv.{h,c}`, `src/encoder.c`, `src/luts.*`, `util/genluts.py`. Encoder-only phase/timing oracle; retain MIT notice if adapted. |
| pySSTV | `dnet/pySSTV@d998fad154d3e6ad2d73af5add49beec0d2ab59f` | MIT | `pysstv/{sstv,color,grayscale}.py`, tests and CLI. Developer-only fixture/timing comparison; never a runtime dependency. |
| QT6SSTV | `pa2eon/QT6SSTV@6ae74b786af926d080bf97ac707395a806cf8e91` | No root licence file; manual says GPLv3 and inherited files are often GPL-2.0-or-later | Same QSSTV lineage under `src/sstv`, `src/dsp`, `src/drmrx`, `src/drmtx`; use only to study Qt6 migration. It is not an independent protocol confirmation. |

Repository URLs are recorded with the commit SHA in the audit evidence; all
future copied/adapted files must additionally record original path, author,
file-level licence, destination, reuse type and modifications in the table
below before they are committed.

## Imported/adapted component ledger

| Decodium destination | Upstream/path/SHA | Author and file licence | Reuse type | Modifications/notices |
|---|---|---|---|---|
| _None_ | — | — | — | No upstream code has been imported. |

The ledger is a merge gate. A component absent from it may be a clean-room
implementation based on public protocol behaviour, but it may not silently
contain upstream expressions or tables.

## Clean-room behaviour components

| Decodium component | Audited behaviour sources | Current evidence and limit |
|---|---|---|
| `src/sstv/core/SstvVisCodec.*` | QSSTV `8c27d6d`, SlowRX `a50a4e2c`, libsstv `193157a9`, pySSTV `d998fad1`, the MMSSTV manual and SSTV Handbook | Original C++17 symbol/framing codec. Standard VIS and MMSSTV wide extended-VIS deterministic tests pass; the DSP tone classifier and mode mapping are separate, and narrow N-VIS is not implemented by this codec. |
| `src/sstv/core/SstvFskIdCodec.*` | QSSTV `8c27d6d`, SlowRX `a50a4e2c`; pySSTV `d998fad1` used only to expose its incomplete framing | Original C++17 bit/symbol codec and tone plan. Tests cover framing, sanitisation, raw diagnostics and checksum failures. Checksum and complete tone envelope still have only one audited implementation lineage, so this is not independent on-air interoperability proof. |
| `src/sstv/core/SstvModeRegistry.*` and `SstvTimingAccumulator.*` | Mission catalogue, this audit and public protocol units | Original validation/fixed-point infrastructure. The canonical registry intentionally contains identities and blockers only; it claims no mode RX/TX support and invents no unresolved timing or VIS values. |

## Functional coverage observed upstream

- QSSTV/QT6SSTV contain RX and TX paths for M1/M2, S1/S2/DX,
  SC2-60/120/180, Robot C24/C36/C72 and BW8/BW12, P3/P5/P7, all required PD,
  AVT24/90/94, the required MP/MR/ML and narrow sets, and FAX480. They do not
  contain M3/M4, S3/S4, Robot C12, BW24/BW36 or an active broad HFFAX/WEFAX
  catalogue.
- SlowRX's old dnet fork contains RX paths for M1-M4, S1/S2/DX, Robot
  C24/C36/C72, BW8/BW12/BW24, SC2-120/180 and P3/P5/P7. Its PD rows do not
  prove PD RX: the decoder path is commented out. Current windytan upstream
  implements PD RX.
- Robot36 contains RX for M1/M2, S1/S2/DX, SC2-180, Robot C36/C72, all required
  PD modes and HFFax/raw fallback. It has no TX, FSK ID or HAMDRM.
- libsstv is explicitly encoder-only. It generates FAX480, M1-M4, S1-S4/DX,
  Robot C12/C24/C36/C72, BW8/BW12/BW24/BW36 (with R/G/B VIS aliases) and all
  required PD modes.
- pySSTV is TX-only for M1/M2, S1/S2/DX, C36, BW8/BW24, SC2-120/180,
  P3/P5/P7 and PD90-PD290 except PD50.
- HAMDRM is available only in the single QSSTV/QT6SSTV lineage. This is not
  independent interoperability evidence.

"Contains a path" means an upstream code path was observed, not that Decodium
supports the mode or that upstream interoperability was executed.

## Conflicts that require independent resolution

1. QSSTV assigns extended VIS `0x4A23` to both MR140 and MR175; QT6SSTV repeats
   the collision. The SSTV Handbook table instead lists MR175 as `0x4C23`.
   That identifies the likely table correction, but no independent waveform
   has yet been executed, so neither mapping may enter a verified mode registry.
2. QSSTV/libsstv often store parity-inclusive eight-bit VIS values while
   SlowRX/Robot36/pySSTV store the seven-bit payload. The Decodium schema must
   separate payload, parity and extended encoding before comparing values.
3. pySSTV declares width 160 for Martin M2 and Scottie S2; QSSTV, SlowRX,
   Robot36 and libsstv use 320. pySSTV is not the geometry oracle for these rows.
4. SlowRX M3 declares 0.2288 ms pixels with a 446.446 ms line, while libsstv
   uses 0.4576 ms pixels. SlowRX is internally inconsistent for this value.
5. Robot C24 is 160x120 in QSSTV, 320x120 in libsstv and 320x240 in SlowRX.
   Sampled, transmitted and displayed geometry must be distinguished and tested.
6. FAX480 is 512x500 in QSSTV and 512x480 in libsstv.
7. Robot monochrome has R/G/B VIS aliases in libsstv while most other sources
   implement a single alias. Aliases must be catalogued, not collapsed silently.
8. QSSTV rounds some analog values (Martin sync to 5 ms) where other references
   use 4.862 ms. TX timing needs a normative source or independent capture.
9. pySSTV SC2-120 contains empirical porch/sync workarounds, not normative proof.
10. QSSTV FSK ID uses six-bit LSB symbols, 22 ms bits, 1900/2100 Hz, `0x2A`
    header, `0x01` end and XOR checksum. SlowRX does not validate the checksum;
    pySSTV omits checksum and the complete preamble. A separate vector is needed.

All conflicts remain visible in `MODE_CATALOG.md` and `MODE_MATRIX.md`. A test
that merely matches one side does not resolve a conflict.

## Fixture provenance

The only upstream audio fixture found was libsstv's self-generated PD180
`docs/sample.wav`, PCM16 mono at 48 kHz, SHA-256:

```text
aced520cf24941e55868045793d7e67449210ef49c496c307912abf7b45be25b
```

Its source bitmap SHA-256 is:

```text
8485f8bc22caa5fdc122c39aaa46253490bad173bfb6dc6c1d8f788e5e4b8e15
```

It may be used as a cross-decoder stimulus after redistribution review, but it
is not independent proof of libsstv-compatible TX. pySSTV contains limited
self-golden data, mainly Martin M1, and no multi-mode WAV set. No audited repo
contained independent AVT, MP/MR/ML, narrow-mode or HAMDRM audio vectors.

Fixture metadata must record producer/version, legal redistribution status,
source image hash, audio hash, sample format/rate, claimed mode and whether the
producer shares lineage with the implementation under test.

## HAMDRM exclusions

The following QSSTV material must not be copied:

- `src/drmrx/newfft.cpp`: restricted to research/education and no-fee use;
- `src/drmrx/lubksb.cpp`, `ludcmp.cpp` and `nrutil.*`: Numerical Recipes
  derivation without a suitable free redistribution grant;
- `src/utils/rs.cpp`: ambiguous "GNU public license" without a version;
- DRM/JP2 files with insufficient file-level provenance until authorship and
  licence are resolved.

HAMDRM will therefore use a clean-room protocol implementation and audited
dependencies such as FFTW and OpenJPEG. The audit of behaviour does not grant a
right to copy restricted source.

## Notice and release requirements

Before merging any adaptation:

- add SPDX identifiers/copyright notices required by the source file;
- preserve ISC/MIT/0BSD text where applicable;
- add the QSSTV attribution when QSSTV expression is adapted;
- update source and binary third-party notices and packaging manifests;
- verify GPL compatibility for each file, not only the repository root;
- include generator/tool provenance for developer-only Python scripts;
- repeat the audit if an upstream revision is changed.
