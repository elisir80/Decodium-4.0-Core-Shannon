#include "speech/RadioTranscript.h"

#include <QHash>
#include <QList>
#include <QRegularExpression>
#include <QStringList>

#include <algorithm>
#include <vector>

namespace decodium::speech {

namespace {

// ─── alfabeto fonetico ──────────────────────────────────────────────────────
//
// Le forme ICAO, piu' quelle che si sentono davvero in aria e che i manuali non
// riportano: "Japan" per Juliett, "Nancy" e "Baker" del vecchio alfabeto
// americano, "Italy" e "Ocean" che gli europei usano di continuo. Un
// riconoscitore addestrato sull'inglese comune preferisce sempre la parola
// comune, quindi vanno elencate.
struct VoceFonetica { const char* parola; char lettera; };

const VoceFonetica kFonetico[] = {
    {"alpha", 'A'},   {"alfa", 'A'},     {"able", 'A'},     {"america", 'A'},
    {"bravo", 'B'},   {"baker", 'B'},    {"boston", 'B'},
    {"charlie", 'C'}, {"canada", 'C'},
    {"delta", 'D'},   {"denmark", 'D'},
    {"echo", 'E'},    {"england", 'E'},  {"easy", 'E'},
    {"foxtrot", 'F'}, {"fox", 'F'},      {"france", 'F'},   {"florida", 'F'},
    {"golf", 'G'},    {"germany", 'G'},  {"george", 'G'},
    {"hotel", 'H'},   {"henry", 'H'},    {"honolulu", 'H'},
    {"india", 'I'},   {"italy", 'I'},    {"italia", 'I'},
    {"juliett", 'J'}, {"juliet", 'J'},   {"japan", 'J'},    {"john", 'J'},
    {"kilo", 'K'},    {"kilowatt", 'K'}, {"king", 'K'},
    {"lima", 'L'},    {"london", 'L'},   {"love", 'L'},
    {"mike", 'M'},    {"mexico", 'M'},   {"mary", 'M'},
    {"november", 'N'},{"norway", 'N'},   {"nancy", 'N'},
    {"oscar", 'O'},   {"ocean", 'O'},    {"ontario", 'O'},
    {"papa", 'P'},    {"portugal", 'P'}, {"peter", 'P'},
    {"quebec", 'Q'},  {"queen", 'Q'},
    {"romeo", 'R'},   {"radio", 'R'},
    {"sierra", 'S'},  {"santiago", 'S'}, {"sugar", 'S'},
    {"tango", 'T'},   {"tokyo", 'T'},
    {"uniform", 'U'}, {"united", 'U'},   {"uncle", 'U'},
    {"victor", 'V'},  {"victoria", 'V'},
    {"whiskey", 'W'}, {"whisky", 'W'},   {"washington", 'W'},
    {"x-ray", 'X'},   {"xray", 'X'},     {"x", 'X'},
    {"yankee", 'Y'},  {"yokohama", 'Y'},
    {"zulu", 'Z'},    {"zanzibar", 'Z'},
    // le cifre, dette in chiaro e nella forma ITU
    {"zero", '0'},    {"nadazero", '0'}, {"oh", '0'},
    {"one", '1'},     {"unaone", '1'},   {"won", '1'},
    {"two", '2'},     {"bissotwo", '2'}, {"too", '2'},      {"to", '2'},
    {"three", '3'},   {"terrathree", '3'}, {"tree", '3'},
    {"four", '4'},    {"kartefour", '4'},{"fower", '4'},    {"for", '4'},
    {"five", '5'},    {"pantafive", '5'},{"fife", '5'},
    {"six", '6'},     {"soxisix", '6'},
    {"seven", '7'},   {"setteseven", '7'},
    {"eight", '8'},   {"oktoeight", '8'},{"ate", '8'},
    {"nine", '9'},    {"novenine", '9'}, {"niner", '9'},
};

// ─── gergo ──────────────────────────────────────────────────────────────────
//
// A sinistra quello che il riconoscitore scrive davvero: sono forme raccolte
// dalle prove, non immaginate. "Chico" per CQ compare in ogni singola prova
// fatta, pulita o rumorosa che fosse.
struct VoceGergo { const char* sentito; const char* giusto; };

const VoceGergo kGergo[] = {
    {"chico", "CQ"},        {"chiku", "CQ"},       {"seek you", "CQ"},
    {"cq", "CQ"},           {"kilo queue", "CQ"},  {"chicko", "CQ"},
    {"cue cue", "CQ"},      {"kyu", "CQ"},
    {"q r z", "QRZ"},       {"qrz", "QRZ"},        {"cure said", "QRZ"},
    {"q s l", "QSL"},       {"qsl", "QSL"},        {"cusell", "QSL"},
    {"q t h", "QTH"},       {"qth", "QTH"},        {"cute h", "QTH"},
    {"q s o", "QSO"},       {"qso", "QSO"},
    {"q s y", "QSY"},       {"qsy", "QSY"},
    {"q r m", "QRM"},       {"qrm", "QRM"},
    {"q s b", "QSB"},       {"qsb", "QSB"},
    {"q r p", "QRP"},       {"qrp", "QRP"},
    {"seventy three", "73"},{"seventy-three", "73"},{"73", "73"},
    {"eighty eight", "88"}, {"88", "88"},
    {"roger", "R"},         {"over", "over"},
};

// ─── distanza fra due parole ────────────────────────────────────────────────
// Levenshtein, fermata appena supera il massimo consentito: su un testo lungo
// si confrontano migliaia di coppie e quasi tutte sono lontanissime.
int distanza (const QString& a, const QString& b, int massimo)
{
    int const n = a.size(), m = b.size();
    if (std::abs(n - m) > massimo)
        return massimo + 1;

    std::vector<int> prec(m + 1), corr(m + 1);
    for (int j = 0; j <= m; ++j) prec[j] = j;

    for (int i = 1; i <= n; ++i) {
        corr[0] = i;
        int migliore = corr[0];
        for (int j = 1; j <= m; ++j) {
            int const costo = (a[i - 1] == b[j - 1]) ? 0 : 1;
            corr[j] = std::min({prec[j] + 1, corr[j - 1] + 1, prec[j - 1] + costo});
            migliore = std::min(migliore, corr[j]);
        }
        if (migliore > massimo)
            return massimo + 1;
        prec.swap(corr);
    }
    return prec[m];
}

// Quanto errore si accetta su una parola: nessuno sotto le quattro lettere,
// uno fino a sette, due oltre. Su "one" un errore la trasformerebbe in "ore" o
// "nine"; su "november" un errore non toglie nulla al riconoscimento.
int erroreAmmesso (int lunghezza)
{
    if (lunghezza <= 3) return 0;
    if (lunghezza <= 7) return 1;
    return 2;
}

// Se la parola e' una voce dell'alfabeto fonetico, la lettera o cifra
// corrispondente; altrimenti 0.
char comeFonetica (const QString& parola, bool* approssimata = nullptr)
{
    QString const p = parola.toLower();
    char vicina = 0;
    for (const VoceFonetica& v : kFonetico) {
        QString const rif = QString::fromLatin1(v.parola);
        if (p == rif) {                                  // esatta: si chiude qui
            if (approssimata) *approssimata = false;
            return v.lettera;
        }
        if (!vicina) {
            int const max = erroreAmmesso(rif.size());
            if (max > 0 && distanza(p, rif, max) <= max)
                vicina = v.lettera;                      // vicina: si tiene da parte
        }
    }
    // Una corrispondenza approssimata si segnala. Su un nominativo la
    // differenza pesa: "make" dista una lettera da "mike" e diventa M, ma
    // l'operatore aveva detto "eight". Una lettera indovinata per somiglianza
    // e' il posto giusto dove mettere un dubbio, non una certezza.
    if (approssimata) *approssimata = (vicina != 0);
    return vicina;
}

QString pulisci (QString s)
{
    // Via la punteggiatura, che il riconoscitore inventa: "India, uniform." va
    // letto come le due parole che sono.
    static const QRegularExpression segni(QStringLiteral("[.,;:!?\"()\\[\\]]"));
    return s.remove(segni).trimmed();
}

} // namespace

// ─── forma ITU del nominativo ───────────────────────────────────────────────
bool formaDiNominativo (const QString& testo)
{
    QString const c = testo.trimmed().toUpper();
    // Fra tre e otto caratteri: sotto non e' un nominativo, sopra e' un'altra
    // cosa. Deve contenere almeno una cifra e almeno una lettera.
    if (c.size() < 3 || c.size() > 8)
        return false;

    // Prefisso, un solo blocco di cifre, suffisso di sole lettere: e' la forma
    // che la ITU assegna, e regge anche i prefissi a due cifre come 2E0 o 3B8.
    static const QRegularExpression forma(
        QStringLiteral("^[A-Z0-9]{1,3}[0-9][A-Z]{1,4}$"));
    if (!forma.match(c).hasMatch())
        return false;

    // Un solo gruppo di cifre nel corpo, ma la cifra iniziale del prefisso non
    // conta: i prefissi 2E0, 3B8, 9A6 ne hanno una loro, e pretendere un solo
    // gruppo li bocciava tutti — 2E0ABC veniva ricomposto giusto e poi
    // scartato perche' "aveva due gruppi di cifre".
    QString corpo = c;
    if (corpo.size() > 1 && corpo.at(0).isDigit() && !corpo.at(1).isDigit())
        corpo = corpo.mid(1);

    int gruppi = 0;
    bool dentro = false;
    for (QChar ch : corpo) {
        if (ch.isDigit()) {
            if (!dentro) { ++gruppi; dentro = true; }
        } else {
            dentro = false;
        }
    }
    return gruppi == 1;
}

// ─── il lavoro ──────────────────────────────────────────────────────────────
Ricomposizione ricomponi (const QString& grezzo, const QString& lingua)
{
    Q_UNUSED(lingua)
    Ricomposizione out;
    if (grezzo.trimmed().isEmpty())
        return out;

    QStringList parole = pulisci(grezzo).split(QRegularExpression(QStringLiteral("\\s+")),
                                               Qt::SkipEmptyParts);
    if (parole.isEmpty())
        return out;

    // ── 1) il gergo, prima di tutto ─────────────────────────────────────────
    // Si prova anche sulle coppie di parole, perche' meta' del gergo arriva
    // spezzato in due: "seventy three", "q r z".
    for (int i = 0; i < parole.size(); ++i) {
        QString const singola = parole[i].toLower();
        QString const doppia = (i + 1 < parole.size())
                               ? singola + QLatin1Char(' ') + parole[i + 1].toLower()
                               : QString();

        bool fatto = false;
        for (const VoceGergo& g : kGergo) {
            QString const rif = QString::fromLatin1(g.sentito);
            if (!doppia.isEmpty() && doppia == rif) {
                parole[i] = QString::fromLatin1(g.giusto);
                parole.removeAt(i + 1);
                ++out.paroleCorrette;
                fatto = true;
                break;
            }
        }
        if (fatto) continue;

        for (const VoceGergo& g : kGergo) {
            QString const rif = QString::fromLatin1(g.sentito);
            if (rif.contains(QLatin1Char(' '))) continue;
            int const max = erroreAmmesso(rif.size());
            if (singola == rif || (max > 0 && distanza(singola, rif, max) <= max)) {
                if (parole[i] != QString::fromLatin1(g.giusto)) ++out.paroleCorrette;
                parole[i] = QString::fromLatin1(g.giusto);
                break;
            }
        }
    }

    // ── 2) le sequenze compitate ────────────────────────────────────────────
    // Due parole fonetiche di fila non bastano: "over" e "one" capitano in una
    // frase normale e diventerebbero "O1". Da tre in su la sequenza non e' piu'
    // un caso: nessuno dice tre parole dell'alfabeto ICAO di seguito per sbaglio.
    QStringList uscita;
    for (int i = 0; i < parole.size(); ) {
        QString lettere;
        QList<int> dubbie;          // posizioni ottenute per somiglianza
        int j = i;
        while (j < parole.size()) {
            bool appross = false;
            char const l = comeFonetica(parole[j], &appross);
            if (!l) break;
            if (appross) dubbie << lettere.size();
            lettere.append(QLatin1Char(l));
            ++j;
        }

        // Una sola parola non riconosciuta in mezzo a una compitazione non
        // interrompe la lettura. Succede quasi sempre sulla cifra, che e' una
        // parola corta e comune e come tale viene scambiata con altre parole
        // corte e comuni. Si tira dritto segnando un punto interrogativo: la
        // cifra e' persa, ma il resto del nominativo no, e sapere che qualcuno
        // ha chiamato IU?LMC vale piu' che non saperlo affatto.
        bool incerto = false;
        if (lettere.size() >= 2 && j < parole.size() - 1) {
            char const dopo = comeFonetica(parole[j + 1]);
            if (dopo) {
                // c'e' altra compitazione dopo il buco: si continua
                QString coda;
                int k = j + 1;
                while (k < parole.size()) {
                    char const l = comeFonetica(parole[k]);
                    if (!l) break;
                    coda.append(QLatin1Char(l));
                    ++k;
                }
                if (lettere.size() + coda.size() >= 4) {
                    lettere.append(QLatin1Char('?')).append(coda);
                    incerto = true;
                    j = k;
                }
            }
        }

        if (incerto) {
            uscita << lettere;
            out.nominativiIncerti << lettere;
            out.paroleCorrette += (j - i);
            i = j;
            continue;
        }

        if (lettere.size() >= 3) {
            // Il gruppo puo' contenere piu' di un nominativo attaccato — capita
            // quando si risponde compitando il proprio dopo quello dell'altro.
            // Se l'intero gruppo ha forma valida lo si tiene intero; altrimenti
            // si prova a spezzarlo in due parti valide.
            if (formaDiNominativo(lettere)) {
                uscita << lettere;
                out.nominativi << lettere;
                out.paroleCorrette += (j - i);
            } else {
                bool spezzato = false;
                for (int t = 3; t <= lettere.size() - 3 && !spezzato; ++t) {
                    QString const a = lettere.left(t), b = lettere.mid(t);
                    if (formaDiNominativo(a) && formaDiNominativo(b)) {
                        uscita << a << b;
                        out.nominativi << a << b;
                        out.paroleCorrette += (j - i);
                        spezzato = true;
                    }
                }
                if (!spezzato && !dubbie.isEmpty()) {
                    // Non e' un nominativo valido, ma qualche lettera veniva da
                    // una somiglianza. Si prova a rimetterla in dubbio: se
                    // senza quella il resto ha forma di nominativo, allora era
                    // proprio quella la parola sbagliata.
                    for (int pos : dubbie) {
                        QString tentativo = lettere;
                        tentativo[pos] = QLatin1Char('?');
                        QString sagoma = tentativo;
                        sagoma[pos] = QLatin1Char('0');   // una cifra qualsiasi, per la forma
                        if (formaDiNominativo(sagoma)) {
                            uscita << tentativo;
                            out.nominativiIncerti << tentativo;
                            out.paroleCorrette += (j - i);
                            spezzato = true;
                            break;
                        }
                    }
                }
                if (!spezzato) {
                    // Compitato ma non e' un nominativo: un locatore, un nome,
                    // un rapporto. Si scrive come lettere maiuscole, che e'
                    // comunque piu' vicino a quello che e' stato detto delle
                    // parole intere.
                    uscita << lettere;
                    out.paroleCorrette += (j - i);
                }
            }
            i = j;
            continue;
        }

        uscita << parole[i];
        ++i;
    }

    out.testo = uscita.join(QLatin1Char(' '));
    return out;
}

} // namespace decodium::speech
