// DecoRTTY — SmartSDR command channel (TCP 4992).
//
// This is the whole of "CAT" in DecoRTTY. The radio's native API carries
// frequency, mode, filter, PTT and stream management as text commands over one
// TCP connection, so there is no serial port, no virtual COM pair and no rig
// back end to configure — and unlike CAT it also reports state changes made by
// other clients.
//
// Wire protocol:
//   → C<seq>|<command>            client command
//   ← R<seq>|<hex code>|<body>    response to that command
//   ← V<version>                  protocol version, sent on connect
//   ← H<handle>                   the handle this client was assigned
//   ← S<handle>|<object> <k=v>…   asynchronous status
//   ← M<code>|<text>              log / error message
#pragma once

#include "flex/FlexDiscovery.h"

#include <QByteArray>
#include <QHash>
#include <QMap>
#include <QObject>
#include <QTcpSocket>
#include <QTimer>

#include <functional>

namespace decortty::flex {

enum class LinkState { Disconnected, Connecting, Handshaking, Connected, Failed };

class FlexApiClient : public QObject {
    Q_OBJECT

public:
    using Reply = std::function<void(int code, const QString& body)>;

    explicit FlexApiClient(QObject* parent = nullptr);
    ~FlexApiClient() override;

    LinkState state() const       { return m_state; }
    bool      isConnected() const { return m_state == LinkState::Connected; }
    quint32   clientHandle() const { return m_handle; }
    QString   radioVersion() const { return m_version; }
    QHostAddress localAddress() const { return m_socket.localAddress(); }

    // The station name other MultiFlex clients see, and the client id the radio
    // uses to restore this client's slices across sessions. Set before connect.
    void setStationName(const QString& name) { m_stationName = name; }
    void setClientId(const QString& uuid)    { m_clientId = uuid; }
    QString clientId() const { return m_clientId; }

    // Come ci si presenta alla radio.
    //
    // Un client GUI possiede le proprie slice e occupa uno dei posti MultiFlex,
    // che su un FLEX-6000 sono due. DecoRTTY non ne ha bisogno: non crea slice,
    // legge quella su cui l'operatore e' gia' sintonizzato e comanda il PTT. Se
    // si dichiarasse GUI mentre SmartSDR e' aperto, occuperebbe il secondo posto
    // — e su un apparato senza licenza MultiFlex verrebbe rifiutato del tutto.
    //
    // Percio' si guarda prima chi c'e': se una stazione GUI e' gia' collegata,
    // ci si registra come client secondario e ci si lega a quella, vedendo le
    // sue slice. Se non c'e' nessuno, si prende il ruolo GUI e si lavora da
    // soli.
    enum class Role {
        Auto,     // secondario se c'e' gia' una GUI, altrimenti GUI
        Gui,      // sempre GUI, come faceva prima
        Bound,    // sempre secondario, anche senza nessuno a cui legarsi
    };
    void setRole(Role role) { m_role = role; }
    Role role() const { return m_role; }
    // Il client GUI a cui ci si e' legati, vuoto se si lavora da soli.
    QString boundTo() const { return m_boundStation; }

    void connectToRadio(const RadioInfo& radio);
    void connectToHost(const QHostAddress& address, quint16 port = 4992);
    void disconnectFromRadio();

    // Send a command. The reply callback, when given, fires with the radio's
    // result code (0 = success) and response body.
    quint32 send(const QString& command, Reply reply = {});

signals:
    void stateChanged(LinkState state);
    // Ci si e' legati a una stazione GUI gia' presente: il nome e' quello che
    // l'operatore vede in SmartSDR.
    void boundToStation(const QString& station);
    void connected();          // handshake finished; safe to create streams
    void disconnected();
    void errorOccurred(const QString& message);
    // object is e.g. "slice 0" or "transmit"; kvs holds the parsed key=value set.
    void statusReceived(const QString& object, const QMap<QString, QString>& kvs);
    void radioMessage(const QString& text, bool isError);

private slots:
    void onConnected();
    void onDisconnected();
    void onReadyRead();
    void onSocketError(QAbstractSocket::SocketError error);
    void onKeepalive();

private:
    void setState(LinkState state);
    void processLine(const QString& line);
    void runHandshake();
    // Registrazione come GUI: si usa quando non c'e' nessuno a cui legarsi.
    void registerAsGui();
    // Registrazione come secondario legato al client GUI indicato.
    void bindTo(const QString& clientId, const QString& station);
    void subscribeEverything();
    // Le stazioni GUI viste finora, dal flusso di stato dei client.
    void noteClientStatus(const QString& object, const QMap<QString, QString>& kvs);

    QTcpSocket m_socket;
    QByteArray m_buffer;
    QTimer     m_keepalive;

    LinkState m_state{LinkState::Disconnected};
    quint32   m_handle{0};
    quint32   m_sequence{0};
    QString   m_version;
    QString   m_stationName{QStringLiteral("DecoRTTY")};
    Role      m_role{Role::Auto};
    QString   m_boundStation;
    bool      m_registered{false};
    QTimer    m_roleTimer;   // attesa prima di decidere di fare da soli
    QString   m_clientId;

    QHash<quint32, Reply> m_pending;
};

// Parse a "key=value key=value" status body. Values may themselves contain '='
// (a macro, say), so only the first one separates.
QMap<QString, QString> parseKeyValues(const QString& body);

} // namespace decortty::flex
