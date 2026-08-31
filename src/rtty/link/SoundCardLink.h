// DecoRTTY — la radio presa da una scheda audio.
//
// Il resto del programma parla con la radio attraverso VITA-49, ed è la strada
// migliore quando si può percorrere: audio e comandi passano dallo stesso
// canale, senza cavi virtuali e senza contendersi una scheda. Ma non si può
// percorrere sempre.
//
// Chi ha SmartSDR aperto trova la radio già occupata, e girare attorno al
// problema costa meno che risolverlo: SmartSDR l'audio lo mette già su un canale
// DAX, e da lì lo può prendere chiunque. Lo stesso vale per un cavo audio
// virtuale, per una scheda esterna, o per una radio qualunque attaccata a un
// ingresso microfonico — un FT-817, un vecchio apparato con l'interfaccia
// autocostruita, quello che c'è.
//
// Quello che si perde è il controllo: niente frequenza letta, niente modo,
// niente PTT via CAT. La frequenza la scrive l'operatore se gli serve nel log, e
// il trasmettitore si eccita in VOX — che è come ha funzionato il RTTY per
// cinquant'anni prima che i CAT esistessero.
#pragma once

#include "gateway/CodecAudio.h"
#include "link/RadioLink.h"

#include <QString>

namespace decortty::link {

class SoundCardLink : public RadioLink {
    Q_OBJECT

public:
    explicit SoundCardLink(QObject* parent = nullptr);
    ~SoundCardLink() override;

    // I nomi dei dispositivi si cercano come sottostringa, senza distinguere
    // maiuscole: "DAX Audio RX 1" basta scriverlo "dax 1". Il dispositivo di
    // uscita può restare vuoto: si riceve soltanto.
    bool open(const QString& captureHint, const QString& playbackHint);

    bool    isConnected() const override { return m_audio.isRunning(); }
    QString statusText() const override  { return m_status; }
    QString radioName() const override   { return m_name; }
    // Non c'è nessun CAT da cui leggerle.
    double  frequencyMhz() const override { return m_frequencyMhz; }
    QString mode() const override         { return QStringLiteral("SOUND"); }
    bool    isTransmitting() const override { return m_transmitting; }
    bool    canTransmit() const override;
    // Da una scheda audio non si comanda l'apparato: la frequenza la scrive
    // l'operatore, e la banda si cambia con la manopola.
    bool    canControl() const override { return false; }
    int     signalStrengthDbm() const override { return -200; }

    void disconnectRadio() override;
    // Senza CAT non c'è niente da comandare: questi restano senza effetto, e
    // l'interfaccia lo mostra disattivando i comandi che li userebbero.
    void setFrequencyMhz(double mhz) override;
    void setMode(const QString&) override {}
    void setFilter(int, int) override {}
    void applyRttyProfile(int, int) override {}
    void setTransmit(bool on) override;

    int sendTransmitAudio(const float* samples, int count) override;

private:
    void setStatus(const QString& text);

    gateway::CodecAudio m_audio;
    QString m_status;
    QString m_name;
    double  m_frequencyMhz{0.0};
    bool    m_transmitting{false};
    bool    m_hasPlayback{false};
};

} // namespace decortty::link
