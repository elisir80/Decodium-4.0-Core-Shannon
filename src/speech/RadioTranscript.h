// Da quello che il riconoscitore ha sentito a quello che l'operatore ha detto.
//
// Un riconoscitore vocale generico non sa niente di radio. Sente "CQ" e scrive
// "Chico", perche' fra le parole che conosce quella e' la piu' vicina; sente un
// nominativo compitato e scrive nove parole comuni di seguito. Misurato su
// audio in banda SSB, l'errore non e' casuale: e' sempre lo stesso, e ha una
// forma riconoscibile.
//
//   sentito     "Chico Chico Chico this is India uniform made Lima Mike
//                Charlie Colling Chico and standing by"
//   detto       "CQ CQ CQ this is IU8LMC calling CQ and standing by"
//
// Questo modulo fa quella traduzione. Non migliora il riconoscimento — lavora
// sul testo, dopo — e non serve a rendere le frasi piu' belle: serve perche'
// senza di esso il nominativo, che e' l'unica cosa che conta davvero in un
// collegamento, non compare da nessuna parte.
//
// Tre cose in ordine, ognuna appoggiata sulla precedente:
//
//   1. le parole del gergo tornano quelle che sono   (Chico -> CQ)
//   2. le sequenze compitate tornano lettere         (India uniform -> IU)
//   3. quello che ne esce, se ha forma di nominativo, viene marcato come tale
//
// Il riconoscimento e' tollerante agli errori, perche' su un segnale rumoroso
// le parole arrivano storpiate: "Aiklima" per "eight Lima", "Colling" per
// "calling". Si accetta una parola vicina, non solo quella esatta, e la
// distanza ammessa cresce con la lunghezza della parola — su tre lettere un
// errore cambia tutto, su nove no.
#pragma once

#include <QString>
#include <QStringList>

namespace decodium::speech {

struct Ricomposizione {
    // Il testo con gergo e nominativi rimessi a posto.
    QString testo;

    // I nominativi riconosciuti, gia' in forma ITU. Vuoto quando non se ne
    // trovano: meglio nessun nominativo che uno inventato, perche' un
    // nominativo sbagliato in un log e' peggio di un campo vuoto.
    QStringList nominativi;

    // I nominativi letti a meta': si vede che sono nominativi — la sequenza
    // compitata c'e' — ma una parola non e' stata riconosciuta e al suo posto
    // resta un punto interrogativo, per esempio IU?LMC.
    //
    // Questo capita quasi sempre sulla cifra, e non per caso: le cifre sono
    // parole corte e comuni, e il riconoscitore le scambia volentieri con
    // altre parole corte e comuni — "eight" diventa "made", "make", "and".
    // Quando succede la cifra e' persa, e nessuna astuzia la riporta indietro.
    //
    // Si mostrano lo stesso, separati da quelli sicuri: chi legge vede che
    // qualcuno sta chiamando e quasi tutto il suo nominativo, e puo' chiedere
    // conferma della sola cifra. Metterli fra i sicuri con una cifra inventata
    // sarebbe la cosa peggiore: un nominativo plausibile e sbagliato.
    QStringList nominativiIncerti;

    // Quante parole sono state cambiate. Serve a chi guarda il risultato per
    // capire quanto e' stato ricostruito e quanto e' arrivato cosi' com'era.
    int paroleCorrette {0};
};

// Il testo grezzo del riconoscitore, ricomposto in linguaggio radiantistico.
// La lingua serve solo a scegliere i dizionari del gergo: l'alfabeto fonetico
// e' lo stesso in tutte, ed e' il pezzo che conta.
Ricomposizione ricomponi (const QString& grezzo,
                          const QString& lingua = QStringLiteral("en"));

// Vero se la stringa ha la forma di un nominativo secondo la ITU: un prefisso,
// un solo gruppo di cifre, un suffisso di sole lettere. Esposta perche' e' lo
// stesso controllo che serve altrove.
bool formaDiNominativo (const QString& testo);

} // namespace decodium::speech
