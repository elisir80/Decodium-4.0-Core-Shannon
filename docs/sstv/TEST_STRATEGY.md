# Native SSTV test strategy

Status: Milestone 0 contract, 2026-08-23.

## Baseline evidence

The unmodified source at
`119947690e2d8a1df99a75f98b915f2115df99e7` was configured and built on macOS
ARM64 with:

```zsh
cmake -S . -B build
cmake --build build --parallel "$(sysctl -n hw.ncpu)"
ctest --test-dir build --output-on-failure -j "$(sysctl -n hw.ncpu)"
```

Configuration and the 734-step build completed successfully. CTest passed
37/37 tests in 96.64 seconds. This is a regression baseline, not SSTV evidence:
there were no SSTV tests or sources. `test_qt_helpers` reported 154 subtests
passed and 12 skipped because several FST4/MSK144/FT8/FT2 fixtures or comparators
were unavailable. The accepted-limit FT8 case also permits a missed decode via
`DECODIUM_WEAK_TEST_ACCEPT_MISS=1`. Hardware RTL-SDR input is built but not
registered in CTest without explicit hardware opt-in.

The build emitted existing CMake policy/Qt private-header warnings and macOS
deployment-target warnings for newer Homebrew libraries. Ninja recovered a
premature dependency-file EOF before rebuilding; no concurrent build may use
the same build directory.

## Evidence classes

Every result is labelled as one of:

1. **unit**: exact pure-code behaviour;
2. **self-generated**: Decodium encoder and decoder through a channel model;
3. **independent synthetic**: another implementation generated the waveform;
4. **independent real recording**: received audio with source/permission;
5. **integration-local**: Qt threads/files/SQLite/local HTTP/mock PTT;
6. **rendered UI**: actual QML application interaction and screenshot;
7. **hardware/radio**: named devices/radio and observed PTT/audio result;
8. **platform/package**: actual OS build and installed/bundled runtime check.

No stronger claim may be inferred from a weaker evidence class. In particular,
self-round-trip does not prove interoperability, a mock PTT test does not prove
a radio transmission, and a macOS build does not prove Windows/Linux support.

## Test target layout

SSTV tests live below `tests/sstv` and are registered in CTest. Pure protocol
and DSP tests link `decodium_sstv_core`; Qt integration tests link only the
smallest relevant integration target. Every SSTV target explicitly requests
C++17 because `tests/CMakeLists.txt` currently defaults to C++11.

The native foundation runs on 2026-08-24 added and passed ten labelled CTest
targets:

```text
ctest --test-dir build -L sstv --output-on-failure
10/10 passed: mode registry, timing accumulator, VIS codec, FSK ID codec,
resampler, audio/replay buffers, tone generator/TX pull stream, RX state
machine, tone detector and progressive image/colour core
```

The second tranche also passed 33 resampler/buffer QtTest cases and 10 tone/TX
stream cases in standalone sanitizer runs (ASan/UBSan, plus TSan for the audio
buffer). The RX state machine/tone detector add 34 QtTest cases, while the
progressive image/colour core adds 14; their standalone ASan/UBSan runs and
strict-warning builds also passed. The in-tree CTest execution passed every
registered SSTV executable. These
remain protocol/DSP unit results: they prove bounded chunk-independent sample
rate conversion, queue/replay policy, fractional duration scheduling and
phase-continuous pull generation, deterministic acquisition state transitions,
discrete-tone classification and bounded progressive frame assembly. They do
not prove a complete analog mode encoder/decoder, live sound hardware, CAT/PTT
sequencing or independent application interoperability.

Planned groups:

- `test_sstv_mode_registry`: IDs, names, dimensions, VIS uniqueness/conflicts,
  rational timing, component ordering, capability/evidence invariants;
- `test_sstv_timing`: fractional sample accumulation for all supported rates,
  bounded accumulated error and encoder total duration;
- `test_sstv_vis`: standard/extended VIS bit order, parity, framing, drift,
  offset, truncation, noise, false positives and repeated headers;
- `test_sstv_fskid`: allowed alphabet, sanitisation, raw symbols, malformed
  input, confidence and deterministic RX/TX framing;
- `test_sstv_colour`: fixed reference pixels for every distinct colour system
  and range, including Robot/PD alternating chroma rules;
- `test_sstv_encoder`: header/VIS/segments/line count/level/phase continuity for
  every TX-capable mode;
- `test_sstv_decoder`: legally redistributable independent fixtures;
- `test_sstv_roundtrip`: waveform passed through an independent channel model;
- `test_sstv_impairments`: acquisition, degradation and graceful partial exit;
- `test_sstv_wav`: RIFF boundaries, streaming writes and hostile imports;
- `test_sstv_storage`: paths, atomic files, migration, concurrency and quota;
- `test_sstv_sharing`: deterministic local server and persistent queues;
- `test_sstv_security`: resource ceilings, redirects, JSON/path validation and
  secret/log redaction;
- `test_sstv_tx_integration`: exclusive TX ownership and PTT release invariants;
- `test_hamdrm`: profiles, objects, CRC, BSR, retransmission and malformed data.

## Registry and protocol correctness

For every catalogued mode, data-driven rows verify:

- stable unique ID and non-empty family/name;
- analog/digital/related-FAX classification;
- dimensions, line/display counts and scans-per-line;
- standard/extended VIS encoding, with documented duplicate/conflict handling;
- component order, colour system and subsampling;
- positive segment durations whose sum matches line and image duration within
  an explicit independent-reference tolerance;
- RX/TX/auto-detect flags that cannot become `verified` without named evidence.

VIS testing samples exact nominal events first, then randomized block boundaries,
frequency offsets, timing drift, bad parity, bad start/stop bits, damaged break,
back-to-back headers and noise-only false-positive runs. FSK tests preserve raw
symbols and reject or replace invalid characters deterministically.

## Encoder proof

For every TX row, encode a fixed colour bars/grid/edge test card at all required
sample rates. Measure event boundaries from PCM, not from private encoder state.
Assertions cover:

- leader/break/VIS/extended-VIS tones and durations;
- phase continuity across every segment and pixel;
- exact transmitted lines and component sequence;
- porch, separator, sync and pixel frequency bounds;
- average fractional timing error and complete waveform duration;
- default headroom, peak and absence of integer clipping;
- optional FSK ID placement and duration;
- streaming cancellation and valid atomic WAV output.

At least one external oracle must agree before the mode can be marked TX
interoperable. pySSTV/libsstv are suitable only for their implemented families;
QSSTV-derived expected values require licence/provenance review and do not count
as a second independent implementation by themselves.

## Decoder and impairment proof

Independent fixtures are decoded with randomized input block sizes. The clean
gate requires correct mode, dimensions, ordering, completion and a documented
image metric. Analog impairment sweeps combine:

- frequency offset and slow drift;
- ±300 ppm and wider exploratory sample-clock error;
- AWGN at recorded SNR values;
- gain changes, clipping and DC offset;
- 50/60 Hz hum and narrow interferers;
- impulses, short drop-outs, missing/false sync and echo;
- resampling and stereo imbalance;
- truncated header/start/end and back-to-back images.

Initial common-mode acquisition target is at least ±100 Hz. Severe cases need
not produce a complete image, but must terminate within a mode-specific bound,
preserve valid lines as partial, stay within memory limits and return to leader
search. Noise-only corpora measure false acquisitions explicitly.

PSNR/SSIM and per-channel error are used only with documented thresholds. Exact
pixels are required for pure colour-conversion vectors, not realistic analog
channels.

## Threading and performance proof

Tests randomize producer block sizes and cancellation times while measuring:

- bounded input/replay queue depth and reported drop count;
- no image/file/SQL/network operation from the audio callback;
- worker affinity and no QObject cross-thread warnings;
- GUI update throttle and dirty-rectangle size;
- average/max DSP block time below the available real-time budget;
- clean stop on workspace exit, mode change and application shutdown;
- negligible inactive-path callbacks/CPU/allocation;
- no regression in an existing panadapter cadence benchmark.

Performance reports record machine, build type, sample rate, mode, corpus and
commit. One fast run is not a cross-platform performance claim.

## Storage, parser and network security proof

Hostile tests impose hard compressed bytes, dimensions, pixel/memory, WAV
duration/chunk, JSON depth/field and response-byte ceilings. Cases cover integer
overflow, truncated RIFF, decompression bombs, invalid MIME, traversal,
absolute/reserved names, Unicode normalization, symlink escape and interrupted
atomic writes.

The local network server verifies HTTPS policy separately from localhost test
exceptions and covers origin-changing redirects, credential forwarding,
timeouts, rate limits, malformed JSON, duplicate completion, wrong hashes,
expiry, cancellation, restart/resume, permanent-auth failures and log redaction.
Fuzz targets retain crashing inputs and run with ASan/UBSan where supported.

## TX safety proof

An instrumented fake existing Decodium coordinator—not a second SSTV PTT
implementation—exercises PTT confirmation, timeout, audio failure, CAT/radio
disconnect, underrun, sleep/shutdown simulation and cancellation during header,
image and FSK ID. Every path asserts:

- no PCM before policy approval;
- no overlap with weak-signal TX;
- bounded watchdog lifetime;
- exactly one ownership release and final PTT-off request;
- restoration of the previous RX state.

Real-device tests separately record OS, radio, interface, serial settings,
audio device, observed lead/tail and measured output. They are recommended
before release and never replaced by the fake coordinator.

## Continuous integration and packaging

Update workflow path filters for `src/sstv/**`, `tests/sstv/**`,
`qml/decodium/sstv/**`, the integration files and this documentation. Required
jobs are:

- pure core/codec tests on maintained Windows, macOS and Linux runners;
- full Decodium build with analog SSTV enabled;
- analog-only and analog+HAMDRM configurations when dependencies are present;
- QML lint plus a headless startup smoke test;
- sanitizer/fuzz job on a suitable Linux runner;
- package inspection for QML files, Qt image plugins and optional OpenJPEG;
- separate opt-in jobs for large pinned fixture packs and hardware.

The final report lists the exact jobs, artifacts, test totals, skips and platform
limitations. A green packaging workflow with `BUILD_TESTING=OFF` is build/package
evidence only.
