// DecoRTTY — the VITA-49 data plane.
//
// One UDP socket carries every stream the radio sends this client: receive
// audio, meters, and (when asked for) panadapter bins. The radio learns where to
// send them from the `client udpport` command, which is why a single socket is
// both necessary and sufficient — there is no DAX device, no virtual audio
// cable and no second transport anywhere in the path.
#pragma once

#include "flex/OpusCodec.h"
#include "flex/VitaPacket.h"

#include <QHash>
#include <QSet>
#include <QHostAddress>
#include <QObject>
#include <QUdpSocket>

#include <vector>

namespace decortty::flex {

class FlexVitaStream : public QObject {
    Q_OBJECT

public:
    explicit FlexVitaStream(QObject* parent = nullptr);

    // Bind a local UDP port. Pass 0 to let the OS choose, which is the normal
    // case; the caller then registers boundPort() with the radio.
    bool start(quint16 preferredPort = 0);
    void stop();

    quint16 boundPort() const { return m_socket.localPort(); }
    bool    isRunning() const { return m_socket.state() == QAbstractSocket::BoundState; }

    // I pacchetti che vanno alla radio non vanno dove arrivano i comandi.
    //
    // 4992 e' l'API a comandi, in TCP, e in UDP e' la porta su cui la radio
    // grida la sua presenza. I dati VITA-49 che un client manda alla radio
    // vanno invece sulla 4991. Mandarli sulla 4992 non da' nessun errore:
    // partono, nessuno li ascolta, e la radio trasmette una portante senza
    // nota — che e' esattamente quello che si e' visto alla prima prova su un
    // apparato vero.
    static constexpr quint16 kVitaDataPort = 4991;
    void setRadioEndpoint(const QHostAddress& address, quint16 port = kVitaDataPort);

    // Only packets on these stream IDs are treated as our audio. Zero disables
    // the filter, which is what the first seconds after `stream create` need:
    // audio can start arriving before the reply carrying the ID does.
    void setAudioStreamId(quint32 id)    { m_audioStreamId = id; }
    void setTransmitStreamId(quint32 id) { m_txStreamId = id; }
    quint32 audioStreamId() const    { return m_audioStreamId; }
    quint32 transmitStreamId() const { return m_txStreamId; }

    // Send one already-encoded Opus frame as a VITA-49 packet on the transmit
    // stream. Returns false when no transmit stream has been created.
    bool sendOpusFrame(const QByteArray& opus);

    // Manda audio non compresso sul flusso di trasmissione: e' quello che vuole
    // un flusso DAX TX, e non chiede nessun codificatore.
    //
    // I campioni sono mono a 24 kHz e vengono duplicati sui due canali. La
    // radio si aspetta pacchetti di lunghezza fissa, percio' quello che avanza
    // resta qui e parte col prossimo blocco: cosi' chi chiama puo' consegnare
    // pezzi di qualunque misura. Torna quanti pacchetti sono partiti.
    int  sendPcmFrames(const float* mono, int count);
    // Butta via il residuo: si fa a fine trasmissione, perche' altrimenti le
    // ultime decine di campioni tornerebbero in testa alla trasmissione dopo.
    void flushTransmitBuffer() { m_txPending.clear(); }

    // Campioni per pacchetto sul flusso DAX. E' la misura che usa la radio;
    // pacchetti piu' corti passano, ma sprecano intestazione a ogni giro.
    static constexpr int kTxFramesPerPacket = 128;

    // ── counters, for the diagnostics panel ─────────────────────────────
    quint64 packetsReceived() const { return m_packets; }
    quint64 bytesReceived() const   { return m_bytes; }
    quint64 packetsLost() const     { return m_lost; }
    quint64 packetsSent() const     { return m_sent; }

signals:
    // Interleaved stereo float at 24 kHz, already byte-swapped and scaled.
    // `frames` is the number of stereo frames, so samples.size() == frames * 2.
    void audioReceived(const std::vector<float>& samples, int frames);
    // Opus payload, to be handed to the decoder by the owner (which holds the
    // codec, so this class stays free of the dependency).
    void opusReceived(const QByteArray& payload);
    // Radio meters: raw values keyed by meter id, still in wire units.
    void metersReceived(const QHash<quint16, qint16>& meters);
    void errorOccurred(const QString& message);

private slots:
    void onDatagrams();

private:
    void handleAudio(const VitaPacket& packet);
    void handleMeters(const VitaPacket& packet);
    void trackSequence(const VitaPacket& packet);

    QUdpSocket   m_socket;
    QHostAddress m_radioAddress;
    quint16      m_radioPort{kVitaDataPort};
    bool         m_txLogged{false};   // il primo pacchetto mandato si annota

    quint32 m_audioStreamId{0};
    quint32 m_txStreamId{0};
    quint8  m_txPacketCount{0};
    // Il numero d'ordine del prossimo campione in trasmissione. Conta per
    // canale, come il campo che riempie, e riparte solo quando riparte la
    // socket: azzerarlo a ogni pressione del PTT vorrebbe dire mandare due volte
    // gli stessi numeri d'ordine dentro lo stesso flusso.
    quint64 m_txSamples{0};

    // Le classi di pacchetto gia' viste. La prima volta che ne arriva una si
    // registra: senza, un flusso che non porta audio e un flusso che non arriva
    // affatto lasciano la stessa traccia, cioe' nessuna.
    QSet<quint16>          m_classesSeen;
    std::vector<float>     m_txPending;   // residuo mono, meno di un pacchetto
    QHash<quint32, quint8> m_lastSeq;
    quint64 m_packets{0};
    quint64 m_bytes{0};
    quint64 m_lost{0};
    quint64 m_sent{0};

    std::vector<float> m_scratch;
};

} // namespace decortty::flex
