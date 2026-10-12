// Sintesi vocale locale con Piper: il testo entra da stdin, il PCM esce da
// stdout. Il motore e' un processo separato del pacchetto vocale, quindi la
// sua licenza non si mescola col codice di Decodium.
#pragma once

#include <QString>
#include <QVector>

#include <optional>

namespace decodium::voicekeyer {

class PiperTts {
public:
    // Sintetizza `text` con il modello `modelPath` e restituisce campioni a 12 kHz.
    // Nullopt con il motivo in *error se il motore non parte o non risponde.
    static std::optional<QVector<short>> synthesize(const QString& piperExe,
                                                    const QString& runtimeDir,
                                                    const QString& modelPath,
                                                    const QString& text,
                                                    QString* error = nullptr);
    // Frequenza di campionamento scritta nel file .json accanto al modello.
    static int modelSampleRate(const QString& modelPath);
};

}  // namespace decodium::voicekeyer
