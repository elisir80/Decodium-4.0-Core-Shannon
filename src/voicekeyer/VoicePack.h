// Pacchetto vocale opzionale: il motore di sintesi Piper (eseguibile separato,
// licenza MIT) e due voci neurali, italiana e inglese. Si scarica una volta sola
// da una sorgente fissata, con SHA256 verificato per ogni file, e poi resta sul
// disco: nessun account, nessun servizio online durante l'uso.
#pragma once

#include <QObject>
#include <QString>

class QNetworkAccessManager;

namespace decodium::voicekeyer {

class VoicePack : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool installed READ installed NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(double progress READ progress NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)

public:
    explicit VoicePack(const QString& root, QObject* parent = nullptr);

    bool installed() const;
    bool busy() const { return m_busy; }
    double progress() const { return m_progress; }
    QString status() const { return m_status; }

    // Percorsi dentro il pacchetto (validi solo se installed()).
    QString piperExecutable() const;
    QString voiceModel(const QString& lang) const;   // "it" o "en"
    QString speechModel() const;                     // modello whisper multilingue
    QString whisperExecutable() const;               // motore di riconoscimento (whisper-cli)
    QString runtimeDirectory() const;
    QString root() const { return m_root; }

    // Scarica, verifica ed estrae tutto. Non fa nulla se e' gia' installato.
    void install();
    void cancel();

signals:
    void changed();

private:
    struct Item {
        QString url;
        QString sha256;
        qint64 bytes;
        QString target;    // percorso relativo alla radice del pacchetto
        bool archive;      // zip da estrarre nella radice
    };
    static const QVector<Item>& manifest();
    void fetchNext();
    void finishFile(const QByteArray& data);
    void extractArchive(const QString& file);
    void fail(const QString& text);
    void done();

    QString m_root;
    QNetworkAccessManager* m_network;
    QVector<Item> m_queue;
    int m_index {0};
    qint64 m_doneBytes {0};
    bool m_busy {false};
    double m_progress {0.0};
    QString m_status;
    QString m_currentFile;
};

}  // namespace decodium::voicekeyer
