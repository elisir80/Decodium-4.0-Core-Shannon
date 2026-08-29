# fastldpc — decodifica LDPC per FT8, FT4 e FT2

> Sezione dedicata al decodificatore LDPC(174,91) di Decodium 4.0 Core Shannon:
> progetto, misure, e i risultati negativi che hanno cambiato le decisioni.
>
> *Autore: **IU8LMC**. Implementazione e misure svolte con l'assistenza di
> Claude (Anthropic) su indicazione dell'autore. GPL-3.0.*

---

## Documenti

| Documento | Contenuto |
|---|---|
| [RAPPORTO_BUDGET_CRC.md](RAPPORTO_BUDGET_CRC.md) | Rapporto tecnico completo: perché allargare la ricerca non paga, che cosa rende rafforzare il test di accettazione, e le tre volte in cui la misura ha smentito la previsione |

## Il codice

| Percorso | Contenuto |
|---|---|
| `Detector/fastldpc/` | Il decodificatore, header-only, integrato in Decodium |
| `Detector/fastldpc/README.md` | Progetto, tabelle di prestazione, provenienza e attribuzione |
| `Detector/fastldpc/lab/` | Laboratorio: banchi di misura, strumenti, dati di prova |
| `decode_bench/` | Soglia FT8 in dB con verità di terra (`ft8sim` di WSJT-X) |

## In due righe

Il decodificatore di FT8 e FT2 non è limitato da quanto cerca, ma da come
accetta. La CRC-14 lascia passare un candidato sbagliato ogni 16 384: allargare
la ricerca compra candidati giusti e falsi nella stessa proporzione, e non paga.
Aggiungendo al test di accettazione due bit di struttura del messaggio, i
nominativi fantasma si dimezzano a decodifiche invariate.

## I numeri principali

| Misura | Risultato |
|---|---|
| Stadio FT8 di produzione, uno slot a −18 dB | **71,5 s → 9,3 s** (7,7×), decodifiche identiche |
| Min-sum, per parola | **139,8 µs → 4,7 µs** (29,7×) |
| Sensibilità rispetto al solo min-sum | **+1,3 dB** a parità di false decodifiche |
| Filtro di plausibilità, a ricerca e soglia invariate | **fantasmi dimezzati**, decodifiche invariate |
| Fallimenti irrecuperabili da qualunque decodificatore, a 1 dB | **1,1%** |

## Attribuzione in breve

Quattro livelli, di cui solo l'ultimo è opera di questo progetto:

- **la classe di codici** — LDPC, Robert Gallager, MIT 1962;
- **gli algoritmi** — min-sum normalizzato (Chen, Fossorier), ordered statistics
  decoding (Fossorier, Lin);
- **il codice specifico** — LDPC(174,91) e CRC-14 (0x2757) del protocollo FT8,
  di Steve Franke K9AN e Joe Taylor K1JT, usati **senza modifiche** per
  garantire compatibilità bit-a-bit;
- **il decodificatore** — `fastldpc`, scritto ex novo, con le ottimizzazioni e
  le misure descritte nel rapporto.

Dettaglio completo in
[`Detector/fastldpc/README.md`](../../Detector/fastldpc/README.md).
