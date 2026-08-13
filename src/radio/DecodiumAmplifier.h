// Lettura della telemetria di un amplificatore lineare.
//
// Serve a mostrare nel DECOMETER i watt all'uscita del PA invece di quelli
// dell'eccitatrice. Il protocollo e' quello pubblicato da SPE nella
// Application Programmer's Guide; vedi doc/protocollo-spe-expert.md.
//
// Due modalita':
//   Passiva      non trasmette nulla e si limita a leggere il dialogo fra
//                l'amplificatore e il software del costruttore, rispecchiato
//                su una porta virtuale. E' la sola via se quel software deve
//                restare aperto: la porta la tiene lui.
//   Interrogante chiede lo stato a intervalli regolari. Richiede la porta
//                libera - la RS-232 sul retro, di solito.
//
// Questa classe non comanda l'amplificatore e non lo mette mai in
// trasmissione: legge e basta.
//
// By IU8LMC

#ifndef DECODIUMAMPLIFIER_H
#define DECODIUMAMPLIFIER_H

#include <QByteArray>
#include <QObject>
#include <QString>

class QSerialPort;
class QTimer;

class DecodiumAmplifier : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool    enabled     READ enabled     NOTIFY configChanged)
    Q_PROPERTY(QString port        READ port        NOTIFY configChanged)
    Q_PROPERTY(bool    passive     READ passive     NOTIFY configChanged)
    Q_PROPERTY(bool    connected   READ connected   NOTIFY connectedChanged)
    Q_PROPERTY(bool    responding  READ responding  NOTIFY telemetryChanged)
    Q_PROPERTY(bool    transmitting READ transmitting NOTIFY telemetryChanged)
    Q_PROPERTY(double  watts       READ watts       NOTIFY telemetryChanged)
    Q_PROPERTY(double  swr         READ swr         NOTIFY telemetryChanged)
    Q_PROPERTY(double  swrAtu      READ swrAtu      NOTIFY telemetryChanged)
    Q_PROPERTY(double  voltage     READ voltage     NOTIFY telemetryChanged)
    Q_PROPERTY(double  current     READ current     NOTIFY telemetryChanged)
    Q_PROPERTY(int     temperature READ temperature NOTIFY telemetryChanged)
    Q_PROPERTY(QString alarm       READ alarm       NOTIFY telemetryChanged)
    Q_PROPERTY(QString warning     READ warning     NOTIFY telemetryChanged)
    Q_PROPERTY(QString status      READ status      NOTIFY connectedChanged)

public:
    // Le grandezze estratte da una trama di stato.
    struct Reading {
        bool    valid {false};
        bool    transmitting {false};
        double  watts {0.0};
        double  swr {1.0};        // ROS d'antenna
        double  swrAtu {1.0};     // ROS prima dell'ATU
        double  voltage {0.0};
        double  current {0.0};
        int     temperature {0};
        QString warning;          // "N" = nessuno
        QString alarm;            // "N" = nessuno
        QString model;            // "20K", "13K", ...
    };

    explicit DecodiumAmplifier(QObject* parent = nullptr);
    ~DecodiumAmplifier() override;

    bool    enabled()      const { return m_enabled; }
    QString port()         const { return m_port; }
    bool    passive()      const { return m_passive; }
    bool    connected()    const;
    bool    responding()   const { return m_responding; }
    bool    transmitting() const { return m_last.transmitting; }
    double  watts()        const { return m_last.watts; }
    double  swr()          const { return m_last.swr; }
    double  swrAtu()       const { return m_last.swrAtu; }
    double  voltage()      const { return m_last.voltage; }
    double  current()      const { return m_last.current; }
    int     temperature()  const { return m_last.temperature; }
    QString alarm()        const { return m_last.alarm; }
    QString warning()      const { return m_last.warning; }
    QString status()       const;

    // Apre o chiude la porta secondo la configurazione. false se l'apertura
    // era richiesta e non e' riuscita; il motivo finisce in status().
    bool configure(bool enabled, const QString& port, int baud,
                   bool passive, int pollMs);

    // Analisi di una trama di stato SPE. Esposta perche' e' la parte che
    // merita una prova automatica: una trama malformata letta come buona
    // darebbe numeri assurdi proprio a piena potenza.
    static Reading parseSpeStatus(const QByteArray& frame);

    // Estrae ogni trama completa presente nel buffer, consumandola.
    static QList<Reading> harvest(QByteArray& buffer);

    static QByteArray statusRequest();

signals:
    void configChanged();
    void connectedChanged();
    void telemetryChanged();

private slots:
    void onReadyRead();
    void onPoll();
    void onSilence();

private:
    void applyReading(const Reading& r);

    QSerialPort* m_serial {nullptr};
    QTimer* m_pollTimer {nullptr};
    QTimer* m_silenceTimer {nullptr};
    QByteArray m_buffer;

    bool m_enabled {false};
    bool m_passive {true};
    QString m_port;
    int m_baud {9600};
    int m_pollMs {500};
    bool m_responding {false};
    QString m_lastError;
    // Il codice, non solo il testo: "porta occupata" e' il caso piu'
    // frequente e merita una risposta diversa da un errore generico. Si
    // tiene come intero per non trascinare QSerialPort in questa
    // intestazione: qui la classe e' solo dichiarata.
    int m_lastErrorCode {0};   // QSerialPort::NoError
    Reading m_last;
};

#endif // DECODIUMAMPLIFIER_H
