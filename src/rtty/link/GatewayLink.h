// DecoRTTY — the client side of the FT-991A gateway.
//
// Everything travels over one UDP socket: audio and context arrive as VITA-49
// packets, control leaves as VITA-49 command packets. There is no session to set
// up beyond announcing ourselves — the gateway starts sending to whoever it
// hears from, and stops when the keepalives do.
#pragma once

#include "flex/FlexDiscovery.h"
#include "link/RadioLink.h"
#include "vita/VitaContext.h"

#include <QHostAddress>
#include <QTimer>
#include <QUdpSocket>

#include <vector>

namespace decortty::link {

class GatewayLink : public RadioLink {
    Q_OBJECT

public:
    explicit GatewayLink(QObject* parent = nullptr);
    ~GatewayLink() override;

    void connectToGateway(const flex::RadioInfo& info);
    void connectToAddress(const QHostAddress& address, quint16 port);

    // ── RadioLink ───────────────────────────────────────────────────────
    bool    isConnected() const override { return m_connected; }
    QString statusText() const override  { return m_statusText; }
    QString radioName() const override   { return m_radioName; }
    double  frequencyMhz() const override { return m_frequencyHz / 1e6; }
    QString mode() const override         { return m_mode; }
    bool    isTransmitting() const override { return m_transmitting; }
    // The gateway carries transmit audio uncompressed, so unlike the FlexRadio
    // path there is no codec to be missing: if the link is up, transmit is
    // available — subject to the gateway having its CAT port open, which it
    // reports through the context state bits.
    bool    canTransmit() const override { return m_connected && m_catOnline; }
    bool    canControl() const override  { return m_connected && m_catOnline; }
    int     signalStrengthDbm() const override { return m_signalDbm; }

    void disconnectRadio() override;
    void setFrequencyMhz(double mhz) override;
    void setMode(const QString& mode) override;
    void setFilter(int lowHz, int highHz) override;
    void applyRttyProfile(int markHz, int shiftHz) override;
    void setTransmit(bool on) override;
    int  sendTransmitAudio(const float* samples, int count) override;

private slots:
    void onDatagrams();
    void onKeepalive();
    void onReconnectAttempt();

private:
    void setStatusText(const QString& text);
    void sendCommand(const vita::RadioCommand& command);
    void applyContext(const vita::RadioContext& context);

    QUdpSocket   m_socket;
    QTimer       m_keepalive;
    QTimer       m_watchdog;
    // Il gateway puo' essere riavviato — per una modifica, per un aggiornamento,
    // perche' e' su un altro PC che si e' riacceso. Senza riconnessione
    // automatica l'operatore si ritrova muto senza che nulla glielo dica, e deve
    // indovinare di dover ricliccare sulla radio.
    QTimer       m_reconnect;
    bool         m_wantConnection{false};
    QHostAddress m_gatewayAddress;
    quint16      m_gatewayPort{0};

    bool    m_connected{false};
    bool    m_catOnline{false};
    bool    m_transmitting{false};
    double  m_frequencyHz{0.0};
    QString m_mode;
    QString m_radioName;
    QString m_statusText;
    int     m_signalDbm{-140};
    qint64  m_lastPacketMs{0};

    quint8 m_commandCount{0};
    quint8 m_audioCount{0};
    // Numero d'ordine del prossimo campione, per il timestamp
    // frazionario che l'intestazione dichiara.
    quint64 m_audioSamples{0};

    std::vector<float> m_rxStereo;
    QByteArray         m_txPayload;
};

} // namespace decortty::link
