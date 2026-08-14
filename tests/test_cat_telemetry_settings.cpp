#include <QtTest>

#include "src/radio/DecodiumCatTelemetrySettings.h"

class CatTelemetrySettingsTest final : public QObject
{
    Q_OBJECT

private slots:
    void swrProtectionAlwaysEnablesTelemetry()
    {
        auto const state = decodium::normalizedCatTelemetrySettings(false, true);
        QVERIFY(state.powerAndSwr);
        QVERIFY(state.checkSwr);
    }

    void disablingTelemetryAlsoDisablesProtection()
    {
        decodium::CatTelemetrySettings current {true, true};
        auto const state =
            decodium::catTelemetrySettingsAfterPowerChange(current, false);
        QVERIFY(!state.powerAndSwr);
        QVERIFY(!state.checkSwr);
    }

    void disablingProtectionKeepsMeterPollingEnabled()
    {
        decodium::CatTelemetrySettings current {true, true};
        auto const state =
            decodium::catTelemetrySettingsAfterProtectionChange(current, false);
        QVERIFY(state.powerAndSwr);
        QVERIFY(!state.checkSwr);
    }

    void enablingProtectionFromFullyOffIsAtomic()
    {
        decodium::CatTelemetrySettings current {false, false};
        auto const state =
            decodium::catTelemetrySettingsAfterProtectionChange(current, true);
        QVERIFY(state.powerAndSwr);
        QVERIFY(state.checkSwr);
    }
};

QTEST_MAIN(CatTelemetrySettingsTest)
#include "test_cat_telemetry_settings.moc"
