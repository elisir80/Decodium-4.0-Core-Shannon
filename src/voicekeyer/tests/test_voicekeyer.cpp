#include "VoiceKeyer.h"
#include "PiperTts.h"
#include "VoicePack.h"

#include <QFileInfo>

#include <QTemporaryDir>
#include <QtTest>

using namespace decodium::voicekeyer;

class TestVoiceKeyer : public QObject {
    Q_OBJECT
private slots:
    void wavRoundTrip()
    {
        QTemporaryDir dir;
        QVector<short> in(12000);
        for (int i = 0; i < in.size(); ++i) in[i] = short(std::sin(i * 0.1) * 12000);
        const QString path = dir.filePath(QStringLiteral("a.wav"));
        QString error;
        QVERIFY(writeWav(path, in, kRadioRate, &error));
        const auto back = readWav(path, &error);
        QVERIFY(back.has_value());
        QCOMPARE(back->size(), in.size());
        QCOMPARE(back->at(5000), in.at(5000));
    }

    void resamplesFortyEightToTwelve()
    {
        QVector<short> in(48000, 1000);
        const QVector<short> out = resampleLinear(in, kCaptureRate, kRadioRate);
        QCOMPARE(out.size(), 12000);
        QCOMPARE(out.at(100), short(1000));
    }

    void rejectsNonWav()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("bad.wav"));
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("not a wave file at all, really not");
        f.close();
        QString error;
        QVERIFY(!readWav(path, &error).has_value());
        QVERIFY(!error.isEmpty());
    }

    void sendsPacedPacketsThenEnds()
    {
        QTemporaryDir dir;
        VoiceKeyer keyer(dir.path(), dir.filePath(QStringLiteral("pack")));
        QVector<short> msg(kRadioRate / 2, 3000);      // mezzo secondo
        QVERIFY(writeWav(QDir(dir.path()).filePath(QStringLiteral("slot1.wav")), msg, kRadioRate));
        int begun = 0, ended = 0, packets = 0;
        VoiceKeyer::Hooks h;
        h.begin = [&] { ++begun; return true; };
        h.end = [&] { ++ended; };
        h.valid = [] { return true; };
        h.send = [&](const QVector<short>& p) { QCOMPARE(p.size(), kPacket); ++packets; };
        h.leadMs = [] { return 0; };
        keyer.setHooks(h);
        keyer.play(0);
        QTRY_COMPARE_WITH_TIMEOUT(ended, 1, 5000);
        QCOMPARE(begun, 1);
        QVERIFY(packets >= msg.size() / kPacket);
        QVERIFY(!keyer.playing());
    }

    void refusesPlayWithoutRecording()
    {
        QTemporaryDir dir;
        VoiceKeyer keyer(dir.path(), dir.filePath(QStringLiteral("pack")));
        int begun = 0;
        VoiceKeyer::Hooks h;
        h.begin = [&] { ++begun; return true; };
        keyer.setHooks(h);
        keyer.play(2);
        QCOMPARE(begun, 0);
        QVERIFY(!keyer.playing());
    }

    void voicePackNotInstalledInEmptyDir()
    {
        QTemporaryDir dir;
        VoicePack pack(dir.path());
        QVERIFY(!pack.installed());
        QVERIFY(!pack.busy());
    }

    void piperSynthesisWhenEngineIsPresent()
    {
        // Facoltativo: serve il pacchetto vocale gia' estratto.
        const QString exe = qEnvironmentVariable("DECODIUM_TEST_PIPER_EXE");
        const QString model = qEnvironmentVariable("DECODIUM_TEST_PIPER_MODEL");
        if (exe.isEmpty() || model.isEmpty()) QSKIP("set DECODIUM_TEST_PIPER_EXE and DECODIUM_TEST_PIPER_MODEL");
        QString error;
        const auto pcm = PiperTts::synthesize(exe, QFileInfo(exe).absolutePath(), model,
                                              QStringLiteral("Prova di sintesi vocale."), &error);
        QVERIFY2(pcm.has_value(), qPrintable(error));
        QVERIFY(pcm->size() > kRadioRate / 2);   // almeno mezzo secondo di parlato
    }

    void voicePackInstallsFromScratch()
    {
        // Lento (circa 150 MB): gira solo con DECODIUM_TEST_NET=1.
        if (qEnvironmentVariable("DECODIUM_TEST_NET") != QStringLiteral("1"))
            QSKIP("set DECODIUM_TEST_NET=1 to download the voice pack");
        QTemporaryDir dir;
        VoicePack pack(dir.path());
        pack.install();
        QTRY_VERIFY_WITH_TIMEOUT(!pack.busy(), 900000);
        QVERIFY2(pack.installed(), qPrintable(pack.status()));
        QVERIFY(QFileInfo::exists(pack.voiceModel(QStringLiteral("en"))));
    }
};

QTEST_GUILESS_MAIN(TestVoiceKeyer)
#include "test_voicekeyer.moc"
