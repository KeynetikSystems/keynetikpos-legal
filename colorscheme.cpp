// =============================================================================
// colorscheme.cpp — Implementation of ThemeManager (see colorscheme.h for the
// full WHAT/HOW/WHY).
// -----------------------------------------------------------------------------
// Implementation notes:
//  - Only the non-inline ThemeManager members live here; the palette getters
//    are inline in the header (see the warning below about linker errors).
//  - The dark/light choice persists in QSettings("KeynetikPOS", "MainWindow")
//    and is loaded in the constructor, so the theme survives restarts.
//  - setDark() rebuilds the scheme and emits themeChanged(bool) so every open
//    window can restyle itself live.
// =============================================================================
#include "colorscheme.h"
#include <QSettings>

// ─────────────────────────────────────────────────────────────────────────────
// ThemeManager
//
// Only the non-inline members live here.  getLightColorScheme(),
// getDarkColorScheme(), isDarkMode() and getColorScheme() are inline and
// defined entirely in colorscheme.h — do NOT define them here or you will get
// "multiple definition" linker errors.
// ─────────────────────────────────────────────────────────────────────────────

ThemeManager& ThemeManager::instance()
{
    static ThemeManager inst;
    return inst;
}

ThemeManager::ThemeManager()
{
    QSettings settings("KeynetikPOS", "MainWindow");
    m_isDark = settings.value("theme/darkMode", false).toBool();
    buildScheme();
}

void ThemeManager::setDark(bool dark)
{
    if (m_isDark == dark) return;
    m_isDark = dark;
    buildScheme();

    QSettings settings("KeynetikPOS", "MainWindow");
    settings.setValue("theme/darkMode", dark);

    emit themeChanged(dark);
}

void ThemeManager::buildScheme()
{
    m_scheme = m_isDark ? getDarkColorScheme() : getLightColorScheme();
}
