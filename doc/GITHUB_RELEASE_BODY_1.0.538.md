# Decodium 4.0 v1.0.538

Version 1.0.538 changes how Decodium identifies itself on the WSJT-X UDP
protocol and removes a source of duplicate decode broadcasts. Both were
reported by an operator running a spot collector, whose log rejected every
packet Decodium sent with `OLD software version. Break`.

## English (British)

### The UDP client is now called Decodium

- Decodium previously announced itself with the client id `WSJTX` and its own
  release number as the version. A WSJT-X aware collector read that as WSJT-X
  1.0.x, compared it with the real 2.7.x and discarded every packet as an old
  program. Nothing was wrong with the data; the label was.
- The default client id is now `Decodium`, in the settings, in the preset list
  and in the fallback used when the field is left empty.
- Incoming traffic addressed to `WSJTX` or `WSJT-X` is still accepted, for both
  control messages and ordinary messages, so GridTracker, JTAlert and any other
  companion that targets the classic name keeps working.
- A one-time migration moves an existing saved `WSJTX` setting to `Decodium`.
  Operators who prefer the previous identifier can select it again from the
  preset list, and the choice is then left alone.

### Duplicate decode broadcasts

- The decode de-duplication key used the exact frequency. The same signal
  re-decoded by the deep pass comes back a few hertz away from the first
  estimate, which produced two distinct keys: two rows in the list and two UDP
  packets for one message. External collectors counted that as double traffic.
- The frequency now enters the key in canonical integer form, and duplicates
  are matched with a tolerance of a few hertz. The FT2 asynchronous path
  already quantised the frequency for exactly this reason.

### Note on decode volume

- Broadcasting one Decode message per decode is the WSJT-X UDP protocol working
  as intended; that is how GridTracker and JTAlert receive their data, and JTDX
  behaves the same way. What changes here is that the packets are no longer
  rejected as coming from an obsolete program, and that the same decode is no
  longer sent twice.

### Validation

- Local `decodium_qml` build completed successfully.
- `test_udp_client_id` passed.
- `qmllint` reported no errors on the modified QML file.
- Verified on the wire: a UDP capture on the primary port received 29 packets,
  all announcing the client id `Decodium`.

## Italiano

La versione 1.0.538 cambia il modo in cui Decodium si presenta sul protocollo
UDP di WSJT-X e rimuove una sorgente di decodifiche inviate due volte. Entrambi
i punti nascono dalla segnalazione di un operatore che gestisce un collettore
di spot: il suo registro rifiutava ogni pacchetto di Decodium con
`OLD software version. Break`.

### Il client UDP ora si chiama Decodium

- Decodium si annunciava con identificativo `WSJTX` e con il proprio numero di
  versione. Un collettore che conosce WSJT-X lo leggeva come WSJT-X 1.0.x, lo
  confrontava con il 2.7.x reale e scartava ogni pacchetto come programma
  vecchio. I dati non avevano nulla di sbagliato: era sbagliata l'etichetta.
- L'identificativo predefinito e' ora `Decodium`, nelle impostazioni, nel menu
  dei preimpostati e nel valore di ripiego quando il campo resta vuoto.
- Il traffico in arrivo indirizzato a `WSJTX` o `WSJT-X` continua a essere
  accettato, sia per i messaggi di controllo sia per quelli ordinari, cosi'
  GridTracker, JTAlert e ogni altro programma che usa il nome classico
  continuano a funzionare.
- Una migrazione una tantum sposta a `Decodium` un'impostazione `WSJTX` gia'
  salvata. Chi preferisce il vecchio identificativo puo' rimetterlo dal menu a
  tendina, e da quel momento la scelta non viene piu' toccata.

### Decodifiche inviate due volte

- La chiave di deduplica usava la frequenza esatta. Lo stesso segnale
  ridecodificato dalla passata profonda torna con qualche hertz di scarto
  rispetto alla prima stima, e questo produceva due chiavi distinte: due righe
  in lista e due pacchetti UDP per un solo messaggio. I collettori esterni lo
  contavano come traffico doppio.
- Ora la frequenza entra nella chiave in forma canonica (intero) e i duplicati
  si riconoscono con una tolleranza di pochi hertz. Il percorso FT2 asincrono
  quantizzava gia' la frequenza proprio per questo motivo.

### Nota sul volume delle decodifiche

- Trasmettere un messaggio Decode per ogni decodifica e' il funzionamento
  previsto dal protocollo UDP di WSJT-X: e' cosi' che GridTracker e JTAlert
  ricevono i dati, e JTDX si comporta allo stesso modo. Qui cambia che i
  pacchetti non vengono piu' rifiutati come provenienti da un programma
  obsoleto e che la stessa decodifica non parte piu' due volte.

### Verifica

- Build locale di `decodium_qml` completata correttamente.
- `test_udp_client_id` superato.
- `qmllint` non ha segnalato errori sul file QML modificato.
- Verificato sul filo: una cattura UDP sulla porta primaria ha ricevuto 29
  pacchetti, tutti con identificativo `Decodium`.

## Release assets

The release workflows publish the Windows x64 executable, macOS Intel and
Apple Silicon DMG packages, and Linux x86_64 and aarch64 AppImages together
with their checksums where provided by the workflow.
