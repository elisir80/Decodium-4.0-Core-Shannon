#include "VoiceKeyer.h"

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
        VoiceKeyer keyer(dir.path());
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
        VoiceKeyer keyer(dir.path());
        int begun = 0;
        VoiceKeyer::Hooks h;
        h.begin = [&] { ++begun; return true; };
        keyer.setHooks(h);
        keyer.play(2);
        QCOMPARE(begun, 0);
        QVERIFY(!keyer.playing());
    }
};

QTEST_GUILESS_MAIN(TestVoiceKeyer)
#include "test_voicekeyer.moc"
