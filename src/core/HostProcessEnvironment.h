#pragma once

#include <QProcessEnvironment>
#include <QString>
#include <QStringList>

namespace decodium {
namespace host_process {

inline void restoreHostEnvironmentVariable(QProcessEnvironment& environment,
                                           QString const& variable)
{
    QString const backup = QStringLiteral("DECODIUM_HOST_") + variable;
    if (environment.contains(backup)) {
        QString const hostValue = environment.value(backup);
        if (hostValue.isEmpty()) {
            environment.remove(variable);
        } else {
            environment.insert(variable, hostValue);
        }
        environment.remove(backup);
        return;
    }

    // Third-party/extracted AppImage launchers may not provide the host
    // snapshots. In that case it is safer to remove bundle-specific paths
    // before starting a host desktop helper.
    if (environment.contains(QStringLiteral("APPIMAGE"))
        || environment.contains(QStringLiteral("APPDIR"))) {
        environment.remove(variable);
    }
}

inline QProcessEnvironment sanitized(QProcessEnvironment environment)
{
#if defined(Q_OS_LINUX)
    const QStringList variables {
        QStringLiteral("LD_LIBRARY_PATH"),
        QStringLiteral("LD_PRELOAD"),
        QStringLiteral("GIO_EXTRA_MODULES"),
        QStringLiteral("GI_TYPELIB_PATH"),
        QStringLiteral("GSETTINGS_SCHEMA_DIR"),
        QStringLiteral("GTK_PATH"),
        QStringLiteral("XDG_DATA_DIRS")
    };
    for (QString const& variable : variables) {
        restoreHostEnvironmentVariable(environment, variable);
    }
#endif
    return environment;
}

inline QProcessEnvironment sanitizedSystemEnvironment()
{
    // DBUS_SESSION_BUS_ADDRESS, XDG_RUNTIME_DIR, DISPLAY, WAYLAND_DISPLAY,
    // HOME and PATH are deliberately retained for desktop integration.
    return sanitized(QProcessEnvironment::systemEnvironment());
}

} // namespace host_process
} // namespace decodium
