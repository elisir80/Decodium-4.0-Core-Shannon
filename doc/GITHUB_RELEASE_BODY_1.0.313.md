# Decodium 4 FT2 1.0.313

Release 1.0.313 is a field-fix release after 1.0.312. It keeps Martino's 1.0.312 logger hardening and adds the local fixes validated during live FT2/AutoCQ testing for TX audio stability, FT2 sequencing, decode-window ordering, and directed false-positive decode handling.

## Highlights

### Legacy TX audio and macOS CoreAudio stability

- Validates legacy bridge TX payloads before raising CAT PTT/Fake-It.
- Immediately drops PTT and cancels the pending legacy TX request when a message is rejected or bridge audio cannot start.
- Adds stronger guards so Decodium does not open a second TX sink while a legacy bridge stream or normal TX audio stream is already active.
- Reduces the stale/duplicated TX sink pattern that led to CoreAudio/QSocketNotifier instability.

### FT2 AutoCQ and QSO sequencing

- Restores the required own final `73` path when FT2 Quick QSO is disabled and the peer sends a final signoff.
- Keeps signoff progression monotonic so late repeated `73`/`RR73` messages cannot move the QSO back to report transmission.
- Extends duplicate-signoff cooldown and restricts final-signoff exceptions to a real active exchange.
- Keeps AutoCQ partner state alive during live exchanges and avoids accidental CQ fallback while the active partner is still valid.
- Improves FT2 async AutoCQ slot timing with a larger start-safety margin.

### FT2/macOS TX timing

- Removes unnecessary initial silence for FT2 async TX when no sync-slot alignment is required.
- Keeps sync-slot alignment only where it is intentional: FT2 AutoCQ calling, FT8, and FT4.
- Adds TX diagnostics for FT2 async/alignment decisions.
- Reduces the short TX power dip seen at the beginning of FT2 transmissions on macOS/CoreAudio.

### Decode display and Signal RX correctness

- Sorts TX rows in Signal RX by visible UTC time before timestamp fallback, so old TX rows no longer reappear out of order.
- Coalesces repeated TX rows before Signal RX sorting.
- Replaces unconditional tail-follow with a TX-aware follow strategy to avoid the brief red-row flash when entering TX.

### Directed false-positive decode filtering

- Rejects directed “to me” FT2/FTx decodes with plausible but DXCC-invalid peer prefixes before they enter autosequence.
- Declassifies those rows from `isMyCall`, so they do not trigger red highlighting or my-call alerts.
- Preserves explicit operator-directed QSOs: current DX, AutoCQ locked call, inferred active partner, current TX payload, and last TX payload are exempt from the invalid-DXCC gate.

## Release assets

This release publishes:

- Windows x64 installer: `Decodium_1.0.313_Setup_x64.exe`
- macOS Apple Silicon DMG/ZIP builds from the GitHub runner
- Linux x86_64 AppImage from the GitHub runner
- GitHub source code archives for tag `1.0.313`
