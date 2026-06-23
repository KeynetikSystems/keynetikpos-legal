// =============================================================================
// statutoryratesform.h — StatutoryRatesForm: the Finance Act rate controls
// -----------------------------------------------------------------------------
// WHAT: A reusable widget holding the editable statutory-payroll variables —
//       PAYE bands + rates, personal relief, NSSF, SHIF, Housing Levy, plus the
//       effective date and source note. No database knowledge: setRates()
//       populates it, harvest() reads it back into a PayrollRates.
// HOW:  A QFormLayout of money spin-boxes / percentage spin-boxes / line edits.
//       Money is shown in major units and converted at the boundary.
// WHY:  Defined once so the same form appears both in the standalone editor
//       (Payroll screen) and as a tab inside the Settings dialog — a compliance
//       form must have exactly one definition, not two that can drift.
// =============================================================================
#ifndef STATUTORYRATESFORM_H
#define STATUTORYRATESFORM_H

#include <QWidget>
#include "payroll.h"   // PayrollRates

class QDoubleSpinBox;
class QLineEdit;

class StatutoryRatesForm : public QWidget
{
    Q_OBJECT
public:
    explicit StatutoryRatesForm(QWidget *parent = nullptr);

    void         setRates(const PayrollRates &r);
    PayrollRates harvest() const;

private:
    QDoubleSpinBox *m_b1 = nullptr, *m_b2 = nullptr, *m_b3 = nullptr, *m_b4 = nullptr;
    QDoubleSpinBox *m_relief = nullptr;
    QDoubleSpinBox *m_nssfR = nullptr, *m_nssfCap = nullptr;
    QDoubleSpinBox *m_shifR = nullptr, *m_shifMin = nullptr;
    QDoubleSpinBox *m_houseR = nullptr;
    QLineEdit      *m_effDate = nullptr;
    QLineEdit      *m_srcNote = nullptr;
};

#endif // STATUTORYRATESFORM_H
