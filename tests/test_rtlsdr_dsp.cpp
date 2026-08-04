#include <QtTest>

#include "src/rtl/RtlSdrDsp.h"

#include <cmath>

class TestRtlSdrDsp final : public QObject
{
    Q_OBJECT

private slots:
    void acceptsIntegerDecimation();
    void rejectsUnsupportedRate();
    void producesPcmForIqTone();
    void wideFmProducesReceiverAudioAndKeepsIq();
    void wideFmRejectsCarrierOffsetWithoutClipping();
    void ncoRecentresOffsetChannelBeforeDecoderPath();
    void ifSpectrumInversionConjugatesIq();
};

void TestRtlSdrDsp::acceptsIntegerDecimation()
{
    RtlSdrDsp dsp;
    QVERIFY(dsp.configure(240000));
    QCOMPARE(dsp.decimationFactor(), 20);
    QCOMPARE(dsp.outputSampleRate(), 12000);
}

void TestRtlSdrDsp::rejectsUnsupportedRate()
{
    RtlSdrDsp dsp;
    QVERIFY(!dsp.configure(250000));
    QVERIFY(!RtlSdrDsp::isSupportedSampleRate(240000, 11025));
}

void TestRtlSdrDsp::producesPcmForIqTone()
{
    RtlSdrDsp dsp;
    QVERIFY(dsp.configure(240000, 12000, 1.0));

    QByteArray iq;
    constexpr int samples = 24000;
    iq.resize(samples * 2);
    for (int index = 0; index < samples; ++index) {
        const double phase = 2.0 * 3.14159265358979323846 * 1000.0 * index / 240000.0;
        const int value = qBound(0, qRound(127.5 + 50.0 * std::sin(phase)), 255);
        iq[2 * index] = static_cast<char>(value);
        iq[2 * index + 1] = static_cast<char>(128);
    }

    const QVector<short> pcm = dsp.process(reinterpret_cast<const unsigned char *>(iq.constData()), iq.size());
    QCOMPARE(pcm.size(), samples / 20);
    int peak = 0;
    for (int index = 100; index < pcm.size(); ++index) {
        peak = qMax(peak, std::abs(static_cast<int>(pcm.at(index))));
    }
    QVERIFY2(peak > 2000, "a valid RTL I/Q tone must survive DC removal and decimation");
}

void TestRtlSdrDsp::wideFmProducesReceiverAudioAndKeepsIq()
{
    RtlSdrDsp dsp;
    QVERIFY(dsp.configure(960000, RtlSdrDsp::Demodulator::WideFm, 1.0));
    QVERIFY(!RtlSdrDsp::isSupportedSampleRateForDemodulator(
        240000, RtlSdrDsp::Demodulator::WideFm));

    QByteArray iq;
    constexpr int samples = 96000;
    iq.resize(samples * 2);
    double phase = 0.0;
    for (int index = 0; index < samples; ++index) {
        const double modulation = std::sin(2.0 * 3.14159265358979323846 * 1000.0
                                           * index / 960000.0);
        phase += 2.0 * 3.14159265358979323846 * 30000.0 * modulation / 960000.0;
        iq[2 * index] = static_cast<char>(qBound(0, qRound(127.5 + 90.0 * std::cos(phase)), 255));
        iq[2 * index + 1] = static_cast<char>(qBound(0, qRound(127.5 + 90.0 * std::sin(phase)), 255));
    }

    RtlSdrDsp::Result frame = dsp.processFrame(
        reinterpret_cast<const unsigned char *>(iq.constData()), iq.size(), true);
    QCOMPARE(frame.iq.size(), samples * 2);
    QCOMPARE(frame.decoderPcm.size(), 0);
    QCOMPARE(frame.audioPcm.size(), samples / 20);
    int peak = 0;
    for (int index = 500; index < frame.audioPcm.size(); ++index) {
        peak = qMax(peak, std::abs(static_cast<int>(frame.audioPcm.at(index))));
    }
    QVERIFY2(peak > 300, "FM discriminator output must reach the separate receiver-audio path");
}

void TestRtlSdrDsp::wideFmRejectsCarrierOffsetWithoutClipping()
{
    RtlSdrDsp dsp;
    QVERIFY(dsp.configure(960000, RtlSdrDsp::Demodulator::WideFm, 3.3));

    QByteArray iq;
    constexpr int samples = 192000;
    iq.resize(samples * 2);
    double phase = 0.0;
    for (int index = 0; index < samples; ++index) {
        const double modulation = std::sin(2.0 * 3.14159265358979323846 * 1000.0
                                           * index / 960000.0);
        const double instantaneousHz = 60000.0 + 30000.0 * modulation;
        phase += 2.0 * 3.14159265358979323846 * instantaneousHz / 960000.0;
        iq[2 * index] = static_cast<char>(qBound(0, qRound(127.5 + 90.0 * std::cos(phase)), 255));
        iq[2 * index + 1] = static_cast<char>(qBound(0, qRound(127.5 + 90.0 * std::sin(phase)), 255));
    }

    const RtlSdrDsp::Result frame = dsp.processFrame(
        reinterpret_cast<const unsigned char *>(iq.constData()), iq.size(), false);
    QCOMPARE(frame.audioPcm.size(), samples / 20);

    qint64 sum = 0;
    qint64 squareSum = 0;
    int peak = 0;
    constexpr int warmup = 2400;
    for (int index = warmup; index < frame.audioPcm.size(); ++index) {
        const int sample = frame.audioPcm.at(index);
        sum += sample;
        squareSum += static_cast<qint64>(sample) * sample;
        peak = qMax(peak, std::abs(sample));
    }
    const int count = frame.audioPcm.size() - warmup;
    const double mean = static_cast<double>(sum) / count;
    const double rms = std::sqrt(static_cast<double>(squareSum) / count);
    QVERIFY2(std::abs(mean) < rms * 0.10,
             "FM carrier mistuning must not become a large DC audio component");
    QVERIFY2(peak < 30000, "normal WFM audio must retain headroom instead of hard clipping");
    QVERIFY2(rms > 500.0, "the wanted FM modulation must remain audible after DC rejection");
}

void TestRtlSdrDsp::ncoRecentresOffsetChannelBeforeDecoderPath()
{
    RtlSdrDsp dsp;
    QVERIFY(dsp.configure(240000, RtlSdrDsp::Demodulator::WeakSignal, 1.0, -10000));

    QByteArray iq;
    constexpr int samples = 24000;
    iq.resize(samples * 2);
    for (int index = 0; index < samples; ++index) {
        const double phase = -2.0 * 3.14159265358979323846 * 10000.0 * index / 240000.0;
        iq[2 * index] = static_cast<char>(qBound(0, qRound(127.5 + 70.0 * std::cos(phase)), 255));
        iq[2 * index + 1] = static_cast<char>(qBound(0, qRound(127.5 + 70.0 * std::sin(phase)), 255));
    }

    const QVector<short> pcm = dsp.process(reinterpret_cast<const unsigned char *>(iq.constData()), iq.size());
    QCOMPARE(pcm.size(), samples / 20);
    qint64 sum = 0;
    for (int index = 200; index < pcm.size(); ++index) {
        sum += pcm.at(index);
    }
    QVERIFY2(std::abs(sum / qMax(1, pcm.size() - 200)) > 5000,
             "the NCO must move the selected offset channel to decoder baseband");
}

void TestRtlSdrDsp::ifSpectrumInversionConjugatesIq()
{
    RtlSdrDsp normal;
    RtlSdrDsp inverted;
    QVERIFY(normal.configure(240000, RtlSdrDsp::Demodulator::WeakSignal,
                             1.0, 0, false));
    QVERIFY(inverted.configure(240000, RtlSdrDsp::Demodulator::WeakSignal,
                               1.0, 0, true));

    QByteArray iq;
    constexpr int samples = 4096;
    iq.resize(samples * 2);
    for (int index = 0; index < samples; ++index) {
        const double phase = 2.0 * 3.14159265358979323846 * 20000.0
            * index / 240000.0;
        iq[2 * index] = static_cast<char>(qBound(
            0, qRound(127.5 + 70.0 * std::cos(phase)), 255));
        iq[2 * index + 1] = static_cast<char>(qBound(
            0, qRound(127.5 + 70.0 * std::sin(phase)), 255));
    }

    const auto normalFrame = normal.processFrame(
        reinterpret_cast<const unsigned char *>(iq.constData()), iq.size(), true);
    const auto invertedFrame = inverted.processFrame(
        reinterpret_cast<const unsigned char *>(iq.constData()), iq.size(), true);
    QCOMPARE(normalFrame.iq.size(), invertedFrame.iq.size());
    for (int sample = 100; sample < samples; ++sample) {
        QCOMPARE(normalFrame.iq.at(2 * sample), invertedFrame.iq.at(2 * sample));
        QVERIFY(std::abs(static_cast<int>(normalFrame.iq.at(2 * sample + 1))
                         + invertedFrame.iq.at(2 * sample + 1)) <= 1);
    }
}

QTEST_MAIN(TestRtlSdrDsp)
#include "test_rtlsdr_dsp.moc"
