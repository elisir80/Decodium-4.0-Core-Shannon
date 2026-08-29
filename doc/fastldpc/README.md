# fastldpc — LDPC decoding for FT8, FT4 and FT2

> Dedicated section on the LDPC(174,91) decoder of Decodium 4.0 Core Shannon:
> design, measurements, and the negative results that changed the decisions.
>
> *Author: **IU8LMC**. Implementation and measurements carried out with the
> assistance of Claude (Anthropic) under the author's direction. GPL-3.0.*

**[Italiano](#italiano) · [English](#english) · [Español](#español)**

---

## The technical report · Il rapporto · El informe

| Lingua | Documento |
|---|---|
| 🇮🇹 Italiano | [**Il budget della CRC**](RAPPORTO_BUDGET_CRC.md) |
| 🇬🇧 English | [**The CRC budget**](CRC_BUDGET_REPORT.md) |
| 🇪🇸 Español | [**El presupuesto del CRC**](INFORME_PRESUPUESTO_CRC.md) |

## The code · Il codice · El código

| Path | Content |
|---|---|
| `Detector/fastldpc/` | The decoder, header-only, integrated into Decodium |
| `Detector/fastldpc/README.md` | Design, performance tables, provenance and attribution |
| `Detector/fastldpc/lab/` | Laboratory: benchmarks, tools, test data |
| `decode_bench/` | FT8 threshold in dB with ground truth (`ft8sim` from WSJT-X) |

## Key figures · I numeri principali · Las cifras principales

| Measurement | Result |
|---|---|
| Production FT8 stage, one slot at −18 dB | **71.5 s → 9.3 s** (7.7×), identical decodes |
| Min-sum, per word | **139.8 µs → 4.7 µs** (29.7×) |
| Sensitivity against min-sum alone | **+1.3 dB** at equal false-decode rate |
| Plausibility filter, at unchanged search and threshold | **phantoms halved**, decodes unchanged |
| Failures irrecoverable by any decoder, at 1 dB | **1.1%** |

---

## Italiano

Il decodificatore di FT8 e FT2 non è limitato da quanto cerca, ma da come
accetta. La CRC-14 lascia passare un candidato sbagliato ogni 16 384: allargare
la ricerca compra candidati giusti e falsi nella stessa proporzione, e non paga.
Aggiungendo al test di accettazione due bit di struttura del messaggio, i
nominativi fantasma si dimezzano a decodifiche invariate.

Il rapporto riporta per intero **tre volte in cui la misura ha smentito la
previsione**, perché sono la parte più utile: ognuna avrebbe portato in banda un
peggioramento presentato come miglioramento.

→ [**Leggi il rapporto**](RAPPORTO_BUDGET_CRC.md)

## English

The FT8 and FT2 decoder is not limited by how much it searches, but by how it
accepts. The 14-bit CRC lets one wrong candidate through every 16,384: widening
the search buys correct and false candidates in the same proportion, and does
not pay. Adding two bits of message structure to the acceptance test halves the
phantom callsigns while leaving decodes unchanged.

The report includes in full **three occasions where the measurement refuted the
prediction**, because they are the most useful part: each would have taken a
regression on air dressed up as an improvement.

→ [**Read the report**](CRC_BUDGET_REPORT.md)

## Español

El decodificador de FT8 y FT2 no está limitado por cuánto busca, sino por cómo
acepta. El CRC de 14 bits deja pasar un candidato erróneo cada 16 384: ampliar
la búsqueda compra candidatos correctos y falsos en la misma proporción, y no
compensa. Añadiendo a la prueba de aceptación dos bits de estructura del
mensaje, los indicativos fantasma se reducen a la mitad sin cambiar las
decodificaciones.

El informe recoge íntegras **tres ocasiones en las que la medición desmintió la
previsión**, porque son la parte más útil: cada una habría llevado al aire un
empeoramiento presentado como mejora.

→ [**Lee el informe**](INFORME_PRESUPUESTO_CRC.md)

---

## Attribution · Attribuzione · Atribución

Four levels, of which only the last is the work of this project ·
Quattro livelli, di cui solo l'ultimo è opera di questo progetto ·
Cuatro niveles, de los cuales sólo el último es obra de este proyecto:

| Level | |
|---|---|
| **The class of codes** | LDPC — Robert Gallager, MIT, 1962 |
| **The algorithms** | Normalised min-sum (Chen, Fossorier), ordered statistics decoding (Fossorier, Lin) |
| **The specific code** | LDPC(174,91) and CRC-14 (`0x2757`) of the FT8 protocol — Steve Franke K9AN and Joe Taylor K1JT — used **unmodified** to guarantee bit-exact compatibility |
| **The decoder** | `fastldpc`, written from scratch, with the optimisations and measurements described in the report |

Full detail in [`Detector/fastldpc/README.md`](../../Detector/fastldpc/README.md).
