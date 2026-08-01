#include "RotatorService.h"

#include <QDateTime>
#include <QHostInfo>
#include <QRegularExpression>
#include <QTimer>
#include <QUdpSocket>
#include <QtMath>

#include <cmath>

namespace {

constexpr qint64 kFeedbackTimeoutMs = 5000;

bool isFiniteValue(double value)
{
    return qIsFinite(value);
}

}

RotatorService::RotatorService(QObject* parent)
    : QObject(parent),
      m_commandSocket(new QUdpSocket(this)),
      m_feedbackSocket(new QUdpSocket(this)),
      m_trackingTimer(new QTimer(this)),
      m_feedbackTimer(new QTimer(this))
{
    m_trackingTimer->setInterval(m_trackingIntervalMs);
    m_feedbackTimer->setInterval(1500);
    connect(m_trackingTimer, &QTimer::timeout,
            this, &RotatorService::onTrackingTick);
    connect(m_feedbackTimer, &QTimer::timeout,
            this, &RotatorService::onFeedbackTick);
    connect(m_feedbackSocket, &QUdpSocket::readyRead,
            this, &RotatorService::onReadyRead);
}

RotatorService::~RotatorService()
{
    if (m_tracking) {
        m_tracking = false;
        m_trackingTimer->stop();
    }
}

QStringList RotatorService::protocols() const
{
    return {QStringLiteral("PSTRotator"), QStringLiteral("CatRotator")};
}

QString RotatorService::normalizedProtocol(const QString& protocol)
{
    return protocol.compare(QStringLiteral("CatRotator"), Qt::CaseInsensitive) == 0
        ? QStringLiteral("CatRotator")
        : QStringLiteral("PSTRotator");
}

double RotatorService::normalizeAzimuth(double value)
{
    double normalized = std::fmod(value, 360.0);
    if (normalized < 0.0) normalized += 360.0;
    return normalized;
}

void RotatorService::setProtocol(const QString& protocol)
{
    QString const normalized = normalizedProtocol(protocol);
    if (normalized == m_protocol) return;
    if (m_tracking) stopTracking();
    m_protocol = normalized;
    m_feedbackAvailable = false;
    m_lastFeedbackMs = 0;
    closeFeedbackSocket();
    configureFeedbackSocket();
    emit configurationChanged();
    emit feedbackChanged();
    setStatus(m_protocol == QStringLiteral("PSTRotator")
                  ? QStringLiteral("PSTRotator selected")
                  : QStringLiteral("CatRotator selected; UDP feedback unavailable"));
}

void RotatorService::setHost(const QString& host)
{
    QString const normalized = host.trimmed();
    if (normalized.isEmpty() || normalized == m_host) return;
    m_host = normalized;
    m_resolvedHost.clear();
    m_resolvedAddress.clear();
    ++m_resolutionGeneration;
    emit configurationChanged();
}

void RotatorService::setPort(int port)
{
    int const bounded = qBound(1, port, 65535);
    if (bounded == m_port) return;
    if (m_tracking) stopTracking();
    m_port = bounded;
    m_feedbackAvailable = false;
    m_lastFeedbackMs = 0;
    closeFeedbackSocket();
    configureFeedbackSocket();
    emit configurationChanged();
    emit feedbackChanged();
}

void RotatorService::setEnabled(bool enabled)
{
    if (enabled == m_enabled) return;
    if (!enabled && m_enabled) {
        stopTracking();
    }
    m_enabled = enabled;
    if (m_enabled) {
        configureFeedbackSocket();
        m_feedbackTimer->start();
        setStatus(m_protocol == QStringLiteral("PSTRotator")
                      ? QStringLiteral("PSTRotator ready")
                      : QStringLiteral("CatRotator ready; feedback unavailable"));
    } else {
        m_feedbackTimer->stop();
        closeFeedbackSocket();
        m_feedbackAvailable = false;
        m_lastFeedbackMs = 0;
        setStatus(QStringLiteral("Rotator disabled"));
    }
    emit enabledChanged();
    emit transportChanged();
    emit feedbackChanged();
}

void RotatorService::setTrackingIntervalMs(int intervalMs)
{
    int const bounded = qBound(250, intervalMs, 10000);
    if (bounded == m_trackingIntervalMs) return;
    m_trackingIntervalMs = bounded;
    m_trackingTimer->setInterval(m_trackingIntervalMs);
    emit configurationChanged();
}

void RotatorService::setSafetyEnabled(bool enabled)
{
    if (enabled == m_safetyEnabled) return;
    m_safetyEnabled = enabled;
    emit safetyChanged();
}

void RotatorService::setMinAzimuth(double value)
{
    double const bounded = qBound(0.0, value, 360.0);
    if (qFuzzyCompare(bounded, m_minAzimuth)) return;
    m_minAzimuth = bounded;
    emit safetyChanged();
}

void RotatorService::setMaxAzimuth(double value)
{
    double const bounded = qBound(0.0, value, 360.0);
    if (qFuzzyCompare(bounded, m_maxAzimuth)) return;
    m_maxAzimuth = bounded;
    emit safetyChanged();
}

void RotatorService::setMinElevation(double value)
{
    double const bounded = qBound(-10.0, value, 180.0);
    if (qFuzzyCompare(bounded, m_minElevation)) return;
    m_minElevation = bounded;
    emit safetyChanged();
}

void RotatorService::setMaxElevation(double value)
{
    double const bounded = qBound(-10.0, value, 180.0);
    if (qFuzzyCompare(bounded, m_maxElevation)) return;
    m_maxElevation = bounded;
    emit safetyChanged();
}

void RotatorService::setParkOnStop(bool enabled)
{
    if (enabled == m_parkOnStop) return;
    m_parkOnStop = enabled;
    emit safetyChanged();
}

void RotatorService::setParkAzimuth(double value)
{
    if (!isFiniteValue(value)) return;
    double const normalized = normalizeAzimuth(value);
    if (qFuzzyCompare(normalized, m_parkAzimuth)) return;
    m_parkAzimuth = normalized;
    emit safetyChanged();
}

void RotatorService::setParkElevation(double value)
{
    double const bounded = qBound(-10.0, value, 180.0);
    if (qFuzzyCompare(bounded, m_parkElevation)) return;
    m_parkElevation = bounded;
    emit safetyChanged();
}

bool RotatorService::validateTarget(double* azimuth, double* elevation,
                                    bool hasElevation, QString* reason) const
{
    if (!azimuth || !elevation || !isFiniteValue(*azimuth)
        || (hasElevation && !isFiniteValue(*elevation))) {
        if (reason) *reason = QStringLiteral("Non-finite rotator target");
        return false;
    }
    *azimuth = normalizeAzimuth(*azimuth);
    if (hasElevation) *elevation = qBound(-10.0, *elevation, 180.0);
    if (!m_safetyEnabled) return true;
    if (m_minAzimuth > m_maxAzimuth
        || *azimuth < m_minAzimuth || *azimuth > m_maxAzimuth) {
        if (reason) {
            *reason = QStringLiteral("Azimuth %1 outside safety limits %2..%3")
                          .arg(*azimuth, 0, 'f', 1)
                          .arg(m_minAzimuth, 0, 'f', 1)
                          .arg(m_maxAzimuth, 0, 'f', 1);
        }
        return false;
    }
    if (hasElevation && (m_minElevation > m_maxElevation
                         || *elevation < m_minElevation
                         || *elevation > m_maxElevation)) {
        if (reason) {
            *reason = QStringLiteral("Elevation %1 outside safety limits %2..%3")
                          .arg(*elevation, 0, 'f', 1)
                          .arg(m_minElevation, 0, 'f', 1)
                          .arg(m_maxElevation, 0, 'f', 1);
        }
        return false;
    }
    return true;
}

QByteArray RotatorService::wrapCommand(const QByteArray& body) const
{
    if (m_protocol == QStringLiteral("PSTRotator")) {
        return QByteArray("<PST>") + body + QByteArray("</PST>");
    }
    return body;
}

bool RotatorService::commandTarget(double azimuth, double elevation,
                                   bool hasElevation)
{
    if (!m_enabled) {
        setStatus(QStringLiteral("Rotator disabled"));
        return false;
    }
    QString reason;
    if (!validateTarget(&azimuth, &elevation, hasElevation, &reason)) {
        setStatus(QStringLiteral("Rotator safety stop: %1").arg(reason));
        return false;
    }
    bool const changed = !qFuzzyCompare(azimuth + 1.0, m_targetAzimuth + 1.0)
        || !qFuzzyCompare(elevation + 1.0, m_targetElevation + 1.0)
        || hasElevation != m_targetHasElevation;
    m_targetAzimuth = azimuth;
    m_targetElevation = elevation;
    m_targetHasElevation = hasElevation;
    if (changed) emit targetChanged();
    sendCurrentTarget();
    return true;
}

bool RotatorService::trackTarget(double azimuth, double elevation,
                                 bool hasElevation)
{
    if (!commandTarget(azimuth, elevation, hasElevation)) return false;
    if (!m_tracking) {
        m_tracking = true;
        m_trackingTimer->start();
        emit trackingChanged();
    }
    setStatus(QStringLiteral("Rotator tracking %1°%2")
                  .arg(m_targetAzimuth, 0, 'f', 1)
                  .arg(m_targetHasElevation
                           ? QStringLiteral(" / %1°").arg(m_targetElevation, 0, 'f', 1)
                           : QString()));
    return true;
}

void RotatorService::stopTracking()
{
    bool const wasTracking = m_tracking;
    m_tracking = false;
    m_trackingTimer->stop();
    if (wasTracking) emit trackingChanged();
    if (m_enabled) {
        sendStopCommand();
        if (m_parkOnStop) sendParkCommand();
    }
    if (wasTracking) setStatus(QStringLiteral("Rotator tracking stopped"));
}

void RotatorService::emergencyStop()
{
    m_tracking = false;
    m_trackingTimer->stop();
    if (m_enabled) sendStopCommand();
    emit trackingChanged();
    setStatus(QStringLiteral("Rotator emergency stop sent"));
}

void RotatorService::park()
{
    if (!m_enabled) {
        setStatus(QStringLiteral("Rotator disabled"));
        return;
    }
    m_tracking = false;
    m_trackingTimer->stop();
    sendParkCommand();
    emit trackingChanged();
    setStatus(QStringLiteral("Rotator park command sent"));
}

void RotatorService::sendCurrentTarget()
{
    QByteArray const azimuthText = m_targetHasElevation
        ? QByteArray::number(m_targetAzimuth, 'f', 1)
        : QByteArray::number(qRound(m_targetAzimuth));
    QByteArray body = QByteArray("<AZIMUTH>")
        + azimuthText
        + QByteArray("</AZIMUTH>");
    if (m_targetHasElevation) {
        body += QByteArray("<ELEVATION>")
            + QByteArray::number(m_targetElevation, 'f', 1)
            + QByteArray("</ELEVATION>");
    }
    sendPayload(wrapCommand(body));
    m_lastCommandMs = QDateTime::currentMSecsSinceEpoch();
}

void RotatorService::sendStopCommand()
{
    sendPayload(wrapCommand(QByteArray("<STOP>1</STOP>")));
}

void RotatorService::sendParkCommand()
{
    if (m_protocol == QStringLiteral("PSTRotator")) {
        sendPayload(wrapCommand(QByteArray("<PARK>1</PARK>")));
        return;
    }
    // CatRotator's UDP listener accepts PARK as a command; the explicit
    // configured position is also sent first for rotators without a park
    // preset, subject to the same safety limits as every other target.
    double azimuth = m_parkAzimuth;
    double elevation = m_parkElevation;
    QString reason;
    if (validateTarget(&azimuth, &elevation, true, &reason)) {
        sendPayload(wrapCommand(QByteArray("<AZIMUTH>")
                                + QByteArray::number(azimuth, 'f', 1)
                                + QByteArray("</AZIMUTH><ELEVATION>")
                                + QByteArray::number(elevation, 'f', 1)
                                + QByteArray("</ELEVATION>")));
    }
    sendPayload(wrapCommand(QByteArray("<PARK>1</PARK>")));
}

void RotatorService::onTrackingTick()
{
    if (!m_tracking || !m_enabled) return;
    sendCurrentTarget();
}

void RotatorService::pollFeedback()
{
    if (!m_enabled || m_protocol != QStringLiteral("PSTRotator")) return;
    configureFeedbackSocket();
    sendPayload(wrapCommand(QByteArray("AZ?")));
    sendPayload(wrapCommand(QByteArray("EL?")));
}

void RotatorService::onFeedbackTick()
{
    if (!m_enabled || m_protocol != QStringLiteral("PSTRotator")) return;
    pollFeedback();
    if (m_feedbackAvailable
        && QDateTime::currentMSecsSinceEpoch() - m_lastFeedbackMs > kFeedbackTimeoutMs) {
        m_feedbackAvailable = false;
        emit feedbackChanged();
        setStatus(QStringLiteral("PSTRotator feedback timeout"));
    }
}

void RotatorService::onReadyRead()
{
    while (m_feedbackSocket->hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(static_cast<int>(m_feedbackSocket->pendingDatagramSize()));
        m_feedbackSocket->readDatagram(datagram.data(), datagram.size());
        QString const text = QString::fromUtf8(datagram);
        QRegularExpression const azimuthExpression(
            QStringLiteral("(?:^|\\s|:)AZ(?:IMUTH)?\\s*[:=]\\s*(-?\\d+(?:[.,]\\d+)?)"),
            QRegularExpression::CaseInsensitiveOption);
        QRegularExpression const elevationExpression(
            QStringLiteral("(?:^|\\s|:)EL(?:EVATION)?\\s*[:=]\\s*(-?\\d+(?:[.,]\\d+)?)"),
            QRegularExpression::CaseInsensitiveOption);
        QRegularExpressionMatch const azimuthMatch = azimuthExpression.match(text);
        QRegularExpressionMatch const elevationMatch = elevationExpression.match(text);
        bool const hasAzimuth = azimuthMatch.hasMatch();
        bool const hasElevation = elevationMatch.hasMatch();
        if (!hasAzimuth && !hasElevation) continue;
        double azimuth = hasAzimuth
            ? azimuthMatch.captured(1).replace(',', '.').toDouble() : m_currentAzimuth;
        double elevation = hasElevation
            ? elevationMatch.captured(1).replace(',', '.').toDouble() : m_currentElevation;
        setFeedback(azimuth, hasAzimuth, elevation, hasElevation);
    }
}

void RotatorService::setFeedback(double azimuth, bool hasAzimuth,
                                 double elevation, bool hasElevation)
{
    if (hasAzimuth) m_currentAzimuth = normalizeAzimuth(azimuth);
    if (hasElevation) m_currentElevation = elevation;
    m_feedbackAvailable = true;
    m_lastFeedbackMs = QDateTime::currentMSecsSinceEpoch();
    emit feedbackChanged();
    setStatus(QStringLiteral("Rotator feedback AZ %1° / EL %2°")
                  .arg(m_currentAzimuth, 0, 'f', 1)
                  .arg(m_currentElevation, 0, 'f', 1));
}

void RotatorService::configureFeedbackSocket()
{
    if (!m_enabled || m_protocol != QStringLiteral("PSTRotator") || m_port >= 65535) {
        return;
    }
    if (m_feedbackSocket->state() != QAbstractSocket::UnconnectedState) return;
    if (!m_feedbackSocket->bind(QHostAddress::AnyIPv4,
                                static_cast<quint16>(m_port + 1),
                                QUdpSocket::ShareAddress
                                    | QUdpSocket::ReuseAddressHint)) {
        setStatus(QStringLiteral("Cannot listen for PSTRotator feedback on UDP %1")
                      .arg(m_port + 1));
    }
}

void RotatorService::closeFeedbackSocket()
{
    if (!m_feedbackSocket) return;
    if (m_feedbackSocket->state() != QAbstractSocket::UnconnectedState) {
        m_feedbackSocket->close();
    }
}

void RotatorService::sendPayload(const QByteArray& payload)
{
    if (!m_commandSocket || payload.isEmpty() || m_host.trimmed().isEmpty()) return;
    QHostAddress address;
    if (address.setAddress(m_host)) {
        writePayload(payload, address);
        return;
    }
    if (m_resolvedHost == m_host && !m_resolvedAddress.isNull()) {
        writePayload(payload, m_resolvedAddress);
        return;
    }
    m_pendingPayload = payload;
    if (m_resolutionInFlight) return;
    m_resolutionInFlight = true;
    QString const host = m_host;
    quint64 const generation = ++m_resolutionGeneration;
    QHostInfo::lookupHost(host, this,
                          [this, host, generation](const QHostInfo& info) {
        if (generation != m_resolutionGeneration || host != m_host) return;
        m_resolutionInFlight = false;
        if (info.addresses().isEmpty()) {
            setStatus(QStringLiteral("Rotator host lookup failed: %1").arg(host));
            return;
        }
        m_resolvedHost = host;
        m_resolvedAddress = info.addresses().first();
        QByteArray const pending = m_pendingPayload;
        m_pendingPayload.clear();
        writePayload(pending, m_resolvedAddress);
    });
}

void RotatorService::writePayload(const QByteArray& payload,
                                  const QHostAddress& address)
{
    qint64 const written = m_commandSocket->writeDatagram(
        payload, address, static_cast<quint16>(m_port));
    if (written != payload.size()) {
        setStatus(QStringLiteral("Rotator UDP send failed"));
    }
}

void RotatorService::setStatus(const QString& status)
{
    if (status == m_status) return;
    m_status = status;
    emit statusChanged();
}
