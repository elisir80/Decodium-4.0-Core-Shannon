# Decodium 4 FT2 v1.0.596

Urgent fix: v1.0.595 crashes within about two minutes of FT8 reception. Please
replace it with this release.

## English (British)

### v1.0.596: fixes the v1.0.595 crash

- v1.0.595 lowered a threshold that had been keeping the FT8 deep follow-up
  decode switched off. The intent was sound — the threshold demanded a budget of
  7000 ms while the maximum obtainable is 6550 ms by construction, so the deep
  stage could never run and a-priori decoding at depth 4 had never executed. But
  re-enabling it also re-enabled a latent memory fault in that path, which had
  not been exercised since the threshold was introduced: the application dies
  with heap corruption (`0xc0000374` in ntdll) within roughly two minutes of FT8
  reception, always immediately after the follow-up is dispatched.
- The threshold is restored to its previous value, which switches that stage off
  again. Verified with nine minutes of continuous reception without a crash,
  against under two minutes before.
- Everything else from v1.0.595 is unaffected and remains in place: the FT2
  phantom-decode fixes, the vectorised decoder on FT8 with batch decoding and the
  recovery pass, the waterfall click-to-call, and the ten settings that were
  written to one place and read from another.

### About the fault, for whoever investigates it

- It is not the decoder. The same deep configuration — depth 4, a-priori
  decoding, supplemental — run offline over recorded slots produces no crash at
  all. It is the application path that dispatches it.
- It appears to be a concurrency problem: the crash occurs when the deep
  follow-up is queued while the first decode is still in flight. FT8 stage 4
  holds global state and static buffers behind a single-flight mutex that
  evidently does not cover everything the two requests share.
- To reproduce: lower `kFt8DeepMinUsefulBudgetMs` in `DecodiumBridge.cpp` to
  2500 and receive FT8 for a couple of minutes. The comment at that constant
  records the details.

### Packaging and compatibility

- GitHub's generated source archives for tag `v1.0.596` are the codebase
  downloads for this release.
- The AVX2 decoder is selected at runtime, so the published binaries remain
  usable on CPUs without AVX2, where the original decoder is used instead.

## Italiano

### v1.0.596: corregge il crash della v1.0.595

- La v1.0.595 abbassava una soglia che teneva spento il decode profondo di
  recupero in FT8. L'intento era corretto: la soglia pretendeva un budget di
  7000 ms mentre il massimo ottenibile è 6550 ms per costruzione, quindi lo
  stadio profondo non poteva mai partire e la decodifica a priori a profondità 4
  non era mai stata eseguita. Riattivandolo però si è riattivato anche un
  difetto di memoria latente in quel percorso, rimasto inutilizzato da quando la
  soglia è stata introdotta: l'applicazione muore con corruzione dello heap
  (`0xc0000374` in ntdll) entro circa due minuti di ricezione FT8, sempre subito
  dopo il lancio del follow-up.
- La soglia è riportata al valore precedente, che disattiva di nuovo quello
  stadio. Verificato con nove minuti di ricezione continua senza cadute, contro
  i meno di due di prima.
- Tutto il resto della v1.0.595 non è toccato e resta al suo posto: le
  correzioni ai nominativi fantasma in FT2, il decoder vettorizzato su FT8 con
  decodifica a blocchi e passata di recupero, il clic sul waterfall che chiama
  la stazione, e le dieci impostazioni che venivano scritte in un posto e lette
  in un altro.

### Sul difetto, per chi vorrà indagarlo

- Non è il decoder. La stessa configurazione profonda — profondità 4, decodifica
  a priori, supplemental — eseguita offline su slot registrati non produce
  nessun crash. È il percorso applicativo che la lancia.
- Sembra un problema di concorrenza: il crash avviene quando il follow-up
  profondo viene accodato mentre il primo decode è ancora in volo. Lo stadio 4
  di FT8 mantiene stato globale e buffer statici dietro un mutex single-flight
  che evidentemente non copre tutto ciò che le due richieste si scambiano.
- Per riprodurlo: abbassare `kFt8DeepMinUsefulBudgetMs` in `DecodiumBridge.cpp`
  a 2500 e ricevere FT8 per un paio di minuti. Il commento accanto alla costante
  riporta i dettagli.

### Packaging e compatibilità

- Gli archivi sorgente generati da GitHub per il tag `v1.0.596` costituiscono i
  download del codebase di questa release.
- Il decoder AVX2 viene scelto a runtime, quindi i binari pubblicati restano
  utilizzabili su CPU senza AVX2, dove viene usato il decoder originale.
