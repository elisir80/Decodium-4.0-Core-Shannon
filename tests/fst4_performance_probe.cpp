// Standalone native-worker measurement; input is signed 16-bit little-endian 12 kHz mono PCM.
#include "Detector/FST4DecodeWorker.hpp"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtEndian>
#include <cstdio>
#include <thread>
#include <chrono>
#include "Detector/FortranRuntimeGuard.hpp"

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    auto args = app.arguments();
    if (args.size() != 3 && !(args.size() == 4 && args[3] == "--cancel")) return 2;
    bool ok = false;
    int period = args[2].toInt(&ok);
    if (!ok || !QList<int>{15,30,60,120,300,900,1800}.contains(period)) return 2;
    QFile file(args[1]);
    if (!file.open(QIODevice::ReadOnly)) return 2;
    QByteArray pcm = file.readAll();
    if (pcm.isEmpty() || pcm.size() % 2 || pcm.size() > period * 12000LL * 2) return 2;
    QTemporaryDir data;
    if (!data.isValid()) return 2;
    decodium::fst4::DecodeRequest request;
    request.mode = QStringLiteral("FST4");
    request.ntrperiod = period;
    request.ndepth = 3;
    request.nfa = 1000;
    request.nfb = 1400;
    request.nfqso = 1331;
    request.ntol = 20;
    request.mycall = "K1ABC";
    request.hiscall = "W9XYZ";
    request.dataDir = data.path().toLocal8Bit();
    request.audio.resize(pcm.size()/2);
    for (qsizetype i=0; i<request.audio.size(); ++i)
        request.audio[i] = qFromLittleEndian<qint16>(reinterpret_cast<const uchar*>(pcm.constData()+2*i));
    QJsonObject cancellation;
    if (args.size() == 4) {
        decodium::fst4::FST4DecodeWorker worker;
        QStringList emitted;
        QObject::connect(&worker, &decodium::fst4::FST4DecodeWorker::decodeReady,
                         &worker, [&](quint64, QStringList rows) { emitted = rows; }, Qt::DirectConnection);
        request.serial = 1;
        std::thread active([&] { worker.decode(request); });
        auto& mutex = decodium::fortran::runtime_mutex();
        QElapsedTimer wait;
        wait.start();
        bool busy = false;
        while (wait.elapsed() < 2000) {
            if (!mutex.tryLock()) { busy = true; break; }
            mutex.unlock();
            std::this_thread::yield();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(40));
        QElapsedTimer stop;
        stop.start();
        worker.retireThrough(1);
        active.join();
        double residualMs = stop.nsecsElapsed()/1e6;
        bool staleEmpty = emitted.isEmpty();
        worker.retireThrough(0); // Must not move the retirement boundary backwards.
        worker.decode(request);
        bool queuedEmpty = emitted.isEmpty();
        request.serial = 2;
        worker.decode(request);
        bool recovered = false;
        for (auto const& row : emitted) recovered |= row.contains("CQ K9KFR EN71");
        cancellation = {{"observedRuntimeBusy",busy},{"residualMs",residualMs},
                        {"staleEmpty",staleEmpty},{"queuedEmpty",queuedEmpty},{"recovered",recovered}};
        if (!busy || !staleEmpty || !queuedEmpty || !recovered || residualMs > 500.0) return 3;
    }
    QJsonArray runs;
    for (int i=0; i<3; ++i) {
        request.serial = i+1;
        QElapsedTimer timer;
        timer.start();
        auto rows = decodium::fst4::decodeFst4Rows(request);
        double ms = timer.nsecsElapsed()/1e6;
        runs.append(QJsonObject{{"run",i+1},{"elapsedMs",ms},{"rows",QJsonArray::fromStringList(rows)}});
    }
    auto result = QJsonDocument(QJsonObject{{"period",period},{"runs",runs},{"cancellation",cancellation}}).toJson();
    fwrite(result.constData(), 1, result.size(), stdout);
    return 0;
}
