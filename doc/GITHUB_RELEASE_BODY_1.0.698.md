# Decodium 4 v1.0.698

## English (British)

Decodium 4 v1.0.698 is the fork release of v1.0.697. It carries the Liquid Glass presentation, the TX dispatch guards, the CW and spectrum-recovery improvements of v1.0.696 and v1.0.697, and the FT2 partner-frequency and drift-search changes of v1.0.695, with the Windows packaging of this fork (com0com, Hamlib and the installer).

### Changes since 1.0.695

- Includes everything in upstream v1.0.696 and v1.0.697: cross-mode stability and UI fixes, the selectable Liquid Glass presentation with the Pop/Dock rule for detached panels, stale-request protection on the TX path (HALT, QSO completion, AutoCQ changes and delayed callbacks), separate received and transmitted text in the CW window, and spectrum recovery after QSO logging.
- Windows x64 installer built from the same code, with com0com, Hamlib and the installer payload of this fork.

### Notes

- This build was compiled and packaged on Windows; the Liquid Glass presentation is aimed at macOS and keeps an opaque fallback on Windows.
- Use a dummy load for the first transmissions of the voice keyer, as in 1.0.686.

## Italiano

Decodium 4 v1.0.698 è la release del fork basata sulla v1.0.697. Contiene la presentazione Liquid Glass, le guardie sul percorso TX, i miglioramenti CW e di recupero dello spettro delle v1.0.696 e v1.0.697, e le modifiche FT2 della v1.0.695 (frequenza del corrispondente e ricerca della deriva), con il packaging Windows di questo fork (com0com, Hamlib e installer).

### Modifiche dalla 1.0.695

- Include tutto ciò che c'è nelle v1.0.696 e v1.0.697 upstream: stabilità tra i modi e correzioni UI, presentazione Liquid Glass selezionabile con la regola Pop/Dock per i pannelli staccati, protezione dalle richieste obsolete sul percorso TX (HALT, completamento del QSO, modifiche ad AutoCQ e callback differiti), testo ricevuto e trasmesso separati nella finestra CW e recupero dello spettro dopo il log di un QSO.
- Installer Windows x64 costruito dallo stesso codice, con com0com, Hamlib e il payload di installazione di questo fork.

### Note

- Questa build è stata compilata e impacchettata su Windows; la presentazione Liquid Glass è pensata per macOS e su Windows mantiene il fallback opaco.
- Per le prime trasmissioni del voice keyer usa un carico fittizio, come nella 1.0.686.

## Downloads

The source archive, Windows x64 installer EXE, macOS Intel and Apple Silicon DMGs, and Linux x86_64 and aarch64 AppImages are attached to this release by the corresponding GitHub Actions workflows.
