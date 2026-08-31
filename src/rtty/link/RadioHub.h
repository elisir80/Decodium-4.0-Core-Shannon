// DecoRTTY — the single radio object the interface talks to.
//
// Runs discovery once for both kinds of radio, decides which link to build when
// the operator picks one, and forwards the link's state on as its own. The
// interface binds to `radio.frequencyMhz` without ever asking whether there is a
// FlexRadio or a Yaesu behind it.
#pragma once

#include "flex/FlexDiscovery.h"
#include "flex/FlexRadioLink.h"
#include "link/GatewayLink.h"
#include "link/SoundCardLink.h"

#include <QObject>
#include <QVariantList>

#include <memory>

namespace decortty::link {

class RadioHub : public QObject {
    Q_OBJECT

    Q_PROPERTY(bool discovering READ discovering NOTIFY discoveringChanged)
    Q_PROPERTY(bool connected READ connected NOTIFY connectionChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)
    Q_PROPERTY(QString radioName READ radioName NOTIFY connectionChanged)
    Q_PROPERTY(QVariantList radioList READ radioList NOTIFY radioListChanged)
    Q_PROPERTY(double frequencyMhz READ frequencyMhz NOTIFY sliceChanged)
    Q_PROPERTY(QString mode READ mode NOTIFY sliceChanged)
    Q_PROPERTY(bool transmitting READ transmitting NOTIFY transmittingChanged)
    Q_PROPERTY(bool canTransmit READ canTransmit NOTIFY connectionChanged)
    Q_PROPERTY(int signalStrengthDbm READ signalStrengthDbm NOTIFY metersChanged)
    // True while the connected radio is an FT-991A behind a gateway. The
    // interface uses it to label the connection honestly rather than implying
    // everything is a network SDR.
    Q_PROPERTY(bool viaGateway READ viaGateway NOTIFY connectionChanged)
    // Vero quando l'audio arriva da una scheda del PC: niente frequenza letta,
    // niente PTT, e l'interfaccia deve dirlo invece di mostrare comandi che non
    // fanno nulla.
    Q_PROPERTY(bool viaSoundCard READ viaSoundCard NOTIFY connectionChanged)
    // La stazione con cui si condivide il FlexRadio, vuoto se si lavora da soli.
    Q_PROPERTY(QString sharedWith READ sharedWith NOTIFY connectionChanged)
    // Vero quando banda e modo si possono davvero cambiare. Dietro il gateway
    // vuol dire che la porta seriale e' nostra: se un altro programma la tiene,
    // i comandi non partono e l'interfaccia deve dirlo invece di fingere.
    Q_PROPERTY(bool canControl READ canControl NOTIFY connectionChanged)
    // Il piano di banda, per la barra sopra il waterfall.
    Q_PROPERTY(QVariantList bands READ bands CONSTANT)
    // In quale banda siamo, -1 se fuori da tutte. Segue la radio anche quando a
    // spostarla e' la manopola o un altro programma.
    Q_PROPERTY(int currentBand READ currentBand NOTIFY sliceChanged)
    // Come presentarsi a un FlexRadio: 0 da se', 1 stazione GUI, 2 client
    // secondario. Da stazione GUI si occupa un posto MultiFlex ma si vedono
    // sempre le proprie slice; da client secondario si vedono quelle di chi ha
    // gia' la radio — quando la radio le concede.
    Q_PROPERTY(int flexRole READ flexRole WRITE setFlexRole NOTIFY flexRoleChanged)
    // Da dove prendere l'audio di un FlexRadio: 0 da se', 1 DAX, 2 audio del
    // client remoto. DAX e' un rubinetto sulla singola slice, a livello fisso e
    // non compresso; l'audio del client e' il misto delle slice come lo sente
    // chi ha le cuffie, e in trasmissione vuole Opus.
    Q_PROPERTY(int flexAudioPath READ flexAudioPath WRITE setFlexAudioPath NOTIFY flexAudioPathChanged)
    // Il canale DAX assegnato alla slice in SmartSDR, da 1 a 8.
    Q_PROPERTY(int flexDaxChannel READ flexDaxChannel WRITE setFlexDaxChannel NOTIFY flexAudioPathChanged)
    // Dove viene scritto il registro del dialogo con la radio. Si mostra perche'
    // il ramo FlexRadio non e' ancora stato provato su un apparato: chi lo prova
    // per primo deve sapere cosa mandarci indietro.
    Q_PROPERTY(QString radioLogPath READ radioLogPath CONSTANT)
    // I modi che hanno senso per un decodificatore AFSK. RTTY-U e RTTY-L non ci
    // sono apposta: la' l'apparato aspetta il tasto FSK e l'audio non uscirebbe.
    Q_PROPERTY(QStringList modes READ modes CONSTANT)

public:
    explicit RadioHub(QObject* parent = nullptr);
    ~RadioHub() override;

    bool    discovering() const { return m_discovery.listening(); }
    bool    connected() const;
    QString statusText() const;
    QString radioName() const;
    QVariantList radioList() const;
    double  frequencyMhz() const;
    QString mode() const;
    bool    transmitting() const;
    bool    canTransmit() const;
    int     signalStrengthDbm() const;
    bool    viaGateway() const { return m_viaGateway; }
    bool    viaSoundCard() const { return m_viaSoundCard; }
    // I dispositivi in uso, per poterli riproporre al prossimo avvio.
    QString captureInUse() const  { return m_captureHint; }
    QString playbackInUse() const { return m_playbackHint; }
    QString sharedWith() const;
    bool    canControl() const;
    int     flexRole() const { return m_flexRole; }
    void    setFlexRole(int role);
    int     flexAudioPath() const { return m_flexAudioPath; }
    void    setFlexAudioPath(int path);
    int     flexDaxChannel() const { return m_flexDaxChannel; }
    void    setFlexDaxChannel(int channel);
    QString radioLogPath() const;
    QVariantList bands() const;
    int     currentBand() const;
    QStringList  modes() const;

    Q_INVOKABLE void startDiscovery();
    Q_INVOKABLE void stopDiscovery();
    Q_INVOKABLE void connectToRadio(const QString& serial);
    // Manual connect. A gateway and a FlexRadio cannot be told apart from an
    // address alone, so the caller says which it is.
    Q_INVOKABLE void connectToAddress(const QString& address, bool gateway = false);
    // Collegamento a una scheda audio invece che a una radio in rete: si ascolta
    // quello che un altro programma — SmartSDR via DAX, un cavo virtuale — mette
    // su un dispositivo. Il dispositivo di uscita puo' restare vuoto: si riceve
    // e basta.
    Q_INVOKABLE void connectToSoundCard(const QString& captureHint,
                                        const QString& playbackHint);
    // I dispositivi disponibili, per i menu delle impostazioni.
    Q_INVOKABLE QStringList captureDevices() const;
    Q_INVOKABLE QStringList playbackDevices() const;
    Q_INVOKABLE void disconnectRadio();

    // Porta la radio sul segmento RTTY della banda scelta. Un secondo tocco
    // sulla banda in cui siamo gia' non fa niente: chi lo preme si aspetta di
    // essere gia' arrivato, non di essere riportato indietro dalla frequenza
    // che si e' cercato.
    Q_INVOKABLE void tuneToBand(int index);

    Q_INVOKABLE void setFrequencyMhz(double mhz);
    Q_INVOKABLE void setMode(const QString& mode);
    Q_INVOKABLE void setFilter(int lowHz, int highHz);
    Q_INVOKABLE void applyRttyProfile(int markHz, int shiftHz);
    Q_INVOKABLE void setTransmit(bool on);

    // Used by the engine for the transmit path.
    int sendTransmitAudio(const float* samples, int count);

signals:
    void discoveringChanged();
    void connectionChanged();
    void statusTextChanged();
    void radioListChanged();
    void flexRoleChanged();
    void flexAudioPathChanged();
    void sliceChanged();
    void transmittingChanged();
    void metersChanged();
    void errorOccurred(const QString& message);
    void audioReady(const std::vector<float>& samples, int frames);

private:
    void adopt(RadioLink* link, bool gateway);
    void releaseLink();

    flex::FlexDiscovery m_discovery;
    RadioLink*          m_link{nullptr};   // owned; one at a time
    bool                m_viaGateway{false};
    bool                m_viaSoundCard{false};
    int                 m_flexRole{0};
    int                 m_flexAudioPath{0};
    int                 m_flexDaxChannel{1};
    QString             m_captureHint;
    QString             m_playbackHint;
    QString             m_idleStatus;
};

} // namespace decortty::link
