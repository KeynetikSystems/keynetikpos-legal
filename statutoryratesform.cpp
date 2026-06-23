// =============================================================================
// statutoryratesform.cpp — Implementation of StatutoryRatesForm (see header).
// =============================================================================
#include "statutoryratesform.h"
#include "cart.h"   // currencySymbol()

#include <QFormLayout>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QLabel>

namespace {
QDoubleSpinBox *moneySpin()
{
    auto *s = new QDoubleSpinBox();
    s->setRange(0.0, 99999999.99);
    s->setDecimals(2);
    s->setPrefix(currencySymbol() + " ");
    return s;
}
QDoubleSpinBox *pctSpin()
{
    auto *s = new QDoubleSpinBox();
    s->setRange(0, 100);
    s->setDecimals(3);
    s->setSuffix(" %");
    return s;
}
}

StatutoryRatesForm::StatutoryRatesForm(QWidget *parent)
    : QWidget(parent)
{
    auto *form = new QFormLayout(this);

    m_b1 = moneySpin(); m_b2 = moneySpin(); m_b3 = moneySpin(); m_b4 = moneySpin();
    m_relief = moneySpin();
    m_nssfR = pctSpin(); m_nssfCap = moneySpin();
    m_shifR = pctSpin(); m_shifMin = moneySpin();
    m_houseR = pctSpin();
    m_effDate = new QLineEdit(); m_effDate->setPlaceholderText("yyyy-MM-dd");
    m_srcNote = new QLineEdit();

    form->addRow("PAYE band 1 upper (10%):", m_b1);
    form->addRow("PAYE band 2 upper (25%):", m_b2);
    form->addRow("PAYE band 3 upper (30%):", m_b3);
    form->addRow("PAYE band 4 upper (32.5%):", m_b4);
    form->addRow("Personal relief / month:", m_relief);
    form->addRow("NSSF rate:", m_nssfR);
    form->addRow("NSSF cap (pensionable):", m_nssfCap);
    form->addRow("SHIF rate:", m_shifR);
    form->addRow("SHIF minimum:", m_shifMin);
    form->addRow("Housing levy rate:", m_houseR);
    form->addRow("Effective date:", m_effDate);
    form->addRow("Source note:", m_srcNote);

    auto *note = new QLabel("Verify against the current KRA / Finance Act figures, "
                            "and update the effective date when you change a rate. "
                            "See docs/STATUTORY_RATES.md.");
    note->setProperty("kind", "secondary");
    note->setWordWrap(true);
    form->addRow(note);
}

void StatutoryRatesForm::setRates(const PayrollRates &r)
{
    m_b1->setValue(r.payeBand1.toMajor());
    m_b2->setValue(r.payeBand2.toMajor());
    m_b3->setValue(r.payeBand3.toMajor());
    m_b4->setValue(r.payeBand4.toMajor());
    m_relief->setValue(r.personalRelief.toMajor());
    m_nssfR->setValue(r.nssfRate * 100.0);
    m_nssfCap->setValue(r.nssfCap.toMajor());
    m_shifR->setValue(r.shifRate * 100.0);
    m_shifMin->setValue(r.shifMin.toMajor());
    m_houseR->setValue(r.housingRate * 100.0);
    m_effDate->setText(r.effectiveDate);
    m_srcNote->setText(r.ratesNote);
}

PayrollRates StatutoryRatesForm::harvest() const
{
    PayrollRates r;
    r.payeBand1      = Money::fromMajor(m_b1->value());
    r.payeBand2      = Money::fromMajor(m_b2->value());
    r.payeBand3      = Money::fromMajor(m_b3->value());
    r.payeBand4      = Money::fromMajor(m_b4->value());
    r.personalRelief = Money::fromMajor(m_relief->value());
    r.nssfRate       = m_nssfR->value() / 100.0;
    r.nssfCap        = Money::fromMajor(m_nssfCap->value());
    r.shifRate       = m_shifR->value() / 100.0;
    r.shifMin        = Money::fromMajor(m_shifMin->value());
    r.housingRate    = m_houseR->value() / 100.0;
    r.effectiveDate  = m_effDate->text().trimmed();
    r.ratesNote      = m_srcNote->text().trimmed();
    return r;
}
