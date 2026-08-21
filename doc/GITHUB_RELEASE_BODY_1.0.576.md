# Decodium 4 FT2 v1.0.576

A fix for a fault introduced in v1.0.575: after that release the remote radio's
audio stopped arriving in the decoder altogether.

## English (British)

### The sink was born inside the door that was closed

v1.0.575 stopped `startAudioCapture()` from reopening the local sound card while
a remote radio is in use — correct, because the audio was coming over the
network and reopening the card would have put two sources in one buffer.

What that overlooked is that the audio **sink** — the object the remote samples
are injected into — is created inside `startAudioCapture()`, well past the point
where the new guard returned. On a computer that never opens a local capture,
the sink therefore never existed, and every injected frame was dropped on the
first line:

    if (!m_decoPortUseRemote || samples.isEmpty() || !m_audioSink) return;

The sink's creation now lives in its own function, called both by
`startAudioCapture()` where it always was, and by the guard before it refuses.
Taking a remote radio into use also asks for it directly, so it exists whatever
route was taken — including a monitor delegated to the legacy backend, which
opens no sink of its own.

### Validation

Builds and links. The fault was found by reading the guard against the injection
point rather than by reproducing it, which is also how it was introduced.

## Italiano

### Il sink nasceva dentro la porta che ho chiuso

La 1.0.575 ha impedito a `startAudioCapture()` di riaprire la scheda audio
locale mentre una radio remota è in uso — ed è giusto, perché l'audio arriva
dalla rete e riaprire la scheda avrebbe messo due sorgenti nello stesso buffer.

Quello che mi era sfuggito è che il **sink** audio — l'oggetto in cui i campioni
remoti vengono iniettati — viene creato dentro `startAudioCapture()`, molto oltre
il punto in cui la nuova guardia usciva. Su un computer che non apre mai una
cattura locale il sink quindi non è mai esistito, e ogni frame iniettato veniva
buttato alla prima riga:

    if (!m_decoPortUseRemote || samples.isEmpty() || !m_audioSink) return;

La creazione del sink adesso sta in una funzione sua, chiamata sia da
`startAudioCapture()` dov'era sempre stata, sia dalla guardia prima che si
rifiuti. Anche prendere in uso una radio remota lo chiede direttamente, così
esiste qualunque strada si sia presa — compreso un monitor delegato al backend
legacy, che un sink suo non lo apre.

### Verifiche

Compila e linka. Il difetto è stato trovato leggendo la guardia contro il punto
di iniezione invece che riproducendolo, che è anche il modo in cui era stato
introdotto.
