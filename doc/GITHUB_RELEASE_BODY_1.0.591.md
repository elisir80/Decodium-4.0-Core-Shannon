# Decodium 4 FT2 v1.0.591

This release finishes the phantom-decode work started in v1.0.590, whose FT2
decoder settings turned out to produce false callsigns, and extends the
vectorised LDPC decoder to FT8.

## English (British)

### v1.0.591: phantom decodes fixed for real, and FT8 on the vectorised decoder

- Fixed the FT2 phantom callsigns that v1.0.590 shipped. That release used a
  wide OSD search (order 3, spans 91/48) which tries about 21400 candidates per
  word against roughly 600 for the narrow one. A 14-bit CRC admits one wrong
  candidate in 16384, so the wide search bought correct and false decodes in the
  same proportion: on air it produced about 2.8 invented callsigns per cycle.
  The search is back to narrow.
- Added the checks that were missing on the BATCH decode path, which is the one
  FT2 actually uses. `nharderror` was computed and returned but never used to
  reject anything, so words that flipped 31, 36, 40 and more bits reached the
  decode list; every threshold tuned so far lived in the single-word function
  that nothing calls any more. The batch path now rejects on flipped bits and on
  coherence with the a-priori hypothesis: when an AP pass imposes bits and the
  decoder overturns them, the word contradicts the hypothesis that produced it.
  Both are adjustable at runtime through `DECODIUM_LDPC_MAX_HARD` and
  `DECODIUM_LDPC_AP_CHECK`, and `DECODIUM_LDPC_GATE_LOG=1` shows what is being
  rejected and why.
- Added a structural plausibility test on the 77-bit payload inside the OSD
  acceptance loop: message type, callsign structure, token position and field
  ranges. These are certain constraints, not statistical thresholds, and they
  add filtering bits to the CRC where it matters.
- Extended the vectorised decoder to FT8, behind a runtime switch
  (`DECODIUM_FT8_FASTLDPC=0` returns to the original decoder). FT8 admits the
  contest message types that FT2 never sees, so the FT8 mode keeps every defined
  type: filtering them out would have made the decoder blind to contest traffic.
- Measured on air over two comparable windows of live FT8 traffic, the
  vectorised decoder is neither better nor worse for sensitivity: 711 decodes
  from 123 distinct callsigns against 663 from 122. It is however slower per
  pass in FT8 (median 604 ms against 382 ms), because FT8 still decodes one word
  at a time while the vectoriser works on sixteen lanes at once. Converting FT8
  to batch decoding, which is what would make the lanes pay, is not part of this
  release.

### Packaging and compatibility

- GitHub's generated source archives for tag `v1.0.591` are the codebase
  downloads for this release.
- The AVX2 decoder is selected at runtime, so the published binaries remain
  usable on CPUs without AVX2, where the original decoder is used instead.
- The FT2 decode gate has been calibrated against live traffic and synthetic
  vectors, but not across a full range of weak-signal conditions. If marginal
  decodes appear to be missing, raise `DECODIUM_LDPC_MAX_HARD` or set
  `DECODIUM_LDPC_AP_CHECK=0`, and report which value restores them.

## Italiano

### v1.0.591: decodifiche fantasma risolte davvero, e FT8 sul decoder vettorizzato

- Corretti i nominativi fantasma in FT2 che la v1.0.590 ha pubblicato. Quella
  release usava una ricerca OSD larga (ordine 3, span 91/48) che prova circa
  21400 candidati per parola contro i circa 600 della stretta. Un CRC a 14 bit
  ne lascia passare uno sbagliato ogni 16384, quindi la ricerca larga comprava
  decodifiche giuste e false nella stessa proporzione: sull'aria produceva circa
  2,8 nominativi inventati per ciclo. La ricerca è tornata stretta.
- Aggiunti i controlli che mancavano sulla via di decodifica BATCH, che è quella
  che FT2 usa davvero. `nharderror` veniva calcolato e restituito ma non
  filtrava nulla, quindi arrivavano in lista parole che ribaltavano 31, 36, 40 e
  più bit; ogni soglia tarata fino a quel momento viveva nella funzione a parola
  singola, che non chiama più nessuno. La via batch ora rifiuta sui bit
  ribaltati e sulla coerenza con l'ipotesi a priori: se una passata AP impone
  dei bit e il decoder li ribalta, la parola contraddice l'ipotesi che l'ha
  prodotta. Entrambi si regolano a runtime con `DECODIUM_LDPC_MAX_HARD` e
  `DECODIUM_LDPC_AP_CHECK`, e `DECODIUM_LDPC_GATE_LOG=1` mostra cosa viene
  scartato e perché.
- Aggiunto un test strutturale di plausibilità sui 77 bit del payload dentro il
  ciclo di accettazione dell'OSD: tipo di messaggio, struttura dei nominativi,
  posizione dei token e intervalli dei campi. Sono vincoli certi, non soglie
  statistiche, e aggiungono bit di filtro al CRC dove serve.
- Esteso il decoder vettorizzato a FT8, dietro un interruttore a runtime
  (`DECODIUM_FT8_FASTLDPC=0` torna al decoder originale). FT8 ammette i tipi di
  messaggio da contest che in FT2 non si vedono, quindi la modalità FT8 tiene
  tutti i tipi definiti: escluderli avrebbe reso il decoder cieco al traffico di
  contest.
- Misurato sull'aria su due finestre confrontabili di traffico FT8 reale, il
  decoder vettorizzato non è né migliore né peggiore quanto a sensibilità: 711
  decodifiche da 123 nominativi distinti contro 663 da 122. È però più lento per
  passata in FT8 (mediana 604 ms contro 382), perché FT8 decodifica ancora una
  parola alla volta mentre il vettorizzatore lavora su sedici corsie insieme.
  La conversione di FT8 alla decodifica a blocchi, che è ciò che ripagherebbe le
  corsie, non fa parte di questa release.

### Packaging e compatibilità

- Gli archivi sorgente generati da GitHub per il tag `v1.0.591` costituiscono i
  download del codebase di questa release.
- Il decoder AVX2 viene scelto a runtime, quindi i binari pubblicati restano
  utilizzabili su CPU senza AVX2, dove viene usato il decoder originale.
- Il filtro dei decode FT2 è stato tarato su traffico reale e su vettori
  sintetici, ma non su tutta la gamma di condizioni di segnale debole. Se
  dovessero mancare decodifiche marginali, alzare `DECODIUM_LDPC_MAX_HARD`
  oppure impostare `DECODIUM_LDPC_AP_CHECK=0`, e segnalare quale valore le
  ripristina.
