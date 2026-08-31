// DecoRTTY — the colour system, shared with the rest of the Decodium family.
//
// The palette is deliberately identical to Decodium's so a station running both
// reads as one instrument rather than two unrelated programs. Exposed as a QML
// singleton rather than duplicated literals in every file, so a theme change is
// one edit.
#pragma once

#include <QColor>
#include <QObject>
#include <QtQml/qqmlregistration.h>

namespace decortty::app {

class Theme : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QColor bgDeep       READ bgDeep       CONSTANT)
    Q_PROPERTY(QColor bgMedium     READ bgMedium     CONSTANT)
    Q_PROPERTY(QColor bgLight      READ bgLight      CONSTANT)
    Q_PROPERTY(QColor primary      READ primary      CONSTANT)
    Q_PROPERTY(QColor secondary    READ secondary    CONSTANT)
    Q_PROPERTY(QColor accent       READ accent       CONSTANT)
    Q_PROPERTY(QColor warning      READ warning      CONSTANT)
    Q_PROPERTY(QColor error        READ error        CONSTANT)
    Q_PROPERTY(QColor success      READ success      CONSTANT)
    Q_PROPERTY(QColor textPrimary  READ textPrimary  CONSTANT)
    Q_PROPERTY(QColor textSecondary READ textSecondary CONSTANT)
    Q_PROPERTY(QColor glassOverlay READ glassOverlay CONSTANT)
    Q_PROPERTY(QColor glassBorder  READ glassBorder  CONSTANT)
    Q_PROPERTY(QString monoFamily  READ monoFamily   CONSTANT)

public:
    explicit Theme(QObject* parent = nullptr) : QObject(parent) {}

    QColor bgDeep()        const { return QColor("#0A0F1A"); }
    QColor bgMedium()      const { return QColor("#111827"); }
    QColor bgLight()       const { return QColor("#1E2D42"); }
    QColor primary()       const { return QColor("#4A90E2"); }
    QColor secondary()     const { return QColor("#00D4FF"); }
    QColor accent()        const { return QColor("#00FF88"); }
    QColor warning()       const { return QColor("#FF8C00"); }
    QColor error()         const { return QColor("#FF5F56"); }
    QColor success()       const { return QColor("#4CAF50"); }
    QColor textPrimary()   const { return QColor("#E8F4FD"); }
    QColor textSecondary() const { return QColor("#89B4D0"); }
    QColor glassOverlay()  const { return QColor(26, 58, 92, 64); }
    QColor glassBorder()   const { return QColor(74, 144, 226, 80); }

    // Teleprinter text wants a fixed pitch — column alignment is how an operator
    // reads a contest exchange at a glance.
    QString monoFamily() const
    {
#ifdef Q_OS_WIN
        return QStringLiteral("Cascadia Mono, Consolas, monospace");
#else
        return QStringLiteral("monospace");
#endif
    }
};

} // namespace decortty::app
