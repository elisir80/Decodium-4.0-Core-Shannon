# Decodium 4 v1.0.645

## English (UK)

This release removes the Italian text that appeared in the interface regardless of the language chosen.

### Ninety-four strings were Italian at source

- Qt's source language is whatever is written in the code. Ninety-four strings were written in Italian, so they appeared in Italian to everyone — to the German, Spanish and Japanese interfaces too. Nothing could be translated, because there was nothing to translate: the "original" was already Italian.
- Twenty-two of them were status messages that did not even go through the translation call: *transmission complete*, *reception stopped*, *settings saved*, *tune finished*, *transmit audio device not found*, *transmit watchdog timed out*, *no WAV file found*. They are now translatable, and the ones that carried a device name or a path now use a placeholder instead of concatenating strings, which is what translation requires.
- The other seventy-two were already inside the translation call but written in Italian: forty-eight in the callsign service (LoTW, QRZ.com, eQSL, Club Log), the rest in CAT, Cloudlog, QRZ Logbook, the QSO log dialog, the main window and four QML files — the lookup panel, the live map and two settings pages.
- All ninety-four now read in English at source and are translated into **all fifteen supplied languages**, each in its correct translation context. Italian users see exactly what they saw before.

### Also in this release

- The update window rebuilt in v1.0.644, the unresolved-hash sequencing safeguard from v1.0.641 and the safe CPU dispatch from v1.0.640, plus upstream v1.0.643 (settings and profile persistence, localised weather, SSTV receive controls).

### Known and not addressed

- The Romanian catalogue is shorter than the others and is stale with respect to the code: it carries the new strings, but older parts of the interface remain untranslated there.

### Downloads

Source ZIP and tar.gz archives are available for this tag. The Windows x64 installer EXE is attached to this release; GitHub workflows add Linux x86_64/aarch64 AppImages with checksum files as they finish.

---

## Italiano

Questo rilascio toglie il testo italiano che compariva nell'interfaccia qualunque lingua fosse stata scelta.

### Novantaquattro stringhe erano italiane all'origine

- La lingua di partenza di Qt è quella scritta nel codice. Novantaquattro stringhe erano scritte in italiano, quindi comparivano in italiano a chiunque — anche nell'interfaccia in tedesco, in spagnolo, in giapponese. Non c'era niente da tradurre, perché l'«originale» era già italiano.
- Ventidue erano messaggi di stato che non passavano nemmeno dalla chiamata di traduzione: *TX completato*, *RX fermato*, *impostazioni salvate*, *Tune terminato*, *audio TX non trovato*, *watchdog di trasmissione scaduto*, *nessun file WAV trovato*. Ora sono traducibili, e quelle che portavano il nome di un dispositivo o un percorso usano un segnaposto invece di sommare stringhe, come la traduzione richiede.
- Le altre settantadue erano già dentro la chiamata di traduzione ma scritte in italiano: quarantotto nel servizio dei nominativi (LoTW, QRZ.com, eQSL, Club Log), le restanti in CAT, Cloudlog, QRZ Logbook, la finestra di log del QSO, la finestra principale e quattro file QML — il pannello lookup, la mappa live e due schede delle impostazioni.
- Tutte e novantaquattro sono ora in inglese all'origine e tradotte in **tutte e quindici le lingue fornite**, ciascuna nel contesto di traduzione giusto. Chi usa l'italiano vede esattamente quello che vedeva prima.

### Nel rilascio c'è anche

- La finestra dell'aggiornamento rifatta nella 1.0.644, la cautela sul destinatario nascosto dietro l'hash della 1.0.641 e il dispatch sicuro della CPU della 1.0.640, oltre alla versione upstream 1.0.643 (persistenza di impostazioni e profili, meteo localizzato, controlli di ricezione SSTV).

### Noto e non risolto

- Il catalogo rumeno è più corto degli altri ed è arretrato rispetto al codice: le stringhe nuove ci sono, ma parti più vecchie dell'interfaccia restano lì non tradotte.

### Download

Per questo tag sono disponibili gli archivi sorgente ZIP e tar.gz. L'installer Windows x64 EXE è allegato a questo rilascio; i workflow di GitHub aggiungono le AppImage Linux x86_64/aarch64 con i file di checksum man mano che finiscono.
