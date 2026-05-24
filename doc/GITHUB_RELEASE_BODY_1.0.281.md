# Decodium 4 FT2 1.0.281

Release 1.0.281 is a focused stability and decode-logic release after 1.0.280. It restores the 1.0.280 baseline locally, then reapplies the field fixes for the macOS FT8 crash, directed CQ modifiers, special-event callsign sequencing, and invalid directed ghost decodes.

## English - Changes Since 1.0.280

- Fixed a macOS FT8 crash reported as `Thread stack size exceeded` in the decoder `QThread`, with the crash stack ending in `run_main_passes` / `ftx_ft8_async_decode_stage4_c`.
- Increased the FT8 decoder worker thread stack size before starting the worker, avoiding the macOS default small QThread stack that can hit the stack guard during the stage4 C++ decode path.
- Kept the larger stack scoped to the FT8 worker thread only, without changing the UI thread or general application thread model.
- Fixed macOS release packaging for Homebrew Qt 6.11 builds where `QtGui.framework` can require `QtDBus.framework` at launch.
- The macOS bundle normalizer now copies missing `@rpath` framework dependencies into `Contents/Frameworks` and fails the release build if any bundled `@rpath` dependency cannot be resolved inside the app.
- Fixed Linux AppImage build compatibility with Qt 6.4.x by avoiding newer `QTimeZone::UTC` / `QTimeZone::utc()` APIs in shared code paths and by disabling Qt Quick pipeline-cache APIs that are unavailable in Qt 6.4.
- Fixed directed CQ parsing so messages such as `CQ POTA IT9ARO JM68` and `CQ SOTA IT9ARO JM68` are accepted as valid directed CQ calls instead of treating `POTA` or `SOTA` as callsigns.
- Extended directed CQ modifier handling consistently across FT8 decode filtering, double-click handling, Signal RX extraction, Live Map enrichment, and replay paths.
- Added decoder-side support for additional directed CQ modifiers: `POTA`, `SOTA`, `QRP`, `IOTA`, `FD`, and `WW`, alongside the existing region and contest modifiers.
- Fixed special-event/nonstandard callsign autosequencing for calls such as `II9MESC`, especially when hashed FT8 replies decode the peer as `<...>`.
- Allowed safe placeholder-peer directed messages like `II9MESC <...> RR73` to resolve against the active QSO partner, so Auto Sequence can complete the exchange instead of repeating the previous report.
- Kept placeholder-peer acceptance guarded by payload validation and active QSO context, so random `<...>` decodes do not become valid directed traffic without a live QSO state.
- Reduced invalid directed "ghost" decodes in Signal RX by rejecting structurally impossible peer calls before mirroring directed messages to the RX pane.
- Added `H1` to the high-confidence invalid prefix block, addressing field reports where a listening station received phantom directed calls from impossible callsigns.
- Added and updated tests for directed POTA/SOTA FT8 round-trips, weak-decode target messages, and nonstandard special-event hashed-call decode behavior.
- Updated release/package version metadata to 1.0.281.

## Italiano - Modifiche Dalla 1.0.280

- Corretto un crash macOS FT8 segnalato come `Thread stack size exceeded` nel `QThread` del decoder, con stack crash in `run_main_passes` / `ftx_ft8_async_decode_stage4_c`.
- Aumentata la dimensione dello stack del thread worker FT8 prima dell'avvio, evitando il limite troppo basso del QThread macOS durante il percorso C++ stage4 del decoder.
- La modifica dello stack resta limitata al solo worker FT8 e non cambia il thread UI o il modello generale dei thread dell'applicazione.
- Corretto il packaging release macOS per le build Homebrew Qt 6.11 in cui `QtGui.framework` puo richiedere `QtDBus.framework` all'avvio.
- Il normalizzatore del bundle macOS ora copia dentro `Contents/Frameworks` le dipendenze framework `@rpath` mancanti e blocca la release se una dipendenza `@rpath` non e risolvibile dentro l'app.
- Corretta la compatibilita della build Linux AppImage con Qt 6.4.x evitando le API piu nuove `QTimeZone::UTC` / `QTimeZone::utc()` nei percorsi condivisi e disabilitando le API Qt Quick pipeline-cache non disponibili in Qt 6.4.
- Corretto il parsing dei CQ diretti: messaggi come `CQ POTA IT9ARO JM68` e `CQ SOTA IT9ARO JM68` ora entrano correttamente invece di interpretare `POTA` o `SOTA` come nominativi.
- Estesa la gestione dei modificatori CQ diretti in modo coerente su filtro decode FT8, doppio click, estrazione Signal RX, arricchimento Live Map e replay.
- Aggiunto supporto lato decoder per ulteriori modificatori CQ diretti: `POTA`, `SOTA`, `QRP`, `IOTA`, `FD` e `WW`, oltre ai modificatori regionali e contest gia esistenti.
- Corretta l'autosequenza per nominativi speciali/non standard come `II9MESC`, soprattutto quando le risposte FT8 hashed decodificano il corrispondente come `<...>`.
- I messaggi diretti sicuri con peer placeholder, per esempio `II9MESC <...> RR73`, possono ora risolversi sul partner QSO attivo, permettendo ad Auto Sequence di completare lo scambio invece di ripetere il report precedente.
- L'accettazione dei peer `<...>` resta protetta da validazione del payload e contesto QSO attivo, quindi decode casuali con `<...>` non diventano traffico diretto valido senza uno stato QSO reale.
- Ridotte le chiamate "fantasma" dirette in Signal RX scartando peer strutturalmente impossibili prima di mostrare il messaggio nel pannello RX.
- Aggiunto `H1` al blocco dei prefissi impossibili ad alta confidenza, in risposta ai report in cui una stazione in ascolto riceveva chiamate dirette fantasma da nominativi impossibili.
- Aggiunti e aggiornati test per round-trip FT8 POTA/SOTA, weak-decode con messaggio target configurabile e decode hashed-call per nominativi special-event non standard.
- Aggiornata la versione di release/package a 1.0.281.

## Platform Assets

- Windows x64 installer is built and uploaded by the Windows GitHub Actions runner.
- macOS Apple Silicon DMG/ZIP assets are built and uploaded by the macOS GitHub Actions runner.
- Linux x86_64 AppImage is built and uploaded by the Linux GitHub Actions runner.

## Notes

- This release is intentionally conservative: it targets the reported field failures without changing the FT8 timing model introduced in 1.0.280.
- The placeholder-peer fix is active only when the payload and current QSO state make the message safe to resolve.
