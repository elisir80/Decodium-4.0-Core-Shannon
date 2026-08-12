# spe-tci-bridge — la potenza dell'amplificatore nel DECØMETER

Richiesta di **PA3GYQ**: vedere nel DECØMETER i watt all'uscita
dell'amplificatore (SPE Expert Taurus) invece dei watt dell'eccitatrice.

## Perché serve un ponte, e perché deve inoltrare anche la radio

Decodium legge la telemetria **dal proprio backend CAT**. Il percorso TCI
accetta già `tx_power` e `tx_swr` e li porta fino allo strumento — questo è
verificato nel codice, non dedotto (vedi
`doc/telemetria-amplificatore-via-tci.md`).

Ma il backend CAT è uno solo: se lo si punta su un ponte che pubblica solo
l'amplificatore, Decodium perde frequenza, modo e PTT. Perciò il ponte deve
fare due cose insieme:

```
    radio ──(rigctld)──┐
                       ├── ponte ──(TCI)──> Decodium
    amplificatore ─────┘
```

La radio la legge da un **rigctld**, il protocollo di rete di Hamlib — che
Decodium stesso sa servire con la CAT condivisa, e che parlano WSJT-X, i log e
molti altri. L'amplificatore lo legge da Hamlib o da una sorgente simulata.

Il TCI è **multi-client**: allo stesso ponte possono collegarsi più programmi,
mentre una seriale la apre uno solo.

## Uso

Solo libreria standard: nessuna dipendenza da installare.

```
# prova della catena, senza hardware e senza mandare in aria la radio
python spe_tci_bridge.py --amp demo --simulate-tx

# amplificatore vero, radio inoltrata dalla CAT condivisa di Decodium
python spe_tci_bridge.py --rigctld 127.0.0.1:4533 --amp hamlib:401:COM7
```

Poi in Decodium: **backend CAT = TCI**, indirizzo `127.0.0.1:50001`, e
l'impostazione **PWR and SWR accesa** — senza quella la telemetria viene
scartata anche se arriva.

| Opzione | |
|---|---|
| `--listen` | dove ascoltare, default `127.0.0.1:50001` |
| `--rigctld` | radio da inoltrare, es. `127.0.0.1:4533` |
| `--amp` | `demo` oppure `hamlib:<modello>:<porta>` (401 = SPE Expert FA) |
| `--rate` | cadenza telemetria, default 0.2 s (5 Hz) |
| `--simulate-tx` | alterna TX/RX ogni 8 s per collaudare senza trasmettere |

## Cosa è verificato e cosa no

**Verificato** con un client TCI: stretta di mano, saluto completo, `trx`
alternato, e `tx_power` / `tx_swr` a 5 Hz con i valori giusti. La radio
inoltrata da rigctld riporta frequenza e modo reali.

**Non verificato**: che un vero SPE risponda a Hamlib. Il backend Expert
dichiara `has_get_level = 0x0` pur avendo la funzione implementata — solo
l'apparato può dirlo, e serve la sonda in `tools/amp-probe/`. Se non
rispondesse, il punto in cui innestare il protocollo SPE è la classe
`HamlibAmp`: tutto il resto del ponte resta valido.

## Due avvertenze pratiche

**Il campo `tx_sensors` non è usato di proposito.** Le sue posizioni variano
fra implementazioni — c'è chi mette la potenza diretta al terzo campo e chi al
quarto — e scambiare diretta con riflessa qui non è accettabile. Il ponte invia
`tx_power` e `tx_swr`, che sono a valore singolo e non ambigui.

**La porta viene aperta in modo esclusivo.** Su Windows `SO_REUSEADDR` lascia
legare la stessa porta a un secondo processo: il primo continua a rispondere e
il secondo sembra partito senza servire nessuno. È successo davvero durante lo
sviluppo, e costa mezz'ora di diagnosi sbagliata. Ora la seconda istanza
fallisce a voce alta.
