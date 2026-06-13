// =============================================================================
// appstyle.cpp — Implementation of appStylesheet()/appPalette() (see
// appstyle.h for the full WHAT/HOW/WHY).
// -----------------------------------------------------------------------------
// Implementation notes:
//  - Every colour comes from ColorScheme (colorscheme.h) — there are no
//    hard-coded hex values here, so light/dark can never drift apart.
//  - Table rules target QTableView (not QTableWidget) so they style both the
//    model/view cart table and the QTableWidget-based dialog tables —
//    stylesheet class selectors match subclasses.
//  - Hover/pressed shades for the semantic buttons are derived with
//    QColor::darker() instead of being stored as extra palette fields.
// =============================================================================
#include "appstyle.h"
#include "colorscheme.h"

#include <QColor>

namespace {

QString darker(const QString &hex, int factor)
{
    return QColor(hex).darker(factor).name();
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

} // namespace

QString appStylesheet(bool dark)
{
    const ColorScheme s = dark ? getDarkColorScheme() : getLightColorScheme();

    QString sheet = QString(R"(
        QMainWindow { background-color: %bgPrimary%; }
        QDialog     { background-color: %bgPrimary%; }
        /* No QWidget background rule: plain containers stay transparent and
           inherit their parent's surface; only real surfaces paint. */
        QWidget     { color: %text%; }

        QGroupBox { background-color: %bgSecondary%; border: 2px solid %border%;
            border-radius: 8px; margin-top: 12px; padding: 15px;
            font-weight: bold; color: %text%; }
        QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 5px; }

        QLineEdit, QComboBox, QSpinBox, QDoubleSpinBox,
        QDateEdit, QTimeEdit, QDateTimeEdit {
            background-color: %inputBg%; border: 2px solid %border%;
            border-radius: 5px; padding: 6px; color: %text%; font-size: 11pt; }
        QLineEdit:focus, QComboBox:focus, QSpinBox:focus, QDoubleSpinBox:focus,
        QDateEdit:focus, QTimeEdit:focus, QDateTimeEdit:focus {
            border: 2px solid %focus%; }
        QComboBox::drop-down { border: none; width: 20px; }
        QComboBox QAbstractItemView { background-color: %inputBg%; color: %text%;
            selection-background-color: %accent%; selection-color: white; }

        QTableView { background-color: %inputBg%; alternate-background-color: %bgSecondary%;
            gridline-color: %border%; border: 1px solid %border%;
            border-radius: 5px; color: %text%; }
        QTableView::item { padding: 8px; }
        QTableView::item:selected { background-color: %accent%; color: white; }
        QHeaderView::section { background-color: %bgSecondary%; color: %text%;
            padding: 8px; border: none; border-right: 1px solid %border%;
            border-bottom: 2px solid %accent%; font-weight: bold; }

        QPushButton { background-color: %bgSecondary%; color: %text%;
            border: 1px solid %border%; border-radius: 5px;
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

        QLabel { color: %text%; background: transparent; }
        QLabel[kind="secondary"] { color: %textSecondary%; }

        QTabWidget::pane { border: none; background-color: %bgPrimary%; }
        QTabBar::tab { background: %bgSecondary%; border: 2px solid %border%;
            border-top-left-radius: 6px; border-top-right-radius: 6px;
            min-width: 120px; padding: 8px 15px; margin-right: 2px;
            font-weight: 600; font-size: 10pt; color: %text%; }
        QTabBar::tab:selected { background: %bgPrimary%; border-color: %accent%;
            border-bottom-color: %bgPrimary%; color: %accent%; }
        QTabBar::tab:hover:!selected { background: %hover%; }

        QScrollArea { border: none; }

        /* Product grid (MainWindow) — cards paint themselves in the delegate */
        QListView#productGrid { background-color: transparent; border: none; }

        /* ── Totals-panel label roles ─────────────────────────────────── */
        QLabel[role="amount"]         { font-size: 14pt; }
        QLabel[role="amountDiscount"] { font-size: 14pt; color: %warning%; }
        QLabel[role="totalTitle"]     { font-size: 18pt; font-weight: bold; }
        QLabel[role="amountTotal"]    { font-size: 20pt; font-weight: bold; color: %success%; }

        /* ── Status banners (combine with kind for colour) ────────────── */
        QLabel[role="banner"] { padding: 10px; border-radius: 5px;
            font-weight: bold; }
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

        /* ── Reusable type-scale (theme-independent sizing) ───────────────
           Opt in with setProperty("textScale", "sm|md|lg|xl|2xl") instead of
           inline `setStyleSheet("font-size: …")`. The attribute selector
           outranks the bare-type base rules (e.g. QLineEdit 11pt), so it works
           on labels, radios, spin boxes and line edits alike. */
        QWidget[textScale="sm"]  { font-size: 10pt; }
        QWidget[textScale="md"]  { font-size: 12pt; }
        QWidget[textScale="lg"]  { font-size: 14pt; }
        QWidget[textScale="xl"]  { font-size: 16pt; }
        QWidget[textScale="2xl"] { font-size: 20pt; }
        QWidget[textScale="lg"][role="control"],
        QWidget[textScale="xl"][role="control"] { padding: 8px; }
        QWidget[bold="true"] { font-weight: bold; }

        /* ── One-off widgets ──────────────────────────────────────────── */
        QStatusBar QPushButton { padding: 5px 15px; border-radius: 3px;
            font-size: 10pt; }
        QPushButton#checkoutButton { font-size: 16pt; }
        QPushButton#newCartButton  { font-size: 13pt; padding: 8px 16px; }
        QLabel#currentUserLabel    { font-weight: bold; padding: 5px; }
        QLabel#summaryValue        { font-size: 24pt; font-weight: bold; padding: 4px; }
        QLabel#summaryDescription  { color: %textSecondary%; font-weight: bold; padding: 2px; }
    )");

    sheet.replace("%bgPrimary%",    s.bgPrimary);
    sheet.replace("%bgSecondary%",  s.bgSecondary);
    sheet.replace("%text%",         s.textPrimary);
    sheet.replace("%textSecondary%",s.textSecondary);
    sheet.replace("%border%",       s.borderColor);
    sheet.replace("%inputBg%",      s.inputBg);
    sheet.replace("%focus%",        s.inputFocusBorder);
    sheet.replace("%accent%",       s.accentPrimary);
    sheet.replace("%accentSecondary%", s.accentSecondary);
    sheet.replace("%accentTertiary%",  s.accentTertiary);
    sheet.replace("%infoBg%",       s.infoBg);
    sheet.replace("%hover%",        s.hoverColor);
    sheet.replace("%active%",       s.activeColor);
    sheet.replace("%disabledBg%",   s.disabledBg);
    sheet.replace("%disabledText%", s.disabledText);
    sheet.replace("%success%",      s.success);
    sheet.replace("%successBg%",    s.successBg);
    sheet.replace("%successBorder%",s.successBorder);
    sheet.replace("%warning%",      s.warning);
    sheet.replace("%warningBg%",    s.warningBg);
    sheet.replace("%error%",        s.error);
    sheet.replace("%errorBg%",      s.errorBg);
    sheet.replace("%errorBorder%",  s.errorBorder);

    sheet += kindBlock("primary", s.accentPrimary,   s.disabledBg, s.disabledText);
    sheet += kindBlock("danger",  s.error,           s.disabledBg, s.disabledText);
    sheet += kindBlock("warning", s.warning,         s.disabledBg, s.disabledText);
    sheet += kindBlock("info",    s.accentSecondary, s.disabledBg, s.disabledText);
    sheet += kindBlock("tertiary",s.accentTertiary,  s.disabledBg, s.disabledText);

    // Coloured text labels (kindBlock already emits QLabel[kind=...] rules for
    // primary/danger/warning/info; add the success text variant).
    sheet += QString("QLabel[kind=\"success\"] { color: %1; }").arg(s.success);

    return sheet;
}

QPalette appPalette(bool dark)
{
    const ColorScheme s = dark ? getDarkColorScheme() : getLightColorScheme();

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
