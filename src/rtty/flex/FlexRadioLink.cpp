#include "flex/FlexRadioLink.h"

#include "flex/FlexTrace.h"

#include <QTimer>

#include <QVariantMap>

#include <algorithm>
#include <cmath>

namespace decortty::flex {

FlexRadioLink::FlexRadioLink(QObject* parent)
    : link::RadioLink(parent)
{
    connect(&m_api, &FlexApiClient::connected,     this, &FlexRadioLink::onApiConnected);
    connect(&m_api, &FlexApiClient::disconnected,  this, &FlexRadioLink::onApiDisconnected);
    // Legati a una stazione gia' presente: l'operatore deve saperlo, perche'
    // cambia dove si guarda — la slice e' quella di SmartSDR, non una nostra.
    connect(&m_api, &FlexApiClient::boundToStation, this, [this](const QString& station) {
        m_boundStation = station;
        setStatusText(tr("Sharing %1 with %2").arg(m_radio.displayName(), station));
        emit connectionChanged();
    });
    connect(&m_api, &FlexApiClient::statusReceived, this, &FlexRadioLink::onStatus);
    connect(&m_api, &FlexApiClient::errorOccurred, this, &FlexRadioLink::errorOccurred);
    connect(&m_api, &FlexApiClient::radioMessage, this,
            [this](const QString& text, bool isError) {
                if (isError)
                    emit errorOccurred(text);
            });

    connect(&m_vita, &FlexVitaStream::audioReceived, this, &FlexRadioLink::audioReady);
    connect(&m_vita, &FlexVitaStream::opusReceived,  this, &FlexRadioLink::onOpusAudio);
    // Il primo pacchetto che arriva e' la sola prova che la strada scelta porta
    // davvero audio; da li' in poi il controllo non serve piu'.
    connect(&m_vita, &FlexVitaStream::audioReceived, this,
            [this](const std::vector<float>&, int) { onAudioArrived(); });
    connect(&m_vita, &FlexVitaStream::opusReceived, this,
            [this](const QByteArray&) { onAudioArrived(); });
    connect(&m_vita, &FlexVitaStream::metersReceived, this, &FlexRadioLink::onMeters);
    connect(&m_vita, &FlexVitaStream::errorOccurred, this, &FlexRadioLink::errorOccurred);

    setStatusText(tr("Disconnected"));
}

FlexRadioLink::~FlexRadioLink()
{
    if (m_api.isConnected())
        tearDownStreams();
}

void FlexRadioLink::setStatusText(const QString& text)
{
    if (m_statusText == text)
        return;
    m_statusText = text;
    emit statusTextChanged();
}






void FlexRadioLink::connectToRadio(const RadioInfo& radio)
{
    m_radio = radio;
    setStatusText(tr("Connecting to %1…").arg(radio.displayName()));
    m_api.connectToRadio(radio);
}

void FlexRadioLink::connectToAddress(const QHostAddress& address, quint16 port)
{
    // Manual connect, for a radio on another subnet where broadcasts do not
    // reach — a VPN or a routed shack network.
    m_radio = RadioInfo{};
    m_radio.address = address;
    m_radio.port    = port;
    m_radio.model   = address.toString();
    setStatusText(tr("Connecting to %1…").arg(address.toString()));
    m_api.connectToHost(address, port);
}

void FlexRadioLink::disconnectRadio()
{
    if (m_transmitting)
        setTransmit(false);
    tearDownStreams();
    m_api.disconnectFromRadio();
}

void FlexRadioLink::onApiConnected()
{
    setStatusText(tr("Connected to %1").arg(m_radio.displayName()));
    bringUpStreams();
    armSliceWatch();
    emit connectionChanged();
}

void FlexRadioLink::onApiDisconnected()
{
    m_rxStreamId      = 0;
    m_txStreamId      = 0;
    m_slice           = SliceState{};
    m_txDaxEnabled     = false;
    m_daxRxBound       = false;
    m_daxTxBound       = false;
    m_audioSeen        = false;
    m_createdSlice     = -1;
    m_awaitingOwnSlice = false;
    m_audioWatch.stop();
    m_sliceWatch.stop();
    m_vita.stop();

    if (m_relinkAsGui) {
        // La disconnessione era voluta: si torna subito, questa volta come
        // stazione GUI.
        m_relinkAsGui = false;
        setStatusText(tr("Reconnecting to %1…").arg(m_radio.displayName()));
        emit connectionChanged();
        QTimer::singleShot(300, this, [this] { m_api.connectToRadio(m_radio); });
        return;
    }

    setStatusText(tr("Disconnected"));
    emit connectionChanged();
    emit sliceChanged();
}

void FlexRadioLink::armSliceWatch()
{
    m_sliceWatch.setSingleShot(true);
    // Quattro secondi. La radio manda gli stati subito dopo l'abbonamento; se
    // dopo tutto questo tempo non e' arrivata nemmeno una slice, non arrivera'.
    m_sliceWatch.setInterval(4000);
    disconnect(&m_sliceWatch, nullptr, this, nullptr);
    connect(&m_sliceWatch, &QTimer::timeout, this, [this] {
        if (m_slice.index >= 0)
            return;

        // Da client secondario una slice ci sarebbe dovuta arrivare: e' quella
        // dell'operatore che ha la radio. Se non arriva, il legame non porta
        // niente e si torna a lavorare da soli.
        if (!m_api.boundTo().isEmpty()) {
            emit errorOccurred(tr("Bound to %1 but the radio shows no slice — "
                                  "reconnecting as a GUI station")
                                   .arg(m_api.boundTo()));
            m_api.setRole(flex::FlexApiClient::Role::Gui);
            m_relinkAsGui = true;
            m_api.disconnectFromRadio();
            return;
        }

        // Da stazione GUI invece nessuna slice vuol dire nessuna slice: siamo
        // arrivati per primi su una radio libera, e chi arriva per primo se la
        // apre da se'. Senza, non ci sarebbe frequenza da leggere ne' canale
        // DAX a cui legarsi — solo un collegamento che non porta niente.
        createOwnSlice();
    });
    m_sliceWatch.start();
}

void FlexRadioLink::createOwnSlice()
{
    if (m_awaitingOwnSlice || m_createdSlice >= 0)
        return;
    m_awaitingOwnSlice = true;
    traceNote(QStringLiteral("nessuna slice sulla radio: ne apro una"));
    setStatusText(tr("Opening a slice on %1…").arg(m_radio.displayName()));

    // DIGU: dati sulla banda laterale superiore, il modo per cui questo
    // decodificatore e' scritto. La frequenza la sceglie la radio — sara'
    // l'operatore a portarla in banda, e mettergliene una noi vorrebbe dire
    // indovinare quale.
    m_api.send(QStringLiteral("slice create mode=DIGU"),
               [this](int code, const QString& body) {
                   if (code != 0) {
                       m_awaitingOwnSlice = false;
                       emit errorOccurred(tr("The radio has no slice and would not open one"));
                       return;
                   }
                   // Alcune versioni rispondono col numero della slice, altre
                   // no. Quando c'e' lo si prende subito; quando manca, la
                   // prima slice che la radio racconta e' la nostra, perche'
                   // fino a un attimo fa non ce n'era nessuna.
                   bool ok = false;
                   const int index = body.trimmed().section(QLatin1Char(' '), 0, 0).toInt(&ok);
                   if (ok)
                       m_createdSlice = index;
               });
}

void FlexRadioLink::setDaxChannel(int channel)
{
    // I canali DAX di un FLEX-6000 vanno da 1 a 8.
    m_daxChannel = std::clamp(channel, 1, 8);
}

// La radio risponde con l'identificativo del flusso in esadecimale, di solito
// col prefisso — e talvolta senza. Base zero lo prende in entrambi i casi; se il
// prefisso manca del tutto lo si rilegge come esadecimale, che e' l'unica
// lettura sensata per un numero di flusso.
static quint32 parseStreamId(const QString& body)
{
    const QString text = body.trimmed().section(QLatin1Char(' '), 0, 0);
    bool ok = false;
    quint32 id = text.toUInt(&ok, 0);
    if (!ok)
        id = text.toUInt(&ok, 16);
    return ok ? id : 0;
}

void FlexRadioLink::bringUpStreams()
{
    if (!m_vita.start()) {
        emit errorOccurred(tr("Could not open the audio transport"));
        return;
    }
    // L'indirizzo e' quello della radio, la porta no: i dati non vanno dove
    // vanno i comandi.
    m_vita.setRadioEndpoint(m_radio.address);
    m_audioSeen  = false;
    m_daxRxBound = false;
    m_daxTxBound = false;

    // Tell the radio where to send everything. Until this lands, no VITA-49
    // traffic arrives at all.
    m_api.send(QStringLiteral("client udpport %1").arg(m_vita.boundPort()),
               [this](int code, const QString&) {
                   if (code != 0) {
                       emit errorOccurred(tr("The radio rejected the UDP port registration"));
                       return;
                   }
                   if (m_audioPath == AudioPath::RemoteAudio)
                       startRemoteAudioPath();
                   else
                       startDaxPath();
               });
}

void FlexRadioLink::createDaxReceiveStream()
{
    // Ricezione: un rubinetto sul canale DAX della slice. Il livello non dipende
    // dal volume di chi ha la radio davanti, ed e' la sola slice che ci
    // interessa invece del misto di tutte.
    const int channel = m_daxChannel;
    m_api.send(QStringLiteral("stream create type=dax_rx dax_channel=%1").arg(channel),
               [this, channel](int code, const QString& body) {
                   if (code != 0) {
                       if (m_audioPath == AudioPath::Dax) {
                           emit errorOccurred(tr("The radio would not open DAX channel %1")
                                                  .arg(channel));
                           return;
                       }
                       emit errorOccurred(tr("DAX channel %1 is not available — "
                                             "falling back to the client audio stream")
                                              .arg(channel));
                       startRemoteAudioPath();
                       return;
                   }
                   m_rxStreamId = parseStreamId(body);
                   m_vita.setAudioStreamId(m_rxStreamId);
                   bindDaxToSlice();
                   setStatusText(tr("Receiving from %1 on DAX %2")
                                     .arg(m_radio.displayName())
                                     .arg(channel));
               });
}

void FlexRadioLink::startDaxPath()
{
    m_pathInUse = AudioPath::Dax;
    traceNote(QStringLiteral("strada dell'audio: DAX, canale %1").arg(m_daxChannel));
    createDaxReceiveStream();

    // Trasmissione: sul flusso DAX i campioni viaggiano non compressi, percio'
    // qui il codificatore non serve e il PTT funziona anche senza.
    m_api.send(QStringLiteral("stream create type=dax_tx"),
               [this](int code, const QString& body) {
                   if (code != 0) {
                       emit errorOccurred(tr("Could not create the DAX transmit stream"));
                       return;
                   }
                   m_txStreamId = parseStreamId(body);
                   m_vita.setTransmitStreamId(m_txStreamId);
                   // Da qui in avanti l'audio di trasmissione della radio viene
                   // da noi e non dal microfono. Si rimette come stava quando ci
                   // si scollega.
                   m_api.send(QStringLiteral("transmit set dax=1"));
                   m_txDaxEnabled = true;
                   // Adesso il flusso esiste: si lega alla slice. Prima non
                   // c'era niente da legare, e la risposta a questo comando
                   // arriva dopo quella del flusso di ricezione.
                   bindDaxToSlice();
                   emit connectionChanged();
               });

    armAudioWatch();
}

void FlexRadioLink::startRemoteAudioPath()
{
    traceNote(m_pathInUse == AudioPath::Dax
                  ? QStringLiteral("ripiego sull'audio del client remoto")
                  : QStringLiteral("strada dell'audio: client remoto"));
    m_pathInUse = AudioPath::RemoteAudio;

    // Se si arriva qui dopo un tentativo DAX, quel flusso va tolto: due sorgenti
    // sullo stesso decodificatore si sommerebbero.
    if (m_rxStreamId) {
        m_api.send(QStringLiteral("stream remove 0x%1")
                       .arg(m_rxStreamId, 8, 16, QLatin1Char('0')));
        m_rxStreamId = 0;
        m_vita.setAudioStreamId(0);
    }
    if (m_txDaxEnabled) {
        m_api.send(QStringLiteral("transmit set dax=0"));
        m_txDaxEnabled = false;
    }
    if (m_txStreamId) {
        m_api.send(QStringLiteral("stream remove 0x%1")
                       .arg(m_txStreamId, 8, 16, QLatin1Char('0')));
        m_txStreamId = 0;
        m_vita.setTransmitStreamId(0);
    }

    // Receive audio, uncompressed. This is the stream SmartSDR itself uses for
    // headphone audio — no DAX channel involved.
    m_api.send(QStringLiteral("stream create type=remote_audio_rx compression=none"),
               [this](int rxCode, const QString& body) {
                   if (rxCode != 0) {
                       // Da client secondario la radio potrebbe non concedere il
                       // flusso audio. Non e' stato possibile verificarlo su un
                       // apparato vero, quindi invece di affermare che funziona
                       // si prevede il caso: si ritenta prendendo il ruolo GUI,
                       // che l'audio lo ottiene di sicuro — al prezzo di un
                       // posto MultiFlex.
                       if (!m_api.boundTo().isEmpty()) {
                           emit errorOccurred(
                               tr("The radio will not give audio to a bound client — "
                                  "reconnecting as a GUI station"));
                           m_api.setRole(flex::FlexApiClient::Role::Gui);
                           m_relinkAsGui = true;
                           m_api.disconnectFromRadio();
                           return;
                       }
                       emit errorOccurred(tr("Could not create the receive audio stream"));
                       return;
                   }
                   m_rxStreamId = parseStreamId(body);
                   m_vita.setAudioStreamId(m_rxStreamId);
                   setStatusText(tr("Receiving from %1").arg(m_radio.displayName()));
               });

    // Transmit audio. The radio enforces Opus on this stream.
    if (m_opus.isAvailable()) {
        m_api.send(QStringLiteral("stream create type=remote_audio_tx compression=opus"),
                   [this](int txCode, const QString& body) {
                       if (txCode != 0) {
                           emit errorOccurred(tr("Could not create the transmit audio stream"));
                           return;
                       }
                       m_txStreamId = parseStreamId(body);
                       m_vita.setTransmitStreamId(m_txStreamId);
                       emit connectionChanged();
                   });
    }
}

void FlexRadioLink::bindDaxToSlice()
{
    // Serve sapere su quale slice si sta: prima che la radio lo racconti non c'e'
    // niente da legare, e si rifa' appena la slice si conosce.
    if (m_pathInUse != AudioPath::Dax || m_slice.index < 0)
        return;

    // La trasmissione si lega per conto suo, appena il suo flusso esiste. Non
    // aspetta il legame di ricezione e non si considera fatta insieme a quello.
    if (m_txStreamId != 0 && !m_daxTxBound) {
        m_daxTxBound = true;
        traceNote(QStringLiteral("lego il canale DAX %1 alla slice %2 in trasmissione")
                      .arg(m_daxChannel)
                      .arg(m_slice.index));
        m_api.send(QStringLiteral("dax audio set %1 slice=%2 tx=1")
                       .arg(m_daxChannel)
                       .arg(m_slice.index));
    }

    if (m_rxStreamId == 0 || m_daxRxBound)
        return;

    m_daxRxBound = true;
    traceNote(QStringLiteral("lego il canale DAX alla slice %1 in ricezione "
                             "(la slice dichiara dax=%2)")
                  .arg(m_slice.index)
                  .arg(m_slice.daxChannel));

    if (m_slice.daxChannel > 0 && m_slice.daxChannel != m_daxChannel) {
        // La slice ha gia' il suo canale. Si segue quello invece di imporne un
        // altro: dietro la slice c'e' la stazione che sta usando la radio, e
        // riconfigurargliela per farci comodo e' l'ultima cosa da fare. Il
        // rubinetto era aperto sul canale sbagliato, quindi si riapre.
        const int wanted = m_slice.daxChannel;
        emit errorOccurred(tr("Slice %1 is on DAX channel %2, not %3 — following the slice")
                               .arg(m_slice.index)
                               .arg(wanted)
                               .arg(m_daxChannel));
        m_daxChannel = wanted;
        if (m_rxStreamId) {
            m_api.send(QStringLiteral("stream remove 0x%1")
                           .arg(m_rxStreamId, 8, 16, QLatin1Char('0')));
            m_rxStreamId = 0;
            m_vita.setAudioStreamId(0);
        }
        m_audioSeen = false;
        createDaxReceiveStream();
        armAudioWatch();
        // Il canale e' cambiato: anche la trasmissione va rilegata a quello
        // nuovo, altrimenti resta appesa al vecchio.
        m_daxTxBound = false;
        bindDaxToSlice();
    } else if (m_slice.daxChannel <= 0) {
        // Nessun canale assegnato: senza, dal rubinetto non esce niente, e qui
        // non si toglie nulla a nessuno.
        m_api.send(QStringLiteral("slice set %1 dax=%2").arg(m_slice.index).arg(m_daxChannel));
    }
}

void FlexRadioLink::armAudioWatch()
{
    m_audioWatch.setSingleShot(true);
    // Sei secondi. Un flusso DAX vivo manda pacchetti decine di volte al
    // secondo: se in tutto questo tempo non ne e' arrivato nemmeno uno, il
    // canale non e' quello giusto o la slice non ci sta sopra.
    m_audioWatch.setInterval(6000);
    disconnect(&m_audioWatch, nullptr, this, nullptr);
    connect(&m_audioWatch, &QTimer::timeout, this, [this] {
        if (m_audioSeen || m_pathInUse != AudioPath::Dax)
            return;
        if (m_audioPath == AudioPath::Dax) {
            // Scelta dall'operatore: non gli si cambia strada sotto i piedi, gli
            // si dice cosa guardare.
            emit errorOccurred(tr("No audio on DAX channel %1 — check that the slice "
                                  "is assigned to it in SmartSDR")
                                   .arg(m_daxChannel));
            return;
        }
        emit errorOccurred(tr("No audio on DAX channel %1 — falling back to the "
                              "client audio stream")
                               .arg(m_daxChannel));
        startRemoteAudioPath();
    });
    m_audioWatch.start();
}

void FlexRadioLink::onAudioArrived()
{
    if (m_audioSeen)
        return;
    m_audioSeen = true;
    m_audioWatch.stop();
    // La riga piu' importante del registro: da qui in avanti l'audio c'e'. Se
    // manca, tutto quello che viene prima e' il posto dove guardare.
    traceNote(QStringLiteral("primo pacchetto audio: la strada %1 porta")
                  .arg(m_pathInUse == AudioPath::Dax ? QStringLiteral("DAX")
                                                     : QStringLiteral("del client remoto")));
}

void FlexRadioLink::noteStreamStatus(const QString& object, const QMap<QString, QString>& kvs)
{
    // "stream 0x1E020001 type=dax_rx dax_channel=2 …". La risposta al comando
    // dice la stessa cosa, ma puo' arrivare dopo i primi pacchetti: chi si
    // presenta per primo vince, e l'altro non cambia niente.
    const quint32 id = parseStreamId(object.mid(7));
    if (id == 0)
        return;

    const QString type = kvs.value(QStringLiteral("type"));
    if (type == QLatin1String("dax_rx")) {
        if (kvs.value(QStringLiteral("dax_channel")).toInt() != m_daxChannel)
            return;
        if (m_rxStreamId != id) {
            m_rxStreamId = id;
            m_vita.setAudioStreamId(id);
        }
        bindDaxToSlice();
    } else if (type == QLatin1String("dax_tx")) {
        if (m_txStreamId != id) {
            m_txStreamId = id;
            m_vita.setTransmitStreamId(id);
            emit connectionChanged();
        }
    }
}

void FlexRadioLink::tearDownStreams()
{
    traceNote(QStringLiteral("chiudo: %1 pacchetti ricevuti, %2 byte, %3 persi, %4 mandati")
                  .arg(m_vita.packetsReceived())
                  .arg(m_vita.bytesReceived())
                  .arg(m_vita.packetsLost())
                  .arg(m_vita.packetsSent()));
    m_audioWatch.stop();
    if (m_createdSlice >= 0) {
        // La slice l'abbiamo aperta noi su una radio che non ne aveva: lasciarla
        // li' vorrebbe dire che chi arriva dopo trova una stazione sintonizzata
        // da nessuno.
        m_api.send(QStringLiteral("slice remove %1").arg(m_createdSlice));
        m_createdSlice = -1;
    }
    m_awaitingOwnSlice = false;
    if (m_txDaxEnabled) {
        // La radio torna a prendere l'audio di trasmissione dal microfono.
        // Lasciarla in DAX vorrebbe dire una stazione muta dopo che ce ne siamo
        // andati, e nessuno legherebbe la cosa a noi.
        m_api.send(QStringLiteral("transmit set dax=0"));
        m_txDaxEnabled = false;
    }
    if (m_rxStreamId)
        m_api.send(QStringLiteral("stream remove 0x%1").arg(m_rxStreamId, 8, 16, QLatin1Char('0')));
    if (m_txStreamId)
        m_api.send(QStringLiteral("stream remove 0x%1").arg(m_txStreamId, 8, 16, QLatin1Char('0')));
    m_rxStreamId = 0;
    m_txStreamId = 0;
    m_daxRxBound = false;
    m_daxTxBound = false;
    m_vita.stop();
}

void FlexRadioLink::onStatus(const QString& object, const QMap<QString, QString>& kvs)
{
    if (object.startsWith(QLatin1String("slice "))) {
        const int index = object.mid(6).toInt();
        applySliceStatus(index, kvs);
        return;
    }

    if (object.startsWith(QLatin1String("stream "))) {
        noteStreamStatus(object, kvs);
        return;
    }

    if (object == QLatin1String("transmit")) {
        const auto it = kvs.constFind(QStringLiteral("mox"));
        if (it != kvs.constEnd()) {
            const bool on = (*it == QLatin1String("1"));
            if (on != m_transmitting) {
                m_transmitting = on;
                emit transmittingChanged();
            }
        }
        return;
    }

    if (object == QLatin1String("meter")) {
        // Meter definitions arrive keyed by id: "<id>.nam=LEVEL", "<id>.src=SLC".
        // The S-meter is the slice's LEVEL meter.
        for (auto it = kvs.constBegin(); it != kvs.constEnd(); ++it) {
            if (!it.key().endsWith(QLatin1String(".nam")) || it.value() != QLatin1String("LEVEL"))
                continue;
            const QString idText = it.key().section(QLatin1Char('.'), 0, 0);
            const QString srcKey = idText + QStringLiteral(".src");
            if (kvs.value(srcKey) == QLatin1String("SLC"))
                m_levelMeterId = static_cast<quint16>(idText.toUInt());
        }
    }
}

void FlexRadioLink::applySliceStatus(int index, const QMap<QString, QString>& kvs)
{
    // Adopt the first slice that is in use, and stay with it afterwards.
    const bool inUse = kvs.value(QStringLiteral("in_use"), QStringLiteral("1")) == QLatin1String("1");
    if (m_slice.index < 0 && inUse) {
        m_slice.index = index;
        m_sliceWatch.stop();   // il legame ha portato quello che doveva
        if (m_awaitingOwnSlice) {
            // E' arrivata dopo il nostro comando su una radio che non ne aveva:
            // e' nostra, e ce la porteremo via andandocene.
            m_awaitingOwnSlice = false;
            if (m_createdSlice < 0)
                m_createdSlice = index;
            setStatusText(tr("Connected to %1").arg(m_radio.displayName()));
        }
        // Adesso si sa dove legare il canale DAX: prima non c'era niente da
        // legare, e il flusso poteva essere gia' aperto da qualche secondo.
        bindDaxToSlice();
    }
    if (index != m_slice.index)
        return;

    if (!inUse) {
        m_slice = SliceState{};
        emit sliceChanged();
        return;
    }

    bool changed = false;
    const auto number = [&kvs](const QString& key, auto& target, auto convert) {
        const auto it = kvs.constFind(key);
        if (it == kvs.constEnd())
            return false;
        bool ok = false;
        const auto value = convert(*it, ok);
        if (!ok || value == target)
            return false;
        target = value;
        return true;
    };

    changed |= number(QStringLiteral("RF_frequency"), m_slice.frequencyMhz,
                      [](const QString& s, bool& ok) { return s.toDouble(&ok); });
    changed |= number(QStringLiteral("filter_lo"), m_slice.filterLowHz,
                      [](const QString& s, bool& ok) { return s.toInt(&ok); });
    changed |= number(QStringLiteral("filter_hi"), m_slice.filterHighHz,
                      [](const QString& s, bool& ok) { return s.toInt(&ok); });

    const auto modeIt = kvs.constFind(QStringLiteral("mode"));
    if (modeIt != kvs.constEnd() && *modeIt != m_slice.mode) {
        m_slice.mode = *modeIt;
        changed = true;
    }

    const auto daxIt = kvs.constFind(QStringLiteral("dax"));
    if (daxIt != kvs.constEnd()) {
        const int channel = daxIt->toInt();
        if (channel != m_slice.daxChannel) {
            m_slice.daxChannel = channel;
            // Il canale e' cambiato sotto di noi — l'operatore l'ha spostato in
            // SmartSDR. Il rubinetto va rifatto, altrimenti si resta in ascolto
            // di un canale che non porta piu' questa slice.
            if (m_pathInUse == AudioPath::Dax && channel > 0 && channel != m_daxChannel) {
                m_daxRxBound = false;
                m_daxTxBound = false;
                bindDaxToSlice();
            }
        }
    }

    const auto txIt = kvs.constFind(QStringLiteral("tx"));
    if (txIt != kvs.constEnd())
        m_slice.transmitSlice = (*txIt == QLatin1String("1"));

    m_slice.active = true;
    if (changed)
        emit sliceChanged();
}

void FlexRadioLink::onMeters(const QHash<quint16, qint16>& meters)
{
    if (m_levelMeterId == 0)
        return;
    const auto it = meters.constFind(m_levelMeterId);
    if (it == meters.constEnd())
        return;
    // dBm meters are sent as sixteenths of a dB times eight — in practice, the
    // raw value divided by 128.
    const int dbm = static_cast<int>(std::lround(static_cast<double>(*it) / 128.0));
    if (dbm != m_signalDbm) {
        m_signalDbm = dbm;
        emit metersChanged();
    }
}

void FlexRadioLink::onOpusAudio(const QByteArray& payload)
{
    // A routed or SmartLink session compresses the receive audio. Decoding it
    // here means the rest of the program never learns the difference.
    std::vector<float> pcm = m_opus.decode(payload);
    if (pcm.empty())
        return;
    const int frames = static_cast<int>(pcm.size() / 2);
    emit audioReady(pcm, frames);
}

void FlexRadioLink::setFrequencyMhz(double mhz)
{
    if (m_slice.index < 0)
        return;
    m_api.send(QStringLiteral("slice tune %1 %2 autopan=0")
                   .arg(m_slice.index)
                   .arg(mhz, 0, 'f', 6));
}

void FlexRadioLink::setMode(const QString& mode)
{
    if (m_slice.index < 0)
        return;
    m_api.send(QStringLiteral("slice set %1 mode=%2").arg(m_slice.index).arg(mode.toUpper()));
}

void FlexRadioLink::setFilter(int lowHz, int highHz)
{
    if (m_slice.index < 0)
        return;
    m_api.send(QStringLiteral("filt %1 %2 %3").arg(m_slice.index).arg(lowHz).arg(highHz));
}

void FlexRadioLink::applyRttyProfile(int markHz, int shiftHz)
{
    if (m_slice.index < 0)
        return;

    setMode(QStringLiteral("DIGU"));

    // Pass the two tones with about 100 Hz of margin either side. Tighter than
    // this and AFC has nowhere to track; wider and the decoder sees needless
    // adjacent-signal energy.
    const int spaceHz = markHz - shiftHz;
    const int low     = std::max(50, spaceHz - 100);
    const int high    = markHz + 100;
    setFilter(low, high);
}

void FlexRadioLink::setTransmit(bool on)
{
    if (!m_api.isConnected())
        return;
    if (on && !canTransmit()) {
        emit errorOccurred(m_pathInUse == AudioPath::Dax
                               ? tr("Transmit is unavailable: no DAX transmit stream")
                               : tr("Transmit is unavailable: no Opus transmit stream"));
        return;
    }
    // A fine trasmissione il residuo che non ha riempito un pacchetto va
    // buttato: alla prossima trasmissione uscirebbe in testa, con mezzo
    // carattere di un messaggio finito da un pezzo.
    if (!on)
        m_vita.flushTransmitBuffer();

    // La radio prende l'audio dalla slice che trasmette. Se quella che stiamo
    // ascoltando non lo e', il PTT chiude e non esce niente — la nostra nota
    // sta su un'altra slice. Lo si fa qui e non al collegamento: finche' non si
    // trasmette davvero, portare il TX via da un'altra stazione e' una cortesia
    // che non ci compete.
    if (on && m_slice.index >= 0 && !m_slice.transmitSlice) {
        traceNote(QStringLiteral("la slice %1 non e' quella di trasmissione: gliela do")
                      .arg(m_slice.index));
        m_api.send(QStringLiteral("slice set %1 tx=1").arg(m_slice.index));
    }

    // PTT over the radio's own protocol. No CAT port, no RTS/DTR line, and it
    // reports back through transmit status so the UI follows the radio rather
    // than assuming.
    m_api.send(QStringLiteral("xmit %1").arg(on ? 1 : 0));
}

int FlexRadioLink::sendTransmitAudio(const float* samples, int count)
{
    if (m_txStreamId == 0 || count <= 0)
        return 0;

    // Su DAX i campioni vanno cosi' come sono: la compressione la vuole solo
    // l'audio del client remoto, ed e' l'unica ragione per cui senza Opus, una
    // volta, non si trasmetteva affatto.
    if (m_pathInUse == AudioPath::Dax)
        return m_vita.sendPcmFrames(samples, count);

    int sent = 0;
    for (const QByteArray& frame : m_opus.encodeMono(samples, count)) {
        if (m_vita.sendOpusFrame(frame))
            ++sent;
    }
    return sent;
}

} // namespace decortty::flex
