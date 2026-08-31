#include "gateway/Ft991Cat.h"

#include <QDateTime>
#include <QtGlobal>

#include <cmath>

namespace decortty::gateway {

namespace {

// The FT-991A MD0 mode table. One table for both directions, so they cannot
// drift apart. `preferred` marks the row modeToCat picks when several codes map
// to the same neutral name: DIGU has both RTTY-USB ('9') and DATA-USB ('C'), and
// DATA-USB is the one an AFSK signal wants.
struct ModeRow {
    char        code;
    const char* neutral;
    bool        preferred;
};

constexpr ModeRow kModes[] = {
    {'1', "LSB",    true},
    {'2', "USB",    true},
    {'3', "CW",     true},
    {'4', "FM",     true},
    {'5', "AM",     true},
    {'6', "RTTY-L", true},    // RTTY-LSB — true FSK, keyed by hardware
    {'7', "CWL",    true},
    {'8', "DIGL",   true},    // DATA-LSB
    {'9', "RTTY-U", true},    // RTTY-USB — true FSK
    {'B', "NFM",    true},
    {'C', "DIGU",   true},    // DATA-USB — the AFSK data mode
    {'D', "AM-N",   true},
};

qint64 digitsOf(const QByteArray& frame, int start, int count)
{
    if (start < 0 || count <= 0 || start + count > frame.size())
        return -1;
    qint64 value = 0;
    for (int i = start; i < start + count; ++i) {
        const char c = frame.at(i);
        if (c < '0' || c > '9')
            return -1;
        value = value * 10 + (c - '0');
    }
    return value;
}

} // namespace

Ft991Cat::Ft991Cat(QObject* parent)
    : QObject(parent)
{
    connect(&m_port, &QSerialPort::readyRead, this, &Ft991Cat::onReadyRead);
    connect(&m_port, &QSerialPort::errorOccurred, this,
            [this](QSerialPort::SerialPortError error) {
                if (error == QSerialPort::NoError)
                    return;
                emit errorOccurred(m_port.errorString());
                if (error == QSerialPort::ResourceError)
                    close();
            });

    connect(&m_pollTimer, &QTimer::timeout, this, &Ft991Cat::onPoll);
    m_replyTimer.setSingleShot(true);
    connect(&m_replyTimer, &QTimer::timeout, this, &Ft991Cat::onReplyTimeout);
}

Ft991Cat::~Ft991Cat()
{
    close();
}

QChar Ft991Cat::modeToCat(const QString& neutralMode)
{
    const QString wanted = neutralMode.toUpper();
    for (const ModeRow& row : kModes) {
        if (row.preferred && wanted == QLatin1String(row.neutral))
            return QLatin1Char(row.code);
    }
    return {};
}

QString Ft991Cat::catToMode(QChar code)
{
    for (const ModeRow& row : kModes) {
        if (code == QLatin1Char(row.code))
            return QString::fromLatin1(row.neutral);
    }
    return {};
}

double Ft991Cat::sMeterToDbm(int raw)
{
    // The FT-991A reports the S-meter as 0..255 with S9 at about 127. Nine S
    // units below S9 is -127 dBm at 6 dB each; above S9 the scale continues at
    // roughly the same slope. This is a calibration of the displayed meter, not
    // a measurement — good enough to drive a bar, not to compare stations.
    const double units = static_cast<double>(raw) / 127.0 * 9.0;
    return -127.0 + units * 6.0;
}

bool Ft991Cat::open(const Config& config)
{
    close();
    m_config = config;

    m_port.setPortName(config.portName);
    m_port.setBaudRate(config.baudRate);
    m_port.setDataBits(QSerialPort::Data8);
    m_port.setParity(QSerialPort::NoParity);
    // Two stop bits: the FT-991A's CAT framing is 8N2, and 8N1 gives
    // intermittent framing errors that look like a bad cable.
    m_port.setStopBits(QSerialPort::TwoStop);
    m_port.setFlowControl(QSerialPort::NoFlowControl);

    if (!m_port.open(QIODevice::ReadWrite)) {
        emit errorOccurred(tr("Cannot open %1: %2")
                               .arg(config.portName, m_port.errorString()));
        return false;
    }

    m_buffer.clear();
    m_queue.clear();
    m_waitingReply = false;
    m_state = State{};

    m_replyTimer.setInterval(config.replyTimeoutMs);
    m_pollTimer.setInterval(config.pollIntervalMs);
    m_pollTimer.start();

    // Ask straight away, so the first context packet carries real state rather
    // than zeros.
    enqueue(QByteArrayLiteral("IF;"), true);
    emit opened();
    return true;
}

void Ft991Cat::close()
{
    // Last chance to unkey: write it straight to the port, synchronously, before
    // anything is torn down.
    if (m_port.isOpen() && (m_state.transmitting || m_desiredTx)) {
        m_port.write(QByteArrayLiteral("TX0;"));
        m_port.waitForBytesWritten(200);
    }
    m_pollTimer.stop();
    m_replyTimer.stop();
    m_queue.clear();
    m_waitingReply = false;
    if (m_port.isOpen()) {
        m_port.close();
        m_state.online = false;
        emit closed();
    }
}

bool Ft991Cat::isOpen() const
{
    return m_port.isOpen();
}

void Ft991Cat::enqueue(const QByteArray& command, bool expectsReply)
{
    if (!m_port.isOpen())
        return;
    // Never let a stalled radio grow the queue without bound; the poll timer
    // would otherwise keep adding queries the radio is not answering.
    if (m_queue.size() > 32)
        m_queue.dequeue();
    m_queue.enqueue(Pending{command, expectsReply});
    pump();
}

void Ft991Cat::pump()
{
    if (m_waitingReply || m_queue.isEmpty() || !m_port.isOpen())
        return;

    const Pending next = m_queue.dequeue();
    m_port.write(next.command);

    if (next.expectsReply) {
        m_waitingReply = true;
        m_replyTimer.start();
    } else {
        // Sets are not acknowledged, so confirm with a query rather than
        // assuming the radio did what it was told.
        pump();
    }
}

void Ft991Cat::onReplyTimeout()
{
    m_waitingReply = false;
    if (m_state.online) {
        m_state.online = false;
        emit stateChanged(m_state);
    }
    pump();
}

void Ft991Cat::onPoll()
{
    if (m_waitingReply)
        return;   // still working through the last one

    // Transmit state is polled like everything else, not merely inferred from
    // the reply to our own command. A dropped reply must not be able to leave
    // the software believing the radio is in a state it is not in.
    switch (m_pollPhase) {
    case 0:  enqueue(QByteArrayLiteral("IF;"), true);  break;
    case 1:  enqueue(QByteArrayLiteral("TX;"), true);  break;
    default:
        if (m_state.transmitting)
            enqueue(QByteArrayLiteral("TX;"), true);
        else
            enqueue(QByteArrayLiteral("SM0;"), true);
        break;
    }
    m_pollPhase = (m_pollPhase + 1) % 3;

    // Re-assert a PTT command the radio never acted on. Repeating a set is
    // harmless — it is idempotent — and the alternative is a transmitter that
    // stays keyed because one serial frame was corrupted.
    if (m_state.online && m_state.transmitting != m_desiredTx && m_desiredTxSetMs > 0) {
        const qint64 age = QDateTime::currentMSecsSinceEpoch() - m_desiredTxSetMs;
        if (age > 700 && m_txRetries < 5) {
            ++m_txRetries;
            emit errorOccurred(tr("Radio still shows %1 — repeating the PTT command (%2)")
                                   .arg(m_state.transmitting ? tr("transmit") : tr("receive"))
                                   .arg(m_txRetries));
            enqueue(m_desiredTx ? QByteArrayLiteral("TX1;") : QByteArrayLiteral("TX0;"), false);
            enqueue(QByteArrayLiteral("TX;"), true);
            m_desiredTxSetMs = QDateTime::currentMSecsSinceEpoch();
        }
    }
}

void Ft991Cat::onReadyRead()
{
    m_buffer.append(m_port.readAll());

    int end;
    while ((end = m_buffer.indexOf(';')) >= 0) {
        const QByteArray frame = m_buffer.left(end);
        m_buffer.remove(0, end + 1);
        if (!frame.isEmpty())
            handleFrame(frame);
    }

    // A radio that answers is a radio that is there.
    if (m_waitingReply) {
        m_waitingReply = false;
        m_replyTimer.stop();
        pump();
    }
}

void Ft991Cat::handleFrame(const QByteArray& frame)
{
    State updated = m_state;
    updated.online = true;
    bool changed = !m_state.online;

    if (frame.startsWith("IF") && frame.size() == 27) {
        // IF response layout, ';' already stripped:
        //   [0..1] "IF"   [2..4] memory channel   [5..13] VFO-A frequency
        //   [14..18] clarifier   [19] RIT   [20] XIT   [21] mode   …
        const qint64 hz = digitsOf(frame, 5, 9);
        const QString mode = catToMode(QLatin1Char(frame.at(21)));
        if (hz > 0 && !mode.isEmpty()) {
            if (!qFuzzyCompare(updated.frequencyHz, static_cast<double>(hz))) {
                updated.frequencyHz = static_cast<double>(hz);
                changed = true;
            }
            if (updated.mode != mode) {
                updated.mode = mode;
                changed = true;
            }
        }
    } else if (frame.startsWith("SM0")) {
        const qint64 raw = digitsOf(frame, 3, frame.size() - 3);
        if (raw >= 0 && raw <= 255 && updated.sMeterRaw != static_cast<int>(raw)) {
            updated.sMeterRaw = static_cast<int>(raw);
            changed = true;
        }
    } else if (frame.startsWith("TX") && frame.size() == 3) {
        // TX0 = receive, TX1 = transmitting under CAT, TX2 = keyed by the
        // radio's own PTT. Both non-zero states mean the transmitter is up.
        const bool tx = frame.at(2) != '0';
        if (updated.transmitting != tx) {
            updated.transmitting = tx;
            changed = true;
        }
    } else if (frame.startsWith("FA") && frame.size() == 11) {
        const qint64 hz = digitsOf(frame, 2, 9);
        if (hz > 0 && !qFuzzyCompare(updated.frequencyHz, static_cast<double>(hz))) {
            updated.frequencyHz = static_cast<double>(hz);
            changed = true;
        }
    } else if (frame.startsWith("MD0") && frame.size() == 4) {
        const QString mode = catToMode(QLatin1Char(frame.at(3)));
        if (!mode.isEmpty() && updated.mode != mode) {
            updated.mode = mode;
            changed = true;
        }
    }

    if (changed) {
        m_state = updated;
        emit stateChanged(m_state);
    } else {
        m_state = updated;
    }
}

void Ft991Cat::setFrequency(double hz)
{
    if (hz <= 0.0)
        return;
    const qint64 rounded = static_cast<qint64>(std::llround(hz));
    enqueue(QByteArrayLiteral("FA")
                + QByteArray::number(rounded).rightJustified(9, '0')
                + ';',
            false);
    enqueue(QByteArrayLiteral("FA;"), true);   // confirm from the radio
}

void Ft991Cat::setMode(const QString& neutralMode)
{
    const QChar code = modeToCat(neutralMode);
    if (code.isNull()) {
        emit errorOccurred(tr("The FT-991A has no mode called '%1'").arg(neutralMode));
        return;
    }
    enqueue(QByteArrayLiteral("MD0") + code.toLatin1() + ';', false);
    enqueue(QByteArrayLiteral("MD0;"), true);
}

void Ft991Cat::setTransmit(bool on)
{
    m_desiredTx      = on;
    m_desiredTxSetMs = QDateTime::currentMSecsSinceEpoch();
    m_txRetries      = 0;

    // Unkeying jumps the queue. Anything already waiting is a query about
    // frequency or signal strength, and none of it matters more than getting the
    // transmitter to stop.
    if (!on) {
        m_queue.clear();
        m_waitingReply = false;
        m_replyTimer.stop();
        if (m_port.isOpen()) {
            const qint64 written = m_port.write(QByteArrayLiteral("TX0;"));
            emit errorOccurred(tr("CAT: wrote TX0; (%1 bytes) on %2")
                                   .arg(written).arg(m_config.portName));
        }
    } else {
        enqueue(QByteArrayLiteral("TX1;"), false);
        emit errorOccurred(tr("CAT: queued TX1; on %1").arg(m_config.portName));
    }
    enqueue(QByteArrayLiteral("TX;"), true);
}

} // namespace decortty::gateway
