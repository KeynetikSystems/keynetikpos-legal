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
    // Prefer the named theme; fall back to the legacy darkMode bool so existing
    // installs keep their choice.
    if (settings.contains("theme/name")) {
        int v = settings.value("theme/name").toInt();
        if (v < 0 || v > static_cast<int>(AppTheme::Silver))
            v = static_cast<int>(AppTheme::Light);
        m_theme = static_cast<AppTheme>(v);
    } else {
        m_theme = settings.value("theme/darkMode", false).toBool()
                      ? AppTheme::Dark : AppTheme::Light;
    }
    buildScheme();
}

void ThemeManager::setTheme(AppTheme t)
{
    if (m_theme == t) return;
    m_theme = t;
    buildScheme();

    QSettings settings("KeynetikPOS", "MainWindow");
    settings.setValue("theme/name", static_cast<int>(t));
    settings.setValue("theme/darkMode", isDark());   // keep legacy key in sync

    emit themeChanged(isDark());
}

void ThemeManager::buildScheme()
{
    switch (m_theme) {
    case AppTheme::Dark:    m_scheme = getDarkColorScheme();    break;
    case AppTheme::Classic: m_scheme = getClassicColorScheme(); break;
    case AppTheme::Silver:  m_scheme = getSilverColorScheme();  break;
    case AppTheme::Native:  // native widgets render themselves; the scheme is
    case AppTheme::Light:   // only used for cell-highlight brushes etc.
    default:                m_scheme = getLightColorScheme();   break;
    }
}
