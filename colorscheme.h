// =============================================================================
// colorscheme.h — ColorScheme + ThemeManager: application theming
// -----------------------------------------------------------------------------
// WHAT: ColorScheme is a flat struct of named colour strings (backgrounds,
//       text, borders, semantic accents, button states). ThemeManager is the
//       singleton holding the current light/dark state and scheme, emitting
//       themeChanged(bool) on toggle.
// HOW:  getLightColorScheme()/getDarkColorScheme() are INLINE header functions
//       returning hard-coded palettes (light mimics classic Windows system
//       colours; dark mimics Win11/VS Code). ThemeManager persists the choice
//       in QSettings and rebuilds the scheme on toggle; widgets either connect
//       to themeChanged or call getColorScheme() when (re)styling.
// WHY:  Qt stylesheets are strings, so a struct of colour strings composes
//       directly into setStyleSheet() calls. Centralizing the palette makes
//       dark mode a single boolean flip rather than per-dialog colour edits;
//       QSettings persistence makes the choice survive restarts.
// =============================================================================
#ifndef COLORSCHEME_H
#define COLORSCHEME_H

#include <QObject>
#include <QString>
#include <QColor>

// ─────────────────────────────────────────────────────────────────────────────
// ColorScheme — flat bag of colours used by all dialogs
//
// This struct is THE palette. appstyle.cpp builds the application-wide
// stylesheet from it, and dialogs that need per-cell colours (table
// highlights, banners) read the same fields, so light/dark stay consistent
// everywhere.
// ─────────────────────────────────────────────────────────────────────────────
struct ColorScheme {
    // Backgrounds
    QString bgPrimary;        // window background
    QString bgSecondary;      // raised surfaces: group boxes, cards, bars

    // Text
    QString textPrimary;
    QString textSecondary;

    // Borders / inputs
    QString borderColor;
    QString inputBg;          // also the table base background
    QString inputFocusBorder;

    // Semantic accent colours
    QString accentPrimary;    // green  – primary action buttons
    QString accentSecondary;  // blue   – secondary action
    QString accentTertiary;   // purple – tertiary metric accent (avg values etc.)
    QString info;             // blue   – informational
    QString success;          // green  – success text (brighter than accent in dark)
    QString warning;          // orange
    QString error;            // red

    // Soft semantic surfaces (table-cell highlights, banners)
    QString successBg;
    QString successBorder;
    QString warningBg;        // also "new / unsaved item" highlight
    QString errorBg;
    QString errorBorder;
    QString infoBg;           // soft blue surface (summary banners, top-row highlights)

    // Button states
    QString hoverColor;
    QString activeColor;
    QString disabledBg;       // also read-only table cells
    QString disabledText;
};

// ─────────────────────────────────────────────────────────────────────────────
// ThemeManager — single source of truth for the current theme
// ─────────────────────────────────────────────────────────────────────────────
class ThemeManager : public QObject
{
    Q_OBJECT

public:
    static ThemeManager& instance();

    bool   isDark()  const { return m_isDark; }
    void   setDark(bool dark);
    void   toggle()        { setDark(!m_isDark); }

    const ColorScheme& scheme() const { return m_scheme; }

signals:
    void themeChanged(bool isDark);

private:
    ThemeManager();
    ~ThemeManager() = default;
    ThemeManager(const ThemeManager&) = delete;
    ThemeManager& operator=(const ThemeManager&) = delete;

    void buildScheme();

    bool        m_isDark  = false;
    ColorScheme m_scheme;
};

// ─────────────────────────────────────────────────────────────────────────────
// Free-function helpers
//
// IMPORTANT: these are inline, so their full bodies must live here in the
// header — NOT in a .cpp file.  Every translation unit that calls them gets
// its own copy, which is exactly what the linker needs.
// ─────────────────────────────────────────────────────────────────────────────

inline bool isDarkMode()
{
    return ThemeManager::instance().isDark();
}

inline ColorScheme getLightColorScheme()
{
    ColorScheme s;
    s.bgPrimary        = "#f5f5f5";
    s.bgSecondary      = "#ffffff";

    s.textPrimary      = "#212121";
    s.textSecondary    = "#6d6d6d";

    s.borderColor      = "#e0e0e0";
    s.inputBg          = "#ffffff";
    s.inputFocusBorder = "#27ae60";

    // Flat-UI accents — the palette the app was already (inconsistently)
    // using; now the single authoritative set.
    s.accentPrimary    = "#27ae60";
    s.accentSecondary  = "#3498db";
    s.accentTertiary   = "#9b59b6";
    s.info             = "#3498db";
    s.success          = "#27ae60";
    s.warning          = "#f39c12";
    s.error            = "#e74c3c";

    s.successBg        = "#e8f5e9";
    s.successBorder    = "#b6dfb9";
    s.warningBg        = "#fff4dc";
    s.errorBg          = "#fdecea";
    s.errorBorder      = "#f5b7b1";
    s.infoBg           = "#e3f2fd";

    s.hoverColor       = "#e8e8e8";
    s.activeColor      = "#d8d8d8";
    s.disabledBg       = "#f0f0f0";
    s.disabledText     = "#a0a0a0";
    return s;
}

inline ColorScheme getDarkColorScheme()
{
    ColorScheme s;
    s.bgPrimary        = "#1e1e1e";
    s.bgSecondary      = "#252525";

    s.textPrimary      = "#e0e0e0";
    s.textSecondary    = "#a0a0a0";

    s.borderColor      = "#3d3d3d";
    s.inputBg          = "#2d2d2d";
    s.inputFocusBorder = "#27ae60";

    s.accentPrimary    = "#27ae60";
    s.accentSecondary  = "#3498db";
    s.accentTertiary   = "#9b59b6";
    s.info             = "#4aa3df";
    s.success          = "#2ecc71";   // brighter than the accent for dark-bg text
    s.warning          = "#f39c12";
    s.error            = "#e74c3c";

    s.successBg        = "#1d3a27";
    s.successBorder    = "#2f5d3a";
    s.warningBg        = "#3d331a";
    s.errorBg          = "#3d2222";
    s.errorBorder      = "#5d3434";
    s.infoBg           = "#1e3a5f";

    s.hoverColor       = "#3a3a3a";
    s.activeColor      = "#2f2f2f";
    s.disabledBg       = "#333333";
    s.disabledText     = "#777777";
    return s;
}

inline ColorScheme getColorScheme()
{
    return ThemeManager::instance().scheme();
}

#endif // COLORSCHEME_H
