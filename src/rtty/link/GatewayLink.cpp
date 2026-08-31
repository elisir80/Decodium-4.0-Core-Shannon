#include "link/GatewayLink.h"

#include <QDateTime>
#include <QNetworkDatagram>
#include <QtEndian>

#include <cmath>
#include <cstring>

namespace decortty::link {

using namespace decortty::vita;
using flex::VitaPacket;

namespace {

constexpr uint32_t kClientStreamId = 0x0B000001;
// Long enough that a momentary hiccup does not tear the link down, short enough
// that a gateway which has gone away is noticed before the operator wonders why
// nothing is decoding.
constexpr int kWatchdogMs = 6000;

} // namespace

GatewayLink::GatewayLink(QObject* parent)
    : RadioLink(parent)
{
    connect(&m_socket, &QUdpSocket::readyRead, this, &GatewayLink::onDatagrams);

    // The gateway sends only to clients it has heard from recently.
    m_keepalive.setInterval(2000);
    connect(&m_keepalive, &QTimer::timeout, this, &GatewayLink::onKeepalive);

    m_watchdog.setInterval(2000);
    connect(&m_watchdog, &QTimer::timeout, this, [this] {
        if (!m_connected)
            return;
        const qint64 silence = QDateTime::currentMSecsSinceEpoch() - m_lastPacketMs;
        if (silence > kWatchdogMs) {
            emit errorOccurred(tr("The gateway is not answering — retrying"));
            // Cade la connessione ma non l'intenzione: il timer di riconnessione
            // continua a bussare finche' il gateway non torna.
            const bool want = m_wantConnection;
            disconnectRadio();
            m_wantConnection = want;
            if (want) {
                setStatusText(tr("Waiting for the gateway…"));
                m_reconnect.start();
            }
        }
    });

    // Ritenta finche' l'operatore non chiude lui la connessione.
    m_reconnect.setInterval(3000);
    connect(&m_reconnect, &QTimer::timeout, this, &GatewayLink::onReconnectAttempt);

    m_statusText = tr("Disconnected");
}

void GatewayLink::onReconnectAttempt()
{
    if (!m_wantConnection || m_connected || m_gatewayAddress.isNull())
        return;
    setStatusText(tr("Reconnecting to %1…").arg(m_radioName));
    connectToAddress(m_gatewayAddress, m_gatewayPort);
}

GatewayLink::~GatewayLink()
{
    if (m_connected)
        disconnectRadio();
}

void GatewayLink::setStatusText(const QString& text)
{
    if (m_statusText == text)
        return;
    m_statusText = text;
    emit statusTextChanged();
}

void GatewayLink::connectToGateway(const flex::RadioInfo& info)
{
    m_radioName = info.displayName();
    connectToAddress(info.address, info.port);
}

void GatewayLink::connectToAddress(const QHostAddress& address, quint16 port)
{
    disconnectRadio();

    if (!m_socket.bind(QHostAddress::AnyIPv4, 0)) {
        emit errorOccurred(tr("Cannot open a local UDP port: %1").arg(m_socket.errorString()));
        return;
    }
    m_socket.setSocketOption(QAbstractSocket::ReceiveBufferSizeSocketOption, QVariant(1 << 20));

    m_gatewayAddress = address;
    m_gatewayPort    = port;
    if (m_radioName.isEmpty())
        m_radioName = address.toString();

    m_connected      = true;
    m_wantConnection = true;
    m_reconnect.stop();
    m_lastPacketMs   = QDateTime::currentMSecsSinceEpoch();
    m_keepalive.start();
    m_watchdog.start();

    // Announce ourselves. An empty command is enough: the gateway registers the
    // sender of any well-formed packet.
    onKeepalive();

    setStatusText(tr("Connected to %1").arg(m_radioName));
    emit connectionChanged();
}

void GatewayLink::disconnectRadio()
{
    // Chiamata dall'operatore: smette anche di voler essere connesso.
    m_wantConnection = false;
    m_reconnect.stop();

    if (!m_connected) {
        m_socket.close();
        return;
    }

    // Leave the radio in receive, whatever state the session was in.
    if (m_transmitting)
        setTransmit(false);

    // Un saluto prima di chiudere. Senza, il gateway continua a contarci fra i
    // suoi client per una dozzina di secondi — e in quel tempo non sa
    // distinguere chi se n'e' andato da chi sta ancora copiando, che e'
    // esattamente la differenza fra chiudersi e restare in piedi per gli altri.
    m_socket.writeDatagram(QByteArrayLiteral("DECORTTY-BYE"),
                           m_gatewayAddress, m_gatewayPort);

    m_keepalive.stop();
    m_watchdog.stop();
    m_socket.close();

    m_connected    = false;
    m_catOnline    = false;
    m_transmitting = false;
    m_frequencyHz  = 0.0;
    m_mode.clear();

    setStatusText(tr("Disconnected"));
    emit connectionChanged();
    emit sliceChanged();
    emit transmittingChanged();
}

void GatewayLink::onKeepalive()
{
    if (!m_connected)
        return;
    // A command packet with nothing set: it carries no instruction, only the
    // fact that we are still here.
    sendCommand(RadioCommand{});
}

void GatewayLink::sendCommand(const RadioCommand& command)
{
    if (!m_connected)
        return;
    const QByteArray packet = buildCommandPacket(kClientStreamId, command, m_commandCount);
    m_commandCount = static_cast<quint8>((m_commandCount + 1) & 0x0F);
    m_socket.writeDatagram(packet, m_gatewayAddress, m_gatewayPort);
}

void GatewayLink::onDatagrams()
{
    while (m_socket.hasPendingDatagrams()) {
        const QNetworkDatagram datagram = m_socket.receiveDatagram();
        const QByteArray data = datagram.data();

        VitaPacket packet;
        if (!flex::parseVitaPacket(data, packet))
            continue;
        if (packet.classId.oui != kDecoRttyOui)
            continue;

        m_lastPacketMs = QDateTime::currentMSecsSinceEpoch();

        switch (static_cast<DecoClass>(packet.classId.packetClass)) {
        case DecoClass::Audio: {
            // float32 stereo big-endian, the same layout the FlexRadio path
            // produces, so the decoder downstream needs no special case.
            const int values = packet.payloadBytes / 4;
            const int frames = values / 2;
            if (frames <= 0)
                break;
            m_rxStereo.resize(static_cast<size_t>(values));
            for (int i = 0; i < values; ++i) {
                const quint32 bits = qFromBigEndian<quint32>(packet.payload + i * 4);
                float value;
                std::memcpy(&value, &bits, sizeof(value));
                m_rxStereo[static_cast<size_t>(i)] = value;
            }
            emit audioReady(m_rxStereo, frames);
            break;
        }

        case DecoClass::RadioContext: {
            RadioContext context;
            if (parseContextPacket(packet, context))
                applyContext(context);
            break;
        }

        case DecoClass::Command: {
            // The gateway reports the operating mode this way: the standard has
            // no context field for it.
            RadioCommand report;
            if (parseCommandPacket(packet, report) && report.mode) {
                if (m_mode != *report.mode) {
                    m_mode = *report.mode;
                    emit sliceChanged();
                }
            }
            break;
        }

        default:
            break;
        }
    }
}

void GatewayLink::applyContext(const RadioContext& context)
{
    bool sliceDirty = false;

    if (context.rfFrequencyHz && !qFuzzyCompare(m_frequencyHz, *context.rfFrequencyHz)) {
        m_frequencyHz = *context.rfFrequencyHz;
        sliceDirty = true;
    }

    if (context.referenceLevelDbm) {
        const int dbm = static_cast<int>(std::lround(*context.referenceLevelDbm));
        if (dbm != m_signalDbm) {
            m_signalDbm = dbm;
            emit metersChanged();
        }
    }

    if (context.stateAndEvent) {
        const uint32_t bits = *context.stateAndEvent;

        const bool transmitting = (bits & kStateUserTransmitting) != 0;
        if (transmitting != m_transmitting) {
            m_transmitting = transmitting;
            emit transmittingChanged();
        }

        const bool catOnline = (bits & kStateUserCatOnline) != 0;
        if (catOnline != m_catOnline) {
            m_catOnline = catOnline;
            // Transmit availability follows the gateway's CAT state: without
            // CAT there is no PTT, and a transmit button that cannot key the
            // radio is worse than one that is visibly disabled.
            emit connectionChanged();
            setStatusText(catOnline
                              ? tr("Connected to %1").arg(m_radioName)
                              : tr("%1 — audio only, no CAT").arg(m_radioName));
        }
    }

    if (sliceDirty)
        emit sliceChanged();
}

void GatewayLink::setFrequencyMhz(double mhz)
{
    RadioCommand command;
    command.tuneFrequencyHz = mhz * 1e6;
    sendCommand(command);
}

void GatewayLink::setMode(const QString& mode)
{
    RadioCommand command;
    command.mode = mode;
    sendCommand(command);
}

void GatewayLink::setFilter(int lowHz, int highHz)
{
    // The FT-991A's own filter is set from its front panel or a CAT command this
    // gateway does not carry; the audio it sends is already filtered. Reporting
    // the request as bandwidth keeps the context honest without pretending to
    // have changed anything.
    RadioCommand command;
    command.bandwidthHz = static_cast<double>(highHz - lowHz);
    sendCommand(command);
}

void GatewayLink::applyRttyProfile(int markHz, int shiftHz)
{
    Q_UNUSED(markHz);
    Q_UNUSED(shiftHz);
    // DATA-USB is what AFSK RTTY needs on this radio: the USB codec audio goes
    // to the transmitter with the microphone processing out of circuit, and the
    // sideband matches what the decoder expects.
    setMode(QStringLiteral("DIGU"));
}

void GatewayLink::setTransmit(bool on)
{
    RadioCommand command;
    command.transmit = on;
    sendCommand(command);
}

int GatewayLink::sendTransmitAudio(const float* samples, int count)
{
    if (!m_connected || count <= 0)
        return 0;

    // Same wire format as the receive direction: float32 stereo big-endian.
    // Uncompressed, because the gateway is on the local network and a codec
    // would only add latency to a signal that is two tones.
    m_txPayload.resize(count * 2 * 4);
    auto* out = reinterpret_cast<quint32*>(m_txPayload.data());
    for (int i = 0; i < count; ++i) {
        quint32 bits;
        std::memcpy(&bits, &samples[i], sizeof(bits));
        const quint32 be = qToBigEndian<quint32>(bits);
        out[i * 2]     = be;
        out[i * 2 + 1] = be;
    }

    QByteArray packet = flex::buildFlexDataPacket(kClientStreamId, flex::FlexClass::DaxAudio,
                                                  m_txPayload, m_audioCount, m_audioSamples);
    m_audioCount = static_cast<quint8>((m_audioCount + 1) & 0x0F);
    m_audioSamples += static_cast<quint64>(count);

    auto* words = reinterpret_cast<quint32*>(packet.data());
    words[2] = qToBigEndian<quint32>(kDecoRttyOui);
    words[3] = qToBigEndian<quint32>((static_cast<uint32_t>(kDecoRttyInformationClass) << 16)
                                     | static_cast<uint32_t>(DecoClass::Audio));

    return m_socket.writeDatagram(packet, m_gatewayAddress, m_gatewayPort) == packet.size() ? 1 : 0;
}

} // namespace decortty::link
