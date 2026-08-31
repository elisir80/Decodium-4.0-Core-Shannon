#include "app/GatewaySupervisor.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QHostAddress>
#include <QUdpSocket>

namespace decortty::app {

namespace {

#ifdef Q_OS_WIN
constexpr auto kExecutableName = "decortty-ft991.exe";
#else
constexpr auto kExecutableName = "decortty-ft991";
#endif

} // namespace

GatewaySupervisor::GatewaySupervisor(QObject* parent)
    : QObject(parent)
{
    setStatus(tr("not started"));
}

GatewaySupervisor::~GatewaySupervisor()
{
    stop();
}

QString GatewaySupervisor::gatewayExecutable()
{
    const QDir dir(QCoreApplication::applicationDirPath());
    // Accanto a noi nella cartella distribuita; un passo più su quando si lavora
    // dall'albero di compilazione, dove ogni eseguibile ha la sua sottocartella.
    for (const QString& candidate : {QString(kExecutableName),
                                     QStringLiteral("../") + kExecutableName}) {
        const QFileInfo info(dir.filePath(candidate));
        if (info.isExecutable() && info.isFile())
            return info.absoluteFilePath();
    }
    return {};
}

bool GatewaySupervisor::available() const
{
    return !gatewayExecutable().isEmpty();
}

bool GatewaySupervisor::isRunning() const
{
    return m_process && m_process->state() != QProcess::NotRunning;
}

void GatewaySupervisor::setStatus(const QString& text)
{
    if (m_status == text)
        return;
    m_status = text;
    emit statusChanged();
}

void GatewaySupervisor::setEnabled(bool on)
{
    if (m_enabled == on)
        return;
    m_enabled = on;
    emit configChanged();
    if (!on)
        stop();
}

// Un nome vuoto non e' mai una scelta dell'operatore: e' quello che produce un
// menu a discesa mentre si sta ancora costruendo, con l'elenco dei dispositivi
// non ancora arrivato. Accettarlo significherebbe avviare il gateway senza
// scheda audio — che e' esattamente il modo in cui non funziona niente.
void GatewaySupervisor::setCatPort(const QString& port)
{
    if (m_catPort == port || port.trimmed().isEmpty())
        return;
    m_catPort = port.trimmed();
    emit configChanged();
}

void GatewaySupervisor::setAudioIn(const QString& name)
{
    if (m_audioIn == name || name.trimmed().isEmpty())
        return;
    m_audioIn = name.trimmed();
    emit configChanged();
}

void GatewaySupervisor::setAudioOut(const QString& name)
{
    if (m_audioOut == name || name.trimmed().isEmpty())
        return;
    m_audioOut = name.trimmed();
    emit configChanged();
}

void GatewaySupervisor::setUdpPort(int port)
{
    if (m_udpPort == port || port < 1024 || port > 65535)
        return;
    m_udpPort = port;
    emit configChanged();
}

void GatewaySupervisor::setAutoConnect(bool on)
{
    if (m_autoConnect == on)
        return;
    m_autoConnect = on;
    emit configChanged();
}

void GatewaySupervisor::startUnlessPresent(bool alreadyPresent)
{
    if (!m_enabled) {
        setStatus(tr("disabled"));
        return;
    }
    if (isRunning())
        return;

    if (alreadyPresent) {
        // Non è un errore: qualcuno ha già aperto il gateway, e va benissimo.
        // Ce ne teniamo fuori, e soprattutto non lo chiuderemo uscendo.
        m_ours = false;
        setStatus(tr("already listening (started elsewhere)"));
        emit log(tr("A gateway is already on the network: not starting another"));
        return;
    }

    const QString exe = gatewayExecutable();
    if (exe.isEmpty()) {
        setStatus(tr("executable not found"));
        emit log(tr("Cannot find %1 beside the application").arg(QString(kExecutableName)));
        return;
    }

    QStringList args{QStringLiteral("--cat-port"),  m_catPort,
                     QStringLiteral("--audio-in"),  m_audioIn,
                     QStringLiteral("--audio-out"), m_audioOut,
                     QStringLiteral("--udp-port"),  QString::number(m_udpPort),
                     QStringLiteral("--parent-pid"),
                     QString::number(QCoreApplication::applicationPid())};
    if (!m_stationName.isEmpty())
        args << QStringLiteral("--name") << m_stationName;
    if (!m_callsign.isEmpty())
        args << QStringLiteral("--callsign") << m_callsign;

    m_process = new QProcess(this);
    m_process->setProgram(exe);
    m_process->setArguments(args);
    m_process->setWorkingDirectory(QFileInfo(exe).absolutePath());
    m_process->setProcessChannelMode(QProcess::MergedChannels);

    connect(m_process, &QProcess::readyReadStandardOutput,
            this, &GatewaySupervisor::readProcessOutput);
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            setStatus(tr("start failed"));
            emit log(tr("The gateway will not start: %1").arg(m_process->errorString()));
        }
    });
    connect(m_process, &QProcess::finished, this, [this](int code, QProcess::ExitStatus) {
        // Non lo si riavvia da soli. Se è uscito è perché la porta CAT non c'era
        // o la scheda audio è occupata: ripartire in ciclo nasconderebbe il
        // motivo dietro una raffica di tentativi identici.
        setStatus(code == 0 ? tr("exited") : tr("exited (code %1)").arg(code));
        m_catOnline = false;
        emit log(tr("Gateway exited, code %1").arg(code));
        m_ours = false;
        // Il processo e' finito: si lascia andare l'oggetto, altrimenti un
        // riavvio lo sovrascriverebbe e il vecchio resterebbe appeso.
        if (m_process) {
            m_process->deleteLater();
            m_process = nullptr;
        }
        emit statusChanged();
    });

    m_process->start();
    m_ours = true;
    setStatus(tr("starting on %1").arg(m_catPort));
    emit log(tr("Starting the gateway: %1 on port %2").arg(QFileInfo(exe).fileName(), m_catPort));
    emit statusChanged();

    if (m_process->waitForStarted(3000)) {
        // Lo stato definitivo arriva dalla riga STATUS del gateway, che sa se la
        // porta si e' aperta davvero.
        setStatus(tr("started"));
        emit gatewayStarted();
    }
}

void GatewaySupervisor::stop()
{
    if (!m_process)
        return;
    if (!m_ours) {
        // Difesa contro un errore di programmazione: se il gateway non è nostro
        // non dobbiamo avere un processo da fermare.
        m_process->deleteLater();
        m_process = nullptr;
        return;
    }

    QProcess* process = m_process;
    m_process = nullptr;
    m_ours = false;

    if (process->state() != QProcess::NotRunning) {
        emit log(tr("Closing the gateway"));
        // Non terminate(): su Windows manda WM_CLOSE alle finestre del processo,
        // e il gateway non ne ha alcuna. Gli si dice di chiudere sulla porta che
        // sta gia' ascoltando, cosi' passa dalla sua uscita ordinata — che
        // sgancia il PTT e rilascia la porta seriale. Solo se resta sordo si
        // arriva alle maniere forti.
        QUdpSocket socket;
        const QByteArray order = QByteArrayLiteral("DECORTTY-SHUTDOWN ")
                                 + QByteArray::number(QCoreApplication::applicationPid());
        socket.writeDatagram(order, QHostAddress::LocalHost,
                             static_cast<quint16>(m_udpPort));
        // Il gateway ci mette un paio di secondi a verificare che nessun altro
        // lo stia usando. Se dopo l'attesa e' ancora in piedi, la spiegazione
        // piu' probabile e' che abbia deciso di restare per un altro client — e
        // allora ucciderlo sarebbe il torto peggiore, perche' e' esattamente il
        // caso che questo meccanismo esiste per evitare. Lo si abbandona: sa
        // chiudersi da solo quando l'ultimo se ne va, e se davvero fosse
        // bloccato ci pensa il suo controllo sul processo padre.
        if (!process->waitForFinished(6000)) {
            emit log(tr("The gateway is staying up: leaving it to the other clients"));
            // Niente deleteLater: distruggere un QProcess uccide il figlio, ed
            // e' proprio quello che non vogliamo. L'oggetto muore con noi.
            process->setParent(nullptr);
            setStatus(tr("left running"));
            return;
        }
    }
    process->deleteLater();
    setStatus(tr("closed"));
}

void GatewaySupervisor::restart()
{
    stop();
    startUnlessPresent(false);
}

void GatewaySupervisor::readProcessOutput()
{
    if (!m_process)
        return;
    const QString text = QString::fromLocal8Bit(m_process->readAllStandardOutput());
    for (const QString& line : text.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
        const QString trimmed = line.trimmed();
        if (trimmed.startsWith(QLatin1String("STATUS\t"))) {
            // Il gateway riferisce com'e' andata l'apertura della porta.
            QString cat, port;
            for (const QString& field : trimmed.split(QLatin1Char('\t'))) {
                if (field.startsWith(QLatin1String("cat=")))
                    cat = field.mid(4);
                else if (field.startsWith(QLatin1String("port=")))
                    port = field.mid(5);
            }
            m_catOnline = cat == QLatin1String("open");
            if (m_catOnline)
                setStatus(tr("running on %1").arg(port));
            else if (cat == QLatin1String("busy"))
                setStatus(tr("%1 is busy — receive only").arg(port));
            else
                setStatus(tr("running, no CAT"));
            emit statusChanged();
            continue;
        }
        emit log(trimmed);
    }
}

void GatewaySupervisor::refreshDevices()
{
    if (m_probe)
        return;
    const QString exe = gatewayExecutable();
    if (exe.isEmpty())
        return;

    // I nomi li chiediamo al gateway invece di enumerarli qui: è lui che aprirà
    // i dispositivi, e su Windows lo stesso apparecchio può comparire con nomi
    // diversi a seconda del back end che lo interroga.
    m_probe = new QProcess(this);
    connect(m_probe, &QProcess::finished, this, [this](int, QProcess::ExitStatus) {
        const QString text = QString::fromLocal8Bit(m_probe->readAllStandardOutput());
        m_captureDevices.clear();
        m_playbackDevices.clear();
        m_serialPorts.clear();
        for (const QString& line : text.split(QLatin1Char('\n'))) {
            const QStringList parts = line.trimmed().split(QLatin1Char('\t'));
            if (parts.size() < 2)
                continue;
            if (parts.at(0) == QLatin1String("IN"))
                m_captureDevices << parts.at(1);
            else if (parts.at(0) == QLatin1String("OUT"))
                m_playbackDevices << parts.at(1);
            else if (parts.at(0) == QLatin1String("COM"))
                m_serialPorts << (parts.size() > 2 && !parts.at(2).isEmpty()
                                      ? parts.at(1) + QStringLiteral("  ") + parts.at(2)
                                      : parts.at(1));
        }
        m_probe->deleteLater();
        m_probe = nullptr;
        emit devicesChanged();
    });
    m_probe->start(exe, {QStringLiteral("--list-devices")});
}

} // namespace decortty::app
