// Voice keyer: otto messaggi preregistrati da mandare in SSB con un tasto.
//
// Ogni slot ha un'etichetta, un testo di riferimento e, se registrato, un file
// WAV. Il messaggio esce dallo stesso percorso di trasmissione della finestra
// SSB (PTT, audio a 12 kHz, rilascio): il modulo non apre niente da solo e non
// trasmette mai senza che l'operatore abbia premuto Play.
#pragma once

#include <QObject>
#include <QString>
#include <QTimer>
#include <QVariantList>
#include <QVector>

#include "VoicePack.h"

#include <functional>
#include <optional>

class QAudioSource;
class QIODevice;

namespace decodium::voicekeyer {

constexpr int kSlots = 8;
constexpr int kRadioRate = 12000;   // il trasporto audio della radio
constexpr int kCaptureRate = 48000; // cattura del microfono, poi ricampionata
constexpr int kPacket = 480;        // 40 ms a 12 kHz
constexpr int kMaxSeconds = 60;     // durata massima di un messaggio registrato

// Lettura di un WAV PCM 16 bit (mono o stereo, canale sinistro): il risultato
// e' a kRadioRate. Nullopt con il motivo in *error se il file non va bene.
std::optional<QVector<short>> readWav(const QString& path, QString* error = nullptr);
bool writeWav(const QString& path, const QVector<short>& samples, int rate, QString* error = nullptr);
QVector<short> resampleLinear(const QVector<short>& in, int fromRate, int toRate);

class VoiceKeyer : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList messages READ messages NOTIFY messagesChanged)
    Q_PROPERTY(bool playing READ playing NOTIFY changed)
    Q_PROPERTY(bool recording READ recording NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(bool voicePackInstalled READ voicePackInstalled NOTIFY changed)
    Q_PROPERTY(bool voicePackBusy READ voicePackBusy NOTIFY changed)
    Q_PROPERTY(double voicePackProgress READ voicePackProgress NOTIFY changed)
    Q_PROPERTY(QString voicePackStatus READ voicePackStatus NOTIFY changed)

public:
    struct Hooks {
        std::function<bool()> begin;                         // alza il PTT, false = rifiutato
        std::function<void()> end;                           // abbassa il PTT
        std::function<bool()> valid;                         // la radio e' ancora pronta a trasmettere
        std::function<void(const QVector<short>&)> send;     // un pacchetto da 40 ms a 12 kHz
        std::function<int()> leadMs;                         // attesa prima dell'audio
    };

    VoiceKeyer(const QString& directory, const QString& voicePackRoot, QObject* parent = nullptr);
    ~VoiceKeyer() override;

    void setHooks(Hooks h) { m_hooks = std::move(h); }

    QVariantList messages() const;
    bool playing() const { return m_playing; }
    bool recording() const { return m_recording; }
    QString status() const { return m_status; }
    bool voicePackInstalled() const { return m_pack->installed(); }
    bool voicePackBusy() const { return m_pack->busy(); }
    double voicePackProgress() const { return m_pack->progress(); }
    QString voicePackStatus() const { return m_pack->status(); }

    Q_INVOKABLE void setSlotText(int index, const QString& label, const QString& text);
    Q_INVOKABLE void play(int index);
    Q_INVOKABLE void stop();
    Q_INVOKABLE bool startRecording(int index);
    Q_INVOKABLE void stopRecording();
    Q_INVOKABLE void clearSlot(int index);
    Q_INVOKABLE void setSlotLanguage(int index, const QString& lang);
    Q_INVOKABLE void installVoicePack();
    Q_INVOKABLE void cancelVoicePack();

signals:
    void changed();
    // Solo quando cambiano testi, lingue o registrazioni: le righe della QML non si ricreano
    // a ogni passo del download.
    void messagesChanged();

private:
    struct Slot {
        QString label;
        QString text;
        QString lang {QStringLiteral("it")};
    };
    void startPlayback(const QVector<short>& pcm, const QString& name);

    QString wavPath(int index) const;
    void load();
    void save();
    void setStatus(const QString& text);
    void tick();
    void finish(const QString& message);
    void captureReady();
    void finishRecording();

    QString m_directory;
    VoicePack* m_pack {nullptr};
    QVector<Slot> m_slots;
    Hooks m_hooks;

    QVector<short> m_pcm;
    int m_pos {0};
    int m_tail {0};
    bool m_playing {false};
    QTimer m_lead;
    QTimer m_pace;

    bool m_recording {false};
    int m_recordIndex {-1};
    QByteArray m_captured;
    QAudioSource* m_source {nullptr};
    QIODevice* m_input {nullptr};

    QString m_status;
};

}  // namespace decodium::voicekeyer
