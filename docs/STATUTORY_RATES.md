# KeynetikPOS — Kenyan Statutory Payroll Rates

**Rates effective: 2024-10-01**
**Source note:** Finance Act 2023 PAYE bands; NSSF enhanced (2024); SHIF 2.75% (Oct 2024); Housing Levy 1.5%

This is the authoritative, dated record of the statutory figures KeynetikPOS uses
to compute payslips (`computePayslip()` in `payroll.cpp`). Payroll is a
compliance feature — being wrong on a rate is worse than not having the feature,
so this file, the in-code defaults (`PayrollRates` in `payroll.h`), and the
in-app **Payroll → Statutory Rates…** editor must always agree.

> **Verify before relying on these.** Statutory rates and bands change with each
> Finance Act and gazette notice. Confirm against current KRA / RBA / SHA
> guidance for the period you are running, and update the effective date.

## Current figures (monthly)

| Item | Value | Basis | Source / effective |
|---|---|---|---|
| PAYE band 1 | 10% up to KSh 24,000 | Taxable pay after NSSF/SHIF/Housing | Finance Act 2023 (from 2023-07-01) |
| PAYE band 2 | 25% on 24,001 – 32,333 | " | " |
| PAYE band 3 | 30% on 32,334 – 500,000 | " | " |
| PAYE band 4 | 32.5% on 500,001 – 800,000 | " | " |
| PAYE top | 35% above 800,000 | " | " |
| Personal relief | KSh 2,400 / month | Credited against PAYE | KRA |
| NSSF | 6% of pensionable pay, capped at KSh 36,000 (max KSh 2,160) | Tier I + II | NSSF Act 2013, enhanced rates 2024 |
| SHIF | 2.75% of gross, minimum KSh 300 | Replaced NHIF | SHA, from 2024-10-01 |
| Housing Levy | 1.5% of gross | Employee share | Affordable Housing Act 2024 |

**Order of operations** (matches `computePayslip`): NSSF, SHIF and Housing Levy
are deducted from gross to get *taxable pay*; progressive PAYE is computed on the
taxable pay; personal relief is then subtracted from the PAYE due. Net pay =
gross − NSSF − SHIF − Housing − PAYE. All money is integer cents.

## How to update the rates (the checklist)

When a Finance Act / gazette notice changes a figure:

1. **Update this file** — change the value(s), the **Rates effective** date at the
   top, and the table's source/effective column. Cite the gazette/Act.
2. **Update the defaults** in `payroll.h` (`PayrollRates`), including
   `effectiveDate` and `ratesNote`, so fresh installs ship correct.
3. **Bump the tests** in `tests/payroll_test.cpp` if a worked example changes
   (e.g. the 50,000 gross breakdown) and run `ctest -R payroll_test`.
4. **In the field**, existing installs can also be corrected without a rebuild via
   **Payroll → Statutory Rates…** — which now stamps the effective date the user
   enters, shown back on the Payroll screen and persisted in `payroll_config`.

The effective date is surfaced in the app (Payroll screen caption) so a user can
see at a glance how current their figures are — and so support can confirm which
rate set a given install is running.
