#include "flex/FlexDiscovery.h"

#include "flex/VitaPacket.h"

#include <QNetworkDatagram>

namespace decortty::flex {

QString RadioInfo::displayName() const
{
    QString name = nickname.isEmpty() ? model : nickname;
    if (!callsign.isEmpty())
        name += QStringLiteral(" · ") + callsign;
    return name;
}

FlexDiscovery::FlexDiscovery(QObject* parent)
    : QObject(parent)
{
    connect(&m_socket, &QUdpSocket::readyRead, this, &FlexDiscovery::onDatagrams);
    connect(&m_staleTimer, &QTimer::timeout, this, &FlexDiscovery::onStaleCheck);
    m_staleTimer.setInterval(2000);
}

bool FlexDiscovery::start()
{
    if (listening())
        return true;

    // ShareAddress matters: SmartSDR, DAX or another client may already hold the
    // port, and refusing to coexist with them would be a needless restriction.
    const bool ok = m_socket.bind(QHostAddress::AnyIPv4, kDiscoveryPort,
                                  QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint);
    if (!ok) {
        emit errorOccurred(tr("Cannot listen on UDP port %1: %2")
                               .arg(kDiscoveryPort)
                               .arg(m_socket.errorString()));
        return false;
    }
    m_staleTimer.start();
    return true;
}

void FlexDiscovery::stop()
{
    m_staleTimer.stop();
    m_socket.close();
    const QStringList serials = m_radios.keys();
    m_radios.clear();
    for (const QString& serial : serials)
        emit radioLost(serial);
}

RadioInfo FlexDiscovery::parsePayload(const QByteArray& payload)
{
    RadioInfo info;

    // Values may be wrapped in the radio's own quoting, and list fields use a
    // non-breaking space as the separator so the top-level split on spaces stays
    // valid. Strip both.
    const auto clean = [](QString v) {
        v.remove(QLatin1Char('"'));
        v.replace(QChar(0x00A0), QLatin1Char(' '));
        return v.trimmed();
    };

    const QString text = QString::fromUtf8(payload).trimmed();
    for (const QString& token : text.split(QLatin1Char(' '), Qt::SkipEmptyParts)) {
        const int eq = token.indexOf(QLatin1Char('='));
        if (eq <= 0)
            continue;
        const QString key   = token.left(eq).toLower();
        const QString value = clean(token.mid(eq + 1));

        if (key == QLatin1String("model"))              info.model     = value;
        else if (key == QLatin1String("serial"))        info.serial    = value;
        else if (key == QLatin1String("version"))       info.version   = value;
        else if (key == QLatin1String("nickname"))      info.nickname  = value;
        else if (key == QLatin1String("callsign"))      info.callsign  = value;
        else if (key == QLatin1String("ip"))            info.address   = QHostAddress(value);
        else if (key == QLatin1String("port"))          info.port      = value.toUShort();
        else if (key == QLatin1String("inuse"))         info.inUse     = (value == QLatin1String("1"));
        else if (key == QLatin1String("mf_enable"))     info.multiFlex = (value != QLatin1String("0"));
        else if (key == QLatin1String("gateway"))
            info.isGateway = (value.compare(QLatin1String("decortty"), Qt::CaseInsensitive) == 0);
        else if (key == QLatin1String("gui_client_stations"))
            info.guiClients = value.split(QLatin1Char(','), Qt::SkipEmptyParts);
    }
    return info;
}

void FlexDiscovery::onDatagrams()
{
    while (m_socket.hasPendingDatagrams()) {
        const QNetworkDatagram dgram = m_socket.receiveDatagram();
        const QByteArray data = dgram.data();

        VitaPacket packet;
        QByteArray payload;
        if (parseVitaPacket(data, packet) && packet.payloadBytes > 0) {
            payload = QByteArray(reinterpret_cast<const char*>(packet.payload), packet.payloadBytes);
        } else {
            // Some firmware revisions send the discovery text without a
            // well-formed VITA wrapper. The text is self-describing, so fall
            // back to scanning the whole datagram rather than dropping a radio.
            payload = data;
        }

        RadioInfo info = parsePayload(payload);
        if (!info.isValid()) {
            // The broadcast omits the address on some builds; the sender is
            // authoritative anyway.
            if (!info.serial.isEmpty() && !dgram.senderAddress().isNull())
                info.address = dgram.senderAddress();
            if (!info.isValid())
                continue;
        }
        info.lastSeen = QDateTime::currentDateTimeUtc();

        const auto existing = m_radios.constFind(info.serial);
        if (existing == m_radios.constEnd()) {
            m_radios.insert(info.serial, info);
            emit radioFound(info);
        } else {
            const RadioInfo previous = *existing;
            m_radios.insert(info.serial, info);
            // Only signal a change the UI would care about; the broadcast
            // arrives every second and a blind emit would repaint constantly.
            if (previous.inUse != info.inUse
                || previous.address != info.address
                || previous.nickname != info.nickname
                || previous.callsign != info.callsign
                || previous.version != info.version
                || previous.guiClients != info.guiClients) {
                emit radioUpdated(info);
            }
        }
    }
}

void FlexDiscovery::onStaleCheck()
{
    const QDateTime now = QDateTime::currentDateTimeUtc();
    for (auto it = m_radios.begin(); it != m_radios.end();) {
        if (it->lastSeen.msecsTo(now) > kStaleMs) {
            const QString serial = it.key();
            it = m_radios.erase(it);
            emit radioLost(serial);
        } else {
            ++it;
        }
    }
}

} // namespace decortty::flex
