#include "DecodiumThemeManager.h"
#include <QSettings>

const DecodiumThemeManager::ThemePalette DecodiumThemeManager::s_oceanBlue {
    /* bgDeep         */ QColor("#0A0F1A"),
    /* bgMedium       */ QColor("#111827"),
    /* bgLight        */ QColor("#1E2D42"),
    /* primaryColor   */ QColor("#4A90E2"),
    /* secondaryColor */ QColor("#00D4FF"),
    /* accentColor    */ QColor("#00FF88"),
    /* warningColor   */ QColor("#FF8C00"),
    /* errorColor     */ QColor("#FF5F56"),
    /* textPrimary    */ QColor("#E8F4FD"),
    /* textSecondary  */ QColor("#89B4D0"),
    /* successColor   */ QColor("#4CAF50"),
    /* glassOverlay   */ QColor(26, 58, 92, 64),
    /* glassBorder    */ QColor(74, 144, 226, 80),
    /* borderColor    */ QColor(74, 144, 226, 80),
    /* borderSoft     */ QColor(74, 144, 226, 40),
    /* panelColor     */ QColor("#1E2D42"),
    /* panelHeader    */ QColor("#283C57"),
    /* rowMatchBg     */ QColor(0, 255, 136, 38),
    /* rowMatchBorder */ QColor("#00FF88"),
    /* ledRed         */ QColor("#FF5F56"),
    /* ledBlue        */ QColor("#4A90E2"),
    /* ledYellow      */ QColor("#FFD700"),
    /* ledMagenta     */ QColor("#FF00FF"),
    /* isLight        */ false
};

const DecodiumThemeManager::ThemePalette DecodiumThemeManager::s_stellarLight {
    /* bgDeep         */ QColor("#DDE6EE"),
    /* bgMedium       */ QColor("#CFDBE6"),
    /* bgLight        */ QColor("#F2F6FA"),
    /* primaryColor   */ QColor("#2D6BB0"),
    /* secondaryColor */ QColor("#1F8AA0"),
    /* accentColor    */ QColor("#2D8956"),
    /* warningColor   */ QColor("#B68726"),
    /* errorColor     */ QColor("#C33D3D"),
    /* textPrimary    */ QColor("#061325"),
    /* textSecondary  */ QColor("#3D5572"),
    /* successColor   */ QColor("#2D8956"),
    /* glassOverlay   */ QColor(255, 255, 255, 200),
    /* glassBorder    */ QColor("#8FA4B8"),
    /* borderColor    */ QColor("#8FA4B8"),
    /* borderSoft     */ QColor("#A9BDCE"),
    /* panelColor     */ QColor("#F2F6FA"),
    /* panelHeader    */ QColor("#E1EAF2"),
    /* rowMatchBg     */ QColor("#A8E0BC"),
    /* rowMatchBorder */ QColor("#2D8956"),
    /* ledRed         */ QColor("#C33D3D"),
    /* ledBlue        */ QColor("#2D6BB0"),
    /* ledYellow      */ QColor("#B68726"),
    /* ledMagenta     */ QColor("#6B5BAB"),
    /* isLight        */ true
};

DecodiumThemeManager::DecodiumThemeManager(QObject* parent)
    : QObject(parent)
{
    QSettings s("Decodium", "Decodium");
    // One-shot migration: from 1.0.70 the dark Ocean Blue theme is the
    // canonical default. Reset any persisted choice once so the upgrade
    // lands on dark; the user can re-select Stellar Light afterwards.
    if (!s.value("theme/migrated_v2", false).toBool()) {
        s.setValue("theme/current", "Ocean Blue");
        // Stellar Light forces palette index 11 (pastel light) on the
        // panadapter. Reset any residual 11 so the dark default lands on
        // a sensible spectrum palette instead of an all-white waterfall.
        // uiPaletteIndex is persisted by DecodiumBridge under the
        // "Decodium3" store, not the same one used for theme/current.
        QSettings bridgeStore("Decodium", "Decodium3");
        if (bridgeStore.value("uiPaletteIndex", 0).toInt() == 11)
            bridgeStore.setValue("uiPaletteIndex", 0);
        s.setValue("theme/migrated_v2", true);
    }
    QString const stored = s.value("theme/current", "Ocean Blue").toString();
    if (stored == "Stellar Light" || stored == "Ocean Blue") {
        m_currentTheme = stored;
    } else {
        m_currentTheme = "Ocean Blue";
    }
    // 1.0.305 (#6) — colori UI personalizzati (sfondo+testo), opt-in default OFF
    m_customColorsEnabled = s.value("theme/customEnabled", false).toBool();
    m_customBgColor       = s.value("theme/customBg", QString()).toString();
    m_customTextColor     = s.value("theme/customText", QString()).toString();
}

// 1.0.305 (#6) — override colori UI ----------------------------------------
QColor DecodiumThemeManager::customBg() const
{
    if (!m_customColorsEnabled) return QColor();
    QColor c(m_customBgColor);
    return c.isValid() ? c : QColor();
}

QColor DecodiumThemeManager::customText() const
{
    if (!m_customColorsEnabled) return QColor();
    QColor c(m_customTextColor);
    return c.isValid() ? c : QColor();
}

// Sfumatura "elevazione" per i livelli di sfondo: su uno sfondo scuro schiarisce,
// su uno chiaro scurisce leggermente → i pannelli restano leggibili su qualsiasi base.
QColor DecodiumThemeManager::elevate(const QColor& base, double factor)
{
    if (!base.isValid()) return base;
    return base.lightnessF() < 0.5
        ? base.lighter(static_cast<int>(100 + factor * 100))
        : base.darker(static_cast<int>(100 + factor * 45));
}

void DecodiumThemeManager::setCustomColorsEnabled(bool v)
{
    if (m_customColorsEnabled == v) return;
    m_customColorsEnabled = v;
    QSettings("Decodium", "Decodium").setValue("theme/customEnabled", v);
    emit paletteChanged();
}

void DecodiumThemeManager::setCustomBgColor(const QString& hex)
{
    QString const h = hex.trimmed();
    if (m_customBgColor == h) return;
    m_customBgColor = h;
    QSettings("Decodium", "Decodium").setValue("theme/customBg", h);
    if (m_customColorsEnabled) emit paletteChanged();
}

void DecodiumThemeManager::setCustomTextColor(const QString& hex)
{
    QString const h = hex.trimmed();
    if (m_customTextColor == h) return;
    m_customTextColor = h;
    QSettings("Decodium", "Decodium").setValue("theme/customText", h);
    if (m_customColorsEnabled) emit paletteChanged();
}

const DecodiumThemeManager::ThemePalette& DecodiumThemeManager::currentPalette() const
{
    if (m_currentTheme == "Stellar Light") return s_stellarLight;
    return s_oceanBlue;
}

void DecodiumThemeManager::setCurrentTheme(const QString& name)
{
    if (m_currentTheme == name) return;
    if (name != "Ocean Blue" && name != "Stellar Light") return;
    m_currentTheme = name;
    QSettings("Decodium", "Decodium").setValue("theme/current", name);
    emit currentThemeChanged();
    emit paletteChanged();
}

bool DecodiumThemeManager::isLightTheme() const
{
    QColor const c = customBg();
    if (c.isValid()) return c.lightnessF() >= 0.5;
    return currentPalette().isLight;
}
QColor DecodiumThemeManager::bgDeep() const
{
    QColor const c = customBg();
    return c.isValid() ? c : currentPalette().bgDeep;
}
QColor DecodiumThemeManager::bgMedium() const
{
    QColor const c = customBg();
    return c.isValid() ? elevate(c, 0.18) : currentPalette().bgMedium;
}
QColor DecodiumThemeManager::bgLight() const
{
    QColor const c = customBg();
    return c.isValid() ? elevate(c, 0.40) : currentPalette().bgLight;
}
QColor DecodiumThemeManager::primaryColor()   const { return currentPalette().primaryColor; }
QColor DecodiumThemeManager::secondaryColor() const { return currentPalette().secondaryColor; }
QColor DecodiumThemeManager::accentColor()    const { return currentPalette().accentColor; }
QColor DecodiumThemeManager::warningColor()   const { return currentPalette().warningColor; }
QColor DecodiumThemeManager::errorColor()     const { return currentPalette().errorColor; }
QColor DecodiumThemeManager::textPrimary() const
{
    QColor const c = customText();
    return c.isValid() ? c : currentPalette().textPrimary;
}
QColor DecodiumThemeManager::textSecondary() const
{
    QColor c = customText();
    if (c.isValid()) { c.setAlphaF(0.62); return c; }
    return currentPalette().textSecondary;
}
QColor DecodiumThemeManager::successColor()   const { return currentPalette().successColor; }
QColor DecodiumThemeManager::glassOverlay()   const { return currentPalette().glassOverlay; }
QColor DecodiumThemeManager::glassBorder()    const { return currentPalette().glassBorder; }
QColor DecodiumThemeManager::borderColor()    const { return currentPalette().borderColor; }
QColor DecodiumThemeManager::borderSoft()     const { return currentPalette().borderSoft; }
QColor DecodiumThemeManager::panelColor() const
{
    QColor const c = customBg();
    return c.isValid() ? elevate(c, 0.40) : currentPalette().panelColor;
}
QColor DecodiumThemeManager::panelHeader() const
{
    QColor const c = customBg();
    return c.isValid() ? elevate(c, 0.26) : currentPalette().panelHeader;
}
QColor DecodiumThemeManager::rowMatchBg()     const { return currentPalette().rowMatchBg; }
QColor DecodiumThemeManager::rowMatchBorder() const { return currentPalette().rowMatchBorder; }
QColor DecodiumThemeManager::ledRed()         const { return currentPalette().ledRed; }
QColor DecodiumThemeManager::ledBlue()        const { return currentPalette().ledBlue; }
QColor DecodiumThemeManager::ledYellow()      const { return currentPalette().ledYellow; }
QColor DecodiumThemeManager::ledMagenta()     const { return currentPalette().ledMagenta; }
