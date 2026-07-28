## Decodium 4 FT2 — 1.0.418

Allineata alla **1.0.417** (elisir80 / Salvatore: `UsStateDataManager`, `soundin`, Full Spectrum UI) con sopra il **fix CW**.

### 🔧 CW (invia CQ) ora funzionante
- Il comando `send_cw` è ora collegato al motore TX del bridge. Prima, in modalità QML, veniva accettato (`accepted_immediate`) ma **nessuno lo gestiva** → niente PTT, niente audio. Era la causa per cui il CW non partiva per più utenti, su qualsiasi radio.
- **CW via audio**: la radio resta in **USB / DATA-U** (come per l'FT8) e Decodium genera il tono Morse manipolato e lo trasmette dalla scheda audio. Indipendente dal keyer CAT/Hamlib (`rig_send_morse`), che su molte radio (es. **Yaesu FT-991 / 991A**) non funziona in modalità dati.

### 🎚️ Soglia SWR configurabile
- Nuova impostazione **Impostazioni → Setup → DIAGNOSTICS → "SWR max"** (default **2.5**, intervallo 2.0–4.0).
- Prima la protezione SWR era fissa a 2.5 e poteva **interrompere il CW** su antenne con SWR moderato. Ora ogni utente la adatta alla propria antenna.

### 📦 Asset
- `Decodium_1.0.418_Setup_x64.exe` — installer **Windows x64** (installazione per-utente, non richiede privilegi di amministratore).

> Build Linux (AppImage) e macOS (dmg/zip) si generano tramite i rispettivi workflow GitHub Actions (`workflow_dispatch`), se necessario.
