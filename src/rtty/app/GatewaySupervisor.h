// DecoRTTY — avvia e ferma il gateway insieme all'applicazione.
//
// Il gateway è un programma a sé perché deve poter girare sul PC a cui la radio
// è attaccata, che non è per forza quello da cui si opera. Ma nel caso normale —
// una macchina sola — chiedere all'operatore di lanciarne due, nell'ordine
// giusto, e di ricordarsi di chiuderli entrambi, è un fastidio senza
// contropartita.
//
// Tre regole che rendono la cosa sicura invece che soltanto comoda:
//   * se un gateway è già in ascolto non se ne avvia un altro. Due gateway sulla
//     stessa radio si contendono porta CAT e scheda audio, e il client finisce
//     per collegarsi a quello sbagliato senza accorgersene.
//   * si chiude solo il gateway avviato da qui. Quello lanciato a mano
//     dall'operatore, o da un'altra istanza, resta dov'è.
//   * il gateway riceve il nostro identificatore di processo e si spegne da sé
//     se ci perde di vista. Vale anche quando DecoRTTY va in crash, ed è il caso
//     che conta: un gateway orfano resterebbe con il PTT premuto.
#pragma once

#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>

namespace decortty::app {

class GatewaySupervisor : public QObject {
    Q_OBJECT

    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY configChanged)
    Q_PROPERTY(QString catPort READ catPort WRITE setCatPort NOTIFY configChanged)
    Q_PROPERTY(QString audioIn READ audioIn WRITE setAudioIn NOTIFY configChanged)
    Q_PROPERTY(QString audioOut READ audioOut WRITE setAudioOut NOTIFY configChanged)
    Q_PROPERTY(int udpPort READ udpPort WRITE setUdpPort NOTIFY configChanged)
    Q_PROPERTY(bool autoConnect READ autoConnect WRITE setAutoConnect NOTIFY configChanged)

    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(bool running READ isRunning NOTIFY statusChanged)
    // Vero quando il gateway in ascolto è stato avviato da noi: solo quello
    // verrà fermato alla chiusura.
    Q_PROPERTY(bool ours READ ours NOTIFY statusChanged)
    // Il gateway puo' essere in piedi e servire audio pur senza la porta CAT,
    // se un altro programma l'ha aperta prima. In quel caso non si sintonizza e
    // non si trasmette, e il pannello non deve mostrare un verde tranquillo.
    Q_PROPERTY(bool catOnline READ catOnline NOTIFY statusChanged)
    Q_PROPERTY(bool available READ available CONSTANT)

    // Riempiti da refreshDevices(), che li chiede al gateway stesso.
    Q_PROPERTY(QStringList captureDevices READ captureDevices NOTIFY devicesChanged)
    Q_PROPERTY(QStringList playbackDevices READ playbackDevices NOTIFY devicesChanged)
    Q_PROPERTY(QStringList serialPorts READ serialPorts NOTIFY devicesChanged)

public:
    explicit GatewaySupervisor(QObject* parent = nullptr);
    ~GatewaySupervisor() override;

    bool    enabled() const     { return m_enabled; }
    QString catPort() const     { return m_catPort; }
    QString audioIn() const     { return m_audioIn; }
    QString audioOut() const    { return m_audioOut; }
    int     udpPort() const     { return m_udpPort; }
    bool    autoConnect() const { return m_autoConnect; }
    QString status() const      { return m_status; }
    bool    isRunning() const;
    bool    ours() const        { return m_ours; }
    bool    catOnline() const   { return m_catOnline; }
    bool    available() const;

    QStringList captureDevices() const  { return m_captureDevices; }
    QStringList playbackDevices() const { return m_playbackDevices; }
    QStringList serialPorts() const     { return m_serialPorts; }

    void setEnabled(bool on);
    void setCatPort(const QString& port);
    void setAudioIn(const QString& name);
    void setAudioOut(const QString& name);
    void setUdpPort(int port);
    void setAutoConnect(bool on);
    void setStationName(const QString& name) { m_stationName = name; }
    void setCallsign(const QString& call)    { m_callsign = call; }

    // Avvia il gateway, a meno che uno non stia già rispondendo sulla rete.
    // `alreadyPresent` viene dal discovery: è l'unica prova affidabile che un
    // gateway sia in ascolto, perché su Windows due processi possono legarsi
    // alla stessa porta UDP senza che nessuno dei due se ne accorga.
    Q_INVOKABLE void startUnlessPresent(bool alreadyPresent);
    // Ferma il gateway, ma solo se l'abbiamo avviato noi.
    Q_INVOKABLE void stop();
    Q_INVOKABLE void restart();
    // Chiede al gateway l'elenco dei dispositivi audio e delle porte seriali.
    Q_INVOKABLE void refreshDevices();

    // Il percorso dell'eseguibile del gateway, accanto al nostro.
    static QString gatewayExecutable();

signals:
    void configChanged();
    void statusChanged();
    void devicesChanged();
    void log(const QString& line);
    // Il gateway è partito ed è ora ragionevole cercarlo nel discovery.
    void gatewayStarted();

private:
    void setStatus(const QString& text);
    void readProcessOutput();

    QProcess*   m_process{nullptr};
    QProcess*   m_probe{nullptr};
    bool        m_enabled{true};
    bool        m_autoConnect{true};
    bool        m_ours{false};
    bool        m_catOnline{false};
    QString     m_catPort{QStringLiteral("COM5")};
    QString     m_audioIn{QStringLiteral("USB Audio CODEC")};
    QString     m_audioOut{QStringLiteral("USB Audio CODEC")};
    int         m_udpPort{4993};
    QString     m_stationName;
    QString     m_callsign;
    QString     m_status;
    QStringList m_captureDevices;
    QStringList m_playbackDevices;
    QStringList m_serialPorts;
};

} // namespace decortty::app
