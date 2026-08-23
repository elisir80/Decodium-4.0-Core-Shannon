# Decodium SSTV Remote Sharing Protocol

Status: protocol design v1 for Milestone 0, 2026-08-24. No production Decodium SSTV sharing backend is known in this checkout. This document is not evidence that a provider, relay, inbox or E2EE implementation exists.

## Scope and separation from radio

DSRP transfers an already received or prepared image over an IP network. It is not analog SSTV, VIS/FSK ID, HAMDRM or any other RF mode. Provider and inbox code must not assert PTT, start TX audio or request CAT changes. Importing an item creates a local gallery record only; on-air transmission requires a later explicit local action through Decodium's existing TX coordinator and interlocks.

All client-side code belongs natively inside Decodium4 and reuses Qt Network, `SecureSettings`, `QStandardPaths`, `QSaveFile`, the native models and a worker-owned Qt SQL connection. A relay, when one actually exists, is an external service and must be configured; local SSTV remains fully usable without it.

`Network/RemoteCommandServer.*` is a radio-control console over HTTP/WebSocket, not a DSRP provider. It must not be advertised or silently extended as a production file-sharing backend. Until a separate backend audit proves all protocol, authentication, storage, expiry and operational requirements, the production-backend status is **none**.

## Protocol identifiers and versioning

- Protocol name: `decodium-sstv-share`.
- Manifest major/minor: `1.0`.
- Media type: `application/vnd.decodium.sstv-share+json;version=1`.
- UUIDs use lower-case canonical RFC 4122 text.
- Times use RFC 3339 UTC with `Z`; clients retain millisecond precision at most.
- Hashes are lower-case hexadecimal SHA-256.
- Major versions are incompatible and must be rejected. A receiver may accept a higher minor version only if all required fields and semantics are understood; unknown optional fields are retained or ignored, never interpreted as security policy.
- The exact UTF-8 manifest bytes are immutable after upload creation. For hashing/AAD, JSON is serialized with RFC 8785 JSON Canonicalization Scheme. Duplicate object keys, non-integer numeric fields, invalid Unicode and non-canonical security-critical input are rejected before use.
- Manifest size is at most 64 KiB, JSON nesting depth at most 8, free-form message at most 2 KiB UTF-8 and every identity/display field has a provider-declared limit no greater than 4 KiB.

## Provider abstraction

`SstvShareProvider` isolates gallery/QML from transport details. It is asynchronous, cancellable and injected with a Qt network manager; it never owns GUI, storage or TX policy. The stable interface supports:

- capability discovery and authentication status;
- recipient lookup by stable provider ID, not callsign alone;
- create upload, upload chunk, resume/query upload, complete and cancel;
- list incoming metadata, download, acknowledge, reject and block where supported;
- revoke/delete remote object and refresh credentials;
- byte progress and structured, redacted errors.

Capabilities are explicit data, including API versions, maximum bytes/chunk size, resumable upload, out-of-order chunks, inbox, acknowledgement, revoke/delete, expiry bounds, E2EE suites and recipient-key lookup. UI actions remain disabled when a capability is absent. A provider must never emulate success for an unsupported operation.

Expected native providers are generic HTTPS REST, WebDAV over HTTPS, trusted-service-issued pre-signed HTTPS PUT, a deterministic local integration provider, and a peer/relay provider only after audit. Provider-specific HTTP and JSON do not leak into QML or gallery models.

Errors are classified as `temporary`, `rate_limited`, `authentication`, `authorization`, `validation`, `conflict`, `expired`, `revoked`, `not_found`, `integrity`, `tls`, `local_io`, `cancelled` or `unsupported`. Only temporary/rate-limited errors retry automatically; authentication and validation never loop indefinitely.

## Manifest v1

Every transfer has one immutable manifest. Synthetic example:

```json
{
  "protocol": {"name": "decodium-sstv-share", "version": "1.0"},
  "transfer_id": "018f2f79-2a3d-7d91-8d42-111111111111",
  "sender": {
    "provider": "local-test",
    "id": "sender-0001",
    "callsign": "N0CALL",
    "grid": "AA00aa",
    "key_id": ""
  },
  "recipient": {
    "id": "recipient-0002",
    "callsign": "N1TEST",
    "key_id": ""
  },
  "created_utc": "2026-08-24T10:00:00.000Z",
  "expires_utc": "2026-08-31T10:00:00.000Z",
  "content": {
    "original_filename": "synthetic-test-card.png",
    "display_filename": "synthetic-test-card.png",
    "mime_type": "image/png",
    "byte_size": 123456,
    "sha256": "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef",
    "width": 320,
    "height": 256,
    "chunk_size": 1048576,
    "chunk_count": 1,
    "disposition": "attachment"
  },
  "sstv": {
    "classification": "analog",
    "mode": "Martin M1",
    "event_utc": "2026-08-24T09:58:00.000Z",
    "completion": "complete",
    "completion_percent": 100
  },
  "message": "Synthetic protocol example",
  "privacy": {
    "public": false,
    "location_included": false,
    "exif_retained": false
  },
  "encryption": {
    "policy": "tls_only",
    "suite": "none",
    "recipient_key_id": "",
    "nonce": "",
    "ciphertext_sha256": ""
  }
}
```

Required validation:

- `transfer_id`, sender/recipient stable IDs, creation/expiry, original and safe display filename, MIME, byte size, plaintext SHA-256, dimensions, source classification, event time, completion, protocol version, encryption policy, chunk size/count, disposition and privacy flags are present.
- Sender/provider and recipient IDs match the authenticated operation; display callsigns are metadata only.
- `created_utc < expires_utc`; expiry is within provider policy and not already elapsed.
- Byte size, dimensions, pixel arithmetic and chunk arithmetic are non-negative, within negotiated application/provider limits and internally consistent. Empty content is rejected.
- MIME is allowlisted and later verified against bytes and decoder output. The original filename is never used as a local path; `display_filename` is sanitized display text.
- `classification` is `analog`, `digital` or `prepared`; remote sharing itself is never a classification/mode.
- `completion` is `complete` or `partial`, and percent is an integer from 0 through 100.
- Public defaults false; location/EXIF default absent or false. Missing security/privacy fields do not become permissive.

The client computes `manifest_sha256` over canonical manifest bytes. Providers bind the transfer object to that digest; a create/resume/complete request presenting a different digest for the same transfer ID returns conflict.

## HTTP transport profile

Production requests are HTTPS only. The client validates URL syntax, scheme, host, port, certificates and hostname with Qt; it never calls `ignoreSslErrors()` and never offers a normal UI bypass. Plain HTTP is permitted solely for an explicitly compiled test/development build and loopback deterministic server. Release builds contain no runtime checkbox for it.

The generic REST mapping to be captured in `remote-sharing-openapi.yaml` is:

| Operation | Method and path | Idempotency |
| --- | --- | --- |
| Capabilities | `GET /api/v1/capabilities` | Cache with bounded expiry. |
| Recipient lookup | `GET /api/v1/recipients/{id}` | Read only; response authenticated by TLS/provider. |
| Create | `POST /api/v1/transfers` | `Idempotency-Key: <transfer_id>` and manifest digest. |
| Status/resume | `GET /api/v1/transfers/{id}` | Returns immutable digest, state and accepted ranges. |
| Chunk | `PUT /api/v1/transfers/{id}/chunks/{index}` | Same bytes/range/hash may repeat; changed bytes conflict. |
| Complete | `POST /api/v1/transfers/{id}/complete` | Idempotent; repeated completion returns the same object state. |
| Cancel/revoke | `DELETE /api/v1/transfers/{id}` | Idempotent; missing/already deleted is success where ownership matches. |
| Inbox | `GET /api/v1/inbox?cursor=...` | Bounded page and opaque cursor. Metadata only by default. |
| Download | `GET /api/v1/transfers/{id}/content` | Range only if advertised; always bounded and re-hashed. |
| Decision | `POST /api/v1/transfers/{id}/decision` | Idempotency key includes transfer and `accept`, `reject` or `ack`. |

Bearer/OAuth credentials stay in C++ and are loaded from a fail-closed `SecureSettings` integration. Authorization headers and cookies are stripped on every origin change. Redirects are bounded, HTTPS-only and accepted only under explicit provider policy. Full signed URLs are credentials: never log them or store them in ordinary settings/SQLite. A trusted service must issue or refresh pre-signed PUT URLs; user-supplied URLs require explicit validation and confirmation.

Responses have allowlisted status codes/MIME, bounded headers/body and strict JSON validation. The client counts actual streamed bytes even when `Content-Length` is missing or false. Rate limiting honors a bounded `Retry-After` plus jitter.

## Idempotency, chunks, resume and integrity

The generated transfer UUID and idempotency key remain stable across retry and restart. The SQLite worker persists manifest digest, object ID, state, retry count/time, accepted byte ranges/chunks and provider account handle before the corresponding network action. Secrets and full content do not enter SQLite.

Chunks cover the content exactly once without gaps/overlap. Index, start, length and SHA-256 are checked with overflow-safe arithmetic. Provider-advertised chunk size is clamped to Decodium's bounds. On resume the client queries status and accepts progress only when transfer ID, manifest digest, total length and already accepted chunk hashes match locally; otherwise it restarts safely or fails conflict. Server-reported offsets beyond total size are rejected.

Completion requires all chunks plus a streaming final SHA-256. The receiver downloads to quarantine, verifies received byte count and ciphertext hash (when encrypted), authenticates/decrypts if required, verifies plaintext SHA-256, then validates image MIME/dimensions before decode. Only after those gates does `QSaveFile` atomically promote a UUID-named object under a `QStandardPaths` root and the SQLite worker publish it to the gallery.

Duplicate delivery is detected by provider/transfer ID and content hash. It never overwrites a distinct local record silently. Cancellation stops network/file work, persists `Cancelled`, removes safe temporary data and requests provider cancellation; it does not claim remote deletion unless acknowledged.

## Durable state machines

Outgoing states and legal principal transitions:

```text
Draft -> Queued -> Preparing -> Encrypting? -> Uploading
Uploading -> WaitingForAcknowledgement -> Completed
any active -> Paused -> Queued
temporary error -> RetryScheduled -> Queued
any nonterminal -> Cancelled | Rejected | Expired | Failed
```

`Encrypting` is skipped only when the persisted policy is `tls_only`. `Uploading` includes create/chunk/complete substate persisted with bytes. Provider upload completion does not imply recipient acknowledgement. Terminal states are durable; retry never moves `Expired`, `Rejected` or `Cancelled` back to active without creating a new transfer UUID.

Incoming states:

```text
ListedMetadata -> AwaitingDecision -> Downloading -> Verifying
Verifying -> ReadyToImport -> Accepted
AwaitingDecision -> Rejected
any nonterminal -> Expired | Revoked | Failed
```

Inbox polling downloads safe, bounded metadata only by default. `Accepted` means a validated atomic local import, not an RF transmission. Restart reconciliation compares persisted state, quarantine/final files and provider status before advancing. Every transition records UTC, reason/error class and monotonically increasing local revision through the SQLite worker.

## Expiry, inbox and deletion

- Expired transfers cannot be newly uploaded, resumed, downloaded or imported. In-flight work aborts when expiry is observed; bounded clock-skew handling may refresh trusted time but cannot extend expiry silently.
- The sender chooses an explicit expiry within provider limits. A provider advertises retention/deletion semantics and should delete content after expiry; the client displays when deletion is best-effort.
- Inbox entries show authenticated sender ID, callsign as display data, provider, safe preview/metadata, size, mode, timestamp, hash, message and expiry. Automatic image download is off. Preview bytes undergo the same bounded validation and cannot reference remote content.
- Accept, reject, acknowledge, block and remote-delete are separate actions with auditable results. Reject does not imply deletion; revoke cannot retract copies already downloaded.
- Quotas are reserved before download. Paging, queue size, polling cadence and retry count are bounded. A malicious sender/provider cannot create an unbounded in-memory model.

## Credential model

SQLite/QSettings store only provider configuration, non-secret policy and an opaque account identifier. Passwords, access/refresh tokens, API keys, persistent signed URLs and private keys use Decodium's platform `SecureSettings` backend. Because the current convenience API can fall back to plain `QSettings`, SSTV must call a fail-closed wrapper/direct backend path: unavailable lookup/store disables authenticated transfer and reports the error. Credentials are never exposed as QML properties, diagnostics or crash context and are held in memory only for the request lifetime where practical.

## E2EE and downgrade prevention

Protocol v1 defines policy even when no E2EE implementation is built:

- `encryption.policy` is `tls_only` or `e2ee_required`; it is immutable and included in manifest hashing/authenticated data.
- `e2ee_required` needs a mutually supported, versioned suite from an audited packaged crypto library, a verified recipient public-key ID/fingerprint, authenticated encryption and library-generated unique nonces. No cryptographic primitive is implemented locally.
- Canonical manifest bytes are authenticated associated data. The envelope records suite, sender/recipient key IDs, nonce and ciphertext SHA-256; the plaintext SHA-256 is verified only after successful authentication/decryption and before image processing.
- Create/status/complete responses echo encryption policy, suite, key IDs and manifest digest. Any missing/changed value fails `integrity`; retry/resume cannot change it.
- Key rotation creates a new key ID. A resumed transfer continues with its original valid key or fails; it never silently selects cleartext or an unverified replacement.
- If E2EE is not compiled, selecting `e2ee_required` is unavailable/fails before queueing. `tls_only` remains a separate explicit choice and the UI states that the provider can read content.

E2EE is **not implemented by this document**. Its availability may be claimed only after dependency, packaging, test-vector and interoperability evidence exists on maintained platforms.

## Privacy, logs and UI contract

Remote sharing is opt-in. Automatic upload/public sharing/content download, EXIF/location retention and metered background transfer default off. Recipient, provider, encryption status and expiry are confirmed visibly. Callsign/grid inclusion is configurable. Upload preparation strips EXIF unless explicitly retained.

Logs contain transfer UUID, provider name, state, redacted error class, byte counts and timing where useful. They exclude authorization data, signed URLs, private keys, full envelopes/content and unnecessary identity/message data. Provider errors are normalized before reaching QML; QML receives typed bounded fields, not credentials or executable markup.

## Implementation and conformance gates

The protocol is conformant only when all of the following are evidence-backed:

1. Provider abstraction and persistent outgoing/incoming queues exist outside QML.
2. Generic HTTPS, WebDAV HTTPS and pre-signed HTTPS PUT behavior is tested against deterministic servers; unsupported capabilities are accurately exposed.
3. Strict TLS, redirect/origin, response-size, JSON and credential-redaction tests pass.
4. Create/chunk/complete idempotency, crash/restart resume, wrong hashes, duplicate completion, rate limits, outage, expiry, revoke and inbox rejection pass.
5. Quarantine, bounded image validation, `QSaveFile` promotion, path safety and worker-thread SQLite ordering pass.
6. `SecureSettings` backend failures prove fail-closed behavior with no plaintext secret persistence.
7. E2EE is advertised only when audited crypto, envelope tests, key rotation and downgrade tests pass.
8. No sharing event or manifest field can invoke TX/PTT, and local SSTV works with all providers disabled.
9. A production provider is listed only after its deployed endpoint, authentication, operations, limits, privacy, retention, monitoring and credentials are independently verified. As of this document, that list is empty.
