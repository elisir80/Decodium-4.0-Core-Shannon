#include "flex/FlexApiClient.h"

#include "flex/FlexTrace.h"

#include <QUuid>

namespace decortty::flex {

QMap<QString, QString> parseKeyValues(const QString& body)
{
    QMap<QString, QString> kvs;
    for (const QString& token : body.split(QLatin1Char(' '), Qt::SkipEmptyParts)) {
        const int eq = token.indexOf(QLatin1Char('='));
        if (eq > 0)
            kvs.insert(token.left(eq), token.mid(eq + 1));
    }
    return kvs;
}

FlexApiClient::FlexApiClient(QObject* parent)
    : QObject(parent)
{
    connect(&m_socket, &QTcpSocket::connected,    this, &FlexApiClient::onConnected);
    connect(&m_socket, &QTcpSocket::disconnected, this, &FlexApiClient::onDisconnected);
    connect(&m_socket, &QTcpSocket::readyRead,    this, &FlexApiClient::onReadyRead);
    connect(&m_socket, &QTcpSocket::errorOccurred, this, &FlexApiClient::onSocketError);

    // The radio drops a client it has not heard from. A ping every 5 s is what
    // SmartSDR itself uses.
    m_keepalive.setInterval(5000);
    connect(&m_keepalive, &QTimer::timeout, this, &FlexApiClient::onKeepalive);

    m_clientId = QUuid::createUuid().toString(QUuid::WithoutBraces);
}

FlexApiClient::~FlexApiClient()
{
    if (m_socket.state() != QAbstractSocket::UnconnectedState)
        m_socket.abort();
}

void FlexApiClient::setState(LinkState state)
{
    if (m_state == state)
        return;
    m_state = state;
    emit stateChanged(state);
}

void FlexApiClient::connectToRadio(const RadioInfo& radio)
{
    connectToHost(radio.address, radio.port);
}

void FlexApiClient::connectToHost(const QHostAddress& address, quint16 port)
{
    disconnectFromRadio();
    traceNote(QStringLiteral("mi collego a %1:%2 come \"%3\", ruolo %4")
                  .arg(address.toString())
                  .arg(port)
                  .arg(m_stationName)
                  .arg(m_role == Role::Gui     ? QStringLiteral("stazione GUI")
                     : m_role == Role::Bound   ? QStringLiteral("client secondario")
                                               : QStringLiteral("da se'")));
    setState(LinkState::Connecting);
    m_socket.connectToHost(address, port);
}

void FlexApiClient::disconnectFromRadio()
{
    m_keepalive.stop();
    m_pending.clear();
    m_buffer.clear();
    m_handle = 0;
    if (m_socket.state() != QAbstractSocket::UnconnectedState) {
        m_socket.disconnectFromHost();
        if (m_socket.state() != QAbstractSocket::UnconnectedState)
            m_socket.waitForDisconnected(500);
    }
    setState(LinkState::Disconnected);
}

quint32 FlexApiClient::send(const QString& command, Reply reply)
{
    if (m_socket.state() != QAbstractSocket::ConnectedState)
        return 0;

    const quint32 seq = ++m_sequence;
    if (reply)
        m_pending.insert(seq, std::move(reply));

    const QString line = QStringLiteral("C%1|%2\n").arg(seq).arg(command);
    m_socket.write(line.toUtf8());
    // Il ping ogni cinque secondi non si registra: riempirebbe il file di
    // battiti fra le poche righe che contano.
    if (command != QLatin1String("ping"))
        traceSent(QStringLiteral("C%1|%2").arg(seq).arg(command));
    return seq;
}

void FlexApiClient::onConnected()
{
    setState(LinkState::Handshaking);
    // Nothing is sent yet: the radio speaks first with V and H lines, and the
    // handshake starts once the handle has been assigned.
}

void FlexApiClient::onDisconnected()
{
    traceNote(QStringLiteral("la radio ha chiuso il collegamento"));
    m_keepalive.stop();
    m_pending.clear();
    m_handle = 0;
    setState(LinkState::Disconnected);
    emit disconnected();
}

void FlexApiClient::onSocketError(QAbstractSocket::SocketError)
{
    traceNote(QStringLiteral("errore della socket: %1").arg(m_socket.errorString()));
    emit errorOccurred(m_socket.errorString());
    if (m_state == LinkState::Connecting || m_state == LinkState::Handshaking)
        setState(LinkState::Failed);
}

void FlexApiClient::onKeepalive()
{
    send(QStringLiteral("ping"));
}

void FlexApiClient::onReadyRead()
{
    m_buffer.append(m_socket.readAll());

    int newline;
    while ((newline = m_buffer.indexOf('\n')) >= 0) {
        const QString line = QString::fromUtf8(m_buffer.left(newline)).trimmed();
        m_buffer.remove(0, newline + 1);
        if (!line.isEmpty())
            processLine(line);
    }
}

void FlexApiClient::processLine(const QString& line)
{
    // Gli stati dei meter non passano di qui — viaggiano in UDP — quindi quello
    // che si registra e' tutto traffico che conta.
    traceRecv(line);

    const QChar kind = line.at(0);
    const QString rest = line.mid(1);

    switch (kind.toLatin1()) {
    case 'V':
        m_version = rest;
        break;

    case 'H':
        m_handle = rest.toUInt(nullptr, 16);
        runHandshake();
        break;

    case 'R': {
        // R<seq>|<hex result>|<body>
        const int bar1 = rest.indexOf(QLatin1Char('|'));
        if (bar1 < 0)
            break;
        const quint32 seq = rest.left(bar1).toUInt();
        const int bar2 = rest.indexOf(QLatin1Char('|'), bar1 + 1);
        const QString codeText = bar2 > 0 ? rest.mid(bar1 + 1, bar2 - bar1 - 1)
                                          : rest.mid(bar1 + 1);
        const QString body = bar2 > 0 ? rest.mid(bar2 + 1) : QString();
        const int code = codeText.toInt(nullptr, 16);

        const auto handler = m_pending.take(seq);
        if (handler)
            handler(code, body);
        break;
    }

    case 'S': {
        // S<handle>|<object> <k=v> …
        const int bar = rest.indexOf(QLatin1Char('|'));
        if (bar < 0)
            break;
        const QString body = rest.mid(bar + 1);

        // The object name can be one or two tokens ("slice 0", "transmit"),
        // which is settled by whether the second token carries an '='.
        const QStringList parts = body.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (parts.isEmpty())
            break;

        int nameTokens = 1;
        if (parts.size() > 1 && !parts.at(1).contains(QLatin1Char('=')))
            nameTokens = 2;

        const QString object = QStringList(parts.mid(0, nameTokens)).join(QLatin1Char(' '));
        const QString kvBody = QStringList(parts.mid(nameTokens)).join(QLatin1Char(' '));
        const QMap<QString, QString> kvs = parseKeyValues(kvBody);
        // Prima di passarlo oltre: se e' un client che si presenta, puo' essere
        // la stazione GUI a cui legarsi.
        noteClientStatus(object, kvs);
        emit statusReceived(object, kvs);
        break;
    }

    case 'M': {
        // M<8 hex digits>|<text> — the top two bits of the number are severity.
        const int bar = rest.indexOf(QLatin1Char('|'));
        if (bar < 0)
            break;
        const quint32 code = rest.left(bar).toUInt(nullptr, 16);
        const quint32 severity = (code >> 30) & 0x03u;
        emit radioMessage(rest.mid(bar + 1), severity >= 2);
        break;
    }

    default:
        break;
    }
}

void FlexApiClient::runHandshake()
{
    // L'ordine conta e ricalca quello di FlexLib: prima ci si presenta, poi ci
    // si registra, poi ci si abbona. Registrarsi prima di presentarsi fa
    // ignorare alla radio tutto quello che viene dopo.
    send(QStringLiteral("client program DecoRTTY"));

    m_registered   = false;
    m_boundStation.clear();

    if (m_role == Role::Gui) {
        registerAsGui();
        return;
    }

    // Ci si abbona subito all'elenco dei client: e' da li' che si scopre se una
    // stazione GUI e' gia' collegata. La risposta arriva in pochi millisecondi
    // se c'e' qualcuno.
    send(QStringLiteral("sub client all"));

    // Se entro questo tempo nessuna stazione GUI si e' fatta viva, vuol dire che
    // la radio e' libera e ci si prende il ruolo GUI. Due secondi: la radio
    // risponde molto prima, e l'attesa si paga una volta sola all'avvio.
    m_roleTimer.setSingleShot(true);
    m_roleTimer.setInterval(2000);
    disconnect(&m_roleTimer, nullptr, this, nullptr);
    connect(&m_roleTimer, &QTimer::timeout, this, [this] {
        if (m_registered)
            return;
        if (m_role == Role::Bound) {
            // Richiesto esplicitamente il ruolo secondario e non c'e' nessuno a
            // cui legarsi: si prosegue lo stesso, con quel che si vede.
            subscribeEverything();
            return;
        }
        registerAsGui();
    });
    m_roleTimer.start();
}

void FlexApiClient::registerAsGui()
{
    if (m_registered)
        return;
    m_registered = true;
    m_roleTimer.stop();
    traceNote(QStringLiteral("mi registro come stazione GUI (client_id %1)").arg(m_clientId));

    send(QStringLiteral("client gui %1").arg(m_clientId),
         [this](int code, const QString& body) {
             if (code != 0) {
                 emit errorOccurred(tr("The radio refused the client registration (code 0x%1)")
                                        .arg(code, 0, 16));
                 setState(LinkState::Failed);
                 return;
             }
             if (!body.trimmed().isEmpty())
                 m_clientId = body.trimmed();

             send(QStringLiteral("client station %1").arg(m_stationName));
             subscribeEverything();
         });
}

void FlexApiClient::bindTo(const QString& clientId, const QString& station)
{
    if (m_registered)
        return;
    m_registered   = true;
    m_boundStation = station;
    m_roleTimer.stop();
    traceNote(QStringLiteral("mi lego alla stazione \"%1\" (client_id %2)")
                  .arg(station, clientId));

    // Legarsi a una stazione GUI: da qui in avanti le slice che la radio ci
    // mostra sono le sue, cioe' quelle che l'operatore vede in SmartSDR. E'
    // esattamente quello che serve a un decodificatore — leggere dove l'altro e'
    // sintonizzato — e non costa un posto MultiFlex.
    send(QStringLiteral("client bind client_id=%1").arg(clientId),
         [this, station](int code, const QString&) {
             if (code != 0) {
                 // Il legame non e' andato: meglio lavorare da soli che restare
                 // a meta'.
                 m_registered = false;
                 m_boundStation.clear();
                 registerAsGui();
                 return;
             }
             subscribeEverything();
             emit boundToStation(station);
         });
}

void FlexApiClient::subscribeEverything()
{
    send(QStringLiteral("keepalive enable"));

    // Slice e trasmissione portano tutto quello che serve al RTTY (frequenza,
    // modo, filtro, stato del PTT); meter da' l'S-meter.
    send(QStringLiteral("sub slice all"));
    send(QStringLiteral("sub tx all"));
    send(QStringLiteral("sub meter all"));
    send(QStringLiteral("sub audio all"));
    // Lo stato dei flussi: e' da qui che si sa quale identificativo ha preso
    // il canale DAX, senza aspettare la risposta al comando.
    send(QStringLiteral("sub dax all"));
    send(QStringLiteral("sub audio_stream all"));
    send(QStringLiteral("sub radio all"));

    m_keepalive.start();
    setState(LinkState::Connected);
    emit connected();
}

void FlexApiClient::noteClientStatus(const QString& object,
                                     const QMap<QString, QString>& kvs)
{
    if (m_registered || m_role == Role::Gui)
        return;
    if (!object.startsWith(QLatin1String("client ")))
        return;
    // Solo i client che si sono appena collegati, non quelli che se ne vanno.
    if (kvs.contains(QStringLiteral("disconnected")))
        return;

    // Una stazione GUI si riconosce dall'identificatore di client: i client
    // secondari non ne hanno uno.
    const QString id = kvs.value(QStringLiteral("client_id"));
    if (id.isEmpty())
        return;
    // Non ci si lega a se stessi.
    if (!m_clientId.isEmpty() && id == m_clientId)
        return;

    const QString program = kvs.value(QStringLiteral("program"));
    if (program == QLatin1String("DecoRTTY"))
        return;

    const QString station = kvs.value(QStringLiteral("station"),
                                      program.isEmpty() ? tr("another station") : program);
    bindTo(id, station);
}

} // namespace decortty::flex
