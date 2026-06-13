#pragma once

// =============================================================================
// shift_dialogs.h — OpenShiftDialog / CloseShiftDialog
// -----------------------------------------------------------------------------
// WHAT: The two small modal forms bracketing a shift: open (cashier name +
//       counted opening float) and close (shift summary display + counted
//       closing float + notes).
// HOW:  Thin QDialogs exposing values through getters; the caller passes them
//       to ShiftManager. CloseShiftDialog receives the current ShiftRecord and
//       currency symbol so the cashier sees expected totals while counting
//       the drawer.
// WHY:  Forcing an explicit counted float entry at both ends (rather than
//       defaulting) is what makes the over/short calculation meaningful.
// =============================================================================

#include "shift_type.h"
#include <QDialog>

class QLineEdit;
class QDoubleSpinBox;
class QTextEdit;

// =============================================================================
// OpenShiftDialog — Dialog to start a new shift
// =============================================================================

class OpenShiftDialog : public QDialog
{
    Q_OBJECT

public:
    explicit OpenShiftDialog(QWidget *parent = nullptr);

    QString cashierName()  const;
    double  openingFloat() const;

private:
    QLineEdit      *m_cashierName  = nullptr;
    QDoubleSpinBox *m_openingFloat = nullptr;
};

// =============================================================================
// CloseShiftDialog — Dialog to close an active shift
// =============================================================================

class CloseShiftDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CloseShiftDialog(const ShiftRecord &shift,
                              const QString &currencySymbol,
                              QWidget *parent = nullptr);

    double  closingFloat() const;
    QString notes()        const;

private:
    QDoubleSpinBox *m_closingFloat = nullptr;
    QTextEdit      *m_notes        = nullptr;
};
