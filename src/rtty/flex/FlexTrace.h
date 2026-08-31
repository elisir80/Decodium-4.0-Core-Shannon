// DecoRTTY — il registro del dialogo con un FlexRadio.
//
// Il ramo FlexRadio non e' mai stato provato su un apparato vero, e la prima
// volta che lo sara' non sara' qui: sara' a casa di qualcun altro, con la sua
// radio, in una sessione che probabilmente non si ripete. Da una prova cosi' si
// torna con un ricordo — "non si sentiva niente" — che non basta a capire dove
// si e' rotta la catena.
//
// Percio' ogni comando che parte, ogni riga che torna e ogni decisione presa
// per conto nostro finiscono in un file di testo, con l'ora al millesimo. Non
// serve a chi lo usa: serve a chi dopo deve capire perche' non funzionava, e
// senza il quale non gli resterebbe che indovinare.
//
// Il file sta nella cartella dati dell'utente e non accanto all'eseguibile: da
// installato quest'ultimo sta sotto Programmi, dove nessun programma puo'
// scrivere, e il registro sparirebbe proprio nell'installazione di chi non
// compila da se'.
#pragma once

#include <QFile>
#include <QString>
#include <QTextStream>

namespace decortty::flex {

class FlexTrace {
public:
    // Uno solo per programma: il dialogo con la radio e' uno.
    static FlexTrace& instance();

    // Un comando in partenza, una riga in arrivo, una decisione nostra. Le tre
    // frecce diverse servono a leggere il file di corsa senza rileggerlo tutto.
    void sent(const QString& command);
    void received(const QString& line);
    void note(const QString& text);

    // Dove sta il file, per poterlo dire a chi deve mandarcelo.
    QString path() const { return m_path; }
    bool    isOpen() const { return m_file.isOpen(); }

private:
    FlexTrace();
    ~FlexTrace();
    FlexTrace(const FlexTrace&)            = delete;
    FlexTrace& operator=(const FlexTrace&) = delete;

    void write(QChar mark, const QString& text);

    // Un tetto, oltre il quale si smette e lo si scrive. Una sessione lunga non
    // deve poter riempire il disco di chi ci ha prestato la radio, e un registro
    // che si interrompe dichiarandolo e' molto meglio di uno troncato in
    // silenzio a meta' frase.
    static constexpr qint64 kMaxBytes = 4 * 1024 * 1024;

    QFile       m_file;
    QTextStream m_out;
    QString     m_path;
    qint64      m_written{0};
    bool        m_full{false};
};

// Scorciatoie, perche' una riga di registro non deve costare piu' di quello che
// registra.
inline void traceSent(const QString& command)  { FlexTrace::instance().sent(command); }
inline void traceRecv(const QString& line)     { FlexTrace::instance().received(line); }
inline void traceNote(const QString& text)     { FlexTrace::instance().note(text); }

} // namespace decortty::flex
