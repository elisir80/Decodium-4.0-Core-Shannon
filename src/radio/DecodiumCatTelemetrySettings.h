#ifndef DECODIUM_CAT_TELEMETRY_SETTINGS_H
#define DECODIUM_CAT_TELEMETRY_SETTINGS_H

namespace decodium
{

struct CatTelemetrySettings
{
    CatTelemetrySettings(bool powerAndSwrValue = false,
                         bool checkSwrValue = false)
        : powerAndSwr(powerAndSwrValue)
        , checkSwr(checkSwrValue)
    {
    }

    bool powerAndSwr;
    bool checkSwr;
};

inline CatTelemetrySettings normalizedCatTelemetrySettings(bool powerAndSwr,
                                                            bool checkSwr)
{
    // SWR protection needs the SWR meter.  Never persist the contradictory
    // state "protection on, telemetry off" because the status bar would hide
    // the value while Hamlib still had to poll it.
    if (checkSwr) {
        powerAndSwr = true;
    }
    return {powerAndSwr, checkSwr};
}

inline CatTelemetrySettings catTelemetrySettingsAfterPowerChange(
    CatTelemetrySettings current,
    bool enabled)
{
    current.powerAndSwr = enabled;
    if (!enabled) {
        current.checkSwr = false;
    }
    return normalizedCatTelemetrySettings(current.powerAndSwr, current.checkSwr);
}

inline CatTelemetrySettings catTelemetrySettingsAfterProtectionChange(
    CatTelemetrySettings current,
    bool enabled)
{
    current.checkSwr = enabled;
    return normalizedCatTelemetrySettings(current.powerAndSwr, current.checkSwr);
}

} // namespace decodium

#endif // DECODIUM_CAT_TELEMETRY_SETTINGS_H
