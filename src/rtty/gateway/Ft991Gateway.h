// DecoRTTY gateway — presents an FT-991A on the network as a VITA-49 radio.
//
//   FT-991A ──USB audio──┐                      ┌── VITA-49 signal data (audio)
//                        ├── Ft991Gateway ──UDP─┤── VITA-49 context (dial state)
//   FT-991A ──USB CAT────┘                      └── VITA-49 command (control) ←
//
// The client never learns there is a serial cable involved: it discovers the
// gateway the same way it discovers a FlexRadio, receives the same audio format,
// and controls it by sending command packets that the gateway turns into Yaesu
// CAT strings. The practical payoff is that the radio becomes usable from any PC
// on the network, not only the one it is plugged into.
#pragma once

#include "gateway/CodecAudio.h"
#include "gateway/Ft991Cat.h"

#include <QDateTime>
#include <QElapsedTimer>
#include <QHostAddress>
#include <QList>
#include <QObject>
#include <QTimer>
#include <QUdpSocket>

namespace decortty::gateway {

class Ft991Gateway : public QObject {
    Q_OBJECT

public:
    // The discovery beacon goes to the same port a FlexRadio uses, so one
    // listener finds both. They stay distinguishable by the class ID: ours
    // carries DecoRTTY's locally administered organisation identifier.
    static constexpr quint16 kBeaconPort  = 4992;
    static constexpr quint16 kDefaultPort = 4993;
    // A client that has not been heard from in this long is dropped, and audio
    // stops being sent to it.
    static constexpr int kSubscriberTimeoutMs = 12000;
    // While the radio is keyed the client must keep feeding audio. If it stops —
    // it crashed, the network broke, the operator's PC froze — the transmitter
    // is unkeyed rather than left running into whatever it is connected to.
    static constexpr int kTransmitStarveMs = 1500;

    struct Config {
        QString  catPort{QStringLiteral("COM5")};
        int      catBaud{38400};
        bool     catEnabled{true};
        QString  captureHint{QStringLiteral("USB Audio CODEC")};
        QString  playbackHint{QStringLiteral("USB Audio CODEC")};
        quint16  dataPort{kDefaultPort};
        QString  stationName;
        QString  callsign;
        // Chi ci ha avviati puo' chiederci di chiudere mandando questa parola
        // sulla porta dati. Vuota, nessuno puo'.
        QString  shutdownToken;
    };

    explicit Ft991Gateway(QObject* parent = nullptr);
    ~Ft991Gateway() override;

    bool start(const Config& config);
    void stop();
    bool isRunning() const { return m_running; }

    int subscriberCount() const { return m_subscribers.size(); }

    // Chiede di chiudere. Non e' detto che accada: se al gateway restano client
    // attivi, chiudersi vorrebbe dire togliere la radio a chi la sta usando.
    // In quel caso si sgancia da chi l'ha avviato e continua a servire gli
    // altri, uscendo quando anche loro se ne sono andati.
    void requestShutdown();
    bool catOpen() const { return m_cat.isOpen(); }
    const Ft991Cat::State& radioState() const { return m_cat.state(); }

signals:
    void log(const QString& message);
    void started();
    void stopped();
    // Un ordine di spegnimento arrivato dal supervisore, gia' verificato.
    void shutdownRequested();

private slots:
    void onAudioReady(const std::vector<float>& mono);
    void onCatState(const Ft991Cat::State& state);
    void onDatagrams();
    void onBeacon();
    void onSubscriberSweep();

private:
    void sendContext(bool full);
    void decideShutdown();
    void touchSubscriber(const QHostAddress& address, quint16 port);
    void broadcastToSubscribers(const QByteArray& packet);

    struct Subscriber {
        QHostAddress address;
        quint16      port{0};
        qint64       lastSeenMs{0};
    };

    Ft991Cat   m_cat;
    CodecAudio m_audio;

    QUdpSocket m_data;
    QUdpSocket m_beaconSocket;
    // Prova ad aprire la porta seriale. Vero se da ora il CAT c'e'.
    bool tryOpenCat();

    QTimer     m_beaconTimer;
    QTimer     m_sweepTimer;
    QTimer     m_contextTimer;
    // Ribussa alla porta seriale finche' non si apre. Un altro programma che la
    // teneva puo' averla lasciata da un pezzo, e senza questo il CAT resterebbe
    // chiuso fino al prossimo riavvio del gateway — cioe' del programma intero.
    QTimer     m_catRetryTimer;

    QList<Subscriber> m_subscribers;
    Config            m_config;
    bool              m_running{false};
    // Chiusura chiesta, in attesa di sapere se qualcun altro ci sta usando.
    bool              m_shutdownPending{false};
    qint64            m_shutdownAskedMs{0};
    // Sganciato da chi ci ha avviati: si va avanti finche' c'e' qualcuno.
    bool              m_emancipated{false};
    qint64            m_aloneSinceMs{0};

    // Il livello dell'audio di trasmissione che arriva dal client, accumulato
    // fra un riepilogo e l'altro. Senza, "ricevuti 4.700.000 campioni" e' la
    // stessa riga sia che il client mandi una nota sia che mandi silenzio: la
    // catena sembra viva in tutti e due i casi, e non si sa da che parte
    // guardare.
    // Il livello dell'ultimo tratto, in parole.
    QString transmitLevelText() const;

    float  m_txPeak{0.0f};
    double m_txSumSquares{0.0};
    quint64 m_txLevelCount{0};

    quint8 m_audioCount{0};
    // Numero d'ordine del prossimo campione, per il timestamp
    // frazionario che l'intestazione dichiara.
    quint64 m_audioSamples{0};
    quint8 m_contextCount{0};
    quint8 m_beaconCount{0};

    // Transmit audio arrives as float32 stereo; unpacked into this before it
    // goes to the codec.
    std::vector<float> m_txMono;
    qint64             m_lastTxAudioMs{0};
    QElapsedTimer      m_txDiagClock;
    QByteArray         m_audioPayload;
};

} // namespace decortty::gateway
