# -*- coding: utf-8 -*-
"""Renders the record of the 2026-07-19 working session (M-Pesa till review,
installer build, version bump) to a PDF. Shares the house style with the other
manuals via _pdf_common.

Run:  python docs/generate_session_log.py
Out:  docs/KeynetikPOS_Session_Log_2026-07-19.pdf
"""
import os
from reportlab.lib.units import mm
from reportlab.platypus import Paragraph, Spacer, PageBreak, HRFlowable

from _pdf_common import (
    Doc, cover, toc, bullets, steps, table, callout, code_block,
    H1, H2, H3, BODY, SMALL, LIGHT, BLUE, WARNBG, WARNBAR, ACCENT, LINE,
)

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                   "KeynetikPOS_Session_Log_2026-07-19.pdf")

S = []


def h1(t):
    S.append(Paragraph(t, H1))
    S.append(HRFlowable(width="100%", thickness=1, color=LINE,
                        spaceBefore=1, spaceAfter=7))


def h2(t):
    S.append(Paragraph(t, H2))


def h3(t):
    S.append(Paragraph(t, H3))


def p(t):
    S.append(Paragraph(t, BODY))


def sp(h=4):
    S.append(Spacer(1, h * mm))


# ───────────────────────────────────────────────────────── cover
S += cover(
    "Session Log",
    "Instructions issued and work performed",
    ["Working session of 19 July 2026",
     "Repository: KeynetikPOS &nbsp;|&nbsp; Branch: feature/erp-tier4",
     "Topics: M-Pesa till payments, installer packaging, release versioning"],
)

# ───────────────────────────────────────────────────────── TOC
h1("Contents")
S.append(toc([
    ("1", "Scope and how to read this document"),
    ("2", "Request 1 — Is M-Pesa till payment wired up?"),
    ("3", "Request 2 — Clarification: for POS sales"),
    ("4", "Request 3 — A test till number for Buy Goods"),
    ("5", "Request 4 — Cloudflare or Supabase for the payment flow"),
    ("6", "Request 5 — How to build the installer"),
    ("7", "Request 6 — Build the installer"),
    ("8", "Request 7 — Till-number fix and version bump"),
    ("9", "Consolidated command reference"),
    ("10", "Changes made to the repository"),
    ("11", "Outstanding items"),
]))
S.append(PageBreak())

# ───────────────────────────────────────────────────────── 1
h1("1 &nbsp; Scope and how to read this document")
p("This document records a single working session held on 19 July 2026 against "
  "the <b>feature/erp-tier4</b> branch of KeynetikPOS. It captures, in order, "
  "each instruction that was issued, the investigation carried out in response, "
  "the conclusion reached, and any commands run or files changed.")
p("It is a record of <b>what was asked and what was done</b> — not a "
  "specification and not a substitute for the technical documentation. Where a "
  "conclusion rests on something that was verified by reading code, the "
  "relevant file and line is cited. Where a conclusion rests on external "
  "behaviour that was <i>not</i> verified in this session — principally the "
  "behaviour of Safaricom's Daraja API — that is stated explicitly rather than "
  "presented as established fact.")
sp(2)
S.append(callout(
    "Verification status",
    "Every claim about this codebase in this document was checked against the "
    "source during the session. Claims about Daraja's Paybill/Buy Goods "
    "semantics come from API knowledge and were <b>not</b> tested against a "
    "live or sandbox Daraja endpoint. They are flagged in section 4 and "
    "section 11.", WARNBG, WARNBAR))
sp(3)

h2("Session at a glance")
S.append(table([
    ["Item", "Outcome"],
    ["Requests handled", "7"],
    ["Source files changed", "3 (index.js, README.md, CMakeLists.txt)"],
    ["Installers produced", "2 (v2.0.0, then v2.1.0)"],
    ["Test suites run", "7 of 7 passing, twice"],
    ["Commits made", "None — all changes left in the working tree"],
    ["Deployments made", "None — the Worker change is not live"],
], center_cols=()))
sp(3)
S.append(callout(
    "Nothing was committed or deployed",
    "The session ended with all changes uncommitted in the working tree and "
    "the Cloudflare Worker still running the previous code. Both were "
    "deliberate — see section 11.", LIGHT, BLUE))

S.append(PageBreak())

# ───────────────────────────────────────────────────────── 2
h1("2 &nbsp; Request 1 — Is M-Pesa till payment wired up?")
p("<i>Instruction: &ldquo;was this wired up to work with mpesa till payments?&rdquo;</i>")
sp(1)

h2("2.1 &nbsp; Investigation")
p("A repository-wide search for M-Pesa, till, paybill, Daraja and STK-push "
  "terms returned 48 files. The three that carry the implementation were read "
  "in full.")
S.append(bullets([
    "<b>mpesaclient.h / mpesaclient.cpp</b> — the desktop client.",
    "<b>license-server/src/index.js</b> — the Cloudflare Worker endpoints.",
    "<b>paymentdialog.cpp</b> — the cashier-facing wiring.",
]))
sp(2)

h2("2.2 &nbsp; What was found")
p("The feature is implemented end to end. The architecture routes every push "
  "through the licence Worker rather than talking to Daraja from the till.")
S.append(table([
    ["Layer", "Implementation"],
    ["Client",
     "<b>MpesaClient::requestPayment()</b> POSTs to "
     "<b>/pos/mpesa/stkpush</b>, authenticated with the till's licence key and "
     "device id, then polls <b>/pos/mpesa/status</b> every 3&nbsp;s for up to "
     "25 attempts (75&nbsp;s). Results surface as the signals "
     "<b>promptSent</b>, <b>paymentConfirmed(receipt)</b> and "
     "<b>paymentFailed(reason)</b>."],
    ["Worker",
     "Initiates the Daraja STK push, receives Safaricom's callback at "
     "<b>/pos/mpesa/callback</b>, and serves the outcome from the "
     "<b>stk_requests</b> table in D1. Daraja credentials are Worker secrets "
     "and never reach the shop PC."],
    ["Cashier UI",
     "A Mobile Money payment line with a <i>Send STK Prompt</i> button; on "
     "confirmation the receipt code is written into the reference field, "
     "validated against <b>^[A-Z0-9]{10}$</b>. Manual entry remains available "
     "when the till is unlicensed or the push times out."],
]))
sp(2)
p("The reason for the Worker sitting in the middle is documented at "
  "mpesaclient.h:13 — a desktop till behind NAT cannot receive Safaricom's "
  "callback, so a public endpoint is required as the rendezvous point.")
sp(2)

h2("2.3 &nbsp; The defect identified")
S.append(callout(
    "Buy Goods (till) sends the wrong PartyB",
    "At the time of review, license-server/src/index.js sent "
    "<b>BusinessShortCode</b> and <b>PartyB</b> as the same value "
    "(<b>MPESA_SHORTCODE</b>). That is correct for Paybill. For Buy Goods, "
    "Daraja expects <b>BusinessShortCode</b> to be the Head Office / store "
    "number — which is also what the <b>Password</b> hash is built from — and "
    "<b>PartyB</b> to be the till number the customer keys in. With a single "
    "value serving both, a genuine Buy Goods configuration would fail. "
    "Fixed later in the session; see section 8.", WARNBG, WARNBAR))
sp(2)
p("A secondary observation: the C2B handler at <b>/webhook/mpesa</b> is for "
  "licence purchases and assigns licence keys. It is unrelated to POS sales, "
  "which travel exclusively through the STK path.")

S.append(PageBreak())

# ───────────────────────────────────────────────────────── 3
h1("3 &nbsp; Request 2 — Clarification: for POS sales")
p("<i>Instruction: &ldquo;i meant for POS sales&rdquo;</i>")
sp(1)
p("The clarification did not change the answer: the STK path <b>is</b> the POS "
  "sale path. The sale-side persistence was then traced to confirm the loop "
  "closes.")
sp(2)

h2("3.1 &nbsp; How a confirmed payment reaches the sale")
S.append(steps([
    "<b>getTenders()</b> (paymentdialog.cpp:447) appends "
    "<b>{ \"Mobile Money\", amount, reference }</b>, where the reference is the "
    "M-Pesa receipt code written into the field by the "
    "<b>paymentConfirmed</b> handler.",
    "<b>getPaymentMethod()</b> reports <i>Mobile Money</i>, or a joined string "
    "such as <i>Cash + Mobile Money</i> for split tenders.",
    "<b>getReferenceNumber()</b> prefixes the code with <i>M-Pesa</i> for "
    "printing on the receipt.",
    "Confirm stays disabled until a valid ten-character code is present, so a "
    "sale cannot close against an unconfirmed push.",
]))
sp(2)
S.append(callout(
    "Conclusion",
    "Cashier tenders &rarr; prompt to the customer's phone &rarr; code returns "
    "&rarr; sale records it. The loop is complete and manual fallback works.",
    LIGHT, ACCENT))

sp(4)

# ───────────────────────────────────────────────────────── 4
h1("4 &nbsp; Request 3 — A test till number for Buy Goods")
p("<i>Instruction: &ldquo;what fake till can i use to test buy goods till "
  "function on this POS?&rdquo;</i>")
sp(1)

h2("4.1 &nbsp; Guidance given")
p("No till number was invented. Sandbox shortcodes are issued per Daraja "
  "account, so a fabricated number would be rejected at Daraja rather than "
  "exercising the POS code path — the failure would teach nothing.")
S.append(bullets([
    "<b>Where the real values live:</b> the Daraja portal, under the "
    "application's <i>Test Credentials</i> / <i>Simulator</i> tab. These are "
    "tied to the account and are not transferable.",
    "<b>Universally available in sandbox:</b> the M-Pesa Express pair — "
    "shortcode <b>174379</b> with the public sandbox passkey, and the test "
    "MSISDN <b>254708374149</b>. Sandbox STK is only really wired for that "
    "shortcode.",
]))
sp(2)

h2("4.2 &nbsp; The limitation that matters")
S.append(callout(
    "Sandbox cannot validate a Buy Goods pairing",
    "Sandbox does not meaningfully enforce the relationship between the Head "
    "Office number and the till number. A buygoods configuration will return "
    "<b>ResponseCode: 0</b> and still be wrong in production. Sandbox "
    "therefore proves the request shape, callback handling, polling and "
    "tender recording — but <b>not</b> the till pairing itself, which can only "
    "be confirmed by a small live transaction at go-live.",
    WARNBG, WARNBAR))
sp(2)
S.append(callout(
    "Not verified in this session",
    "The Paybill-versus-Buy-Goods field semantics described here and in "
    "section 8 were not tested against a live or sandbox Daraja endpoint. They "
    "should be confirmed against current Daraja documentation before "
    "go-live.", WARNBG, WARNBAR))

S.append(PageBreak())

# ───────────────────────────────────────────────────────── 5
h1("5 &nbsp; Request 4 — Cloudflare or Supabase for the payment flow")
p("<i>Instruction: a diagram of the STK push flow, with &ldquo;how would we "
  "implement these for POS sales using cloudflare or should we go the supabase "
  "way&rdquo;</i>")
sp(1)

h2("5.1 &nbsp; Finding: the design was already built")
p("The supplied diagram described the existing implementation rather than "
  "missing work. Each element maps onto shipped code:")
S.append(table([
    ["Diagram element", "Existing code"],
    ["Qt POS &rarr; Backend &rarr; M-Pesa &rarr; phone",
     "requestPayment() &rarr; /pos/mpesa/stkpush &rarr; Daraja "
     "(mpesaclient.cpp:84)"],
    ["M-Pesa &rarr; Backend (callback)",
     "/pos/mpesa/callback writes status and receipt (index.js:423)"],
    ["Qt polls /status",
     "poll() &rarr; /pos/mpesa/status (mpesaclient.cpp:136)"],
    ["Every 2–5 seconds",
     "kPollMs = 3000, 25 polls, 75&nbsp;s ceiling"],
    ["Backend updates status",
     "stk_requests table in D1 (schema.sql:38)"],
    ["POS knows &rarr; receipt prints",
     "paymentConfirmed &rarr; tender plus receipt code"],
]))
sp(2)
p("The diagram's central point — that M-Pesa replies to the backend and not to "
  "the Qt application — is the documented rationale for the Worker's existence "
  "at mpesaclient.h:13.")
sp(2)

h2("5.2 &nbsp; Recommendation: remain on Cloudflare")
p("Migrating to Supabase was advised against, for reasons beyond sunk cost:")
S.append(bullets([
    "<b>Authentication already exists.</b> The Worker is the licence server, "
    "so <b>posAuthorised()</b> reuses the same licence-key and device-id pair "
    "as <b>/validate</b>. M-Pesa authentication came for free; a migration "
    "would mean reimplementing it against a different identity store.",
    "<b>Polling is the correct choice here</b>, not a limitation. The worst "
    "case is three seconds of latency on a transaction where the customer is "
    "physically present entering a PIN, which takes 15–30&nbsp;s. A WebSocket "
    "buys nothing a cashier can perceive while adding a connection that breaks "
    "on unreliable shop connectivity. Polling is stateless and self-healing.",
    "<b>Workers is the better webhook receiver</b> — no cold start, and "
    "Safaricom's retry behaviour is unforgiving of latency.",
    "<b>Realtime is not exclusive to Supabase.</b> Durable Objects plus "
    "WebSocket or SSE provide the same capability if it is ever wanted.",
]))
sp(2)
S.append(callout(
    "Gap noted in passing — unreconciled payments",
    "If a callback arrives while the till is offline or closed, the sale is "
    "lost from the cashier's view even though the customer was charged. The "
    "receipt exists in <b>stk_requests</b>, but nothing reconciles it. A "
    "&ldquo;look up an unclaimed M-Pesa payment by phone and amount&rdquo; "
    "screen would close this. Recommended before go-live; not built in this "
    "session.", WARNBG, WARNBAR))

S.append(PageBreak())

# ───────────────────────────────────────────────────────── 6
h1("6 &nbsp; Request 5 — How to build the installer")
p("<i>Instruction: &ldquo;how do i buil installer for this?&rdquo;</i>")
sp(1)

h2("6.1 &nbsp; Finding: packaging was already configured")
p("CPack with the NSIS generator was already wired into CMakeLists.txt, and a "
  "prior installer existed in the release build directory. All prerequisites "
  "were present: NSIS at <b>C:\\Program Files (x86)\\NSIS</b>, "
  "<b>LICENSE.txt</b>, and <b>resources/app.ico</b>.")
sp(2)
S.append(code_block([
    "$env:PATH = \"C:\\Qt\\Tools\\mingw1310_64\\bin;C:\\Qt\\6.11.0\\mingw_64\\bin;\" + $env:PATH",
    "cmake --build \"build\\Desktop_Qt_6_11_0_MinGW_64_bit-Release\"",
    "cpack --config \"build\\Desktop_Qt_6_11_0_MinGW_64_bit-Release\\CPackConfig.cmake\" \\",
    "      -B \"build\\Desktop_Qt_6_11_0_MinGW_64_bit-Release\"",
]))
sp(2)

h2("6.2 &nbsp; How the packaging works")
p("A POST_BUILD step (CMakeLists.txt:164) runs <b>windeployqt</b> into "
  "<b>windeployqt_stage/</b>, collecting the Qt DLLs, platform plugins and SQL "
  "drivers beside a copy of the executable. <b>install()</b> ships that "
  "directory and CPack's NSIS generator wraps it, adding Start Menu and "
  "desktop shortcuts, matching uninstaller entries, the application icon and a "
  "run-on-finish option.")
sp(2)

h2("6.3 &nbsp; Two problems flagged")
S.append(table([
    ["Problem", "Detail"],
    ["build-release.bat is stale",
     "It configures a different build directory, runs windeployqt in place "
     "rather than into the staging directory, and stops before cpack — so it "
     "never produces an installer. It also deletes the build tree first. "
     "Recommended: fix it to call cpack against the Qt&nbsp;Creator directory, "
     "or delete it."],
    ["Version was hardcoded",
     "CPACK_PACKAGE_FILE_NAME derives from APP_VERSION, so shipping without "
     "bumping it silently overwrites the previous installer with an "
     "identically named file."],
]))

sp(4)

# ───────────────────────────────────────────────────────── 7
h1("7 &nbsp; Request 6 — Build the installer")
p("<i>Instruction: &ldquo;yup&rdquo;</i>")
sp(1)
p("This reply was ambiguous between two offers — building the installer, and "
  "committing the branch work first. It was read as the former, and the commit "
  "was deliberately left alone as the more consequential and less reversible "
  "of the two.")
sp(2)
S.append(steps([
    "Build ran and compiled nothing — the tree was already current.",
    "All seven ctest suites passed: carttotals, database, ledger, payroll, "
    "vat, salejournal, checkoutservice.",
    "CPack produced <b>KeynetikPOS-2.0.0-Setup.exe</b>, 28.2&nbsp;MB.",
]))
sp(2)
S.append(callout(
    "Two caveats recorded at the time",
    "The build overwrote the previous 2.0.0 installer, since the filename was "
    "unchanged. And the installer contained the then-unfixed M-Pesa till "
    "defect — acceptable for Paybill or sandbox use, not for a Buy Goods "
    "merchant.", WARNBG, WARNBAR))

S.append(PageBreak())

# ───────────────────────────────────────────────────────── 8
h1("8 &nbsp; Request 7 — Till-number fix and version bump")
p("<i>Instruction: &ldquo;both&rdquo;</i>")
sp(1)

h2("8.1 &nbsp; The till-number split")
p("<b>license-server/src/index.js:395</b> — <b>PartyB</b> now resolves "
  "independently of <b>BusinessShortCode</b>:")
S.append(code_block([
    'const buyGoods = env.MPESA_TXN_TYPE === "buygoods";',
    'const txnType  = buyGoods ? "CustomerBuyGoodsOnline" : "CustomerPayBillOnline";',
    'const partyB   = (buyGoods && env.MPESA_TILL_NUMBER)',
    '                   ? String(env.MPESA_TILL_NUMBER) : env.MPESA_SHORTCODE;',
]))
sp(2)
p("<b>BusinessShortCode</b> and the <b>Password</b> hash remain on "
  "<b>MPESA_SHORTCODE</b>. The fallback means an unset "
  "<b>MPESA_TILL_NUMBER</b> reproduces the previous request byte for byte, so "
  "Paybill behaviour is unchanged.")
sp(2)
S.append(table([
    ["Transaction type", "BusinessShortCode", "PartyB"],
    ["Paybill", "Paybill number", "The same number"],
    ["Buy Goods", "Head Office / store number", "The <b>till</b> number"],
]))
sp(2)
p("The same table was added to <b>license-server/README.md:148</b> along with "
  "the sandbox warning, and the environment-variable header in index.js was "
  "expanded to document <b>MPESA_TILL_NUMBER</b>. "
  "<b>node --check</b> passed on the modified Worker source.")
sp(3)

h2("8.2 &nbsp; The version bump")
p("<b>2.0.0 &rarr; 2.1.0</b> in both <b>project()</b> (CMakeLists.txt:2) and "
  "<b>APP_VERSION</b> (CMakeLists.txt:21). Changing the <b>project()</b> "
  "version forces a full CMake reconfigure and recompile, so this rebuild was "
  "not incremental.")
sp(2)

h2("8.3 &nbsp; Result")
S.append(table([
    ["Artefact", "Size", "Built"],
    ["KeynetikPOS-2.0.0-Setup.exe", "28.2&nbsp;MB", "19:05"],
    ["KeynetikPOS-2.1.0-Setup.exe", "28.2&nbsp;MB", "19:57"],
]))
sp(2)
p("Both installers now coexist — the version bump achieved its purpose. The "
  "full rebuild completed cleanly and all seven test suites passed against it.")

S.append(PageBreak())

# ───────────────────────────────────────────────────────── 9
h1("9 &nbsp; Consolidated command reference")
p("Every command that materially advanced the work, in the order run. All were "
  "executed from the repository root in PowerShell.")
sp(2)

h3("Build the release tree")
S.append(code_block([
    "$env:PATH = \"C:\\Qt\\Tools\\mingw1310_64\\bin;C:\\Qt\\6.11.0\\mingw_64\\bin;\" + $env:PATH",
    "cmake --build \"build\\Desktop_Qt_6_11_0_MinGW_64_bit-Release\"",
]))
sp(2)

h3("Run the test suite")
S.append(code_block([
    "ctest --test-dir \"build\\Desktop_Qt_6_11_0_MinGW_64_bit-Release\" --output-on-failure",
]))
sp(2)

h3("Produce the installer")
S.append(code_block([
    "$env:PATH = \"C:\\Program Files (x86)\\NSIS;\" + $env:PATH",
    "cpack --config \"build\\Desktop_Qt_6_11_0_MinGW_64_bit-Release\\CPackConfig.cmake\" \\",
    "      -B \"build\\Desktop_Qt_6_11_0_MinGW_64_bit-Release\"",
]))
sp(2)

h3("Syntax-check the Worker")
S.append(code_block(["node --check \"license-server\\src\\index.js\""]))
sp(2)

h3("Deploy the Worker — NOT run in this session")
S.append(code_block([
    "cd license-server",
    "wrangler deploy --var MPESA_TILL_NUMBER:<till>",
    "# or set MPESA_TILL_NUMBER in wrangler.toml [vars], then: wrangler deploy",
]))
sp(2)
S.append(callout(
    "The deploy command above was not executed",
    "It is recorded for completeness. The live Worker still runs the previous "
    "code, and deployment was deliberately deferred until a real till number "
    "is available.", WARNBG, WARNBAR))

S.append(PageBreak())

# ───────────────────────────────────────────────────────── 10
h1("10 &nbsp; Changes made to the repository")
p("Three files were modified. None were committed.")
sp(2)
S.append(table([
    ["File", "Change"],
    ["license-server/src/index.js",
     "PartyB reads MPESA_TILL_NUMBER for buygoods, falling back to "
     "MPESA_SHORTCODE. BusinessShortCode and Password unchanged. "
     "Environment-variable header documents the new variable and the "
     "HO-versus-till distinction."],
    ["license-server/README.md",
     "Added the Paybill / Buy Goods field table, the MPESA_TILL_NUMBER setup "
     "line, and the warning that sandbox does not validate the pairing."],
    ["CMakeLists.txt",
     "APP_VERSION and project() version raised from 2.0.0 to 2.1.0."],
]))
sp(3)
S.append(callout(
    "Pre-existing uncommitted work",
    "The branch already carried roughly twenty modified files and one "
    "untracked test file (tests/checkoutservice_test.cpp) before this session "
    "began. Those changes are unrelated to this work but are included in the "
    "2.1.0 installer, since it was built from the working tree. Splitting them "
    "into separate commits was offered and not taken up.",
    WARNBG, WARNBAR))

sp(4)

# ───────────────────────────────────────────────────────── 11
h1("11 &nbsp; Outstanding items")
p("In the order they matter.")
sp(2)
S.append(table([
    ["#", "Item", "Detail"],
    ["1", "Worker not deployed",
     "Live traffic still reaches the previous code. Requires wrangler deploy "
     "with MPESA_TILL_NUMBER set — worth doing only once the real till number "
     "is in hand."],
    ["2", "Nothing committed",
     "No commit reproduces the 2.1.0 binary. The branch holds this session's "
     "three files plus the pre-existing changes."],
    ["3", "Buy Goods pairing untested",
     "Sandbox returns success regardless of the pairing, so only a small live "
     "transaction confirms it."],
    ["4", "Clean-machine install test",
     "windeployqt occasionally misses a plugin that surfaces only at runtime "
     "on a machine without Qt installed; the SQL driver is the usual suspect."],
    ["5", "Unclaimed-payment reconciliation",
     "No way to recover a payment whose callback landed while the till was "
     "offline. Recommended before go-live (section 5.2)."],
    ["6", "build-release.bat is misleading",
     "Does not produce an installer and targets the wrong build directory. "
     "Fix or delete (section 6.3)."],
], col_widths=None))
sp(3)
S.append(Paragraph(
    "Record of the working session of 19 July 2026. Generated from the session "
    "transcript by docs/generate_session_log.py.", SMALL))


def main():
    doc = Doc(OUT, "Session Log — 19 July 2026",
              "KeynetikPOS — session record")
    doc.build(S)
    print("Wrote %s" % OUT)


if __name__ == "__main__":
    main()
