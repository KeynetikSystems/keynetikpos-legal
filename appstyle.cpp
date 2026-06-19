// =============================================================================
// appstyle.cpp — Implementation of the theme stylesheets/palette (see appstyle.h).
// -----------------------------------------------------------------------------
// Implementation notes:
//  - The sheet is built in two parts: CHROME (window/inputs/buttons/tables/tabs)
//    and SEMANTIC (role/kind/textScale labels, banners, chips, coloured action
//    buttons). Semantic rules apply to every theme; chrome varies:
//      Light/Dark/Classic -> flat chrome (Classic squares the corners)
//      Silver             -> glossy gradient chrome
//      Native             -> no chrome at all, so the OS style renders controls
//  - Colours come from ColorScheme; the glossy chrome derives gradient stops
//    with QColor::lighter()/darker() so it tracks the silver palette.
// =============================================================================
#include "appstyle.h"
#include "colorscheme.h"

#include <QColor>
#include <QApplication>
#include <QStyle>
#include <QStyleFactory>

namespace {

ColorScheme schemeFor(AppTheme t)
{
    switch (t) {
    case AppTheme::Dark:    return getDarkColorScheme();
    case AppTheme::Classic: return getClassicColorScheme();
    case AppTheme::Silver:  return getSilverColorScheme();
    case AppTheme::Native:
    case AppTheme::Light:
    default:                return getLightColorScheme();
    }
}

QString darker(const QString &hex, int factor)  { return QColor(hex).darker(factor).name(); }
QString lighter(const QString &hex, int factor) { return QColor(hex).lighter(factor).name(); }

QString vgrad(const QString &top, const QString &bottom)
{
    return QString("qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 %1, stop:1 %2)")
        .arg(top, bottom);
}

// One block per semantic button/label kind: solid fill, white text,
// derived hover/pressed shades.
QString kindBlock(const QString &kind, const QString &color,
                  const QString &disabledBg, const QString &disabledText)
{
    return QString(
        "QPushButton[kind=\"%1\"] { background-color: %2; color: white; "
        "    border: none; font-weight: bold; border-radius: 5px; }"
        "QPushButton[kind=\"%1\"]:hover   { background-color: %3; }"
        "QPushButton[kind=\"%1\"]:pressed { background-color: %4; }"
        "QPushButton[kind=\"%1\"]:disabled { background-color: %5; color: %6; }"
        "QLabel[kind=\"%1\"] { color: %2; }")
        .arg(kind, color, darker(color, 112), darker(color, 130),
             disabledBg, disabledText);
}

// Flat widget chrome (Light / Dark / Classic). Radii come from %rad% tokens so
// Classic can square the corners. Colours are %tokens% replaced by the caller.
QString flatChrome()
{
    return QString(R"(
        QMainWindow { background-color: %bgPrimary%; }
        QDialog     { background-color: %bgPrimary%; }
        QWidget     { color: %text%; }

        QGroupBox { background-color: %bgSecondary%; border: 2px solid %border%;
            border-radius: %radGroup%; margin-top: 12px; padding: 15px;
            font-weight: bold; color: %text%; }
        QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 5px; }

        QLineEdit, QComboBox, QSpinBox, QDoubleSpinBox,
        QDateEdit, QTimeEdit, QDateTimeEdit {
            background-color: %inputBg%; border: 2px solid %border%;
            border-radius: %rad%; padding: 6px; color: %text%; font-size: 11pt; }
        QLineEdit:focus, QComboBox:focus, QSpinBox:focus, QDoubleSpinBox:focus,
        QDateEdit:focus, QTimeEdit:focus, QDateTimeEdit:focus {
            border: 2px solid %focus%; }
        QComboBox::drop-down { border: none; width: 20px; }
        QComboBox::down-arrow { image: none; width: 0; height: 0;
            border-left: 5px solid transparent; border-right: 5px solid transparent;
            border-top: 6px solid %text%; margin-right: 8px; }
        QComboBox QAbstractItemView { background-color: %inputBg%; color: %text%;
            selection-background-color: %accent%; selection-color: white; }

        QTableView { background-color: %inputBg%; alternate-background-color: %bgSecondary%;
            gridline-color: %border%; border: 1px solid %border%;
            border-radius: %rad%; color: %text%; }
        QTableView::item { padding: 8px; }
        QTableView::item:selected { background-color: %accent%; color: white; }
        QHeaderView::section { background-color: %bgSecondary%; color: %text%;
            padding: 8px; border: none; border-right: 1px solid %border%;
            border-bottom: 2px solid %accent%; font-weight: bold; }

        QPushButton { background-color: %bgSecondary%; color: %text%;
            border: 1px solid %border%; border-radius: %rad%;
            padding: 10px; font-size: 11pt; }
        QPushButton:hover    { background-color: %hover%; }
        QPushButton:pressed  { background-color: %active%; }
        QPushButton:disabled { background-color: %disabledBg%; color: %disabledText%; }

        QCheckBox, QRadioButton { color: %text%; }

        QMenuBar { background-color: %bgSecondary%; color: %text%;
            border-bottom: 1px solid %border%; }
        QMenuBar::item { background-color: transparent; padding: 8px 12px; }
        QMenuBar::item:selected { background-color: %hover%; }
        QMenu { background-color: %bgSecondary%; color: %text%; border: 1px solid %border%; }
        QMenu::item:selected { background-color: %accent%; color: white; }

        QStatusBar { background-color: %bgSecondary%; color: %text%;
            border-top: 1px solid %border%; }

        QLabel[kind="secondary"] { color: %textSecondary%; }

        QTabWidget::pane { border: none; background-color: %bgPrimary%; }
        QTabBar::tab { background: %bgSecondary%; border: 2px solid %border%;
            border-top-left-radius: %radTab%; border-top-right-radius: %radTab%;
            min-width: 120px; padding: 8px 15px; margin-right: 2px;
            font-weight: 600; font-size: 10pt; color: %text%; }
        QTabBar::tab:selected { background: %bgPrimary%; border-color: %accent%;
            border-bottom-color: %bgPrimary%; color: %accent%; }
        QTabBar::tab:hover:!selected { background: %hover%; }

        QScrollArea { border: none; }
    )");
}

// Glossy "brushed metal" chrome for the Silver theme: vertical gradients on
// surfaces, raised bevels on buttons/tabs. Colours embedded directly (derived
// from the silver scheme) rather than via %tokens%.
QString glossyChrome(const ColorScheme &s)
{
    const QString edge   = s.borderColor;
    const QString panel  = vgrad(lighter(s.bgPrimary, 108), s.bgPrimary);
    const QString raised = vgrad(lighter(s.bgSecondary, 104), darker(s.bgSecondary, 108));
    const QString btn    = vgrad("#ffffff", darker(s.bgSecondary, 112));
    const QString btnHi  = vgrad("#ffffff", s.bgSecondary);
    const QString btnDn  = vgrad(darker(s.bgSecondary, 112), "#ffffff");
    const QString header = vgrad(lighter(s.bgPrimary, 112), darker(s.bgPrimary, 106));

    return QString(R"(
        QMainWindow { background: %PANEL%; }
        QDialog     { background: %PANEL%; }
        QWidget     { color: %TEXT%; }

        QGroupBox { background: %RAISED%; border: 1px solid %EDGE%;
            border-radius: 7px; margin-top: 12px; padding: 15px;
            font-weight: bold; color: %TEXT%; }
        QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 5px; }

        QLineEdit, QComboBox, QSpinBox, QDoubleSpinBox,
        QDateEdit, QTimeEdit, QDateTimeEdit {
            background: #ffffff; border: 1px solid %EDGE%; border-radius: 4px;
            padding: 6px; color: %TEXT%; font-size: 11pt; }
        QLineEdit:focus, QComboBox:focus, QSpinBox:focus, QDoubleSpinBox:focus {
            border: 1px solid %FOCUS%; }
        QComboBox::drop-down { border: none; width: 20px; }
        QComboBox::down-arrow { image: none; width: 0; height: 0;
            border-left: 5px solid transparent; border-right: 5px solid transparent;
            border-top: 6px solid %TEXT%; margin-right: 8px; }
        QComboBox QAbstractItemView { background: #ffffff; color: %TEXT%;
            selection-background-color: %ACCENT%; selection-color: white; }

        QTableView { background: #ffffff; alternate-background-color: %ALT%;
            gridline-color: %EDGE%; border: 1px solid %EDGE%; border-radius: 4px;
            color: %TEXT%; }
        QTableView::item { padding: 8px; }
        QTableView::item:selected { background: %ACCENT%; color: white; }
        QHeaderView::section { background: %HEADER%; color: %TEXT%; padding: 8px;
            border: none; border-right: 1px solid %EDGE%;
            border-bottom: 1px solid %EDGE%; font-weight: bold; }

        QPushButton { background: %BTN%; color: %TEXT%; border: 1px solid %EDGE%;
            border-radius: 6px; padding: 10px; font-size: 11pt; }
        QPushButton:hover    { background: %BTNHI%; }
        QPushButton:pressed  { background: %BTNDN%; }
        QPushButton:disabled { background: %DISABLED%; color: %DISABLEDTEXT%; }

        QCheckBox, QRadioButton { color: %TEXT%; }

        QMenuBar { background: %HEADER%; color: %TEXT%; border-bottom: 1px solid %EDGE%; }
        QMenuBar::item { background: transparent; padding: 8px 12px; }
        QMenuBar::item:selected { background: %ACCENT%; color: white; border-radius: 4px; }
        QMenu { background: %RAISED%; color: %TEXT%; border: 1px solid %EDGE%; }
        QMenu::item:selected { background: %ACCENT%; color: white; }

        QStatusBar { background: %HEADER%; color: %TEXT%; border-top: 1px solid %EDGE%; }

        QLabel[kind="secondary"] { color: %TEXTSECONDARY%; }

        QTabWidget::pane { border: 1px solid %EDGE%; border-radius: 4px; background: %RAISED%; }
        QTabBar::tab { background: %BTN%; border: 1px solid %EDGE%;
            border-top-left-radius: 6px; border-top-right-radius: 6px;
            min-width: 110px; padding: 8px 15px; margin-right: 2px;
            font-weight: 600; font-size: 10pt; color: %TEXT%; }
        QTabBar::tab:selected { background: %BTNHI%; }
        QTabBar::tab:hover:!selected { background: %BTNHI%; }

        QScrollArea { border: none; }
    )")
        .replace("%PANEL%", panel)
        .replace("%RAISED%", raised)
        .replace("%BTN%", btn)
        .replace("%BTNHI%", btnHi)
        .replace("%BTNDN%", btnDn)
        .replace("%HEADER%", header)
        .replace("%EDGE%", edge)
        .replace("%ALT%", s.bgSecondary)
        .replace("%FOCUS%", s.inputFocusBorder)
        .replace("%ACCENT%", s.accentPrimary)
        .replace("%DISABLEDTEXT%", s.disabledText)
        .replace("%DISABLED%", s.disabledBg)
        .replace("%TEXTSECONDARY%", s.textSecondary)
        .replace("%TEXT%", s.textPrimary);
}

// Semantic rules (role/kind/textScale, banners, chips, titles, separators,
// stat-card values). Applied to EVERY theme so the app's structure reads the
// same even under the Native style. Uses %tokens%.
QString semanticPart()
{
    return QString(R"(
        QLabel { background: transparent; }

        /* Product grid (MainWindow) — cards paint themselves in the delegate */
        QListView#productGrid { background-color: transparent; border: none; }

        /* ── Totals-panel label roles ─────────────────────────────────── */
        QLabel[role="amount"]         { font-size: 14pt; }
        QLabel[role="amountDiscount"] { font-size: 14pt; color: %warning%; }
        QLabel[role="totalTitle"]     { font-size: 18pt; font-weight: bold; }
        QLabel[role="amountTotal"]    { font-size: 20pt; font-weight: bold; color: %success%; }

        /* ── Status banners (combine with kind for colour) ────────────── */
        QLabel[role="banner"] { padding: 10px; border-radius: 5px; font-weight: bold; }
        QLabel[role="banner"][kind="success"] { background-color: %successBg%;
            border: 1px solid %successBorder%; color: %success%; }
        QLabel[role="banner"][kind="danger"]  { background-color: %errorBg%;
            border: 1px solid %errorBorder%; color: %error%; }
        QLabel[role="banner"][kind="warning"] { background-color: %warningBg%;
            border: 1px solid %border%; color: %text%; }
        QLabel[role="banner"][kind="info"]    { background-color: %infoBg%;
            border: 1px solid %border%; color: %text%; }

        /* ── Dialog / section titles ──────────────────────────────────── */
        QLabel[role="dialogTitle"]  { font-size: 18pt; font-weight: bold; }
        QLabel[role="sectionTitle"] { font-size: 16pt; font-weight: bold; }

        /* ── Stat-card values (combine with kind for colour) ──────────── */
        QLabel[role="statValue"]   { font-size: 18pt; font-weight: bold; }
        QLabel[role="statValueLg"] { font-size: 24pt; font-weight: bold; }

        /* ── Chips: solid semantic fill, white text ───────────────────── */
        QLabel[role="chip"] { padding: 8px 10px; border-radius: 5px;
            font-weight: bold; color: white; }
        QLabel[role="chip"][kind="primary"]   { background-color: %accent%;          color: white; }
        QLabel[role="chip"][kind="success"]   { background-color: %accent%;          color: white; }
        QLabel[role="chip"][kind="info"]      { background-color: %accentSecondary%; color: white; }
        QLabel[role="chip"][kind="warning"]   { background-color: %warning%;         color: white; }
        QLabel[role="chip"][kind="danger"]    { background-color: %error%;           color: white; }
        QLabel[role="chip"][kind="secondary"] { background-color: %textSecondary%;   color: white; }
        QLabel[role="chip"][kind="tertiary"]  { background-color: %accentTertiary%;  color: white; }

        /* ── Semantic group boxes (alert sections) ────────────────────── */
        QGroupBox[kind="danger"]  { border-color: %error%;   color: %error%; }
        QGroupBox[kind="warning"] { border-color: %warning%; color: %warning%; }

        /* ── Horizontal separator lines ───────────────────────────────── */
        QFrame[role="hline"] { color: %border%; }

        /* ── Reusable type-scale (theme-independent sizing) ───────────── */
        QWidget[textScale="sm"]  { font-size: 10pt; }
        QWidget[textScale="md"]  { font-size: 12pt; }
        QWidget[textScale="lg"]  { font-size: 14pt; }
        QWidget[textScale="xl"]  { font-size: 16pt; }
        QWidget[textScale="2xl"] { font-size: 20pt; }
        QWidget[textScale="lg"][role="control"],
        QWidget[textScale="xl"][role="control"] { padding: 8px; }
        QWidget[bold="true"] { font-weight: bold; }

        /* ── One-off widgets ──────────────────────────────────────────── */
        QStatusBar QPushButton { padding: 5px 15px; border-radius: 3px; font-size: 10pt; }
        QPushButton#checkoutButton { font-size: 16pt; }
        QPushButton#newCartButton  { font-size: 13pt; padding: 8px 16px; }
        QLabel#currentUserLabel    { font-weight: bold; padding: 5px; }
        QLabel#summaryValue        { font-size: 24pt; font-weight: bold; padding: 4px; }
        QLabel#summaryDescription  { color: %textSecondary%; font-weight: bold; padding: 2px; }
    )");
}

// Slim, themed scrollbars. Added to every theme EXCEPT Native (which keeps the
// OS scrollbars). Without this the app inherited the base style's scrollbars —
// e.g. a light native bar on the Dark theme. Uses %tokens%.
QString scrollbarRules()
{
    return QString(R"(
        QScrollBar:vertical   { background: %bgPrimary%; width: 12px;  margin: 0; border: none; }
        QScrollBar:horizontal { background: %bgPrimary%; height: 12px; margin: 0; border: none; }
        QScrollBar::handle:vertical   { background: %border%; min-height: 28px;
            border-radius: 6px; margin: 2px; }
        QScrollBar::handle:horizontal { background: %border%; min-width: 28px;
            border-radius: 6px; margin: 2px; }
        QScrollBar::handle:hover { background: %textSecondary%; }
        QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; background: none; border: none; }
        QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }
        QAbstractScrollArea::corner { background: %bgPrimary%; border: none; }
    )");
}

} // namespace

QString appStylesheet(AppTheme theme)
{
    const ColorScheme s = schemeFor(theme);

    QString sheet;
    if (theme == AppTheme::Silver)
        sheet = glossyChrome(s);
    else if (theme != AppTheme::Native)   // Native: no chrome — let the OS style render
        sheet = flatChrome();
    sheet += semanticPart();
    if (theme != AppTheme::Native)        // keep native scrollbars under Native
        sheet += scrollbarRules();

    // Corner radii — Classic squares them off for the WinForms look.
    const bool squared = (theme == AppTheme::Classic);
    sheet.replace("%radGroup%", squared ? "2px" : "8px");
    sheet.replace("%radTab%",   squared ? "2px" : "6px");
    sheet.replace("%rad%",      squared ? "2px" : "5px");

    sheet.replace("%bgPrimary%",       s.bgPrimary);
    sheet.replace("%bgSecondary%",     s.bgSecondary);
    sheet.replace("%text%",            s.textPrimary);
    sheet.replace("%textSecondary%",   s.textSecondary);
    sheet.replace("%border%",          s.borderColor);
    sheet.replace("%inputBg%",         s.inputBg);
    sheet.replace("%focus%",           s.inputFocusBorder);
    sheet.replace("%accentSecondary%", s.accentSecondary);
    sheet.replace("%accentTertiary%",  s.accentTertiary);
    sheet.replace("%accent%",          s.accentPrimary);
    sheet.replace("%infoBg%",          s.infoBg);
    sheet.replace("%hover%",           s.hoverColor);
    sheet.replace("%active%",          s.activeColor);
    sheet.replace("%disabledBg%",      s.disabledBg);
    sheet.replace("%disabledText%",    s.disabledText);
    sheet.replace("%successBg%",       s.successBg);
    sheet.replace("%successBorder%",   s.successBorder);
    sheet.replace("%success%",         s.success);
    sheet.replace("%warningBg%",       s.warningBg);
    sheet.replace("%warning%",         s.warning);
    sheet.replace("%errorBg%",         s.errorBg);
    sheet.replace("%errorBorder%",     s.errorBorder);
    sheet.replace("%error%",           s.error);

    sheet += kindBlock("primary", s.accentPrimary,   s.disabledBg, s.disabledText);
    sheet += kindBlock("danger",  s.error,           s.disabledBg, s.disabledText);
    sheet += kindBlock("warning", s.warning,         s.disabledBg, s.disabledText);
    sheet += kindBlock("info",    s.accentSecondary, s.disabledBg, s.disabledText);
    sheet += kindBlock("tertiary",s.accentTertiary,  s.disabledBg, s.disabledText);
    sheet += QString("QLabel[kind=\"success\"] { color: %1; }").arg(s.success);

    return sheet;
}

QPalette appPalette(AppTheme theme)
{
    const ColorScheme s = schemeFor(theme);

    QPalette pal;
    pal.setColor(QPalette::Window,          QColor(s.bgPrimary));
    pal.setColor(QPalette::WindowText,      QColor(s.textPrimary));
    pal.setColor(QPalette::Base,            QColor(s.inputBg));
    pal.setColor(QPalette::AlternateBase,   QColor(s.bgSecondary));
    pal.setColor(QPalette::Text,            QColor(s.textPrimary));
    pal.setColor(QPalette::Button,          QColor(s.bgSecondary));
    pal.setColor(QPalette::ButtonText,      QColor(s.textPrimary));
    pal.setColor(QPalette::Highlight,       QColor(s.accentPrimary));
    pal.setColor(QPalette::HighlightedText, QColor("#ffffff"));
    pal.setColor(QPalette::PlaceholderText, QColor(s.textSecondary));
    pal.setColor(QPalette::ToolTipBase,     QColor(s.bgSecondary));
    pal.setColor(QPalette::ToolTipText,     QColor(s.textPrimary));
    return pal;
}

QString appStyleName(AppTheme theme)
{
    if (theme == AppTheme::Classic || theme == AppTheme::Silver)
        return QStringLiteral("Fusion");   // clean, consistent base for custom QSS

    // Light / Dark / Native ride on the platform-native style.
    const QStringList keys = QStyleFactory::keys();
    for (const char *cand : {"windowsvista", "macos", "macintosh", "windows"})
        if (keys.contains(QLatin1String(cand), Qt::CaseInsensitive))
            return QString::fromLatin1(cand);
    return QStringLiteral("Fusion");
}

void applyAppTheme(AppTheme theme)
{
    if (QStyle *style = QStyleFactory::create(appStyleName(theme)))
        QApplication::setStyle(style);

    if (theme == AppTheme::Native)
        QApplication::setPalette(QApplication::style()->standardPalette());
    else
        QApplication::setPalette(appPalette(theme));

    if (qApp)
        qApp->setStyleSheet(appStylesheet(theme));
}
