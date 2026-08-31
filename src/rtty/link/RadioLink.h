// DecoRTTY — what the application knows about a radio.
//
// Two very different things sit behind this interface: a FlexRadio, spoken to
// over its own TCP command protocol, and an FT-991A behind the gateway, spoken
// to with VITA-49 command packets. Above the seam they are the same object —
// a dial frequency, a mode, a PTT, and a stream of audio — which is what lets
// the decoder, the interface and the transmit path stay free of any knowledge
// about which radio is connected.
#pragma once

#include <QObject>
#include <QString>

#include <vector>

namespace decortty::link {

class RadioLink : public QObject {
    Q_OBJECT

public:
    explicit RadioLink(QObject* parent = nullptr) : QObject(parent) {}
    ~RadioLink() override = default;

    virtual bool    isConnected() const = 0;
    virtual QString statusText() const = 0;
    // Il nome della stazione con cui si sta condividendo la radio, vuoto quando
    // si lavora da soli. Riguarda solo i FlexRadio, dove piu' programmi possono
    // usare lo stesso apparato insieme.
    virtual QString sharedWith() const { return {}; }
    virtual QString radioName() const = 0;
    virtual double  frequencyMhz() const = 0;
    virtual QString mode() const = 0;
    virtual bool    isTransmitting() const = 0;
    virtual bool    canTransmit() const = 0;
    // Vero quando i comandi di sintonia arrivano davvero all'apparato. Non e' la
    // stessa cosa di canTransmit(): dietro il gateway servono entrambi la porta
    // seriale, ma su un FlexRadio si puo' comandare la slice di una stazione con
    // cui si condivide l'apparato senza avere il diritto di trasmettere. E da una
    // scheda audio non si comanda niente: si ascolta e basta.
    virtual bool    canControl() const { return isConnected(); }
    virtual int     signalStrengthDbm() const = 0;

    virtual void disconnectRadio() = 0;
    virtual void setFrequencyMhz(double mhz) = 0;
    virtual void setMode(const QString& mode) = 0;
    virtual void setFilter(int lowHz, int highHz) = 0;
    // Put the radio into the state RTTY wants: a data mode on the upper
    // sideband, and a filter around the two tones.
    virtual void applyRttyProfile(int markHz, int shiftHz) = 0;
    virtual void setTransmit(bool on) = 0;

    // Modulator audio, mono at 24 kHz. Returns how many packets went out, or 0
    // when the link cannot transmit.
    virtual int sendTransmitAudio(const float* samples, int count) = 0;

signals:
    void connectionChanged();
    void statusTextChanged();
    void sliceChanged();
    void transmittingChanged();
    void metersChanged();
    void errorOccurred(const QString& message);

    // Receive audio: interleaved stereo at 24 kHz.
    void audioReady(const std::vector<float>& samples, int frames);
};

} // namespace decortty::link
