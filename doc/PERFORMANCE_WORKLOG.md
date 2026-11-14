# Decodium4 optimisation worklog

Active goal requested on 2026-11-14. Preserve decode output at identical settings,
including the intentional behaviour of the existing Low CPU profile. Baseline
and acceptance rules are in `PERFORMANCE_BASELINE.md`.

User-specified target: **Windows 11, 8 GB RAM, third-generation Intel Core i5**.
Local arm64/macOS measurements and constrained-thread runs are not substitutes
for that machine. Check Ivy Bridge fallback paths as well as UI and memory costs;
do not introduce an AVX2-only requirement for the general executable.

## Work remaining

- [x] Freeze initial executable/QML and measure startup in native/legacy modes.
- [x] Fix laboratory UDP isolation; stop a run on its first unbound-socket error.
- [x] Establish native FT8 RX over BlackHole with controlled test settings.
- [x] First local optimisation: immutable indexed role keys in DecodeListModel.
- [ ] Complete paired multistation/weak-signal app-level validation.
- [ ] Verify two Decodium instances over virtual audio after retained changes.
- [ ] Profile busy decode UI, maps, reporting and audio paths; optimise measured costs.
- [ ] Measure first-ready RX and service initialisation; improve deferred/lazy work where useful.
- [ ] Measure repeated mode cycles, retained buffers, active work and cancellation across families.
- [ ] Implement warranted memory/lifecycle improvements without dropping useful decoder state.
- [ ] Validate other affected modes, normal/Low CPU profiles and limited-resource settings.
- [ ] Verify on an older target computer when available; keep local simulation distinct from hardware evidence.

## 2026-11-14 — Decode model role access

`DecodeListModel::data()` previously scanned the role table and allocated a
QString field name on every delegate read. It now indexes immutable keys created
once. A compile-time check preserves the role-table ordering contract. Invalid
roles/indices, missing fields and whole-entry access retain their behaviour.
No DSP, scheduling, search depth or Low CPU policy was changed.

Isolated Release A/B builds used the same test driver and compile/link options,
with the original model source from `3529dae3` compiled into a separate binary.
Three alternating trials, each 4,000 iterations and five QtTest median samples:

| Trial | Before (ms) | After (ms) |
| --- | ---: | ---: |
| 1 | 0.0927 | 0.0539 |
| 2 | 0.0924 | 0.0544 |
| 3 | 0.0940 | 0.0544 |

Each iteration reads all roles for 50 visible rows against a 500-row history.
Median cost fell about 41%, or 0.0383 ms per iteration. This is a small local
cost reduction, **not** a 41% application speed-up. All 17 model tests passed.
Evidence and exact compiler commands: `build/performance-baseline/model-role-ab/`.

Native live-audio smoke validation before/after recorded three `CQ K1ABC FN42`
messages at confidence 100 in each build, no UDP errors and clean exits. The
first baseline run played a continuous repeated fixture; the candidate test
explicitly injected three complete slots. This establishes functional reception,
not yet a paired multistation or latency equivalence result. Evidence:
`rx-baseline-clean/` and `rx-role-cache-after/` under `build/performance-baseline/`.

Paired multistation RX: `busy-901.wav`, three injected slots, baseline and
candidate each persisted all 36 messages (12 per slot). The comparator confirmed
identical per-slot message multisets, no missing or extra rows, zero UDP errors
and clean exits. It also rejects same-count message substitutions and mismatched
input hashes; comparison is relative to each run's injected UTC slots.
Evidence: `busy-role-before/`, `busy-role-after/`, `busy-role-comparison.json`.

## Profiling and lifecycle observations

- A ten-second stack sample showed the main thread waiting for the Qt render
  thread and the render thread waiting for a Metal drawable. These are waiting
  samples, not CPU self-time; do not infer that they prove a CPU bottleneck.
- The initial sampled RX run decoded the fixture but a residual laboratory
  blacklist prevented persistence. It is excluded from decode-equivalence
  evidence. The RX harness now temporarily uses empty *test* legacy settings,
  restoring the original exact bytes at the end, including failure paths.
- FT8 worker objects themselves contain little state. Several downsample/FFT
  caches use heap-allocated thread-local pointers (`FtxDownsample.cpp`,
  `FtxBitmetrics.cpp`, `FtxSharedDsp.cpp`). Deleting/recreating worker threads is
  not automatically equivalent to reclaiming this memory. Audit ownership and
  measured high-water marks before changing teardown policy.
- Four frozen twelve-station recordings are available in
  `build/performance-baseline/ft8-corpus/`, with manifests and SHA-256 hashes.
  Two busy and two mixed weak-signal scenes use stored simulator output and
  seeded noise. Nominal synthetic SNR is not an RF sensitivity calibration.

The goal remains active. No overall performance or zero-regression claim is
made until the relevant paired and functional checks are completed.

## 2026-11-14 — Initialized profile lookup

`ensureSettingsProfileInitialized()` and `beginActiveSettingsProfile()` now read
the existing initialization marker before enumerating all keys. Marked profiles
take the short path; unmarked, false-marker and legacy profiles retain discovery
and precedence. There is no persistent routing cache, so deletion/replacement or
live profile changes remain visible.

The same test driver was saved before changing the header and then rebuilt after
the change. Three alternating Release A/B trials (2,000 iterations, five QtTest
median samples), reading one value from an initialized 2,000-key profile:

| Trial | Before (ms) | After (ms) |
| --- | ---: | ---: |
| 1 | 0.193 | 0.016 |
| 2 | 0.191 | 0.017 |
| 3 | 0.193 | 0.017 |

About 91% less time in this specific lookup benchmark, not overall application
speed. All nine profile tests passed, including cross-process persistence,
async handoff, direct/legacy precedence, false markers and same-process removal
of a direct profile in favour of its legacy counterpart. Evidence:
`profile-lookup-ab/`, `profile-lookup-tests.txt` under the baseline directory.
Combined live-audio validation: the mixed weak-signal scene `mixed_weak-901.wav`
returned 30 messages in each build over three slots, with identical per-slot
message multisets, no missing/extra rows, no UDP errors and clean exits. Both
runs used the same fixture hash and denied access to the macOS security
executable so the lab did not access operator credential storage. Evidence:
`weak-role-before/`, `weak-optim-after/`, `weak-optim-comparison.json`.

CPU compatibility checks: all nine `test_superldpc_cpu_*` cases passed, including
the Ivy Bridge feature combination. These are dispatcher tests with backend
stubs on ARM, not execution of a Windows binary on an Intel i5.

## Next execution steps

Current application SHA-256:
`49a8a17ad96439acf40824bbf71f65fd8b8e1070498ef27636dc1af48436a250`.
Reference executable is also staged at `build/decodium-performance-reference`,
beside the same runtime resources, SHA-256:
`c026d64841cedc7ae30e1f1d42557e417c66cdeaf32c722e9b3839bd3ffaa56b`.

1. Complete both-change busy-scene validation and other frozen noise seeds;
   use `profile_rx_runtime.py` sequentially, then `compare_rx_runtime.py` on the
   resulting `result.json` files. The earlier busy comparison covered only the
   first (model-role) optimisation.
2. Add explicit limited-thread/Low CPU fixture settings to the isolated harness
   and compare each profile against its own baseline. Do not equate fewer FT
   threads on this Mac with an Ivy Bridge PC.
3. Run the two-app BlackHole functional test with corrected UDP bind policy,
   local UDP sinks, isolated credential access and test-INI restoration.
4. Re-measure startup after both changes; investigate lifecycle/memory with
   repeated actual mode cycles before implementing any worker disposal.

No background application/test process remains at the checkpoint. No commit,
push, release or production configuration change has been performed.

## 2026-11-14 — Explicit two-thread profile

The RX harness now supports `--ft-threads` and `--low-cpu`, writes only a fresh
isolated profile, verifies the startup CPU profile and dispatch limits, and
records the requested settings in the result. The comparator rejects pairs with
different requested profiles or credential isolation. Default UDP destinations
2237/2239 are consumed by local sinks; occupied ports abort before app launch.

Both-change candidate versus original, `busy-902.wav`, manual two threads,
Low CPU off: **36/36 messages**, identical per-slot multisets across three slots,
no missing or extra messages, clean exits and no UDP errors. The reference log
explicitly confirms `low=0 ft_threads=2 reason=startup`; candidate verification
also checks this automatically. Evidence: `busy-two-threads-before/`,
`busy-two-threads-after/`, `busy-two-threads-comparison.json`.
This is a limited-thread test on Apple Silicon, not an emulation of Ivy Bridge.

The `threads_active=24` FT8 diagnostic field is the OpenMP maximum pool setting
(`omp_get_max_threads`), not measured concurrent CPU activity. The worker keeps
a fixed pool ceiling and passes the requested budget separately to native
regions; do not infer 24 busy cores from that diagnostic.

Low CPU on, manual two threads, `mixed_weak-902.wav`: original and combined
candidate both return **21/21 messages**, identical per-slot identities and
multiplicities across three slots. Both logs confirm `low=1 ft_threads=2` at
startup, with no UDP errors or timeouts. Evidence: `weak-lowcpu-before/`,
`weak-lowcpu-after/`, `weak-lowcpu-comparison.json`. Low CPU policy is unchanged.
Comparator negative checks reject a changed profile and detect replacement of
one message even when the total decode count is unchanged.

The existing two-app and mode-change laboratories now guard against an already
running Decodium, restore the shared test INI, deny credential-tool execution,
allow UDP binds, consume reporting destinations locally, and remove only their
own test directories. The two-app test remains virtual audio, CAT disabled.

Lifecycle follow-up from source inspection: FT4 invalidates queued/stale serials
and checks before/after acquiring the Fortran runtime mutex, but exposes no
cooperative cancellation for a decode already inside its native decoder. Q65
likewise has no cancellation method in its worker interface. Measure work still
running after a mode change before choosing a cancellation/retirement change;
retaining an idle reusable worker and retaining an active obsolete decode are
different costs. This is an audit finding, not a verified runtime regression.

Combined-candidate two-instance functional proof passed: one isolated Decodium
transmitted `CQ 9H1SR JM75` through BlackHole 2ch and the receiver persisted it.
Both processes exited zero, measured audio peak 0.402, onset at slot +0.478 s,
no UDP errors. QML and shared test INI hashes matched the originals after cleanup.
Evidence: `two-app-optim-after.txt`, `two-app-optim-after-artifacts/`,
`two-app-restoration-before.json`. This proves the virtual TX/audio/RX chain;
it is not physical radio, Windows, or complete protocol-interoperability proof.

Resource samples from these single paired runs do not establish a whole-app
CPU/RAM improvement. Busy/two-thread peak RSS was 512.6 -> 564.7 MiB; weak/Low
CPU 566.6 -> 561.5 MiB. Median sampled `ps %cpu` was approximately 35% in all
four runs. RSS includes retained/native/graphics allocations and needs repeated
controlled measurements; the busy increase must not be hidden behind the
microbenchmark gains. No claim of reduced application memory is made.

Combined-candidate FT8 -> FT4 -> FT8 file-decode check also passed:
`cleared=true, recovered=true, tx=false`; output in
`mode-decode-optim-after.txt`. Both the QML file and shared test INI again matched
their pre-test hashes. This check exercises mode clearing and FT8 recovery, not
an active FT4 decode or repeated-cycle memory plateau.

### Remaining execution queue

1. Re-measure whole-app startup with matched before/after laboratory settings;
   update startup isolation to the shared credential-denying policy and local
   UDP sinks before running new paired series (retain old evidence as historical).
2. Measure repeated FT8/FT4 cycles with actual decodes in both modes and idle
   dwell periods, RSS/high-water marks and obsolete work after mode changes.
   Investigate the busy-scene RSS difference above with repeated runs.
3. Choose further UI/service/lifecycle optimisations from those measurements,
   preserving DSP settings and per-slot A/B decode equivalence.
4. Windows 11 / 8 GB / third-generation i5 remains the acceptance hardware;
   local Apple Silicon tests are supporting evidence only. Physical RF and that
   machine's performance remain unverified.

Goal remains active. At this checkpoint all launched applications have exited,
syntax checks and `git diff --check` pass, and no commit/push/release occurred.

## 2026-11-14 — Paired startup and real FT4 cycles

Twelve visible startup runs completed with fresh bridge/legacy test defaults,
credential execution denied, UDP loopback sinks and exact test-INI restoration.
Three before/after pairs, both backends; all valid, no UDP errors. Median values:

| Backend / metric | Original | Two optimisations |
| --- | ---: | ---: |
| Native Main.qml ready | 2.154 s | 2.125 s |
| Native live-map stage | 7.630 s | 5.522 s |
| Native peak sampled RSS | 413.55 MiB | 432.78 MiB |
| Legacy Main.qml ready | 3.158 s | 2.172 s |
| Legacy live-map stage | 7.531 s | 6.901 s |
| Legacy peak sampled RSS | 503.11 MiB | 479.97 MiB |

Evidence: `startup-paired-{1,2,3}-{before,after}/` and
`startup-paired-summary.json`. These are small local series, with visible
render-event timing variability (including roughly one-second UpdateRequest
stalls on this Mac); they do not establish a universal startup speedup or RAM
reduction. Native initial-window time is effectively unchanged in this series.

`profile_mode_cycles.py` now measures actual alternating FT8 and FT4 file
decodes, four-second dwell periods, per-step clear/recovery, and sampled idle
RSS. The original completed six cycles / twelve expected messages with no UDP
errors. Its RSS rose during initial decoder use, plateaued around 580 MiB and
later fell; this run does not show monotonic per-cycle growth. FT4 uses a frozen
native-generator fixture with recorded SHA-256, not a mode toggle without data.

## 2026-11-14 — Static GPU map cadence

`WorldMapGpuItem` previously requested a scene-graph update on every animation
tick even with no contacts, no transmitting path, no earthquake pulses and no
viewport motion. Static maps now request the timed refresh once per UTC second
(the clock/greyline resolution). Direct setters still update immediately;
contacts, TX, earthquake overlays, pruning and viewport motion retain the
original animation cadence. No decoder/search/depth settings change.

Actual `updatePaintNode` calls in a standalone Qt Quick scheduling test:
**31 -> 3 frames in 2.5 seconds** for an idle, fixed viewport. The original source
was compiled with the same test driver and failed the idle-frame regression
assertion as expected. The candidate passed idle, contact animation (14 frames
in one second), earthquake animation, return to idle, immediate setting change,
viewport motion and deactivate/reactivate checks. This is a software-rendered
scheduler test, not a GPU speed or whole-app CPU percentage claim.
Evidence: `map-cadence-after.txt`, `map-cadence-ab/{before.txt,commands.json}`.
An initial test correctly kept animation-rate frames while home-grid auto-zoom
was still moving; the idle fixture now explicitly fixes its viewport and tests
motion separately. The application Release build also succeeded.

The two-optimisation candidate also completed six real FT8/FT4 cycles with all
twelve expected per-step messages, matching the original fixture hashes and
message identities. Its RSS likewise plateaued and then fell. Evidence:
`mode-cycles-two-optim-after/`. These tests do not cover cancellation of a decode
already running when the mode changes; that remains a separate measurement.

Static-map application pilot (native backend, monitor off, 45 seconds each,
25–42 s sampling window): process `ps %cpu` median **20.4 -> 4.9**, mean
**20.68 -> 13.41**. Before is the two-optimisation executable, so the incremental
change is the map cadence. Both runs valid, no UDP errors. This is one local
pair, not a Windows or total-system-CPU result. The fresh lab window used the
minimal default geometry: the GPU map was instantiated, but did not emit its
nonempty first-frame diagnostic; the independent 640x360 renderer test covers
actual map-node updates. Repeat with representative visible map geometry before
extrapolating this CPU reduction. Evidence: `map-idle-before/`, `map-idle-after/`.

The renderer test additionally passes TX animation without contacts and checks
that reactivation produces a new frame, not just a previously counted frame.

After the map change, the busy/two-thread live audio scene again returns all
**36/36 messages**, identical per-slot multisets against original 3529dae3,
with no UDP errors. Evidence: `busy-map-after/`, `busy-map-comparison.json`.
Current three-optimisation application SHA-256:
`787142580c5cbd5257d896da8d8d03b465e5d6d791d385513bfc42d5eb30b126`.
The incremental two-optimisation reference remains available as
`build/decodium-two-optim-reference` (SHA-256 `49a8a17ad96439acf40824bbf71f65fd8b8e1070498ef27636dc1af48436a250`).

Follow-up map pilots **do not confirm a whole-app CPU improvement**:

| Layout | Median process %CPU before/after | Mean before/after |
| --- | ---: | ---: |
| Minimal window, empty map rectangle | 20.4 / 4.9 | 20.68 / 13.41 |
| 1600x900, map 356x38 | 27.8 / 30.7 | 22.19 / 20.45 |
| 1600x900, waterfall 180, map 356x208 | 11.1 / 31.6 | 15.73 / 21.57 |

All use the same 25–42 s window, one pair per layout, no UDP errors; the latter
two verify a nonempty map frame on Metal with the production shaders. The
instantaneous/smoothed `ps %cpu` samples are too variable and include results
worse after the change. Do not select only the favourable minimal-window pair.
Evidence: `map-visible-*`, `map-layout-*`, `map-idle-summary.json`.
The harness now also captures cumulative process CPU time, to measure actual CPU
seconds over matched intervals and reverse the execution order in the next pair.
The renderer scheduling reduction remains verified; application-level savings
are still under evaluation.

Cumulative-CPU follow-up, reversed order (candidate then reference), visible
356x208 map, same production Metal shaders and fixed layout: over ~26.3 seconds
inside the 15–42 s window, CPU time was **5.46 s before / 5.08 s after** (20.74%
vs 19.31% of one logical CPU). This pilot is consistent with a modest saving,
not the large reduction suggested by the first instantaneous-CPU median. More
repetitions are needed to quantify whole-app benefit. Evidence:
`map-cputime-before/`, `map-cputime-after/`, `map-cputime-summary.json`.

### Current continuation queue

- Keep the goal active. Three candidate optimisations are implemented and built:
  model role lookup, initialized profile lookup, idle GPU-map scheduling.
- Rendering regression checks pass, and the three-change build preserves 36/36
  busy-scene messages at two threads. Weak/Low CPU and two-instance functional
  proof currently cover the two-change build; extend those to the final set.
- Six actual FT8/FT4 cycles pass before/after the first two changes, without
  monotonic memory growth in these runs. No worker destruction policy was added.
- Measure cancellation of already-active FT4/Q65 work during a mode change;
  investigate timer/UI work still dominating process CPU. Do not infer a leak
  or safely reclaimable cache merely from retained RSS/thread-local pointers.
- Whole-app CPU measurement now uses cumulative CPU seconds. Any further paired
  runs must match layout profile, fixture, CPU settings, rendering mode and
  credential/network isolation. Do not compare sampled runs with build activity.
- A text question is pending about how the Windows reference PC can run the
  eventual A/B package. Continue local work independently; no access is assumed.
- At this checkpoint all lab applications are stopped, QML/test settings have
  been restored by the harnesses, Python syntax and `git diff --check` pass.
  No commit, push or release has been performed.

## 2026-11-14 — FT4 cooperative cancellation on mode exit

The existing test-mode hook proves the gap: with a live FT4 worker, changing to
FT8 completed in 7 ms, while the now-obsolete FT4 call continued until 97 ms.
The old result was already rejected by the mode/serial checks. A worker-owned
cancellation generation now advances only on explicit invalidation (the sentinel
used by mode reset) or shutdown. Ordinary new serials/slots do not advance it.
The native FT4 decoder polls at subpass/candidate/segment/AP-pass boundaries;
partial output is discarded when cancelled. The thread-local RAII scope restores
its predecessor and does not affect other decoder callers/threads. FFTW buffers
and reusable workers are retained; this change does not attempt unsafe teardown.

In the same active-worker laboratory, the invalidated FT4 call now completes in
**8 ms**, and the restored FT4 decode completes normally (102 ms in that run).
Both runs finish all six expected per-step messages, including recovery, with
no UDP errors. Evidence: `ft4-active-switch-before/`,
`ft4-active-switch-after/`. This is one local timing comparison, not a bound on
all cancellation latencies: a native operation between checkpoints must finish.
The pre-change binary includes the first three performance optimisations; its
FT4 decoder/worker implementation was still unchanged from 3529dae3.

`test_ft4_decode_cancellation` passes four QtTest checks covering known-message
recovery, nested cancellation scopes and independent-thread isolation. The
initial std::thread test hit its small default stack's guard on macOS in
`generate_ft4_reference`, confirmed by the crash report. The test now uses an
8 MiB QThread stack, matching the production worker, and passes. This was a test
stack sizing failure, not hidden as a passing result.

A/B native FT4 corpus: seven frozen fixtures (clean plus three nominal noise
levels and two seeds), depths 1 and 3, **14/14 identical complete outputs**.
Comparison includes message, SNR, DT, frequency, AP and quality fields. The corpus
contains both successful weak decodes and below-threshold zero-decode cases;
noise -20 seed 902 decodes only at depth 3, identically in both builds. Nominal
noise is not RF-calibrated sensitivity. Evidence: `ft4-cancel-ab/manifest.json`
and per-case before/after text. The pre-change CLI binary is retained as
`build/performance-baseline/ft4-stage-before`.

The three-change build also passed Low CPU weak-scene live audio: **21/21**
identical per-slot messages against 3529dae3, evidence
`weak-lowcpu-map-after/`, `weak-lowcpu-map-comparison.json`.
Current four-change app SHA-256:
`3ae6b48e77b1b681122e15e2261d4711ac4239a3b55e7f60b3485d48785e9454`.

The native A/B reference was strengthened by compiling
`git show 3529dae3:Detector/FtxFt4Decoder.cpp` with the current target's exact
compiler flags and linking that original object into the comparison CLI. All
**14/14 outputs still match** the candidate byte-for-byte. The older pre-existing
CLI was only a preliminary comparison; canonical evidence is the
`*-3529dae3.txt` reports, `reference-commands.json`, and the updated manifest.

Final four-change two-instance BlackHole test passed: both processes exited
zero, RX persisted `CQ 9H1SR JM75`, TX onset was slot +0.410 s, no UDP errors.
QML and the shared test INI again match their original hashes exactly. Evidence:
`two-app-four-optim-after.txt`, `two-app-four-optim-artifacts/`.

### Latest checkpoint / remaining work

Four candidate optimisations are built and verified to the scopes above.
No application/test process remains running. Syntax checks and diff whitespace
checks pass. No commit, push, release or production-settings change was made.
The goal remains active; do not call the overall work complete.

Next: audit/measure already-running Q65 and long-period worker behaviour across
mode changes, complete the resource-lifecycle inventory beyond FT8/FT4, and
investigate remaining UI/service CPU costs using cumulative CPU time rather than
instantaneous percentages. Retaining idle threads is still intentional because
native thread-local cache destruction/recreation has separate safety costs.
Keep the frozen fixtures, original app, canonical FT4 reference and per-build
hashes when adding further changes. Windows/RF acceptance remains unverified;
the question about executing the Windows A/B package is still pending.

### Session-boundary audit (2026-11-14)

Hardware reference confirmed: Windows 11, 8 GiB RAM, third-generation Intel i5.
No Windows runtime result is implied by the macOS measurements.

Long-period audit found a correctness prerequisite to queue optimisation:
WSPR and legacy JT callbacks can accept a result from an invalidated session
when request metadata has already been cleared. The first Q65-only QML probe
was not callable (private method), and the first C++ probe's fresh positive
control failed; neither is acceptance evidence. A subsequent Q65 positive
control also correctly failed because the test used `Q65` rather than the
canonical current mode `Q65-60A`. These failed attempts remain in the artifacts.

The corrected temporary C++ hook uses the existing invokable `logSstvQso`,
guarded by test mode and a specific probe key. Source is restored after build;
this instrumented binary is laboratory-only. The normal app must be rebuilt
after the probe. `profile_mode_cycles.py` now rejects a callback test unless
all three fresh positive controls pass, CAT/TX stay off, and WSPR temporary-file
cleanup succeeds. `--expect-late-rejected` additionally checks every old result.

Baseline `context-probe-before3/` is valid: real FT8 and FT4 fixture decodes pass;
Q65 old result is already rejected, WSPR and JT65 old results are accepted;
all three fresh results are accepted, and WSPR removes its temporary file.
No UDP error. The WSPR/JT65 fresh controls intentionally omit metadata to cover
slow valid results after the existing 60-second metadata expiration.

Candidate fix records the highest issued serial at each existing session
invalidation (mode, band, RX restart, native-to-legacy handoff). Old serials are
rejected independently of the expiring maps. WSPR still removes its temporary
WAV before rejection; stale results cannot clear the newer session's busy flag.
JT callbacks now also discard their finished request metadata. Decoder search,
iteration depth and Low CPU policy are unchanged.

Instrumented A/B now passes: `context-probe-after/` uses the same three controls
and requires stale rejection. Q65/WSPR/JT65 all reject old results and accept
fresh ones; WSPR temporary WAV and map entry are removed. Real FT8/FT4 fixture
decodes also succeed, CAT/TX remain off, and UDP errors are absent. This is a
callback/session regression test, not a sensitivity test of the WSPR/JT/Q65
signal-processing engines, nor an active-worker cancellation test. Before/after
instrumented binary hashes and fixture hashes are recorded in each result JSON.
The first failed attempts are not used for this conclusion.

Further lifecycle work remains: WSPR currently holds the shared decoder runtime
mutex through `wsprd_run`, with no cooperative cancellation API; queued WSPR
calls cannot simply be removed because their completion cleans the temporary
WAV. FST4 and Q65 also use shared runtime state; JT/Q65 averaging state must be
preserved deliberately. No claim of cancellation or unloaded memory for these
workers is made by the callback fix.

The normal application was rebuilt after removing the temporary hook.
SHA-256: `fbe7f6facde54523f2471d4eeb1872a8bfad9986bcde899775d8bbbcaf1a078d`.
Four focused CTest suites pass (profile restart, decode model, map cadence,
FT4 cancellation); see `context-focused-tests.txt`. The final build contains
only the existing macOS deployment-target linker warnings.

Two normal app instances pass the BlackHole TX/RX check again: both exit zero,
RX persists `CQ 9H1SR JM75`, TX onset is slot +0.385 s, no UDP errors. Evidence:
`two-app-context-after.txt` and `two-app-context-artifacts/`. This verifies
virtual-audio operation, not physical radio or Windows hardware behaviour.

Final weak-scene A/B against original 3529dae3: **21/21 identical per-slot
messages**, two threads and Low CPU enabled in both builds. Requested settings
were verified in runtime diagnostics. No missing or extra messages, no UDP
errors. Evidence: `weak-lowcpu-context-after/` and
`weak-lowcpu-context-comparison.json`. This preserves Low CPU's existing policy;
it does not claim parity with a different CPU/depth configuration.

Latest checkpoint: four performance optimisations plus session-result isolation
are built. QML and the shared test INI match their pre-lab hashes exactly;
no Decodium process remains running. Source instrumentation is absent, Python
syntax checks and `git diff --check` pass. No commit, push or release.
The goal remains active: long-period queued/active-work cancellation, broader
resource lifecycle and UI/service profiling, and Windows i5/8 GiB acceptance
are still outstanding. This checkpoint does not claim completion of those tasks.

### Requested Windows test package

User requested a package to run on the Windows i5 machine. Added a PowerShell
5.1-compatible runner (no Python dependency on the target), CMD launcher,
Italian instructions, frozen FT8/FT4 fixtures and a temporary-QML probe. Runs
are A-B-B-A, with two threads and Low CPU off by default; optional Low CPU is a
separate matched run. It records cumulative CPU seconds, working set/private
bytes, first observed rows, complete per-case message multisets and raw logs.
The 15-second minimum per-case settling interval is explicitly not decode time.
CAT/monitor are disabled. Portable apps are copied before QML instrumentation;
legacy Qt test settings are backed up and restored. Ports 2237/2239 must be free.
Only the test-area files decodium4.ini and ft2.ini are temporarily emptied.
Qt Windows test/config path mapping was checked against the Qt 6.11 source.

The bundle manifest records every fixture and portable-build file hash and a
candidate source patch generated through a temporary Git index (no user index
changes, commit or push). Missing builds cause the runner to stop before
launching Decodium. No Windows binary exists locally for the current changes;
the staged ZIP is deliberately `ready:false`, not a ready-to-run application.
Windows executable/runner acceptance still requires Windows execution.

PowerShell parser check passes using an official SHA-256-verified macOS
PowerShell runtime. Comparator tests pass for reordered equal messages,
same-count changed messages, missing duplicates and empty results, for both
A/B pairs. This checks portable script logic, not Windows process APIs.
The first local run of the exact QML probe passes all four fixtures:
clean FT8=1, busy FT8=12, weak FT8=9, clean FT4=1; no UDP errors. Evidence:
`windows-kit-probe/`. A small ordering fix starts the observation timer before
opening the WAV so very fast zero-result callbacks are also observed.

Final staged kit: `build/performance-baseline/Decodium4-Windows-AB-kit.zip`,
SHA-256 `0d680881c6a5540abc49877514098fdaa0199e35b215f24298d54a1c04f130a4`.
The corrected exact packaged QML probe passes again (1/12/9/1 messages) in
`windows-kit-final-probe/`. Packaged PowerShell files parse, the comparator's
four positive/negative cases pass, ZIP and fixture/patch hashes verify. The
incomplete-build preflight was exercised and correctly refuses to launch or
create results. Original QML/shared test settings were restored exactly.
These are macOS probe/script checks; Windows process/runtime validation is open.
The ZIP freezes the source patch before this documentation-only checkpoint.

Pending user approval: create/push two laboratory branches
`perf/windows-ab-baseline` and `perf/windows-ab-candidate`, then dispatch the
existing Windows workflow with no release tag. GitHub authentication is
available and those remote branch names are unused. No branch, commit, push,
workflow dispatch or release has been performed. Approval is needed to move
currently local candidate code onto GitHub; absent Windows binaries are not
silently replaced with unrelated releases. The optimisation goal stays active.

### WSPR queue retirement and authorised Windows builds (2026-11-14)

User authorised the two laboratory branches and Windows builds, without a
release. Baseline branch now points to exact 3529dae3; workflow run
https://github.com/elisir80/Decodium-4.0-Core-Shannon/actions/runs/43613527579
is building. The first dispatch used gh's inferred upstream repository and was
rejected (422, no matching ref); explicit `--repo elisir80/...` dispatched the
correct run. No workflow ran on the upstream repository.

WSPR previously executed old queued requests after mode/session changes, even
though their callback was discarded. `invalidateDecodeSession()` now updates
the existing retired-serial boundary and notifies the WSPR worker. The worker
checks it before and after acquiring the shared runtime mutex. A skipped request
still emits completion, so the bridge's existing pre-gate WAV cleanup runs.
No active `wsprd_run` is interrupted, no worker is destroyed/recreated, and no
depth/Low CPU policy changes. The two small legacy worker files were normalised
from CRLF to LF to avoid new trailing-whitespace errors.

`test_wspr_worker_queue` exercises an actually blocked native entry point with
old/fresh requests behind it, plus retirement while waiting for the runtime
mutex. All four QtTest checks pass. Compiling the original 3529dae3 worker against
the same driver makes the test fail (three native calls instead of two). A
separate deliberate mutant without the post-lock check fails the lock-wait
case. These are controlled-worker tests, not measurements of native WSPR speed.
Evidence: `wspr-queue-tests.txt`, `wspr-queue-ab/` including compiler commands.

Native application A/B uses WSJT recording `150426_0918.wav`, SHA-256
`0f1e41e6a94fefd3df9efd8234dd6342dfd7e6675700f70e79e69eec955537da`.
The exact original application 3529dae3 and the candidate both produce **8/8**
identical message/DT/frequency multisets at depth 3, with no UDP errors.
Evidence: `wspr-native-3529dae3/`, `wspr-native-after/`,
`wspr-native-comparison.json`, `wspr-queue-source-record.json`.

The Windows kit generator now includes this WSPR fixture alongside FT8/FT4,
with provenance in its manifest. The QML probe reads the actual `db` SNR field.
The candidate Windows workflow additionally runs the five focused performance/
lifecycle test suites before creating its portable bundle. Earlier staged ZIPs
remain frozen checkpoints; the final package must use the new candidate patch
and the successful Windows artifacts, not an older staging ZIP.
