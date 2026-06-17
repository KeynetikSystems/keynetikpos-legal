// =============================================================================
// onboardingwizard.h — OnboardingWizard: first-launch setup flow
// -----------------------------------------------------------------------------
// WHAT: A QWizard shown on first launch. Walks the user through: naming their
//       business, configuring tax, and confirming they're ready to start.
//       On Finish, writes the collected settings via SettingsManager.
// HOW:  Three QWizardPages. Page 1 uses registerField() so QWizard enforces
//       the business name before advancing. Tax fields are only shown/required
//       when the "Enable Tax" checkbox is ticked. DonePage echoes back the
//       name from the registered field.
// WHY:  A wizard gives a structured first-run experience without overwhelming
//       new users with the full settings dialog. SettingsManager is injected
//       rather than accessed as a singleton so the wizard can be unit-tested
//       or reused in demos.
// =============================================================================
#ifndef ONBOARDINGWIZARD_H
#define ONBOARDINGWIZARD_H

#include <QWizard>
#include <QWizardPage>
#include <QLineEdit>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QVBoxLayout>
#include <QFormLayout>

#include "settingsmanager.h"

// ── Page 1: Business identity ─────────────────────────────────────────────────
class WelcomePage : public QWizardPage
{
    Q_OBJECT
public:
    explicit WelcomePage(QWidget *parent = nullptr);

private:
    QLineEdit *m_businessNameEdit { nullptr };
};

// ── Page 2: Tax configuration ─────────────────────────────────────────────────
class TaxPage : public QWizardPage
{
    Q_OBJECT
public:
    explicit TaxPage(QWidget *parent = nullptr);

private slots:
    void onTaxEnabledChanged(bool enabled);

private:
    QCheckBox      *m_taxEnabledCheck  { nullptr };
    QDoubleSpinBox *m_taxRateSpin      { nullptr };
    QLineEdit      *m_taxLabelEdit     { nullptr };
    QCheckBox      *m_taxInclusiveCheck{ nullptr };
};

// ── Page 3: Confirmation ──────────────────────────────────────────────────────
class DonePage : public QWizardPage
{
    Q_OBJECT
public:
    explicit DonePage(QWidget *parent = nullptr);
    void initializePage() override;   // update label with final business name

private:
    QLabel *m_summaryLabel { nullptr };
};

// ── Wizard ────────────────────────────────────────────────────────────────────
class OnboardingWizard : public QWizard
{
    Q_OBJECT

public:
    explicit OnboardingWizard(SettingsManager *settings, QWidget *parent = nullptr);

private slots:
    void onFinished(int result);

private:
    SettingsManager *m_settings { nullptr };
};

#endif // ONBOARDINGWIZARD_H
