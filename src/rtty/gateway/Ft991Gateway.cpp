#include "gateway/Ft991Gateway.h"

#include "vita/VitaContext.h"

#include <QNetworkDatagram>
#include <QtEndian>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace decortty::gateway {

using namespace decortty::vita;
using flex::VitaPacket;

namespace {

// Stream identifiers this gateway emits. Fixed rather than negotiated: there is
// exactly one radio behind a gateway, so there is nothing to allocate.
constexpr uint32_t kAudioStreamId   = 0x0A000001;
constexpr uint32_t kContextStreamId = 0x0A000002;

// The audio the gateway sends is float32 stereo big-endian, byte for byte the
// same layout as a FlexRadio's uncompressed remote audio — so the client's
// existing decode path handles it without a special case.
QByteArray packAudioPayload(const std::vector<float>& mono)
{
    QByteArray payload(static_cast<int>(mono.size()) * 2 * 4, Qt::Uninitialized);
    auto* out = reinterpret_cast<quint32*>(payload.data());
    for (size_t i = 0; i < mono.size(); ++i) {
        quint32 bits;
        const float value = mono[i];
        std::memcpy(&bits, &value, sizeof(bits));
        const quint32 be = qToBigEndian<quint32>(bits);
        out[i * 2]     = be;   // left
        out[i * 2 + 1] = be;   // right
    }
    return payload;
}

int unpackAudioPayload(const VitaPacket& packet, std::vector<float>& mono)
{
    const int values = packet.payloadBytes / 4;
    const int frames = values / 2;
    if (frames <= 0)
        return 0;

    mono.resize(static_cast<size_t>(frames));
    for (int i = 0; i < frames; ++i) {
        // Take the left channel; the modulator sends the same signal on both.
        const quint32 bits = qFromBigEndian<quint32>(packet.payload + i * 8);
        float value;
        std::memcpy(&value, &bits, sizeof(value));
        mono[static_cast<size_t>(i)] = value;
    }
    return frames;
}


} // namespace

Ft991Gateway::Ft991Gateway(QObject* parent)
    : QObject(parent)
{
    connect(&m_audio, &CodecAudio::audioReady, this, &Ft991Gateway::onAudioReady);
    connect(&m_audio, &CodecAudio::errorOccurred, this, &Ft991Gateway::log);

    connect(&m_cat, &Ft991Cat::stateChanged, this, &Ft991Gateway::onCatState);
    connect(&m_cat, &Ft991Cat::errorOccurred, this, &Ft991Gateway::log);

    connect(&m_data, &QUdpSocket::readyRead, this, &Ft991Gateway::onDatagrams);
    connect(&m_beaconTimer, &QTimer::timeout, this, &Ft991Gateway::onBeacon);
    connect(&m_sweepTimer, &QTimer::timeout, this, &Ft991Gateway::onSubscriberSweep);
    connect(&m_contextTimer, &QTimer::timeout, this, [this] { sendContext(false); });

    m_beaconTimer.setInterval(1000);
    m_sweepTimer.setInterval(500);
    // A context packet a second keeps a late-joining client current without
    // flooding; anything that actually changes is sent immediately anyway.
    m_contextTimer.setInterval(1000);

    // Cinque secondi: abbastanza spesso da accorgersi che la porta si e'
    // liberata mentre si guarda il waterfall, abbastanza raro da non riempire
    // il registro di tentativi mentre l'altro programma lavora.
    m_catRetryTimer.setInterval(5000);
    connect(&m_catRetryTimer, &QTimer::timeout, this, [this] {
        if (tryOpenCat())
            m_catRetryTimer.stop();
    });
}

Ft991Gateway::~Ft991Gateway()
{
    stop();
}

bool Ft991Gateway::start(const Config& config)
{
    stop();
    m_config = config;

    if (!m_data.bind(QHostAddress::AnyIPv4, config.dataPort)) {
        emit log(tr("Cannot bind UDP port %1: %2")
                     .arg(config.dataPort)
                     .arg(m_data.errorString()));
        return false;
    }
    m_data.setSocketOption(QAbstractSocket::ReceiveBufferSizeSocketOption, QVariant(1 << 20));

    if (!m_beaconSocket.bind(QHostAddress::AnyIPv4, 0)) {
        emit log(tr("Cannot open the discovery socket: %1").arg(m_beaconSocket.errorString()));
        m_data.close();
        return false;
    }

    if (!m_audio.start(config.captureHint, config.playbackHint)) {
        m_data.close();
        m_beaconSocket.close();
        return false;
    }
    emit log(tr("Audio in: %1").arg(m_audio.captureDeviceName()));
    if (!m_audio.playbackDeviceName().isEmpty())
        emit log(tr("Audio out: %1").arg(m_audio.playbackDeviceName()));

    if (config.catEnabled) {
        // Un fallimento del CAT non e' fatale: la ricezione funziona lo stesso e
        // l'operatore puo' girare la manopola. Viene detto, non nascosto — e poi
        // si riprova, perche' quasi sempre e' un altro programma che tiene la
        // porta e prima o poi la lascia.
        if (!tryOpenCat())
            m_catRetryTimer.start();
    } else {
        emit log(tr("CAT disabled — receive only, no tuning or PTT"));
    }

    m_beaconTimer.start();
    m_sweepTimer.start();
    m_contextTimer.start();
    m_running = true;

    emit log(tr("Gateway listening on UDP %1").arg(config.dataPort));
    emit started();
    return true;
}

bool Ft991Gateway::tryOpenCat()
{
    if (m_cat.isOpen())
        return true;

    Ft991Cat::Config catConfig;
    catConfig.portName = m_config.catPort;
    catConfig.baudRate = m_config.catBaud;
    if (!m_cat.open(catConfig))
        return false;

    emit log(tr("CAT on %1 at %2 baud").arg(m_config.catPort).arg(m_config.catBaud));
    // Il contesto lo dice subito ai client: chi aspettava di poter cambiare
    // banda vede i pulsanti accendersi senza dover fare niente.
    sendContext(true);
    return true;
}

void Ft991Gateway::stop()
{
    if (!m_running)
        return;

    // Never leave the transmitter keyed because the gateway went away.
    if (m_cat.isOpen() && m_cat.state().transmitting)
        m_cat.setTransmit(false);

    m_beaconTimer.stop();
    m_sweepTimer.stop();
    m_contextTimer.stop();
    m_catRetryTimer.stop();
    m_audio.stop();
    m_cat.close();
    m_data.close();
    m_beaconSocket.close();
    m_subscribers.clear();
    m_running = false;
    emit stopped();
}

// ── discovery ───────────────────────────────────────────────────────────────

void Ft991Gateway::onBeacon()
{
    // Same shape as a FlexRadio's announcement — a VITA-49 packet whose payload
    // is ASCII key=value — so one discovery listener handles both kinds of
    // radio. The class ID is what says which is which.
    // The beacon is a space-separated key=value list, so a value containing a
    // space would be read as a new token and truncate the field. FlexRadio
    // solves this with a non-breaking space inside values, and the client
    // already converts it back — so do the same rather than inventing quoting.
    const auto escape = [](QString value) {
        return value.replace(QLatin1Char(' '), QChar(0x00A0));
    };

    const QString station = escape(m_config.stationName.isEmpty()
                                       ? QStringLiteral("FT-991A")
                                       : m_config.stationName);

    // Il numero di serie include la porta UDP, non solo quella CAT: due gateway
    // sulla stessa macchina — uno vero e uno di prova, o due radio — altrimenti
    // si presentano con la stessa identita', il client ne vede uno solo e finisce
    // per collegarsi a quello sbagliato senza che nulla lo segnali.
    // Deliberately no ip= field. A machine with a VPN, a virtual switch or two
    // network cards has several addresses and no reliable way to guess which one
    // a client can reach; the address the beacon actually arrived from always
    // works, and that is what the client falls back to.
    QString payload = QStringLiteral(
                          "model=FT-991A serial=%1 version=%2 nickname=%3 port=%4 "
                          "inuse=%5 gateway=decortty")
                          .arg(QStringLiteral("FT991A-%1-%2")
                                   .arg(m_config.catPort)
                                   .arg(m_config.dataPort),
                               QStringLiteral(DECORTTY_VERSION),
                               station)
                          .arg(m_config.dataPort)
                          .arg(m_subscribers.isEmpty() ? 0 : 1);
    if (!m_config.callsign.isEmpty())
        payload += QStringLiteral(" callsign=%1").arg(escape(m_config.callsign));

    const QByteArray text = payload.toUtf8();
    const QByteArray packet = flex::buildFlexDataPacket(0, flex::FlexClass::Discovery,
                                                        text, m_beaconCount);
    m_beaconCount = static_cast<quint8>((m_beaconCount + 1) & 0x0F);

    // Rebuilt with our own class ID: buildFlexDataPacket stamps FlexRadio's, and
    // a gateway must never claim to be one.
    QByteArray ours = packet;
    auto* words = reinterpret_cast<quint32*>(ours.data());
    words[2] = qToBigEndian<quint32>(kDecoRttyOui);
    words[3] = qToBigEndian<quint32>((static_cast<uint32_t>(kDecoRttyInformationClass) << 16)
                                     | static_cast<uint32_t>(DecoClass::Discovery));

    m_beaconSocket.writeDatagram(ours, QHostAddress::Broadcast, kBeaconPort);
}

// ── subscribers ─────────────────────────────────────────────────────────────

void Ft991Gateway::touchSubscriber(const QHostAddress& address, quint16 port)
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    for (Subscriber& subscriber : m_subscribers) {
        if (subscriber.address == address && subscriber.port == port) {
            subscriber.lastSeenMs = now;
            return;
        }
    }
    m_subscribers.append(Subscriber{address, port, now});
    emit log(tr("Client %1:%2 connected").arg(address.toString()).arg(port));
    // Bring the newcomer up to date at once rather than making it wait for the
    // next periodic context packet.
    sendContext(true);
}

void Ft991Gateway::onSubscriberSweep()
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    for (int i = m_subscribers.size() - 1; i >= 0; --i) {
        if (now - m_subscribers.at(i).lastSeenMs > kSubscriberTimeoutMs) {
            emit log(tr("Client %1:%2 timed out")
                         .arg(m_subscribers.at(i).address.toString())
                         .arg(m_subscribers.at(i).port));
            m_subscribers.removeAt(i);
        }
    }

    // A client that vanished mid-transmission must not leave the radio keyed.
    if (m_subscribers.isEmpty() && m_cat.isOpen() && m_cat.state().transmitting) {
        emit log(tr("No clients left — dropping PTT"));
        m_cat.setTransmit(false);
        m_audio.flushTransmit();
        return;
    }

    // Nor must a client that is still answering keepalives but has stopped
    // producing audio: the transmitter would sit there keyed, sending silence.
    if (m_cat.isOpen() && m_cat.state().transmitting && m_lastTxAudioMs > 0) {
        const qint64 starved = QDateTime::currentMSecsSinceEpoch() - m_lastTxAudioMs;
        if (starved > kTransmitStarveMs) {
            emit log(tr("No transmit audio for %1 ms — dropping PTT").arg(starved));
            m_cat.setTransmit(false);
            m_audio.flushTransmit();
            m_lastTxAudioMs = 0;
        }
    }
    // Sganciati e senza nessuno da servire: si chiude, ma non al primo istante
    // di solitudine — un client che si riavvia torna entro pochi secondi.
    if (m_emancipated) {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        if (m_subscribers.isEmpty()) {
            if (m_aloneSinceMs == 0)
                m_aloneSinceMs = now;
            else if (now - m_aloneSinceMs > 30000) {
                emit log(tr("No client for thirty seconds: closing"));
                emit shutdownRequested();
            }
        } else {
            m_aloneSinceMs = 0;
        }
    }
}

void Ft991Gateway::broadcastToSubscribers(const QByteArray& packet)
{
    for (const Subscriber& subscriber : m_subscribers)
        m_data.writeDatagram(packet, subscriber.address, subscriber.port);
}

// ── receive path ────────────────────────────────────────────────────────────

void Ft991Gateway::onAudioReady(const std::vector<float>& mono)
{
    if (m_subscribers.isEmpty() || mono.empty())
        return;

    m_audioPayload = packAudioPayload(mono);
    QByteArray packet = flex::buildFlexDataPacket(kAudioStreamId, flex::FlexClass::DaxAudio,
                                                  m_audioPayload, m_audioCount, m_audioSamples);
    m_audioCount = static_cast<quint8>((m_audioCount + 1) & 0x0F);
    m_audioSamples += static_cast<quint64>(mono.size());

    auto* words = reinterpret_cast<quint32*>(packet.data());
    words[2] = qToBigEndian<quint32>(kDecoRttyOui);
    words[3] = qToBigEndian<quint32>((static_cast<uint32_t>(kDecoRttyInformationClass) << 16)
                                     | static_cast<uint32_t>(DecoClass::Audio));

    broadcastToSubscribers(packet);
}

void Ft991Gateway::onCatState(const Ft991Cat::State&)
{
    sendContext(true);
}

void Ft991Gateway::sendContext(bool full)
{
    if (m_subscribers.isEmpty())
        return;

    const Ft991Cat::State& state = m_cat.state();

    RadioContext context;
    context.rfFrequencyHz = state.frequencyHz;
    context.sampleRateHz  = CodecAudio::kWireRate;
    if (full) {
        // The audio the gateway carries is the transceiver's own filtered
        // output, so the meaningful bandwidth is what fits in the wire rate.
        context.bandwidthHz = CodecAudio::kWireRate / 2.0;
    }
    context.referenceLevelDbm = Ft991Cat::sMeterToDbm(state.sMeterRaw);

    uint32_t stateBits = kStateEnableValidData | kStateValidData;
    if (state.transmitting)
        stateBits |= kStateUserTransmitting;
    if (state.online)
        stateBits |= kStateUserCatOnline;
    context.stateAndEvent = stateBits;

    QByteArray packet = buildContextPacket(kContextStreamId, context, m_contextCount);
    m_contextCount = static_cast<quint8>((m_contextCount + 1) & 0x0F);
    broadcastToSubscribers(packet);

    // The operating mode has no CIF0 field, so it rides in a vendor context
    // packet of its own rather than being forced into a standard one.
    if (full && !state.mode.isEmpty()) {
        RadioCommand modeReport;
        modeReport.mode = state.mode;
        QByteArray modePacket = buildCommandPacket(kContextStreamId, modeReport, m_contextCount);
        broadcastToSubscribers(modePacket);
    }
}

// ── control and transmit ────────────────────────────────────────────────────

void Ft991Gateway::onDatagrams()
{
    while (m_data.hasPendingDatagrams()) {
        const QNetworkDatagram datagram = m_data.receiveDatagram();
        const QByteArray data = datagram.data();

        // Ordine di spegnimento. Va riconosciuto prima del parsing VITA perche'
        // non e' un pacchetto VITA: e' il modo in cui chi ci ha avviati ci
        // chiede di chiudere, e deve funzionare anche quando nessun client e'
        // connesso. Due condizioni perche' valga: deve arrivare dalla stessa
        // macchina, e deve portare la parola che solo il supervisore conosce.
        static const QByteArray kShutdown = QByteArrayLiteral("DECORTTY-SHUTDOWN ");
        if (data.startsWith(kShutdown)) {
            const QString token = QString::fromLatin1(data.mid(kShutdown.size())).trimmed();
            if (!m_config.shutdownToken.isEmpty() && token == m_config.shutdownToken
                && datagram.senderAddress().isLoopback()) {
                emit log(tr("Shutdown requested by whoever started me"));
                requestShutdown();
            }
            continue;
        }

        // Un client che se ne va lo dice, e viene tolto dall'elenco subito
        // invece che dopo la scadenza.
        if (data == QByteArrayLiteral("DECORTTY-BYE")) {
            const int before = m_subscribers.size();
            for (int i = m_subscribers.size() - 1; i >= 0; --i) {
                if (m_subscribers.at(i).address == datagram.senderAddress()
                    && m_subscribers.at(i).port == datagram.senderPort())
                    m_subscribers.removeAt(i);
            }
            if (before != m_subscribers.size())
                emit log(tr("Client %1 is leaving").arg(datagram.senderAddress().toString()));
            continue;
        }

        VitaPacket packet;
        if (!flex::parseVitaPacket(data, packet))
            continue;

        // Any well-formed packet from a client counts as a keepalive.
        touchSubscriber(datagram.senderAddress(), datagram.senderPort());

        if (packet.classId.oui != kDecoRttyOui)
            continue;

        switch (static_cast<DecoClass>(packet.classId.packetClass)) {
        case DecoClass::Command: {
            RadioCommand command;
            if (!parseCommandPacket(packet, command))
                break;

            if (command.tuneFrequencyHz && m_cat.isOpen())
                m_cat.setFrequency(*command.tuneFrequencyHz);
            if (command.mode && m_cat.isOpen())
                m_cat.setMode(*command.mode);
            if (command.transmit) {
                if (*command.transmit)
                    m_lastTxAudioMs = QDateTime::currentMSecsSinceEpoch();
                emit log(tr("PTT requested by the client: %1  (CAT %2)")
                             .arg(*command.transmit ? tr("TRANSMIT") : tr("receive"),
                                  m_cat.isOpen() ? tr("open") : tr("CLOSED")));
                if (m_cat.isOpen()) {
                    m_cat.setTransmit(*command.transmit);
                } else {
                    emit log(tr("Transmit requested but CAT is not open — ignored"));
                }
                if (!*command.transmit)
                    m_audio.flushTransmit();
            }
            break;
        }

        case DecoClass::Audio: {
            // Transmit audio from the client. Passed to the codec whether or not
            // the radio is keyed: the FT-991A ignores it while receiving, and
            // holding it back would clip the start of every transmission.
            const int frames = unpackAudioPayload(packet, m_txMono);
            if (frames > 0) {
                m_lastTxAudioMs = QDateTime::currentMSecsSinceEpoch();
                m_audio.writeTransmitAudio(m_txMono.data(), frames);

                for (int i = 0; i < frames; ++i) {
                    const float v = m_txMono[static_cast<size_t>(i)];
                    m_txPeak = std::max(m_txPeak, std::fabs(v));
                    m_txSumSquares += static_cast<double>(v) * v;
                }
                m_txLevelCount += static_cast<quint64>(frames);

                // Riepilogo ogni due secondi finche' arriva audio: e' l'unico
                // modo per sapere se il silenzio nasce prima o dopo la scheda.
                if (!m_txDiagClock.isValid() || m_txDiagClock.hasExpired(2000)) {
                    emit log(tr("TX: %1  %2").arg(m_audio.transmitDiagnostics(),
                                                  transmitLevelText()));
                    m_txPeak       = 0.0f;
                    m_txSumSquares = 0.0;
                    m_txLevelCount = 0;
                    m_txDiagClock.restart();
                }
            }
            break;
        }

        default:
            break;
        }
    }
}


QString Ft991Gateway::transmitLevelText() const
{
    if (m_txLevelCount == 0)
        return tr("livello: niente da misurare");

    const double rms = std::sqrt(m_txSumSquares / static_cast<double>(m_txLevelCount));
    // Sotto questa soglia non c'e' segnale: c'e' il rumore dei numeri. Un tono
    // vero, anche piano, sta decine di dB piu' su.
    if (m_txPeak < 1.0e-5)
        return tr("livello: MUTO — il client manda campioni a zero");

    const double peakDb = 20.0 * std::log10(static_cast<double>(m_txPeak));
    const double rmsDb  = rms > 0.0 ? 20.0 * std::log10(rms) : -120.0;
    return tr("livello: picco %1 dB, medio %2 dB")
        .arg(peakDb, 0, 'f', 1)
        .arg(rmsDb, 0, 'f', 1);
}

void Ft991Gateway::requestShutdown()
{
    if (m_shutdownPending)
        return;
    if (m_emancipated) {
        // Chi ci aveva avviati se n'e' andato e noi siamo rimasti per gli
        // altri. Le sue richieste non ci riguardano piu': si chiude quando
        // l'ultimo client se ne va, e lo decide lo sweep.
        return;
    }
    m_shutdownPending = true;
    m_shutdownAskedMs = QDateTime::currentMSecsSinceEpoch();

    // Non si decide subito. Chi ha chiesto la chiusura ha appena mollato la
    // radio, ma il suo client resta in elenco ancora per qualche secondo: a
    // contarlo adesso non si distingue da un altro operatore che sta copiando.
    // Due secondi di ascolto e la differenza si vede da se', perche' chi c'e'
    // davvero continua a farsi vivo.
    QTimer::singleShot(2500, this, &Ft991Gateway::decideShutdown);
}

void Ft991Gateway::decideShutdown()
{
    m_shutdownPending = false;

    int stillThere = 0;
    for (const Subscriber& subscriber : m_subscribers) {
        if (subscriber.lastSeenMs > m_shutdownAskedMs)
            ++stillThere;
    }

    if (stillThere == 0) {
        emit shutdownRequested();
        return;
    }

    // Qualcuno sta ancora usando la radio attraverso di noi. Si continua, ma da
    // qui in avanti nessuno ci sorveglia: si esce quando l'ultimo se ne va.
    m_emancipated  = true;
    m_aloneSinceMs = 0;
    emit log(tr("Shutdown deferred: %1 client(s) are still using the radio")
                 .arg(stillThere));
}

} // namespace decortty::gateway
