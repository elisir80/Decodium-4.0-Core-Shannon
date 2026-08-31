// DecoRTTY gateway — Yaesu FT-991A CAT control.
//
// The FT-991A speaks a terminated-string protocol over a virtual serial port:
// each command ends in ';', sets are not acknowledged, queries answer with a
// frame of the same name. Defaults are 38400 baud with 8N2 framing (menu 031),
// and the radio presents two ports through its CP2105 bridge — the *Enhanced*
// one carries CAT.
//
// Only one query is allowed in flight. The radio will drop a second query sent
// before it has answered the first, and the resulting confusion looks exactly
// like a flaky cable, so the queue enforces it.
#pragma once

#include <QByteArray>
#include <QObject>
#include <QQueue>
#include <QSerialPort>
#include <QString>
#include <QTimer>

namespace decortty::gateway {

class Ft991Cat : public QObject {
    Q_OBJECT

public:
    struct Config {
        QString portName{QStringLiteral("COM5")};   // the Enhanced CP2105 port
        int     baudRate{38400};
        int     pollIntervalMs{250};
        int     replyTimeoutMs{600};
    };

    // State the gateway publishes as VITA-49 context.
    struct State {
        double  frequencyHz{0.0};
        QString mode;                // neutral name: USB, DIGU, DIGL, CW…
        bool    transmitting{false};
        int     sMeterRaw{0};        // 0..255 as the radio reports it
        bool    online{false};
    };

    explicit Ft991Cat(QObject* parent = nullptr);
    ~Ft991Cat() override;

    bool open(const Config& config);
    void close();
    bool isOpen() const;

    const State& state() const { return m_state; }
    QString portName() const { return m_config.portName; }

    // ── control ─────────────────────────────────────────────────────────
    void setFrequency(double hz);
    // Neutral mode name. DIGU maps to DATA-USB, which is what an AFSK RTTY
    // signal wants on this radio: the USB codec feeds the modulator audio
    // straight into the transmitter with the speech processing out of circuit.
    void setMode(const QString& neutralMode);
    void setTransmit(bool on);

    // S-meter in dBm, converted from the radio's 0..255 scale.
    static double sMeterToDbm(int raw);

    // Map a neutral mode name to the FT-991A's MD0 code, and back. Returns a
    // null QChar when the name has no mode on this radio.
    static QChar modeToCat(const QString& neutralMode);
    static QString catToMode(QChar code);

signals:
    void stateChanged(const State& state);
    void errorOccurred(const QString& message);
    void opened();
    void closed();

private slots:
    void onReadyRead();
    void onPoll();
    void onReplyTimeout();

private:
    void enqueue(const QByteArray& command, bool expectsReply);
    void pump();
    void handleFrame(const QByteArray& frame);

    struct Pending {
        QByteArray command;
        bool       expectsReply{false};
    };

    QSerialPort    m_port;
    QByteArray     m_buffer;
    QQueue<Pending> m_queue;
    bool           m_waitingReply{false};
    QTimer         m_pollTimer;
    QTimer         m_replyTimer;
    Config         m_config;
    State          m_state;
    // The periodic query rotates through dial state, S-meter and transmit
    // state, so a busy poll never starves any of them.
    int            m_pollPhase{0};
    // What we last asked the transmitter to do, and when. The radio does not
    // acknowledge sets, so the only way to know a PTT command actually landed is
    // to keep reading the state back and repeat the command if it disagrees.
    // Getting this wrong leaves a transmitter keyed, which is the one failure
    // here that must not be possible.
    bool           m_desiredTx{false};
    qint64         m_desiredTxSetMs{0};
    int            m_txRetries{0};
};

} // namespace decortty::gateway
