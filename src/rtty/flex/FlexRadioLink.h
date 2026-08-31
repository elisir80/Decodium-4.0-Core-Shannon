// DecoRTTY — a FlexRadio, as a RadioLink.
//
// Owns the command channel and the VITA-49 data plane, and drives the sequence
// that turns "a radio answered discovery" into "audio is flowing and PTT works":
// connect, register a UDP port, create the receive and transmit audio streams,
// adopt the active slice.
//
// Everything here goes over the radio's own network protocol. There is no DAX
// device to install, no virtual audio cable to route and no serial CAT port to
// configure.
#pragma once

#include "flex/FlexApiClient.h"
#include "flex/FlexDiscovery.h"
#include "flex/FlexVitaStream.h"
#include "flex/OpusCodec.h"
#include "link/RadioLink.h"

#include <QObject>
#include <QTimer>

#include <vector>

namespace decortty::flex {

// State of the slice DecoRTTY is listening to.
struct SliceState {
    int     index{-1};
    double  frequencyMhz{0.0};
    QString mode;
    int     filterLowHz{0};
    int     filterHighHz{0};
    // Il canale DAX che la radio dice assegnato a questa slice, 0 se nessuno.
    int     daxChannel{0};
    bool    transmitSlice{false};
    bool    active{false};
};

class FlexRadioLink : public link::RadioLink {
    Q_OBJECT

public:
    explicit FlexRadioLink(QObject* parent = nullptr);
    ~FlexRadioLink() override;

    void connectToRadio(const RadioInfo& radio);
    void connectToAddress(const QHostAddress& address, quint16 port = 4992);

    // Da quale flusso arriva l'audio della radio.
    //
    // Sono due strade diverse dentro lo stesso trasporto VITA-49. `remote_audio`
    // e' l'audio delle cuffie del client: comodo perche' non chiede niente a chi
    // sta davanti alla radio, ma e' il misto di tutte le slice, passa per il
    // volume e per il muto della stazione GUI, e in trasmissione la radio lo
    // vuole in Opus — senza codificatore non si trasmette affatto.
    //
    // DAX e' invece un rubinetto per slice: livello fisso, indipendente dal
    // volume, e in trasmissione accetta campioni non compressi. E' la strada che
    // usa un decodificatore, al prezzo di un canale DAX assegnato alla slice in
    // SmartSDR.
    //
    // Da se' significa: prima DAX, e se non ne esce audio si ripiega
    // sull'audio delle cuffie invece di restare muti.
    enum class AudioPath { Auto, Dax, RemoteAudio };
    void setAudioPath(AudioPath path) { m_audioPath = path; }
    AudioPath audioPath() const { return m_audioPath; }
    // Quale delle due si sta davvero usando: e' l'unica cosa che si puo' dire
    // con onesta' all'operatore, perche' la scelta la fa la radio rispondendo.
    AudioPath audioPathInUse() const { return m_pathInUse; }

    // Il canale DAX su cui la radio manda la slice. In SmartSDR e' il numero
    // che l'operatore assegna alla slice; qui lo si dichiara e basta.
    void setDaxChannel(int channel);
    int  daxChannel() const { return m_daxChannel; }

    // ── RadioLink ───────────────────────────────────────────────────────
    bool    isConnected() const override { return m_api.isConnected(); }
    QString statusText() const override  { return m_statusText; }
    QString radioName() const override   { return m_radio.displayName(); }
    double  frequencyMhz() const override { return m_slice.frequencyMhz; }
    QString mode() const override         { return m_slice.mode; }
    bool    isTransmitting() const override { return m_transmitting; }
    // Sul flusso del client remoto la radio pretende Opus, e senza codificatore
    // quella strada non trasmette affatto. Sul flusso DAX i campioni vanno
    // com'e', percio' li' il codificatore non conta.
    bool    canTransmit() const override
    {
        return m_txStreamId != 0
            && (m_pathInUse == AudioPath::Dax || m_opus.isAvailable());
    }
    int     signalStrengthDbm() const override { return m_signalDbm; }

    void disconnectRadio() override;
    void setFrequencyMhz(double mhz) override;
    void setMode(const QString& mode) override;
    void setFilter(int lowHz, int highHz) override;
    void applyRttyProfile(int markHz, int shiftHz) override;
    void setTransmit(bool on) override;
    int  sendTransmitAudio(const float* samples, int count) override;

    const SliceState& slice() const { return m_slice; }
    FlexVitaStream&   stream()      { return m_vita; }

private slots:
    void onApiConnected();
    void onApiDisconnected();
    void onStatus(const QString& object, const QMap<QString, QString>& kvs);
    void onMeters(const QHash<quint16, qint16>& meters);
    void onOpusAudio(const QByteArray& payload);

private:
    void setStatusText(const QString& text);
    // Il ruolo con cui presentarsi alla radio. Di norma lo decide da se'
    // guardando chi c'e' gia' collegato; si puo' imporre perche' l'automatismo
    // vede solo quello che la radio racconta, e se racconta male non c'e' altra
    // via d'uscita che dirglielo.
public:
    void setRole(FlexApiClient::Role role) { m_api.setRole(role); }

private:
    void bringUpStreams();
    // Le due strade per l'audio, e il controllo che dice se quella scelta porta
    // davvero qualcosa.
    void startDaxPath();
    void startRemoteAudioPath();
    void armAudioWatch();
    void onAudioArrived();
    // Lega il canale DAX alla slice adottata. Si fa quando la slice si conosce,
    // che puo' essere dopo la creazione del flusso.
    void bindDaxToSlice();
    // Apre (o riapre) il rubinetto DAX in ricezione sul canale corrente.
    void createDaxReceiveStream();
    // Lo stato `stream 0x… type=dax_rx` e' la seconda via per sapere quale
    // identificativo ci e' toccato: la risposta al comando puo' arrivare dopo i
    // primi pacchetti, lo stato no.
    void noteStreamStatus(const QString& object, const QMap<QString, QString>& kvs);
    // Arma il controllo che verifica se, passato il tempo, sulla radio c'e' una
    // slice da ascoltare.
    void armSliceWatch();
    // Crea la slice quando sulla radio non ce n'e' nessuna. Si fa solo da
    // stazione GUI: da client secondario le slice sono di chi ci ha preceduto.
    void createOwnSlice();
    void tearDownStreams();
    void applySliceStatus(int index, const QMap<QString, QString>& kvs);

    FlexApiClient  m_api;
    FlexVitaStream m_vita;
    OpusCodec      m_opus;

    RadioInfo  m_radio;
    SliceState m_slice;
    QString    m_statusText;

    // Vero mentre si sta rifacendo il collegamento perche' il ruolo secondario
    // non ha ottenuto l'audio.
    bool    m_relinkAsGui{false};
    // Legarsi a una stazione GUI ha senso solo se da quel legame arrivano le
    // sue slice: e' l'unica cosa che ci interessa vedere. Se non arrivano,
    // restiamo collegati e ciechi — nessuna frequenza, nessuna sintonia — ed e'
    // molto peggio che occupare un posto MultiFlex.
    QTimer  m_sliceWatch;
    // La stazione GUI con cui si condivide la radio, vuoto se si lavora da soli.
    QString m_boundStation;
    // La slice l'abbiamo creata noi: allora e' anche nostro il compito di
    // toglierla andandocene. Se invece l'abbiamo solo adottata resta dov'era,
    // che appartiene a chi l'ha fatta. -1 quando non ne abbiamo create.
    int  m_createdSlice{-1};
    // Vero fra il comando di creazione e l'arrivo della slice: la radio non
    // dice sempre nella risposta quale numero le ha dato.
    bool m_awaitingOwnSlice{false};
public:
    QString sharedWith() const override { return m_boundStation; }

private:
    AudioPath m_audioPath{AudioPath::Auto};
    AudioPath m_pathInUse{AudioPath::Auto};
    int       m_daxChannel{1};
    // Il flusso DAX c'e', ma finche' non ne esce un pacchetto non si sa se il
    // canale e' davvero quello assegnato alla slice.
    QTimer    m_audioWatch;
    bool      m_audioSeen{false};
    bool      m_txDaxEnabled{false};
    // Due legami, non uno. Il flusso di ricezione e quello di trasmissione
    // nascono da due comandi diversi e la radio risponde quando le pare: con un
    // flag solo, il primo che arriva dichiara fatto anche il lavoro dell'altro,
    // e il comando che lega il canale DAX alla slice in trasmissione non parte
    // mai. Il PTT chiude lo stesso, e va in aria una portante muta.
    bool      m_daxRxBound{false};
    bool      m_daxTxBound{false};

    quint32 m_rxStreamId{0};
    quint32 m_txStreamId{0};
    bool    m_transmitting{false};
    int     m_signalDbm{-140};
    quint16 m_levelMeterId{0};
};

} // namespace decortty::flex
