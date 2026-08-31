// Il modello di riconoscimento: dov'e', e come arriva la prima volta.
//
// Non sta nell'installer. Il modello che serve davvero pesa 465 MB e
// l'installer di Decodium ne pesa 67: metterlo dentro vorrebbe dire farlo
// scaricare a tutti, anche ai molti che la fonia non la fanno. Si scarica
// percio' alla prima accensione della funzione, una volta sola, e resta.
//
// Il file finisce accanto agli altri dati dell'applicazione, non nella cartella
// dei programmi: li' Windows chiede i permessi di amministratore, e un
// aggiornamento di Decodium cancellerebbe mezzo giga che va benissimo dov'e'.
#pragma once

#include <QObject>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;
class QFile;

namespace decodium::speech {

class SpeechModelStore : public QObject
{
    Q_OBJECT

public:
    explicit SpeechModelStore (QObject* parent = nullptr);
    ~SpeechModelStore () override;

    // Il percorso del modello, esista o no.
    static QString percorsoModello ();

    // Vero se il file c'e' ed e' di dimensione plausibile. Un file troncato —
    // scaricamento interrotto, disco pieno — non conta come presente: il
    // riconoscitore lo aprirebbe e fallirebbe con un errore che non dice nulla.
    static bool modelloPresente ();

    // Scarica il modello. Non fa nulla se c'e' gia' o se un altro scaricamento
    // e' in corso.
    void scarica ();
    void annulla ();
    bool inCorso () const { return m_risposta != nullptr; }

signals:
    // Quanto e' stato scaricato, in percentuale, e i megabyte per l'etichetta.
    void avanzamento (int percentuale, qint64 fattiMB, qint64 totaliMB);
    void finito (bool riuscito, const QString& messaggio);

private:
    void pulisciParziale ();

    QNetworkAccessManager* m_rete {nullptr};
    QNetworkReply* m_risposta {nullptr};
    QFile* m_file {nullptr};
};

} // namespace decodium::speech
