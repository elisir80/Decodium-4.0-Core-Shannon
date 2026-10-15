# Decodium 4 v1.0.694

## English (British)

Decodium 4 v1.0.694 is the fork release of the code of v1.0.693: serial-port ownership, CAT and CW recovery, the DecoPort port hub with real owners for every TCP, UDP and COM port, and the Power On / Power Off fixes for direct CAT radios.

### Changes since 1.0.692

- The fork release now carries the same code as the upstream v1.0.693, with the Windows packaging of this fork (com0com, Hamlib and the installer).
- Serial-port ownership is serialised: CAT, CW keyer, WinKeyer, PTT and DecoPort lines reserve their port through one registry.
- The DecoPort port hub reports the real owner (program and PID) of each port. Serial ports are never opened to check them.

### Notes

- Verified on a Yaesu FT-991 over direct USB: Power Off followed by Power On, without restarting Decodium.
- Use a dummy load for the first transmissions of the voice keyer, as in 1.0.686.

## Italiano

Decodium 4 v1.0.694 è la release del fork con il codice della v1.0.693: proprietà delle porte seriali, recupero di CAT e CW, il hub porte DecoPort con il proprietario reale di ogni porta TCP, UDP e COM, e le correzioni Power On / Power Off per le radio con CAT diretto.

### Modifiche dalla 1.0.692

- La release del fork ora contiene lo stesso codice della v1.0.693 upstream, con il packaging Windows di questo fork (com0com, Hamlib e installer).
- La proprietà delle porte seriali è in sequenza: CAT, keyer CW, WinKeyer, PTT e linee DecoPort riservano la propria porta tramite un unico registro.
- Il hub porte DecoPort mostra il proprietario reale (programma e PID) di ogni porta. Le porte seriali non vengono mai aperte per controllarle.

### Note

- Verificata con una Yaesu FT-991 via USB diretto: Power Off seguito da Power On, senza riavviare Decodium.
- Per le prime trasmissioni del voice keyer usa un carico fittizio, come nella 1.0.686.

## Downloads

The Windows x64 installer EXE is attached to this release.
