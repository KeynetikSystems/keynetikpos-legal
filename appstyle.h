// =============================================================================
// appstyle.h — application-wide theme stylesheets and palette
// -----------------------------------------------------------------------------
// WHAT: appStylesheet() returns the full light/dark Qt stylesheet, built from
//       the ColorScheme in colorscheme.h (the single palette); appPalette()
//       returns the matching QPalette so native widgets follow.
// HOW:  The stylesheet is applied APPLICATION-WIDE (qApp->setStyleSheet) so
//       every dialog — including parentless ones like LoginDialog — is themed.
//       Widgets opt into semantic styling with dynamic properties instead of
//       inline setStyleSheet calls:
//         button->setProperty("kind", "primary|danger|warning|info|tertiary")
//         label ->setProperty("kind", "success|danger|warning|info|tertiary|
//                                      secondary")                   (text colour)
//         label ->setProperty("role", "amount|amountTotal|amountDiscount|
//                                      totalTitle|banner|chip|dialogTitle|
//                                      sectionTitle|statValue|statValueLg")
//       "banner" combines with "kind" for a padded, bordered status banner;
//       "chip" combines with "kind" for a solid-fill label with white text;
//       "statValue"/"statValueLg" combine with "kind" for coloured metric
//       values on summary cards. QGroupBox accepts kind="danger|warning" for
//       alert sections, and QFrame role="hline" themes separator lines.
// WHY:  One palette + property selectors replaces the per-widget hard-coded
//       hex styles that drifted into four different greens and broke in dark
//       mode. setStyleProperty() exists because Qt only re-evaluates property
//       selectors after an unpolish/polish cycle.
// =============================================================================
#ifndef APPSTYLE_H
#define APPSTYLE_H

#include <QString>
#include <QPalette>
#include <QWidget>
#include <QStyle>
#include <QVariant>

QString  appStylesheet(bool dark);
QPalette appPalette(bool dark);

// Change a style-driving dynamic property at runtime and force the stylesheet
// to re-evaluate. Needed whenever "kind"/"role" changes after the widget is
// shown (e.g. the payment dialog's change label flipping success -> danger).
inline void setStyleProperty(QWidget *w, const char *name, const QVariant &value)
{
    if (!w || w->property(name) == value)
        return;
    w->setProperty(name, value);
    w->style()->unpolish(w);
    w->style()->polish(w);
}

#endif // APPSTYLE_H
