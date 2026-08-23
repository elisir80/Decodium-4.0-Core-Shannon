# Native SSTV threat model

Status: Milestone 0 security design, 2026-08-24. This document defines requirements; it does not claim that the SSTV subsystem or its controls are implemented.

## Scope and repository evidence

The scope is the native Decodium4 SSTV subsystem: analog and HAMDRM input, imported images and WAV files, local storage/gallery, remote image sharing, and the hand-off of a prepared image to the existing Decodium TX/CAT/PTT path. Remote IP sharing is not RF SSTV and must never be represented as an on-air mode.

The current checkout provides useful building blocks, but no `src/sstv` implementation or production SSTV sharing backend was found at this audit point:

- `src/security/SecureSettings.*` exposes macOS Keychain, Linux Secret Service via `secret-tool`, and Windows DPAPI backends. Its convenience functions can fall back to plain `QSettings` when the platform backend is unavailable or a store fails. That fallback is unacceptable for SSTV passwords, tokens, signed URLs and private keys: SSTV must use a fail-closed path.
- `Network/NetworkAccessManager.*` reports TLS errors and aborts the reply; it does not call `ignoreSslErrors()`. It is a reuse point, not by itself an HTTPS, redirect, size or response-validation policy.
- Decodium already uses `QStandardPaths` for writable application data and `QSaveFile` for atomic downloads/caches. SSTV storage must follow those patterns and add root/path and content validation.
- `lib/persistence/DecodeHistoryWorker.*` demonstrates the Qt SQL threading rule: a named SQLite connection is created and used on its owning worker thread, with queued calls and transactional batches. SSTV needs its own schema/migrations and worker-owned connection; this existing worker is not an SSTV database.
- `Network/RemoteCommandServer.*` is an HTTP/WebSocket radio-control console, including non-TLS transports. It has no recipient directory, resumable object store, inbox, expiry or E2EE contract. It is not an SSTV sharing relay and must not be treated as one.

## Assets and security objectives

| Asset | Required property |
| --- | --- |
| CAT/PTT and TX audio ownership | Only an explicit local TX action can key the radio; PTT is released on every path. |
| Received/prepared images and retained audio | Integrity, bounded processing, predictable retention and no unintended disclosure. |
| Gallery metadata and QSO links | Integrity, privacy, transactional persistence and correct ownership. |
| Provider credentials and E2EE private keys | Confidentiality, least lifetime, redacted diagnostics and secure deletion. |
| Sender/recipient identity | Stable provider ID; a callsign or display name alone is not authentication. |
| Transfer state | Durable, idempotent and resistant to replay, duplication and rollback. |
| Decodium availability | Bounded CPU, memory, disk, queue and network use; no work on GUI/audio callbacks. |
| Existing Decodium modes and audio | Isolation from SSTV failure or overload; no competing capture or PTT stack. |

## Trust boundaries and data flow

```text
untrusted RF/audio/WAV --> bounded native RX/DSP --> candidate pixels
                                                    |
untrusted HTTPS provider --> quarantine --> validators + hash --> image store
                                                    |
                                      QSaveFile + QStandardPaths roots
                                                    |
                                  SSTV SQLite worker --> gallery/QML

gallery/preparer -- explicit local confirmation --> existing TX coordinator
                                                   --> existing audio/CAT/PTT
```

The following boundaries are security-significant:

1. RF, sound-card, RTL-SDR, DecoPort and imported WAV samples entering native DSP.
2. User-selected or remotely supplied image files entering Qt image codecs.
3. JSON, bytes, redirects and authentication challenges crossing Qt Network.
4. Quarantine files becoming gallery objects.
5. C++ models exposing bounded, non-secret data to QML.
6. `SecureSettings` platform storage versus ordinary settings/SQLite/logs.
7. Worker queues versus GUI/audio threads.
8. A prepared image becoming a request to the existing TX/PTT coordinator.

## Actors and attacker capabilities

- A legitimate local operator who can make mistakes, choose unsafe files or misconfigure a provider.
- A malicious RF station or crafted WAV author controlling the complete audio waveform.
- A malicious remote sender controlling metadata, JSON, filenames and image bytes.
- A compromised, curious or faulty provider/relay that can inspect, reorder, replay, truncate or delete traffic.
- A network attacker able to redirect, intercept or delay traffic but not defeat correctly validated TLS.
- Another process or local user able to modify writable files or exhaust local resources; compromise of the logged-in OS account is not fully mitigated.
- A compromised image codec, Qt/network dependency, credential backend, radio or CAT endpoint.

## Mandatory security invariants

- **TM-01 — Network/RF separation:** sharing code has no API that asserts PTT, starts TX audio or selects an on-air mode. Accepting an inbox item only imports it. Transmit always requires a separate, visible local action and existing Decodium TX interlocks.
- **TM-02 — No implicit trust:** every image, WAV, manifest, server response, database row and filename is untrusted even after TLS or E2EE.
- **TM-03 — Bounded work:** parsers and queues enforce byte, dimension, pixel, duration, depth, string, chunk, redirect and retry limits before allocation.
- **TM-04 — Quarantine first:** remote bytes are streamed to a private temporary-transfer location, hashed and validated before decoding or gallery import.
- **TM-05 — Atomic promotion:** final files are written with `QSaveFile` under explicit `QStandardPaths`-derived roots; database publication occurs only after commit succeeds.
- **TM-06 — Secret fail-closed:** if the platform secure backend is unavailable, SSTV authenticated sharing/E2EE is disabled with an actionable error. Secrets never fall back to `QSettings` or SQLite.
- **TM-07 — TLS fail-closed:** production providers use HTTPS, never ignore certificate errors and never silently fall back to HTTP. Credentials are not forwarded across origins.
- **TM-08 — E2EE policy is sticky:** a transfer created as `required` cannot be retried, resumed or completed as cleartext. Missing keys/capability is a hard failure, not a downgrade.
- **TM-09 — Thread ownership:** networking, hashing, file encoding/decoding and SQLite writes do not run on the GUI or audio callback thread. Each Qt SQL connection remains on its owning worker thread.
- **TM-10 — TX fail-safe:** SSTV uses the existing Decodium audio/CAT/PTT coordinator, exclusive TX ownership, lead/tail policy and watchdog; cancellation, exception, disconnect and shutdown all release PTT.
- **TM-11 — Privacy defaults:** upload, public sharing, automatic content download, EXIF retention and metered-network background transfer default off. Recipient and expiry require confirmation.
- **TM-12 — Redaction:** logs and diagnostic exports omit authorization headers, passwords, tokens, signed URLs, private keys, complete envelopes, image bytes and unnecessary personal metadata.

## Abuse cases and required controls

| Threat / abuse | Required controls | Verification gate |
| --- | --- | --- |
| PNG/JPEG/WebP decompression bomb or huge animation | Check compressed bytes; probe dimensions/frame count before full decode; checked pixel/memory arithmetic; hard codec and thumbnail budgets; no external references. | Corpus at and beyond each limit; peak allocation remains bounded. |
| MIME spoofing, corrupt or polyglot image | Compare declared MIME, magic and decoder result; allowlist formats; decode in worker; strip EXIF by default; never execute embedded content. | Mismatch, truncation, animation and malformed-metadata tests. |
| Malformed RIFF/WAV and adversarial RF audio | Checked RIFF/chunk arithmetic, format/channel/rate/duration limits, streaming decode and bounded rings; deterministic abort. | Truncated, overlapping, oversized and integer-overflow corpus plus fuzzing. |
| Deep/large/ambiguous JSON | Manifest maximum 64 KiB, depth 8, bounded strings/arrays, integer range checks, duplicate-key rejection before semantic validation, supported major version only. | Parser hostile-input and fuzz tests. |
| Path traversal or overwrite | Treat remote names as display text; generate UUID storage names; reject absolute paths, separators, `..`, Windows reserved names, controls and invalid Unicode; verify destination remains under its root and does not escape through a symlink; explicit overwrite only. | Cross-platform traversal/symlink/collision tests. |
| Partial/crash-corrupt save | Stream to quarantine; `QSaveFile::commit()` for final object; fsync/transaction ordering; recover or remove orphan temporary data after restart. | Fault injection before/after file and DB commits. |
| MITM, bad certificate or HTTP downgrade | HTTPS-only URL policy, strict Qt TLS verification, abort on any TLS error, compile-time-only loopback exception for tests. | Expired, self-signed, wrong-host and downgrade tests. |
| Redirect credential theft / SSRF | Maximum redirects; resolve each URL; permit HTTPS; reject userinfo and local/link-local targets for Internet providers; strip auth on any origin change and require provider-approved redirect policy. | Cross-origin, DNS/port/scheme and redirect-loop tests. |
| Oversized/chunked network response | Preflight `Content-Length` when present, count actual streamed bytes, abort above manifest/provider/application limit; validate ranges and chunk hashes. | Missing/false length, endless stream and range-overlap tests. |
| Replay, duplicate completion or corrupt resume | Transfer UUID plus idempotency key, immutable manifest hash, server status reconciliation, per-chunk range/hash, final SHA-256, bounded retry. | Restart/resume, repeated request and changed-body conflict tests. |
| Expired or revoked object accepted | Compare UTC using a sane-clock policy; provider enforces expiry; client refuses new download/import after expiry and requests deletion/revocation. | Boundary clock, stale listing and in-flight expiry tests. |
| Inbox spam and disk exhaustion | Metadata-only polling by default, paging/rate limits, bounded persistent queues, quota reservation before download, explicit accept, block/reject where supported. | Flood, pagination, quota and restart tests. |
| Sender spoofing by callsign | Authenticate stable provider recipient/sender ID; callsign is display metadata; surface verification/fingerprint/trust state. | Same-callsign/different-ID and changed-key tests. |
| Credential disclosure through current fallback | Use `secure_settings::Backend` availability/store/lookup directly or add a fail-closed SSTV wrapper; persist only opaque credential handles in SQLite; never expose values to QML. | Backend-unavailable/store-failure tests and settings/log scans. |
| Signed URL leakage | Treat full pre-signed URLs as bearer secrets; never log or persist in ordinary DB; reacquire after restart or store only through secure backend with short expiry. | Redaction and restart tests. |
| Compromised relay reads content | E2EE with an audited packaged library, authenticated encryption, unique nonce, recipient key, manifest-bound AAD and integrity before decode. UI must say “provider can read” when TLS-only. | Ciphertext tamper, wrong key/AAD and downgrade tests. |
| Nonce/key reuse or rotation confusion | Library-generated nonce, per-recipient envelope, key ID/fingerprint, algorithm allowlist, rotation history and no private-key export in diagnostics. | Duplicate nonce detection and rotated/revoked-key tests. |
| SQLite race/tampering | Dedicated worker connection, prepared statements, constraints, transactional versioned migrations, WAL/busy timeout, bounded queue and validation when reading rows. No secrets/BLOB images. | Migration interruption, concurrency and corrupt-row tests. |
| QML injection or secret lifetime | Expose typed roles, not executable markup/URLs; escape display strings; keep credentials in C++; bound model text and collection sizes. | Malicious filenames/messages and object-lifetime tests. |
| Network item triggers RF transmission | No inbox/provider-to-TX signal; remote item becomes ordinary gallery content; explicit user action passes normal TX permission/exclusivity checks. | Architectural test plus attempted forged “transmit” metadata. |
| PTT remains asserted | RAII/fail-safe TX ownership, watchdog, idempotent release and shutdown ordering through existing coordinator. | CAT failure, timeout, audio loss, cancellation at every TX state, sleep and shutdown. |
| Diagnostic/privacy leakage | Structured `sstv.*` categories, centralized redaction, consent before image/raw-audio export and bounded logs. | Golden redaction tests and diagnostic archive inspection. |

## Storage and permissions requirements

All default roots derive from `QStandardPaths` after Decodium application identity is established. Received, transmitted, digital, remote, WAV, raw-audio, thumbnail and temporary-transfer data use separate subdirectories. Installation and source directories are never targets. Temporary and final roots are created with user-only permissions where the platform permits; permission failures stop the operation.

The file is authoritative and SQLite stores its validated path, SHA-256 and metadata. Promotion order is: validate and hash quarantine bytes; atomically commit the final file; transactionally insert/update the database record. A database failure leaves a recoverable orphan for a bounded reconciliation job, never a row pointing at partially written bytes. Deletion checks favourites/QSO retention, removes the database record transactionally and reports whether remote deletion is only best-effort.

## Credentials and E2EE decisions

SSTV provider passwords, bearer/refresh tokens, API keys, persistent pre-signed URLs and private encryption keys use namespaced accounts in Decodium `SecureSettings`. Only an opaque provider/account identifier may be stored in the SSTV SQLite tables. The implementation must not call a helper path that returns the plaintext for ordinary-settings persistence when the secure backend fails.

E2EE is a capability and policy, not a checked box. Until an audited cryptographic library is selected, packaged on all release platforms and tested, E2EE remains **not implemented**. TLS-only sharing may be implemented, but the UI must state that the provider can read content. A user selecting “E2EE required” cannot send through a TLS-only provider.

## Required security test evidence

Before release, evidence must include:

1. Unit and fuzz coverage for VIS/FSK ID, WAV, image metadata, manifest JSON and range/chunk parsing.
2. A deterministic local provider/server exercising upload/download, authentication, cancellation, retry, idempotency, resume, wrong hash, expiry, revocation and inbox rejection.
3. TLS tests for certificate, hostname, redirect and scheme failures; a repository scan proving production code never ignores TLS errors.
4. Secure-backend unavailable/read/store/remove failures proving no SSTV secret enters `QSettings`, SQLite, QML, logs or diagnostic exports.
5. Storage tests for traversal, Unicode/reserved names, symlink escape, collisions, atomic failure, quotas and restart reconciliation.
6. Image/WAV hostile corpora with measured peak memory/CPU and no GUI/audio-thread work.
7. SQLite migration, thread-affinity, concurrent read/write, bounded-queue and shutdown tests.
8. E2EE tests, if enabled, for tamper, wrong recipient/key/AAD, key rotation, nonce uniqueness and silent-downgrade prevention.
9. TX tests proving a network transfer cannot key the radio and PTT is released after every success, failure and cancellation path.

## Residual risks and release blockers

- No production SSTV sharing backend has been identified or audited. A protocol, abstraction and local test provider do not create one.
- The existing `SecureSettings` convenience fallback conflicts with TM-06; SSTV authenticated sharing is blocked until it has a fail-closed integration.
- TLS-only providers can inspect content and visible manifest metadata. This must remain explicit until E2EE is packaged and independently reviewed.
- A compromised logged-in OS account, credential store, Qt image plugin, crypto library, radio firmware or recipient device can defeat application controls.
- Revoke/delete cannot retract copies already downloaded, backed up or captured by the recipient/provider.
- Callsigns are not cryptographic identity; initial key verification remains a social/operational trust decision.
- Metadata exposed outside an E2EE payload can reveal callsigns, timestamps, mode and object size; minimisation remains necessary.
- Existing Decodium remote-control transports are outside this protocol and must not be reused as an Internet image relay without a separate audit.
- Live RF/CAT/audio behavior and platform permission semantics require hardware and maintained-platform verification; static review is insufficient.
