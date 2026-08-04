#include <QtTest>

#include "src/rtl/RtlSdrCapabilities.h"
#include "src/rtl/RtlSdrInput.h"

class TestRtlSdrInput final : public QObject
{
    Q_OBJECT

private slots:
    void bothHardwareModesProducePcm();
    void inPlaceRetuneKeepsUsbReaderOpen();
    void wideFmProducesIqAndReceiverAudio();
};

void TestRtlSdrInput::bothHardwareModesProducePcm()
{
    if (!qEnvironmentVariableIsSet("DECODIUM_RTLSDR_HARDWARE_TEST")) {
        QSKIP("Set DECODIUM_RTLSDR_HARDWARE_TEST=1 with an RTL-SDR connected to run this opt-in hardware smoke test.");
    }
    if (!RtlSdrInput::compiledIn()) {
        QSKIP("This build does not include librtlsdr.");
    }

    QString enumerationError;
    const QStringList devices = RtlSdrInput::enumerateDevices(&enumerationError);
    QVERIFY2(!devices.isEmpty(), qPrintable(enumerationError));
    const bool blogV4 = decodium::rtl_sdr::isRtlSdrBlogV4Identity(devices.first());

    const QList<RtlSdrInput::Mode> modes {
        RtlSdrInput::Mode::SdrRadio,
        RtlSdrInput::Mode::DirectSampling
    };
    for (const RtlSdrInput::Mode mode : modes) {
        RtlSdrInput input;
        QSignalSpy startedSpy(&input, &RtlSdrInput::started);
        QSignalSpy pcmSpy(&input, &RtlSdrInput::pcmSamplesReady);
        QSignalSpy stoppedSpy(&input, &RtlSdrInput::stopped);
        QSignalSpy adjustedSpy(&input, &RtlSdrInput::configurationAdjusted);
        QSignalSpy errorSpy(&input, &RtlSdrInput::error);

        RtlSdrInput::Config config;
        config.deviceIndex = 0;
        config.mode = mode;
        config.centerFrequencyHz = mode == RtlSdrInput::Mode::DirectSampling
            ? 7100000U : 14074000U;
        config.sampleRate = 240000;
        input.start(config);

        QTRY_VERIFY_WITH_TIMEOUT(startedSpy.count() == 1 || errorSpy.count() > 0, 5000);
        if (errorSpy.count() > 0) {
            QFAIL(qPrintable(errorSpy.first().value(0).toString()));
        }
        QCOMPARE(startedSpy.count(), 1);
        QTRY_VERIFY_WITH_TIMEOUT(pcmSpy.count() > 2, 5000);
        QCOMPARE(errorSpy.count(), 0);
        if (mode == RtlSdrInput::Mode::DirectSampling && blogV4) {
            QCOMPARE(adjustedSpy.count(), 1);
            QCOMPARE(input.activeConfig().mode, RtlSdrInput::Mode::SdrRadio);
        }
        input.stop();
        QTRY_VERIFY_WITH_TIMEOUT(stoppedSpy.count() == 1, 5000);
    }
}

void TestRtlSdrInput::inPlaceRetuneKeepsUsbReaderOpen()
{
    if (!qEnvironmentVariableIsSet("DECODIUM_RTLSDR_HARDWARE_TEST")) {
        QSKIP("Set DECODIUM_RTLSDR_HARDWARE_TEST=1 with an RTL-SDR connected to run this opt-in hardware smoke test.");
    }
    if (!RtlSdrInput::compiledIn()) {
        QSKIP("This build does not include librtlsdr.");
    }

    RtlSdrInput input;
    QSignalSpy startedSpy(&input, &RtlSdrInput::started);
    QSignalSpy retunedSpy(&input, &RtlSdrInput::retuned);
    QSignalSpy pcmSpy(&input, &RtlSdrInput::pcmSamplesReady);
    QSignalSpy stoppedSpy(&input, &RtlSdrInput::stopped);
    QSignalSpy errorSpy(&input, &RtlSdrInput::error);

    RtlSdrInput::Config config;
    config.deviceIndex = 0;
    config.mode = RtlSdrInput::Mode::SdrRadio;
    config.centerFrequencyHz = 144160000U;
    config.channelOffsetHz = -60000;
    config.sampleRate = 240000;
    input.start(config);

    QTRY_COMPARE_WITH_TIMEOUT(startedSpy.count(), 1, 5000);
    QTRY_VERIFY_WITH_TIMEOUT(pcmSpy.count() > 2, 5000);
    const int pcmBeforeRetune = pcmSpy.count();

    QVERIFY(input.retune(144260000U, -60000));
    QTRY_COMPARE_WITH_TIMEOUT(retunedSpy.count(), 1, 5000);
    QCOMPARE(startedSpy.count(), 1);
    QCOMPARE(input.activeConfig().centerFrequencyHz, 144260000U);
    QCOMPARE(input.activeConfig().channelOffsetHz, -60000);
    QTRY_VERIFY_WITH_TIMEOUT(pcmSpy.count() > pcmBeforeRetune + 2, 5000);
    QCOMPARE(errorSpy.count(), 0);

    input.stop();
    QTRY_COMPARE_WITH_TIMEOUT(stoppedSpy.count(), 1, 5000);
}

void TestRtlSdrInput::wideFmProducesIqAndReceiverAudio()
{
    if (!qEnvironmentVariableIsSet("DECODIUM_RTLSDR_HARDWARE_TEST")) {
        QSKIP("Set DECODIUM_RTLSDR_HARDWARE_TEST=1 with an RTL-SDR connected to run this opt-in hardware smoke test.");
    }
    if (!RtlSdrInput::compiledIn()) {
        QSKIP("This build does not include librtlsdr.");
    }

    RtlSdrInput input;
    QSignalSpy startedSpy(&input, &RtlSdrInput::started);
    QSignalSpy iqSpy(&input, &RtlSdrInput::iqSamplesReady);
    QSignalSpy audioSpy(&input, &RtlSdrInput::audioSamplesReady);
    QSignalSpy stoppedSpy(&input, &RtlSdrInput::stopped);
    QSignalSpy errorSpy(&input, &RtlSdrInput::error);

    RtlSdrInput::Config config;
    config.deviceIndex = 0;
    config.mode = RtlSdrInput::Mode::SdrRadio;
    config.demodulator = RtlSdrDsp::Demodulator::WideFm;
    config.centerFrequencyHz = 100340000U;
    config.channelOffsetHz = -240000;
    config.sampleRate = 960000;
    input.start(config);

    QTRY_VERIFY_WITH_TIMEOUT(startedSpy.count() == 1 || errorSpy.count() > 0, 5000);
    if (errorSpy.count() > 0) {
        QFAIL(qPrintable(errorSpy.first().value(0).toString()));
    }
    QTRY_VERIFY_WITH_TIMEOUT(iqSpy.count() > 0, 5000);
    QTRY_VERIFY_WITH_TIMEOUT(audioSpy.count() > 2, 5000);
    QCOMPARE(errorSpy.count(), 0);
    input.stop();
    QTRY_VERIFY_WITH_TIMEOUT(stoppedSpy.count() == 1, 5000);
}

QTEST_MAIN(TestRtlSdrInput)
#include "test_rtlsdr_input.moc"
