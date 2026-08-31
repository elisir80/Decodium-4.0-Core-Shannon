#include "link/SoundCardLink.h"

#include <vector>

namespace decortty::link {

SoundCardLink::SoundCardLink(QObject* parent)
    : RadioLink(parent)
{
    // L'audio arriva mono a 24 kHz; il resto del programma lo aspetta
    // intercalato su due canali, come lo manda una radio in rete. Si duplica il
    // canale invece di cambiare l'interfaccia: dietro questa cucitura tutte le
    // radio devono restare uguali.
    connect(&m_audio, &gateway::CodecAudio::audioReady, this,
            [this](const std::vector<float>& mono) {
                if (mono.empty())
                    return;
                std::vector<float> stereo(mono.size() * 2);
                for (size_t i = 0; i < mono.size(); ++i) {
                    stereo[i * 2]     = mono[i];
                    stereo[i * 2 + 1] = mono[i];
                }
                emit audioReady(stereo, static_cast<int>(mono.size()));
            });

    connect(&m_audio, &gateway::CodecAudio::errorOccurred, this,
            &SoundCardLink::errorOccurred);
}

SoundCardLink::~SoundCardLink()
{
    m_audio.stop();
}

bool SoundCardLink::open(const QString& captureHint, const QString& playbackHint)
{
    m_hasPlayback = !playbackHint.trimmed().isEmpty();

    if (!m_audio.start(captureHint, playbackHint)) {
        setStatus(tr("Cannot open the sound card"));
        emit connectionChanged();
        return false;
    }

    m_name = m_audio.captureDeviceName();
    setStatus(m_hasPlayback ? tr("Listening to %1").arg(m_name)
                            : tr("Listening to %1 — receive only").arg(m_name));
    emit connectionChanged();
    emit sliceChanged();
    return true;
}

void SoundCardLink::disconnectRadio()
{
    if (m_transmitting)
        setTransmit(false);
    m_audio.stop();
    setStatus(tr("Disconnected"));
    emit connectionChanged();
}

void SoundCardLink::setFrequencyMhz(double mhz)
{
    // Non c'è nessuna radio a cui dirlo, ma il numero serve al registro: è
    // quello che l'operatore legge sulla scala e scrive qui.
    if (qFuzzyCompare(m_frequencyMhz, mhz))
        return;
    m_frequencyMhz = mhz;
    emit sliceChanged();
}

bool SoundCardLink::canTransmit() const
{
    return m_audio.isRunning() && m_hasPlayback;
}

void SoundCardLink::setTransmit(bool on)
{
    if (m_transmitting == on)
        return;
    if (on && !canTransmit())
        return;

    m_transmitting = on;
    if (!on)
        m_audio.flushTransmit();

    // Nessun PTT da comandare: il trasmettitore si eccita in VOX quando l'audio
    // comincia ad arrivare, e ricade quando smette. Per questo il modulatore
    // deve avere una coda di silenzio a fine invio — ce l'ha — e per questo il
    // VOX va regolato con un ritardo abbastanza lungo da non cadere fra un
    // carattere e l'altro.
    emit transmittingChanged();
}

int SoundCardLink::sendTransmitAudio(const float* samples, int count)
{
    if (!canTransmit() || count <= 0)
        return 0;
    m_audio.writeTransmitAudio(samples, count);
    return count;
}

void SoundCardLink::setStatus(const QString& text)
{
    if (m_status == text)
        return;
    m_status = text;
    emit statusTextChanged();
}

} // namespace decortty::link
