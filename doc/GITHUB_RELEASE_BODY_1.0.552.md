# Decodium 4 FT2 v1.0.552

This maintenance release restores reliable CAT power and SWR telemetry in both the status bar and DECOMETER. It also makes the related settings deterministic: a real operator action is applied once, persisted consistently and passed to Hamlib before the single required CAT rebuild.

## English (British)

### Power and SWR telemetry

- Fixed a settings-propagation defect which could leave Hamlib reporting `effectivePower=false` and `effectiveSwr=false` even after the operator enabled **PWR and SWR**. The radio could still report ALC because ALC follows an independent calibration path, while power and SWR remained empty in the status bar and DECOMETER.
- Added one atomic CAT telemetry operation shared by Settings and DECOMETER. Power/SWR polling and SWR protection can no longer be written as two partially applied changes.
- **Check SWR** now necessarily enables power/SWR telemetry, because protection cannot work without an SWR reading. Disabling meter polling also disables SWR protection, preventing an inconsistent hidden-polling state.
- The settings are persisted in the active Decodium settings profile and mirrored to the legacy INI at the same time, before Hamlib is rebuilt.

### CAT connection behaviour

- The two controls now react to a real click only. Merely opening Settings or restoring a saved visual state does not reconnect the radio.
- Enabling both telemetry and protection causes only one CAT rebuild, avoiding duplicate serial-port activity and repeated relay clicks.
- Opening DECOMETER uses the same atomic path and enables only meter reading; it does not silently enable SWR transmission protection.
- No panadapter, waterfall, decoding or audio path is changed by this release.

### Diagnostics

- Added a `CAT telemetry setting request` diagnostic record containing the requested and previous values, active backend, connection state and whether anything really changed.
- Hamlib construction now records `pwrSetting`, `checkSwrSetting` and `pwrPollEncoded`. Together with `effectivePower` and `effectiveSwr`, these fields show exactly where telemetry was accepted or rejected.

### Validation

- The complete `decodium_qml` application target builds successfully on macOS Apple Silicon.
- New CAT telemetry state tests cover protection enabling, polling disabling, protection disabling and the fully-off-to-protected transition.
- The existing QMX telemetry test continues to pass.
- QML linting reports no errors in the modified Settings and DECOMETER components.

---

## Italiano

Questa release di manutenzione ripristina una telemetria CAT affidabile di potenza e ROS sia nella barra di stato sia nel DECOMETER. Rende inoltre deterministiche le relative impostazioni: un’azione reale dell’operatore viene applicata una sola volta, salvata in modo coerente e passata ad Hamlib prima dell’unica ricostruzione CAT necessaria.

### Telemetria potenza e ROS

- Corretto un difetto di propagazione delle impostazioni che poteva lasciare Hamlib con `effectivePower=false` ed `effectiveSwr=false` anche dopo l’attivazione di **PWR and SWR**. La radio poteva continuare a fornire l’ALC, che segue un percorso di calibrazione indipendente, mentre potenza e ROS rimanevano vuoti nella barra di stato e nel DECOMETER.
- Aggiunta un’unica operazione atomica per la telemetria CAT, condivisa da Impostazioni e DECOMETER. Il polling di potenza/ROS e la protezione ROS non possono più essere applicati parzialmente come due modifiche separate.
- **Check SWR** abilita necessariamente anche la telemetria di potenza/ROS, perché la protezione non può funzionare senza una misura del ROS. Disabilitando il polling dei meter viene disabilitata anche la protezione ROS, evitando uno stato incoerente con polling nascosto.
- Le impostazioni vengono salvate nel profilo Decodium attivo e replicate contemporaneamente nell’INI legacy, prima della ricostruzione di Hamlib.

### Comportamento della connessione CAT

- I due controlli reagiscono ora soltanto a un clic reale. La semplice apertura delle Impostazioni o il ripristino dello stato grafico salvato non riconnette la radio.
- L’attivazione contemporanea di telemetria e protezione provoca una sola ricostruzione CAT, evitando attività seriale duplicata e ripetuti scatti dei relè.
- L’apertura del DECOMETER usa lo stesso percorso atomico e abilita soltanto la lettura dei meter; non attiva silenziosamente la protezione della trasmissione per ROS elevato.
- Questa release non modifica panadapter, waterfall, decodifica o catena audio.

### Diagnostica

- Aggiunto il record diagnostico `CAT telemetry setting request`, contenente valori richiesti e precedenti, backend attivo, stato della connessione e indicazione di una modifica effettiva.
- La costruzione di Hamlib registra ora `pwrSetting`, `checkSwrSetting` e `pwrPollEncoded`. Insieme a `effectivePower` ed `effectiveSwr`, questi campi mostrano esattamente dove la telemetria viene accettata o rifiutata.

### Verifica

- Il target completo dell’applicazione `decodium_qml` viene compilato correttamente su macOS Apple Silicon.
- I nuovi test dello stato della telemetria CAT coprono attivazione della protezione, disattivazione del polling, disattivazione della protezione e passaggio dallo stato completamente spento a quello protetto.
- Il test esistente della telemetria QMX continua a essere superato.
- Il controllo QML non segnala errori nei componenti Impostazioni e DECOMETER modificati.
