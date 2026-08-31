// DecoRTTY — FlexRadio discovery listener.
//
// Every SmartSDR radio broadcasts a VITA-49 packet once a second to UDP 4992.
// Its payload is plain ASCII key=value text, so no CAT port, no vendor library
// and no configuration is needed to find a radio on the LAN — which is the whole
// point of talking to the radio natively.
#pragma once

#include <QDateTime>
#include <QHostAddress>
#include <QMap>
#include <QObject>
#include <QTimer>
#include <QUdpSocket>

namespace decortty::flex {

struct RadioInfo {
    QString      model;        // "FLEX-6600M"
    QString      serial;
    QString      version;      // firmware, e.g. "4.2.20.41343"
    QString      nickname;
    QString      callsign;
    QHostAddress address;
    quint16      port{4992};
    bool         inUse{false};
    bool         multiFlex{true};
    // True when the beacon came from a DecoRTTY gateway rather than a real
    // FlexRadio: same discovery format, different control protocol.
    bool         isGateway{false};
    QStringList  guiClients;   // station names of GUI clients already connected
    QDateTime    lastSeen;

    bool isValid() const { return !serial.isEmpty() && !address.isNull(); }
    QString displayName() const;
};

class FlexDiscovery : public QObject {
    Q_OBJECT

public:
    static constexpr quint16 kDiscoveryPort = 4992;
    // A radio that has missed five broadcasts is treated as gone.
    static constexpr int kStaleMs = 6000;

    explicit FlexDiscovery(QObject* parent = nullptr);

    bool start();
    void stop();
    bool listening() const { return m_socket.state() == QAbstractSocket::BoundState; }

    QList<RadioInfo> radios() const { return m_radios.values(); }

signals:
    void radioFound(const RadioInfo& radio);
    void radioUpdated(const RadioInfo& radio);
    void radioLost(const QString& serial);
    void errorOccurred(const QString& message);

private slots:
    void onDatagrams();
    void onStaleCheck();

private:
    static RadioInfo parsePayload(const QByteArray& payload);

    QUdpSocket             m_socket;
    QTimer                 m_staleTimer;
    QMap<QString, RadioInfo> m_radios;   // keyed by serial
};

} // namespace decortty::flex
