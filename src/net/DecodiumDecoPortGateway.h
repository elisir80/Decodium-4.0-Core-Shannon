// DecoPort — il lato gateway: mette in rete la radio che trova, senza chiedere
// a nessuno quale sia.
//
// Protocollo: doc/DECOPORT_PROTOCOL.md.
//
// Il gateway non conosce nessun dialetto e non conosce nemmeno il backend CAT.
// Per sapere COSA c'e' attaccato usa DecodiumRigDetector, che legge l'identita'
// USB delle porte e le abbina alle schede audio dello stesso apparato. Per
// controllarlo usa i ganci che gli passa chi lo ospita: cosi' funziona con il
// backend nativo, con Hamlib, con OmniRig o con TCI senza saperlo — e chi lo
// ospita non deve cambiare nulla qui quando cambia backend.
//
// Non apre e non configura il CAT da solo. La politica di connessione — quale
// profilo, quando riconnettere — appartiene all'applicazione, e aprire una
// seconda volta la stessa porta seriale e' il modo sicuro per non aprirla.
#pragma once

#include "DecoPortPacket.h"

#include <QByteArray>
#include <QHash>
#include <QHostAddress>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QVector>

#include <functional>

class QTimer;
class QUdpSocket;

class DecodiumDecoPortGateway : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool running READ running NOTIFY runningChanged)
    Q_PROPERTY(QString rigLabel READ rigLabel NOTIFY radioChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(int clientCount READ clientCount NOTIFY clientsChanged)
    Q_PROPERTY(int sessionPort READ sessionPort NOTIFY runningChanged)
    // Su quali indirizzi ci si puo' collegare a questa radio. Serve a chi sta
    // davanti al PC di stazione per sapere cosa scrivere sull'altra macchina.
    Q_PROPERTY(QString primaryAddress READ primaryAddress NOTIFY runningChanged)
    Q_PROPERTY(QStringList addresses READ addresses NOTIFY runningChanged)

public:
    // I ganci verso la radio. Tutti facoltativi: quelli non forniti fanno
    // semplicemente si' che il gateway si dichiari senza controllo.
    struct RigHooks {
        std::function<bool()>                connected;
        std::function<double()>              frequencyHz;
        std::function<QString()>             modeName;     // nome del modo dell'applicazione
        std::function<bool()>                pttActive;
        std::function<bool()>                canTransmit;
        std::function<void(double)>          setFrequencyHz;
        std::function<void(const QString&)>  setModeName;
        std::function<void(bool)>            setPtt;
    };

    explicit DecodiumDecoPortGateway(QObject* parent = nullptr);
    ~DecodiumDecoPortGateway() override;

    void setRigHooks(RigHooks hooks) { m_hooks = std::move(hooks); }

    // Senza chiave il gateway NON si accende. Pubblicare una radio in chiaro
    // vuol dire lasciare che chiunque sulla rete la sintonizzi e ascolti; una
    // porta senza serratura non e' una porta.
    void setAuthKey(const QByteArray& key) { m_authKey = key; }
    bool hasAuthKey() const { return !m_authKey.isEmpty(); }

    bool    running() const { return m_running; }
    QString rigLabel() const { return m_rigLabel; }
    QString status() const { return m_status; }
    int     clientCount() const { return m_clients.size(); }
    int     sessionPort() const { return m_sessionPort; }
    QStringList addresses() const;
    QString primaryAddress() const;

    // Cosa ha trovato il rilevamento: nome, porta, schede audio, quanto e'
    // sicuro e su cosa si basa. Solo lettura, nessun effetto sulla radio.
    Q_INVOKABLE QVariantMap detectedRadio() const { return m_detected; }
    Q_INVOKABLE void refreshDetection();

    Q_INVOKABLE bool start(int sessionPort = decoport::kSessionPort);
    Q_INVOKABLE void stop();

    // La frequenza di campionamento non si presume: la dichiara chi fornisce
    // l'audio, e il gateway la pubblica nel contesto. Decodium consegna 12 kHz
    // (il suo sink decima di 4 da 48), che per un modem e' tutto il passabanda
    // che serve a un quarto della banda di rete.
    void setAudioFormat(quint32 sampleRate, quint8 channels);

    // Audio RX in ingresso dal rubinetto che l'applicazione ha gia': niente
    // secondo QAudioSource sulla stessa scheda. I campioni arrivano a pezzi di
    // dimensione qualsiasi; qui vengono ricuciti in frame regolari da 10 ms.
    void pushRxAudio(const QVector<short>& samples, quint64 captureTsNs);

signals:
    void runningChanged();
    void radioChanged();
    void statusChanged();
    void clientsChanged();

    // Il gateway consegna i campioni TX quando e' arrivato il loro istante di
    // riproduzione, non quando sono arrivati dalla rete. E' tutta qui la
    // ragione dei timestamp: il jitter di rete finisce in un buffer invece che
    // nell'allineamento allo slot.
    void txAudioDue(const QVector<short>& samples);
    void txKeyRequested(bool on);

private slots:
    void onSessionDatagrams();
    void onAnnounceTick();
    void onContextTick();
    void onPlayoutTick();

private:
    struct Client {
        QHostAddress address;
        quint16      port {0};
        qint64       lastSeenMs {0};
        quint32      rxSequence {0};
    };
    struct PendingTx {
        quint64        dueNs {0};
        QVector<short> samples;
    };

    static QString clientKey(const QHostAddress& addr, quint16 port);

    void setStatus(const QString& s);
    decoport::Context buildContext() const;
    void sendTo(const Client& c, decoport::Type type, const QByteArray& payload, quint64 tsNs);
    void broadcastToClients(decoport::Type type, const QByteArray& payload, quint64 tsNs);
    void handleCommand(const decoport::Header& h, const QByteArray& payload,
                       const QHostAddress& from, quint16 fromPort);
    void handleAudioTx(const decoport::Header& h, const QByteArray& payload);
    void touchClient(const QHostAddress& addr, quint16 port);
    void reapClients(qint64 nowMs);
    // Vero se questo mittente e' in castigo per firme sbagliate.
    bool isBlocked(const QString& key, qint64 nowMs) const;
    void noteAuthFailure(const QString& key, const QHostAddress& addr, qint64 nowMs);

    RigHooks   m_hooks;
    QByteArray m_authKey;

    // Tentativi falliti per mittente. Non e' un firewall: serve a rendere
    // inutile provare le password a raffica su una rete che gia' raggiunge il
    // gateway, e a lasciarne traccia nel log.
    struct AuthFailures {
        int    count {0};
        qint64 windowStartMs {0};
        qint64 blockedUntilMs {0};
    };
    QHash<QString, AuthFailures> m_authFailures;

    QUdpSocket* m_session {nullptr};
    QUdpSocket* m_announce {nullptr};
    QTimer*     m_announceTimer {nullptr};
    QTimer*     m_contextTimer {nullptr};
    QTimer*     m_playoutTimer {nullptr};

    QHash<QString, Client> m_clients;
    QVector<PendingTx>     m_txQueue;

    bool     m_running {false};
    int      m_sessionPort {decoport::kSessionPort};
    quint32  m_streamId {0};
    quint32  m_announceSeq {0};
    quint32  m_contextSeq {0};
    quint32  m_statusSeq {0};

    QString     m_rigLabel;
    QString     m_status;
    QVariantMap m_detected;
    QString     m_audioInputName;
    QString     m_audioOutputName;

    quint32 m_sampleRate {decoport::kDefaultSampleRate};
    quint8  m_channels {1};
    quint16 m_txAudioLeadMs {200};

    QVector<short> m_rxAccum;      // resto fra un frame e il successivo

    // Solo per la diagnosi: quanti frame TX sono arrivati dopo il loro momento.
    qint64 m_txLateFrames {0};
    qint64 m_txPlayedFrames {0};
    qint64 m_rxFramesSent {0};
};
