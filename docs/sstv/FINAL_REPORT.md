# Native SSTV required final report

Evidence snapshot: 2026-08-24. This document follows the 20-item final-report
contract in the canonical native-SSTV mission. It is an evidence report for the
current feature worktree, **not a declaration that the Definition of Done is
complete**. The exact remaining gates are recorded in
[DEFINITION_OF_DONE.md](DEFINITION_OF_DONE.md).

## 1. Starting branch and starting commit

- Branch: main
- Tag: v1.0.583
- Commit: 119947690e2d8a1df99a75f98b915f2115df99e7
- State: fetched origin and upstream heads agreed with the clean local checkout
  before the feature branch was created.

The untouched baseline built on macOS Apple Silicon and passed 37/37 registered
CTest tests in 96.64 seconds. Its documented fixture skips and the unregistered
hardware RTL-SDR test are recorded in
[ARCHITECTURE_AUDIT.md](ARCHITECTURE_AUDIT.md).

## 2. Final branch and final commit

- Working branch: feature/native-sstv
- Immutable native implementation commit:
  2aeb0e6660636bca2db797826ce44f10cc476a06
- Immutable build/packaging/CI commit and current committed HEAD:
  6f74fb937d0515c29bad61290fd2bc30b55a1314
- Final documentation-only commit: **not yet available**

The native implementation and build/CI work are committed at the two immutable
SHAs above. The remaining worktree changes are this final documentation pass;
its later commit will not change the tested implementation. Record that final
documentation SHA after it is created rather than inventing it here.

Committed inventory so far:

~~~text
e63dbc905 Document native SSTV architecture and delivery contract
dd4bcbc3e Add native SSTV protocol core foundation
a7699f744 Add SSTV streaming audio and TX foundations
dfe9c3e4c Add SSTV RX acquisition and progressive image core
9f216e74b Add SSTV RX frontend and streaming WAV
64cc8e17d Add optional SSTV hum and impulse filtering
2aeb0e666 Complete native SSTV workspace and HAMDRM integration
6f74fb937 Add native SSTV build packaging and CI coverage
~~~

## 3. Architecture summary

SSTV is compiled into Decodium4 and uses the application's existing services:

~~~text
existing local / RTL-SDR / TCI / DecoPort PCM fan-out
                    |
          bounded Bridge audio relay
                    |
       SstvAudioIngress -> SstvRxRuntime worker
                    |
  preprocess/demodulate/VIS/N-VIS/AVT/timing fallback
                    |
      family RX session -> progressive immutable image
                    |
          storage worker / Gallery / QSO

Studio prepared image -> native family pull encoder
                    |
      SstvTxCoordinator -> existing SoundOutput/CAT/PTT

Gallery file -> provider-neutral sharing queue/inbox -> HTTPS provider

shared PCM / TX authority -> separate HAMDRM controller/backends
~~~

There is no second QAudioSource, independent serial PTT stack, external SSTV
process, Java component or Python runtime. DSP, image codecs, SQLite, file I/O
and networking remain in C++ workers; QML consumes bounded properties, models
and image-provider snapshots.

Analog SSTV, remote IP sharing and HAMDRM are separate modules. A downloaded
network item cannot key the radio: explicit acceptance produces a revalidated
Gallery object, and a later local Studio/TX action must pass the ordinary
Decodium TX interlocks.

## 4. File-by-file summary of significant changes

| File or cohesive path | Significant change |
|---|---|
| CMakeLists.txt; src/sstv/CMakeLists.txt; src/sstv/digital/CMakeLists.txt; tests/sstv/CMakeLists.txt | Target-scoped SSTV/HAMDRM feature gates, libraries, application linkage, tests, fuzzers and developer tools without changing the global C++17 level. |
| src/bridge/DecodiumAudioSink.h; DecodiumBridge.{h,cpp}; DecodiumBridgeSstv.cpp | Existing-audio fan-out, Bridge-owned RX/TX/storage/gallery/sharing/HAMDRM services, settings, QML properties, shutdown and actions. |
| Audio/soundout.{h,cpp} | Bounded pull-source support for long SSTV/HAMDRM streams under the existing output lifecycle. |
| RTL-SDR, DecoPort, transceiver and legacy-backend integration files | Source-labelled PCM forwarding to the same SSTV relay; no additional capture ownership. |
| src/sstv/core/* | Canonical registry/specification, standard/wide/narrow VIS, FSK ID and fractional timing. |
| src/sstv/dsp/*; src/sstv/rx/* | Bounded preprocessing, hum/impulse options, resampling, demodulation/AFC, sync/slant, leader/VIS/FSK/timing detection, replay and explicit RX state. |
| src/sstv/analog/* | Table-driven native RX/TX sessions for Martin, Scottie, Robot, Wraase/Pasokon, PD, AVT and MMSSTV wide/narrow. |
| src/sstv/tx/*; integration/SstvTx*; SstvWav* | Phase-continuous encoding, preparation, optional FSK ID, calibration, atomic WAV, loopback and fail-safe existing CAT/PTT/SoundOutput coordination. |
| integration/SstvRx*; SstvAudioIngress.* | Worker runtime, bounded ingress, correction controls, retained-audio jobs and replay/re-decode. |
| src/sstv/storage/*; GalleryModel; ThumbnailProvider | QStandardPaths layout, atomic images/sidecars, versioned SQLite, metadata, retention/delete recovery, paging and lazy thumbnails. |
| src/sstv/sharing/*; SstvShareController | Versioned manifests, durable schema-v3 queue/inbox, REST/WebDAV/pre-signed providers, validation, TLS/credential policy and bounded sessions. |
| src/sstv/digital/* | Separate HAMDRM profile, MOT/BSR/object, persistence, channel/PHY/waveform, OpenJPEG and controller layers. |
| src/sstv/diagnostics/* | Allowlisted event ring, bounded scalar snapshots and atomic export; stable source tokens, at-most-4-Hz active-TX refresh plus terminal update, explicit unavailable HAMDRM state and persistent guarded test-tone result. |
| qml/decodium/components/sstv/*; qml/decodium/Main.qml | Lazy workspace with Receive, Studio, Gallery, Sharing, HAMDRM, Settings and Diagnostics pages. |
| translations/decodium_it.{ts,qm} | Integrated Italian strings through the existing workflow; validation reports 6,825 finished and 0 unfinished entries. |
| tests/sstv/* | 81 SSTV-labelled protocol, DSP, mode, integration, QML, storage, sharing, security, HAMDRM, performance and fuzz-smoke tests; the final current-tree invocation passed 81/81. |
| workflows, scripts and packaging/docker files | Platform/feature matrices and explicit Qt image-format, QSQLITE, ShaderTools and optional OpenJPEG packaging checks. Workflow definitions are not executed platform evidence. |
| docs/sstv/*; doc/THIRD_PARTY_LICENSES_OPENJPEG.md | Architecture, modes, provenance, RX/TX, storage, QSO, sharing/OpenAPI, HAMDRM, security, test, performance, user/developer and release evidence. |

## 5. Complete analog mode matrix

The authoritative complete row-by-row matrix is
[MODE_MATRIX.md](MODE_MATRIX.md). Its generated columns are checked against the
canonical C++ registry by test_sstv_mode_docs; copying its 64 rows here would
create a second manually maintained table.

| Family | Implemented modes | Count | External evidence boundary |
|---|---|---:|---|
| Martin | M1, M2, M3, M4 | 4 | M2 has PySSTV PCM; M2/M3/M4 have libsstv landmarks; no on-air result |
| Scottie | S1, S2, DX, S3, S4 | 5 | S3/S4 have libsstv landmarks; no compatible external decoder/RF run |
| Robot colour | C12, C24, C36, C72 | 4 | C36 has PySSTV PCM; conflicting upstream profiles remain explicit |
| Robot monochrome | B/W 8, 12, 24, 36 | 4 | B/W 8 has PySSTV PCM; other rows and aliases remain externally unverified |
| Wraase | SC2-60, SC2-120, SC2-180 | 3 | SC2-120/180 have PySSTV landmarks; SC2-60 has native evidence only |
| Pasokon | P3, P5, P7 | 3 | PySSTV timing landmarks; no cross-application/RF result |
| PD | PD50, 90, 120, 160, 180, 240, 290 | 7 | pySSTV/libsstv landmarks; libsstv's defective suffix is rejected |
| AVT normal | AVT24, AVT90, AVT94 | 3 | Handbook/source landmarks and native loopback only |
| MMSSTV extended/narrow | MP73/115/140/175; MR73/90/115/140/175; ML180/240/280/320; MP73N/110N/140N; MC110N/140N/180N | 19 | pinned source landmarks, no independent PCM |
| Related/catalogue-only | FAX480, HFFAX, WEFAX; AVT Narrow/QRM variants | 12 rows beyond the 52 above | blocked or unavailable, and not advertised as implemented SSTV |

All 52 native rows implement RX, TX and automatic protocol detection and have
deterministic coverage. “Implemented” does not mean independently interoperable.

## 6. Digital/HAMDRM compatibility matrix

The authoritative table is
[HAMDRM_COMPATIBILITY_MATRIX.md](HAMDRM_COMPATIBILITY_MATRIX.md).

| Capability | Local native status | Independent status |
|---|---|---|
| 72 named A/B/amateur-E profile tuples | registry and validation implemented | no independent waveform for all tuples |
| MOT header/body, CRC, segmentation/reassembly | implemented and tested | no QSSTV object exchange |
| BSR, missing ranges, retransmission/resume | implemented and tested locally | EasyPal expectation only |
| JPEG2000 | bounded OpenJPEG 2.5.4 lossless local round-trip | no QSSTV JP2 exchange/package proof |
| FAC/MSC pinned subset and OFDM waveform | implemented for documented subset | no independent RF/QSSTV waveform |
| Existing-audio RX and existing-coordinator TX adapters | connected and self-roundtrip tested | no sound-card/radio/on-air run |
| Full broadcast DRM, SDC, MSC Part A, VSPP, hierarchical/soft/CSI decode | not claimed or implemented | none |
| KG-STV | not implemented | public authoritative specification/vector gate remains |

No QSSTV, EasyPal or live HAMDRM interoperability result is claimed.

## 7. Upstream code and licence provenance

Exact revisions, paths, licences and conflicts are in
[UPSTREAM_PROVENANCE.md](UPSTREAM_PROVENANCE.md): QSSTV 8c27d6d (GPL
lineage, behaviour only), SlowRX a50a4e2/ca6d701 (ISC), Robot36 75146a5
(0BSD), libsstv 193157a (MIT), pySSTV d998fad (MIT), MMSSTV mirror 8060b5f
(LGPL/GPL-labelled source, behaviour only), QT6SSTV 6ae74b7 (migration audit)
and OpenJPEG 2.5.4 (BSD-2-Clause).

The SSTV Handbook PDF consulted has SHA-256
e244de9d5cbba525d33b25906c3751ab0ed62af2a3b373feffda44de4f13909d.
The imported/adapted component ledger remains None: no upstream implementation
source was copied or adapted. Restricted/ambiguous DRM and Numerical
Recipes-derived material is explicitly excluded.

## 8. Build commands used

Untouched baseline:

~~~zsh
cmake -S . -B build
cmake --build build --parallel "$(sysctl -n hw.ncpu)"
ctest --test-dir build --output-on-failure -j "$(sysctl -n hw.ncpu)"
~~~

Current local builds:

~~~zsh
cmake --build /tmp/decodium-hamdrm.csmxg7 \
  --target wsjtx decodium_qml decodium_sstv_test_binaries translations \
  --parallel 6

cmake --build /tmp/decodium-sstv-analog.biDO0t \
  --target wsjtx decodium_qml decodium_sstv_test_binaries translations \
  --parallel 6

cmake --build /tmp/decodium-sstv-off.Hu9Bvh \
  --target wsjtx decodium_qml translations --parallel 6
~~~

The caches confirm Release/Ninja/deployment target 13.0 and respectively
SSTV=ON,HAMDRM=ON; SSTV=ON,HAMDRM=OFF; and SSTV=OFF,HAMDRM=OFF. The enabled
cache resolves OpenJPEG through /opt/homebrew/lib/cmake/openjpeg-2.5.
The final main application/test build completed successfully in 8.28 seconds;
the final analog-only build passed in 10.21 seconds and SSTV-off in 6.58 seconds.

## 9. Platforms built

| Platform/configuration | Actual result |
|---|---|
| macOS 26.5.2, Apple Silicon arm64, Qt 6.11, Release, SSTV+HAMDRM | final main application/test build succeeded in 8.28 seconds; both application executables and all test binaries were built locally |
| Same host, analog-only | final build passed in 10.21 seconds; both application executables and analog test binaries built locally |
| Same host, SSTV disabled | final build passed in 6.58 seconds; both application executables built locally |
| macOS Intel | workflow updated; not executed |
| Windows x64 | workflow updated; not executed |
| Linux x86_64 | workflow/package scripts updated; not executed |
| Linux ARM64 | workflow/package scripts updated; not executed |

No final DMG/AppImage/Windows package has been inspected for this final
worktree. macOS compilation cannot establish those platform claims.

## 10. Tests executed

Executed evidence includes:

- untouched baseline: 37/37 CTest tests;
- current focused mode-doc, external-vector, performance, sharing-core and
  schema-v3 queue run;
- family protocol/RX/TX/WAV/Studio tests for all 52 native analog rows;
- offscreen Receive, Studio, Gallery, Sharing, Settings, QSO, Diagnostics and
  Digital QML tests in focused runs;
- storage/migration/retention/delete, 5,000-row Gallery, sharing provider/
  session/queue/inbox and incoming-import tests;
- TX success/failure/cancellation/PTT-release tests, including cancellation
  during header, image and FSK ID;
- HAMDRM object, BSR, partial store, OpenJPEG, channel, PHY, waveform, adapter
  and controller tests;
- deterministic parser fuzz smoke and focused ASan+UBSan repetitions.

The enabled build registers 81 SSTV-labelled tests and 118 tests overall. The
final SSTV invocation passed 81/81 in 91.90 seconds. An attempted all-test run
is deliberately not called an aggregate pass: it ran the 81 already-built tests
successfully but reported 36 historical executables as `Not Run`. Those 36
binaries were then built, and the non-SSTV invocation passed 37/37 in 132.59
seconds. All 118 current-tree tests therefore have passing executable evidence
across two invocations; no single 118/118 result is claimed.

## 11. Test results

The current focused command was:

~~~zsh
ctest --test-dir /tmp/decodium-hamdrm.csmxg7 \
  -R '^(test_sstv_mode_docs|test_sstv_external_vectors|test_sstv_share_queue_manager|test_sstv_sharing_core|sstv_performance)$' \
  --output-on-failure
~~~

Final current-tree results:

- main application/test build: success, 8.28 seconds;
- analog-only application/test build: success, 10.21 seconds;
- SSTV-off application build: success, 6.58 seconds;
- SSTV-labelled suite: 81/81 passed, 91.90 seconds;
- historical non-SSTV suite after building its 36 missing binaries: 37/37
  passed, 132.59 seconds;
- total coverage: all 118 registered tests across those two CTest invocations.

The first attempted all-test invocation had 81 successful executions and 36
`Not Run` entries because those historical binaries had not been built. It is
retained as setup evidence and is **not** reported as a passing aggregate run.

The additional focused command above passed 5/5, 0 failed, in 6.16 seconds.

The schema-v3 queue tests include migration/restart/rollback, more than 10,000
closed inbox cycles, oldest-first terminal reclamation and protection of
active/retryable/file-owning rows. Focused sanitizer runs reported no
ASan/UBSan failure. These focused results supplement the final two-invocation
coverage; they are not presented as a separate single aggregate run.

## 12. Independent interoperability vectors used

The pinned PySSTV d998fad pack is independent of the Decodium encoder:

| Mode | WAV SHA-256 | Native replay result |
|---|---|---|
| Robot 36 | 6d5164a9294cbc597a7ef6494efea15a02d5a6267662e5ff98023acbed4bf0cb | complete 320x240, coverage 1.0, 18.024 dB PSNR |
| Robot B/W 8 | 660d52ca4427d4d3271281285336bc3feb86559615b005066667c4dc233ecaf0 | complete 160x120, coverage 1.0, 18.661 dB PSNR |
| Martin M2 | 4cad290aec3ee249541bcd56c85717263e3d03af18755d806d5e4418085152d5 | complete 320x256, coverage 1.0, 24.250 dB PSNR |

All reported zero ingress drops and zero processing failures. Compact
libsstv/pySSTV timing/hash landmarks and MMSSTV/AVT source-document fixtures are
developer oracles only where the matrix says so. They are not full independent
PCM, another decoder, live RF or on-air evidence. No independent receiver has
decoded Decodium TX output.

Robot B/W 8 now keeps canonical TX at a 10 ms sync plus 56 ms scan (66 ms) and
explicitly recognises the independent PySSTV compatibility waveform at 7 ms
plus 60 ms (67 ms). The 66/67 ms decoder selection passed 40 repetitions and
ASan; the pinned Robot B/W 8 WAV remained green alongside Robot 36 and Martin
M2. This strengthens that one RX interoperability vector without proving an
external decoder or RF path.

## 13. Performance measurements

Executed locally:

~~~zsh
/tmp/decodium-hamdrm.csmxg7/tests/sstv/sstv_performance
~~~

~~~text
audio_seconds:             15
dsp_wall_seconds:          0.040
dsp_realtime_ratio:        377.408
frequency_observations:    179968
inactive_wall_ms:          755.027
inactive_cpu_ms:           0.015
inactive_worker_running:   false
inactive_chunks_processed: 0
pass:                      true
~~~

This is one Release run on macOS Apple Silicon. It does not establish
cross-platform latency, long-duration stability, waterfall frame-rate
non-regression or real device callback behavior.

## 14. Security controls implemented

- strict image, WAV, JSON, manifest, path, response and HAMDRM object bounds;
- literal, nested and escaped-equivalent duplicate JSON key rejection;
- checked arithmetic and MIME/magic/hash/dimension/pixel/allocation/frame gates;
- private quarantine, metadata-free PNG normalization, QSaveFile, transactional
  SQLite and revalidation before Gallery import;
- HTTPS-only production policy, certificate validation, bounded redirects, no
  cross-origin credential forwarding and no runtime plaintext switch;
- direct fail-closed SecureSettings backend use, opaque leases and no secret
  values in QML models, SQLite or diagnostic export;
- bounded HTTP operations, sessions, queue rows, retries and response budgets;
- central diagnostic allowlists, a 512-event ring and 1 MiB scalar-only export;
- network-to-RF separation and existing TX ownership/watchdog/release.

E2EE is not implemented. TLS-only provider endpoints can read content, and the
UI/documentation says so.

## 15. Remote-sharing providers implemented

| Provider | Implemented capability | Important limit |
|---|---|---|
| Generic HTTPS REST v1 | capabilities, recipient lookup, upload/resume/complete/status, authenticated inbox/download/ack/reject/delete/block when advertised | requires a compatible deployed service; none is bundled |
| WebDAV over HTTPS | directory validation, upload/status/delete and bounded direct GET | no standard recipient/inbox/ack contract |
| Trusted pre-signed PUT | short-lived broker lease and bounded PUT without cloud SDK | no trusted broker is shipped |
| Process-local integration provider | deterministic upload/download/inbox/idempotency tests | developer/test only; no socket or production UI selection |
| Decodium peer/relay | protocol boundary documented | no existing channel met the secure inbox/object contract; no relay invented |

All sharing is opt-in. Automatic upload, public sharing and automatic content
download default off.

## 16. Required external backend/deployment steps

An operator or deployer must:

1. deploy/select a real HTTPS REST v1 or HTTPS WebDAV service;
2. for REST, implement the capability, identity, upload, inbox, download,
   acknowledgement/rejection, expiry and idempotency OpenAPI contract;
3. provision CA-valid TLS and stable recipient identities;
4. create credentials through Decodium's secure backend;
5. configure size, expiry, retry and provider limits;
6. for pre-signed PUT, deploy an authenticated broker issuing short-lived
   leases without exposing URLs to QML/settings/queues/logs;
7. test upload/download/restart/revocation/deletion against that deployment;
8. disclose that TLS-only service operators can read objects.

No unauthenticated public relay or fictional Decodium cloud endpoint is
included.

## 17. Known limitations with precise technical reasons

- Final Windows, Linux x86_64/ARM64 and macOS Intel build/package evidence is
  absent; workflow edits are configuration only.
- Implementation and build/CI have immutable SHAs; only the documentation-only
  commit produced after this report remains to be recorded.
- Only Martin M2, Robot 36 and Robot B/W 8 have independent full PCM
  encoder-to-native-decoder vectors. Most rows have native loopback or
  timing/source landmarks only.
- No Decodium TX waveform has been decoded by another application and no image
  has been exchanged over RF.
- No real sound card, RTL-SDR/TCI/DecoPort session, CAT interface or PTT line
  has been exercised for SSTV.
- HAMDRM has no independent QSSTV/EasyPal exchange and is not full broadcast DRM.
- No production sharing backend, real provider account or complete
  maintained-platform secure-store test exists.
- E2EE lacks an audited dependency, envelope/key lifecycle, packaging and vectors.
- FAX480 has unresolved 512x500 versus 512x480 geometry; HFFAX/WEFAX and AVT
  Narrow/QRM lack complete defensible semantics.
- Final packages have not been inspected for QSQLITE, Qt image plugins,
  ShaderTools/QML assets and OpenJPEG closure.
- Full libFuzzer runs, complete-tree sanitizer stress, forced process-kill
  recovery and cross-platform performance remain open.

## 18. Manual radio tests still recommended

Before release, record:

1. RX from named radio/audio hardware for common, extended and long modes;
2. real DecoPort/RTL-SDR/TCI routing where advertised;
3. TX into a dummy load/monitor receiver, verifying PTT lead, level, tail and
   release;
4. cancellation during header/image/FSK ID plus CAT disconnect, device loss,
   sleep/resume and shutdown;
5. bidirectional analog decoding with QSSTV and another legal implementation;
6. HAMDRM object/corruption/BSR/resume exchange with QSSTV;
7. long Scottie DX/PD/ML runs for clock drift, underrun and watchdog behavior.

Each record should name OS, commit, radio/interface, audio device, sample rate,
mode, frequency, counterpart/version and observed result. Mock PTT cannot
replace this evidence.

## 19. Completed UI description and screenshot status

Access is through the top-left hamburger menu: **SSTV - image radio...**. The
lazy workspace is titled **SSTV - Decodium** and contains:

- Receive: progressive image and VIS/mode/sync/level/AFC/slant/FSK controls;
- Transmit Studio: source/prepared/loopback views, edits, overlays/templates,
  mode/FSK, WAV, loopback, calibration and TX/cancel;
- Gallery: lazy thumbnails, filters/search, metadata and local actions;
- Remote Sharing: provider/recipient/privacy, queue, inbox and transfer actions;
- Digital HAMDRM: named profiles, object/missing/BSR/resume and RX/TX;
- Settings and Diagnostics with bounded scalar export, stable source tokens,
  explicit HAMDRM availability and a persistent test-tone result. Active TX
  refresh is capped at 4 Hz plus terminal state; the tone control warns that it
  keys PTT/transmits RF and repeats the normal TX safety guard.

Offscreen QML tests exercised 1040x700 and page-specific layouts. No final
human-reviewed screenshot from the finished packaged build is recorded, so
this section provides a description rather than implying visual evidence.

## 20. Suggested pull-request title and body

Suggested title while external gates remain:

~~~text
Draft: add native analog SSTV, secure sharing and separate HAMDRM support
~~~

Suggested body:

~~~markdown
## Summary

Adds an in-process Decodium4 SSTV workspace with native analog RX/TX for the
52 implemented rows in docs/sstv/MODE_MATRIX.md, progressive reception,
Studio/WAV/loopback, existing SoundOutput/CAT/PTT coordination, Gallery/QSO
storage, opt-in provider-neutral sharing, diagnostics and separately gated
HAMDRM.

No second audio capture device or external SSTV/Python/Java runtime is used.

## Evidence

- untouched baseline: 37/37 CTest tests;
- macOS Apple Silicon builds: SSTV+HAMDRM, analog-only and SSTV-off;
- current tree: 81/81 SSTV and 37/37 non-SSTV tests passed in two explicit
  invocations (no false single aggregate claim);
- three pinned PySSTV WAVs decoded through production replay/runtime;
- Robot B/W 8 canonical 66 ms and compatibility 67 ms paths passed 40 repeats
  plus ASan;
- Italian translations: 6,825 finished, 0 unfinished;
- focused sanitizer, QML, storage, sharing, TX and HAMDRM coverage.

See docs/sstv/FINAL_REPORT.md and DEFINITION_OF_DONE.md for exact limits.

## Required before merge/release

- record the immutable final SHA and commit inventory;
- execute/inspect maintained Windows, macOS and Linux CI/packages;
- run real radio/audio/CAT/PTT and cross-application trials;
- audit a production sharing provider and secure-store behavior;
- complete libFuzzer/full-tree sanitizer and package inspection.

Do not treat loopback, source landmarks or workflow YAML as on-air,
interoperability or platform-package evidence.
~~~
