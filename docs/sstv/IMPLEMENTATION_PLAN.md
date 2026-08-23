# Native SSTV implementation plan

Status: active design contract, 2026-08-23.

This plan implements SSTV inside the existing Decodium4 process. It does not
define a companion program, a second audio capture path, or a runtime bridge to
QSSTV, Python, Java, or Android code. The baseline is tag `v1.0.583`, commit
`119947690e2d8a1df99a75f98b915f2115df99e7`, on branch
`feature/native-sstv`.

## Invariants

- The production application remains `decodium_qml` (output name `decodium`)
  with its QML tree copied by the existing `sync_decodium_qml` target.
- RX samples enter SSTV only from the existing Decodium audio fan-out. Local
  sound-card, RTL-SDR, TCI and DecoPort sources must converge before the SSTV
  adapter; SSTV never opens a `QAudioSource`.
- TX ownership is granted by the existing Decodium TX/PTT policy. SSTV never
  opens a serial port or keys a rig directly.
- Core codecs do not depend on QML, a sound device, the gallery, networking, or
  mutable global mode state.
- Heavy DSP, image encoding, SQLite and networking never run on the GUI or
  audio callback thread.
- The project stays at C++17. Tests currently set a directory default of C++11;
  each SSTV test target will explicitly request C++17 rather than changing
  unrelated test targets.
- Analog SSTV remains usable when HAMDRM, remote sharing, or both are disabled.
- A mode is not marked supported until its timing, colour order, VIS handling,
  deterministic tests, and an independent source/vector are recorded in the
  generated mode matrix.

## Target structure

The implementation will use target-scoped CMake integration and these native
modules:

```text
src/sstv/
  core/         value types, registry, timing, VIS, FSK ID, colour conversion
  dsp/          resampling, tone/FM detection, sync, AFC, slant and metrics
  rx/           bounded audio ingress, replay buffer and analog RX state machine
  tx/           image preparation, DDS encoder, WAV stream and TX state machine
  integration/  Decodium audio, settings, controller and fail-safe TX ownership
  storage/      atomic files, SQLite worker, thumbnails and gallery model
  sharing/      providers, persistent queues, manifests and incoming inbox
  digital/      separately gated HAMDRM codecs, profiles, objects and BSR
qml/decodium/sstv/
tests/sstv/
docs/sstv/
```

`decodium_sstv_core` will be a C++17 static library linked by focused tests and
the Decodium application target. Qt-facing integration will be separate from
the codec library so command-line vector tests cannot accidentally depend on
QML or hardware. Optional libraries will be attached to the smallest target
that needs them.

## Delivery sequence and gates

### M0: audit and baseline

Deliverables:

- starting SHA/branch and clean-worktree evidence;
- actual audio, TX/PTT, QML, storage, security, network and packaging audit;
- upstream commit/licence inventory;
- the eight mandatory design documents;
- a clean baseline build and complete current CTest run.

Gate: the documents contain repository paths and observed limitations, the
baseline commands and results are recorded, and no production SSTV code has
been added before the audit.

### M1: reusable protocol core and encoder foundation

Implement in this order:

1. strongly typed `SstvModeId`, classification/family, capabilities and
   rational microsecond timing fields;
2. immutable canonical registry with uniqueness and consistency validation;
3. fractional sample accumulator that preserves long-run duration at every
   supported sample rate;
4. standard and extended VIS parser/generator with raw bits and confidence;
5. FSK ID framing, sanitisation, deterministic RX/TX symbol codecs;
6. audited RGB/GBR/Y-RY-BY/monochrome conversions;
7. phase-continuous DDS tone stream and bounded PCM sink interface;
8. deterministic analog encoder and streaming RIFF/WAV writer.

Gate: registry/timing/VIS/FSK/colour tests pass; common TX modes match an
independent timing oracle; phase continuity and exact segment counts are
measured. A clean self-round-trip alone is insufficient.

### M2: streaming analog receiver

Implement a bounded SPSC-style ingress queue fed by the current PCM signal and
a worker-owned pipeline:

1. source-rate metadata and stateful anti-alias resampler;
2. DC blocker, level/clipping metrics and conservative optional preprocessing;
3. leader/break detector and frequency-offset estimate;
4. VIS state machine and manual/no-VIS entry;
5. streaming FM estimator, sync tracker and line-period classifier;
6. per-line timing/slant correction and missing-sync prediction;
7. progressive dirty-rectangle events throttled before QML;
8. bounded replay buffer, partial-image completion and immediate return to
   leader search.

Gate: common modes decode independent fixtures; the state machine terminates
cleanly under truncation/corruption; ±100 Hz acquisition and ±300 ppm clock
error targets are measured; queue drops and DSP time are exported.

### M3: complete analog catalogue

Move one family at a time from `catalogued` to `verified`: Martin, Scottie,
Robot colour, Robot monochrome, Wraase, Pasokon, PD, AVT, MP/MR/ML, MMSSTV
narrow modes, then separately classified FAX/HFFAX/WEFAX variants. Each family
gets mode-specific colour/line tests and at least one independent oracle or
legally redistributable vector. Conflicting specifications remain blocked in
the matrix until resolved; they are never silently guessed.

Gate: every mandatory mode has an explicit RX/TX/auto-detect status and proof
cell. No user or release text says "all modes" unless every required row is
verified.

### M4: storage, gallery and native QML workspace

Add a controller and C++ models registered by `main_qml.cpp`, then a navigable
SSTV workspace under the existing QML application. Implement receive,
transmit, gallery, sharing, settings and diagnostics pages with existing theme
and `qsTr` localisation conventions.

Storage uses `QStandardPaths`, `QSaveFile`, content-validated `QImageReader`, a
named worker-thread SQLite connection, versioned transactional migrations and
path-only image records. Thumbnails are lazy and gallery changes are
incremental.

Gate: QML lint and rendered smoke checks pass; a large synthetic gallery stays
incremental; atomic-save, migration, quota-preview and hostile-image limits are
tested. The feature is reachable from normal Decodium navigation.

### M5: Decodium TX/CAT/PTT integration

The SSTV coordinator validates prepared content, obtains exclusive TX
ownership, waits for policy-approved PTT, observes lead/tail delays, streams
PCM through the existing output selection, watches progress/underruns, and
releases ownership on every success, cancellation, error, disconnect,
shutdown and destructor path. A fail-safe guard is mandatory.

Gate: tests cover PTT success/timeout, cancellation at header/image/FSK,
device loss, watchdog and concurrent weak-signal TX rejection. WAV export and
internal loopback never key the radio. Real-radio validation remains a separate
manual result, not inferred from mocks.

### M6: secure remote sharing

Implement the provider-neutral persistent state machine first, then the local
test provider, generic HTTPS REST, WebDAV over HTTPS and pre-signed PUT flows.
Audit the existing DecoPort/WebSocket channel before deciding whether it can
carry a Decodium relay provider. No fictional production service is embedded.

Gate: local HTTP integration tests cover auth redaction, idempotency, restart,
resume, hash failure, redirect-origin restrictions, expiry and inbox rejection;
the OpenAPI description matches the implementation. Remote sharing stays
opt-in and local SSTV works with no credentials or service.

### M7: separate HAMDRM subsystem

After the analog path is stable, implement a separate profile registry,
waveform RX/TX, object segmentation/integrity, partial persistence, BSR and
retransmission. OpenJPEG is optional, audited and target-scoped. QSSTV is an
interoperability reference; code with incompatible or non-commercial/research
licence terms is excluded and replaced clean-room.

Gate: enabled and disabled builds pass; known-good independent vectors cover
each advertised profile; malformed objects and carrier loss fail safely. The
UI never equates HAMDRM with analog SSTV or KG-STV.

### M8: hardening and release readiness

Add fuzz targets for VIS, FSK, WAV, manifests, chunk ranges, image limits and
HAMDRM objects; sanitizer CI; performance counters/benchmarks; full docs and
notices; and platform build/package jobs. Run complete regressions and verify
actual produced bundles contain required Qt image plugins and optional native
libraries.

Gate: maintained Windows, macOS and Linux jobs build the application and SSTV
tests. Hardware/radio/platform claims are limited to what was actually run.

## Commit discipline

Commits are grouped by audit/docs, core types/registry, VIS/FSK/timing, encoder,
receiver DSP, individual mode families, storage, QML, TX coordination, sharing,
HAMDRM, hardening and final documentation. Unrelated formatting and cleanup are
excluded. Each commit must compile its affected targets and update the matrix
when capability evidence changes.

## Known prerequisites and early corrections

- The current CMake Hamlib symbol checks run two probes before setting required
  include paths, incorrectly hiding installed caching/get-conf2 capabilities.
  Correct this in a small baseline commit and test the cache result before SSTV
  TX integration relies on it.
- Existing CI exercises only selected tests; SSTV paths and a full application
  build need explicit triggers/gates.
- No native SSTV code or vectors exist in the starting tree. All compatibility
  rows begin as unimplemented until code and evidence change them.
- Large third-party fixture packs must not be committed blindly. Prefer
  deterministic generators or pinned optional packs with redistribution and
  SHA-256 metadata.
