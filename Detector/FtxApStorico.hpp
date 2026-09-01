// FtxApStorico.hpp — elenco a scorrimento delle stazioni sentite di recente,
// per usarle come ipotesi a priori.
//
// PERCHE'. L'a priori vale molto: fissare 29 bit del messaggio porta la soglia
// giu' di 1,33 dB (misurato, lab/cpp/apriori.cpp), e un'ipotesi SBAGLIATA a
// quel livello viene accettata 0-1 volte su 20000, cioe' e' sicura. Oggi pero'
// il decoder ha una sola fonte di ipotesi: i nominativi del QSO in corso. Le
// decine di stazioni che ha sentito negli ultimi minuti non le usa.
//
// COSA DICE IL TRAFFICO VERO (60 000 decodifiche FT8 dal registro,
// lab/tools/pool_ipotesi.py e gli script accanto):
//
//   * nell'83% delle decodifiche il MITTENTE era gia' stato sentito entro
//     +-5 Hz nei dieci cicli precedenti. Il destinatario solo nel 40%, e da
//     solo aggiunge il 2%: l'ipotesi giusta e' "chi trasmette e' X", cioe' i
//     bit 29-57, non "e' chiamato X".
//
//   * la finestra in frequenza puo' essere STRETTA. Da +-5 a +-50 Hz la resa
//     passa da 85,8% a 86,8%, ma le ipotesi per candidato da 2,9 a 9,5: le
//     stazioni stanno ferme, e allargare compra rumore e costo, non copertura.
//
//   * bastano poche ipotesi, prendendo le piu' recenti: K=1 copre il 42,5%,
//     K=2 il 70,2%, K=3 il 77,3%, K=5 l'82,1%. Oltre K=3 il guadagno per
//     ipotesi crolla, e ogni ipotesi in piu' e' una chiamata al decoder in
//     piu' PER CANDIDATO -- cioe' esposizione alla CRC-14, che e' la valuta
//     con cui in questo progetto si pagano i nominativi fantasma.
//
// Da qui i valori predefiniti: +-5 Hz, dieci cicli di memoria, al massimo tre
// ipotesi. Sono tarati sul traffico, non scelti a occhio.
#pragma once

#include <cstddef>

namespace decodium::apstorico
{

// Un nominativo sta in 13 caratteri piu' il terminatore, come nel resto della
// catena (pack77 usa c13).
constexpr int kLunghezzaCall = 14;

// Registra un MITTENTE sentito a una certa frequenza. `ciclo` e' un contatore
// monotono di slot: chi chiama decide la scala, purche' sia coerente.
// Chiamarla piu' volte con lo stesso nominativo e' normale e voluto -- serve a
// sapere quale sia il piu' recente.
void registra (int ciclo, float freq_hz, char const* nominativo);

// I mittenti sentiti entro +-hz nei `memoria` cicli precedenti a `ciclo`, dal
// piu' recente al piu' vecchio, senza ripetizioni. Ritorna quanti ne ha
// scritti in `out`, al massimo `max`.
//
// out deve essere un array [max][kLunghezzaCall].
int vicini (int ciclo, float freq_hz, float hz, int memoria, int max,
            char (*out)[kLunghezzaCall]);

// Estrae il MITTENTE da un messaggio decodificato e lo registra.
//
// Il mittente e' il secondo campo ("IU8LMC K1ABC R-10" -> K1ABC), tranne che
// nelle chiamate dirette a un'area, dove il secondo campo e' un qualificatore
// e non un nominativo ("CQ DX DL9XYZ JO62" -> DL9XYZ). Qui si prova il secondo
// e, se non regge come nominativo, il terzo: chi costruisce i bit rifiuta
// comunque tutto cio' che non e' codificabile in forma standard, quindi un
// errore di lettura costa un'ipotesi saltata e non un'ipotesi SBAGLIATA.
void registra_da_messaggio (int ciclo, float freq_hz, char const* messaggio);

// Il contatore di slot. avanza_ciclo() va chiamata una volta per invocazione
// del decodificatore, ciclo_corrente() ovunque serva sapere a che punto si e'.
// Sono qui e non nel chiamante perche' l'elenco e' l'unico che debba conoscere
// la propria scala temporale.
int avanza_ciclo ();
int ciclo_corrente ();

// Svuota l'elenco. Serve ai banchi di prova, per rendere le misure ripetibili,
// e a un cambio di banda, dopo il quale le stazioni sentite prima non dicono
// piu' niente su quali frequenze siano occupate.
void azzera ();

// Quante voci sono in memoria. Solo per diagnostica.
int quante ();

}  // namespace decodium::apstorico
