# Decodium 4 FT2 v1.0.549

Release 1.0.549 advances the work delivered in 1.0.548 with a GPU-native 3D
panadapter path, safer and more transparent Linux graphics offload, stronger
AppImage keyring integration, immediate B4 display updates and improved
RTL-SDR control visibility. The ordinary 2D panadapter path remains unchanged
when 3D is disabled, and every new accelerated path retains a CPU fallback.

## English (British)

### Highlights since 1.0.548

#### GPU-native 3D panadapter

- Added dedicated Qt RHI shaders for the 3D spectrum, allowing the graphics
  processor to retain and draw the spectral history instead of requiring a
  CPU copy for every trace.
- Reworked the 3D mesh into ordered per-trace draws so that inner ridges remain
  visible on Metal as well as OpenGL, including at the centre and across the
  full spectrum width.
- Corrected the projected floor, trace span and resize calculations so the 3D
  view fills the available panadapter area at different window sizes.
- Kept the normal GPU-direct 2D waterfall and panadapter route untouched when
  3D is switched off, avoiding additional work in the most common display
  mode.
- Retained the asynchronous CPU/FFTW history implementation as an automatic
  fallback if the required shader, texture or graphics resource is not
  available.

#### Linux graphics selection, offload and diagnostics

- Added an optional Linux-only **OpenGL GPU FFT** control under Advanced
  settings. It is disabled by default, applies after restart and accelerates
  the visual panadapter FFT only; the decoder remains on its established CPU
  path.
- Added an eligibility probe for Vulkan devices on hybrid-GPU Linux systems.
  Decodium can prefer a suitable discrete GPU only when it exposes the
  required graphics, compute, presentation and swapchain capabilities, while
  respecting explicit Qt or user overrides.
- Unsupported, failed or stalled accelerated FFT paths fall back automatically
  to the asynchronous CPU implementation.
- Improved DRM accounting for Intel i915 and Xe as well as other Linux DRM
  drivers: duplicate file descriptors are de-duplicated, engine capacity is
  considered and both engine-time and cycle counters are supported.
- Improved primary-device selection on multi-GPU systems and added a
  device-wide busy counter fallback where reliable per-process DRM counters
  are unavailable.
- The status bar now distinguishes measured per-process/device utilisation,
  render activity, software rendering and the active panadapter FFT backend;
  estimated frame activity is no longer presented as a measured 100% GPU
  load.
- Linux AppImage workflows now install the Vulkan development dependency
  required by the graphics capability probe.

#### Linux AppImage keyring reliability

- Centralised the Linux environment used for every external `secret-tool`
  lookup, store and clear operation.
- Prevented bundled AppImage linker, GLib and GIO paths from leaking into the
  host keyring process, while preserving the D-Bus session, runtime directory,
  display and Wayland variables needed to reach the desktop keyring.
- The AppRun wrapper now records the original host values before adding bundled
  library paths, so secure settings can restore a clean environment reliably.
- Added regression tests with a simulated external `secret-tool`, covering
  credential reading, writing, deletion, standard input and the environment
  received by the helper.

#### Worked-before display and RTL-SDR controls

- B4 strikethrough now applies consistently in both Full Spectrum and Signal
  RX when the corresponding option is enabled.
- Logging a contact refreshes the worked-before cache and re-enriches matching
  visible decode rows immediately, for both native and legacy logging routes;
  a restart is no longer required.
- The B4 setting now reacts only to a real user toggle and is persisted in the
  active profile while retaining compatibility with the legacy setting.
- Added focused worked-before tests for canonical callsigns, portable suffixes,
  active-profile persistence and immediate row refresh.
- Increased the contrast and size of the RTL-SDR checkboxes locally within the
  RTL-SDR settings page, including clearer unchecked and disabled states,
  without changing the application-wide theme.

### Compatibility and safeguards

- The new Vulkan selection and keyring-environment handling are Linux-only.
- OpenGL GPU FFT is opt-in and disabled by default.
- The GPU-native 3D route falls back to CPU processing whenever acceleration
  cannot be initialised safely.
- The regular 2D GPU-direct display path remains unchanged when 3D is off.
- Windows and macOS continue to use their existing graphics and secure-storage
  behaviour.

### Validation

- Built the Decodium QML application target locally on macOS.
- Passed the focused worked-before and decode-list-model tests.
- Extended the Linux DRM and secure-settings test coverage for the new GPU
  counters and clean keyring-helper environment.
- Made the final AppImage payload validator architecture-safe and added clear
  diagnostics for missing QML, launcher and RTL-SDR runtime components.
- Release workflows validate the repository layout and declared version before
  producing the Windows installer, macOS DMGs and Linux AppImages.

## Italiano

### Novità dalla 1.0.548

#### Panadapter 3D nativo su GPU

- Aggiunti shader Qt RHI dedicati allo spettro 3D, consentendo alla GPU di
  conservare e disegnare la cronologia spettrale senza richiederne una copia
  sulla CPU per ogni traccia.
- La mesh 3D usa ora disegni separati e ordinati per ogni traccia, così le
  creste interne rimangono visibili sia con Metal sia con OpenGL, anche al
  centro e lungo l’intera larghezza dello spettro.
- Corretti il piano prospettico, l’estensione delle tracce e i calcoli di
  ridimensionamento, affinché la vista 3D riempia correttamente il panadapter a
  diverse dimensioni della finestra.
- Il normale percorso GPU-direct del panadapter e del waterfall 2D resta
  invariato quando il 3D è spento, senza introdurre lavoro aggiuntivo nella
  modalità grafica più utilizzata.
- La cronologia asincrona CPU/FFTW rimane disponibile come fallback automatico
  se shader, texture o risorse grafiche richieste non sono utilizzabili.

#### Selezione GPU, accelerazione e diagnostica su Linux

- Aggiunta nelle impostazioni Avanzate l’opzione **FFT GPU OpenGL**, disponibile
  solo su Linux. È disattivata di serie, richiede il riavvio e accelera
  esclusivamente la FFT visiva del panadapter; il decoder conserva il percorso
  CPU esistente.
- Aggiunto un controllo delle capacità dei dispositivi Vulkan nei sistemi
  Linux con GPU ibride. Decodium può preferire una GPU discreta idonea solo se
  offre grafica, calcolo, presentazione e swapchain richiesti, rispettando le
  selezioni Qt o dell’utente già esplicite.
- I percorsi FFT accelerati non supportati, falliti o bloccati tornano
  automaticamente all’implementazione CPU asincrona.
- Migliorata la lettura dei contatori DRM per Intel i915 e Xe e per gli altri
  driver Linux: vengono eliminati i duplicati dei descrittori, considerata la
  capacità dei motori e supportati sia i tempi motore sia i contatori a cicli.
- Migliorata la selezione del dispositivo principale nei sistemi multi-GPU e
  aggiunto il fallback al carico globale del dispositivo quando non esistono
  contatori DRM affidabili per processo.
- La barra di stato distingue ora utilizzo misurato per processo/dispositivo,
  attività di rendering, renderer software e backend FFT attivo; l’attività
  stimata dai frame non viene più mostrata come un falso utilizzo GPU al 100%.
- I workflow AppImage Linux installano ora la dipendenza di sviluppo Vulkan
  necessaria al controllo delle capacità grafiche.

#### Affidabilità del portachiavi nelle AppImage Linux

- Centralizzato l’ambiente Linux usato da tutte le operazioni esterne
  `secret-tool`: lettura, salvataggio e cancellazione.
- Impedito ai percorsi del linker, GLib e GIO inclusi nell’AppImage di
  contaminare il processo del portachiavi di sistema, conservando però D-Bus,
  directory runtime, display e variabili Wayland necessari per raggiungerlo.
- Il wrapper AppRun registra i valori originali dell’ambiente host prima di
  aggiungere le librerie incluse nel pacchetto, permettendo alle impostazioni
  sicure di ripristinare sempre un ambiente pulito.
- Aggiunti test di regressione con un `secret-tool` esterno simulato, inclusi
  lettura, scrittura, cancellazione, standard input e ambiente ricevuto dal
  programma di supporto.

#### Indicazione dei collegamenti già effettuati e controlli RTL-SDR

- La barratura B4 viene applicata in modo coerente sia in Full Spectrum sia in
  Signal RX quando la relativa opzione è attiva.
- Dopo il salvataggio di un QSO, la cache dei collegamenti già effettuati viene
  aggiornata e le righe decodificate visibili corrispondenti vengono
  ricalcolate immediatamente, sia nel log nativo sia nel percorso legacy; non è
  più necessario riavviare.
- L’opzione B4 reagisce soltanto a un reale intervento dell’utente e viene
  salvata nel profilo attivo, mantenendo la compatibilità con l’impostazione
  precedente.
- Aggiunti test mirati per nominativi canonici, suffissi portatili, persistenza
  nel profilo attivo e aggiornamento immediato delle righe.
- Aumentati contrasto e dimensione dei checkbox RTL-SDR nella sola pagina delle
  impostazioni RTL-SDR, rendendo più chiari anche gli stati non selezionato e
  disabilitato senza modificare il tema globale.

### Compatibilità e protezioni

- La selezione Vulkan e la pulizia dell’ambiente del portachiavi si applicano
  esclusivamente a Linux.
- L’FFT GPU OpenGL è opzionale e disattivata per impostazione predefinita.
- Il percorso 3D nativo su GPU torna alla CPU ogni volta che l’accelerazione
  non può essere inizializzata in sicurezza.
- Il normale percorso GPU-direct 2D rimane invariato quando il 3D è spento.
- Windows e macOS continuano a usare i comportamenti grafici e di archiviazione
  sicura esistenti.

### Verifica

- Compilato localmente su macOS il target QML di Decodium.
- Superati i test mirati per i collegamenti già effettuati e per il modello
  delle righe decodificate.
- Estesa la copertura dei test Linux DRM e delle impostazioni sicure per i
  nuovi contatori GPU e per l’ambiente pulito del programma di portachiavi.
- Reso indipendente dall’architettura il controllo finale del contenuto
  AppImage, con diagnostica esplicita per componenti QML, launcher e runtime
  RTL-SDR mancanti.
- I workflow di rilascio verificano layout del repository e versione dichiarata
  prima di produrre installer Windows, DMG macOS e AppImage Linux.
