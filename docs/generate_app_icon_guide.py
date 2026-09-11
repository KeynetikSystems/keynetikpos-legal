# -*- coding: utf-8 -*-
"""Renders the "Setting the application icon in a Qt project" guide to PDF —
general method plus the specific repair KeynetikPOS needs. Shares the house
style with the other manuals via _pdf_common.

State of the repository described in section 6 was verified on 19 July 2026.

Run:  python docs/generate_app_icon_guide.py
Out:  docs/KeynetikPOS_App_Icon_Guide.pdf
"""
import os
from reportlab.lib.units import mm
from reportlab.platypus import Paragraph, Spacer, PageBreak, HRFlowable

from _pdf_common import (
    Doc, cover, toc, bullets, steps, table, callout, code_block,
    H1, H2, H3, BODY, SMALL, LIGHT, BLUE, WARNBG, WARNBAR, ACCENT, LINE,
)

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                   "KeynetikPOS_App_Icon_Guide.pdf")

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
    "Setting the Application Icon",
    "A practical guide for Qt&nbsp;+&nbsp;CMake on Windows",
    ["Applies to Qt 6.11 / CMake / MinGW",
     "Includes the specific repair required by KeynetikPOS",
     "Verified against the repository on 19 July 2026"],
)

# ───────────────────────────────────────────────────────── TOC
h1("Contents")
S.append(toc([
    ("1", "Four different icons — know which one you mean"),
    ("2", "Preparing the .ico file"),
    ("3", "Method A — CMake with a Windows resource script"),
    ("4", "Method B — qmake, for reference"),
    ("5", "The runtime window icon"),
    ("6", "The current state of KeynetikPOS"),
    ("7", "The repair, step by step"),
    ("8", "Verifying it worked"),
    ("9", "Troubleshooting"),
]))
S.append(PageBreak())

# ───────────────────────────────────────────────────────── 1
h1("1 &nbsp; Four different icons — know which one you mean")
p("Most confusion about Qt application icons comes from treating a single "
  "problem as four. They are set in different places, and setting one does "
  "nothing for the others.")
sp(2)
S.append(table([
    ["Icon", "Where it appears", "How it is set"],
    ["Executable icon",
     "The .exe in Explorer, the taskbar, Alt+Tab, pinned shortcuts",
     "Embedded into the binary by a Windows resource script (.rc) compiled "
     "into the target"],
    ["Window icon",
     "The title bar and, on some systems, the taskbar button",
     "<b>QApplication::setWindowIcon()</b> at runtime, from a Qt resource"],
    ["Installer icon",
     "The Setup .exe itself and the wizard's own chrome",
     "<b>CPACK_NSIS_MUI_ICON</b> / <b>MUI_UNIICON</b> in CMakeLists"],
    ["Shortcut icon",
     "Start Menu and desktop shortcuts",
     "Inherited from the executable icon — nothing separate to set"],
]))
sp(3)
S.append(callout(
    "The common trap",
    "Setting the installer icon makes the <i>Setup</i> file look right while "
    "the installed application still shows a blank default. They are different "
    "icons. A correct installer icon is not evidence that the executable icon "
    "is working — which is precisely the situation this project is in "
    "(section 6).", WARNBG, WARNBAR))
sp(3)

h2("1.1 &nbsp; What Qt does for you on Windows")
p("On Windows, Qt's platform plugin falls back to the <b>executable's</b> "
  "embedded icon when no window icon has been set explicitly. This is why "
  "embedding an icon in the .rc often appears to fix the title bar and taskbar "
  "at the same time. It is still worth calling "
  "<b>setWindowIcon()</b> explicitly — it is predictable, it works "
  "cross-platform, and it covers dialogs that are created before the main "
  "window.")

sp(4)

# ───────────────────────────────────────────────────────── 2
h1("2 &nbsp; Preparing the .ico file")
p("A Windows <b>.ico</b> is a container holding several bitmaps at different "
  "sizes. Windows picks the closest match for the context it is drawing.")
sp(2)

h2("2.1 &nbsp; Sizes to include")
S.append(table([
    ["Size", "Used for"],
    ["16 × 16", "Title bar, small Explorer list view"],
    ["32 × 32", "Desktop shortcuts, Alt+Tab"],
    ["48 × 48", "Explorer medium icons — the common default view"],
    ["256 × 256", "Large and extra-large thumbnails, the Properties dialog"],
]))
sp(2)
S.append(callout(
    "A single-size .ico looks wrong everywhere except one size",
    "If the file contains only a 256×256 image, Windows downscales it to "
    "16×16 for the title bar with no hinting, and the result is a blurred "
    "smudge. If it contains only 48×48, the large thumbnail is upscaled and "
    "soft. Always ship at least the four sizes above in one file.",
    WARNBG, WARNBAR))
sp(3)

h2("2.2 &nbsp; Producing a multi-size icon")
p("From a square source image of 1024&nbsp;×&nbsp;1024 or larger, with "
  "ImageMagick:")
S.append(code_block([
    "magick logo.png -define icon:auto-resize=256,48,32,16 resources/app.ico",
]))
sp(2)
p("Any icon editor will do the same. What matters is that the four sizes are "
  "present in a single file. Verify the count before shipping:")
S.append(code_block([
    "# Byte 4-5 of an .ico header is the image count",
    "$b = [byte[]]::new(6)",
    "$fs = [IO.File]::OpenRead(\"resources\\app.ico\")",
    "$null = $fs.Read($b, 0, 6); $fs.Close()",
    "[BitConverter]::ToUInt16($b, 4)      # expect 4, not 1",
]))
sp(2)
S.append(callout(
    "Design note",
    "A 16×16 rendering of a detailed logo is unreadable. Most well-made icon "
    "sets use a simplified mark at the small sizes and the full logo at 256. "
    "The .ico format supports exactly this — the bitmaps need not be "
    "mechanical downscales of one another.", LIGHT, BLUE))

S.append(PageBreak())

# ───────────────────────────────────────────────────────── 3
h1("3 &nbsp; Method A — CMake with a Windows resource script")
p("This is the correct approach for a CMake project and the one KeynetikPOS "
  "is built around. A <b>.rc</b> file is listed among the target's sources; "
  "CMake compiles it with <b>windres</b> under MinGW, or <b>rc.exe</b> under "
  "MSVC, and links the result into the binary.")
sp(3)

h2("3.1 &nbsp; The minimal resource script")
p("<b>resources/app.rc</b>:")
S.append(code_block([
    "#include <windows.h>",
    "",
    "IDI_ICON1 ICON \"app.ico\"",
]))
sp(2)
S.append(callout(
    "Two rules that cause most failures",
    "<b>The identifier matters.</b> Windows uses the icon with the "
    "lowest-sorting resource ID as the application icon. <b>IDI_ICON1</b> is "
    "the conventional choice — an arbitrary name may leave the icon embedded "
    "but unused.<br/><br/>"
    "<b>The path is relative to the .rc file</b>, not to the project root or "
    "the build directory. Use forward slashes, or escape backslashes as "
    "<b>\\\\</b>.", WARNBG, WARNBAR))
sp(3)

h2("3.2 &nbsp; Wiring it into CMakeLists")
S.append(code_block([
    "set(WINDOWS_RC_FILE \"\")",
    "if(WIN32)",
    "    set(RC_SOURCE \"${CMAKE_CURRENT_SOURCE_DIR}/resources/app.rc\")",
    "    if(EXISTS \"${RC_SOURCE}\")",
    "        set(WINDOWS_RC_FILE \"${RC_SOURCE}\")",
    "    endif()",
    "endif()",
    "",
    "qt_add_executable(MyApp",
    "    ${PROJECT_SOURCES}",
    "    ${WINDOWS_RC_FILE}      # <- the .rc must be in the source list",
    ")",
]))
sp(2)
p("The guard means the same CMakeLists still configures on Linux and macOS, "
  "where the variable stays empty and contributes nothing.")
sp(3)

h2("3.3 &nbsp; Generating the script so the version stays in sync")
p("Hardcoding a version string into the .rc guarantees it will drift from the "
  "real one. The better pattern is a template that CMake fills in. Name it "
  "<b>app.rc.in</b> and substitute at configure time:")
S.append(code_block([
    "# app.rc.in",
    "IDI_ICON1 ICON \"@CMAKE_CURRENT_SOURCE_DIR@/resources/app.ico\"",
    "",
    "VS_VERSION_INFO VERSIONINFO",
    "FILEVERSION @PROJECT_VERSION_MAJOR@,@PROJECT_VERSION_MINOR@,@PROJECT_VERSION_PATCH@,0",
    "...",
    "    VALUE \"ProductVersion\", \"@APP_VERSION@\"",
]))
sp(2)
S.append(code_block([
    "# CMakeLists.txt",
    "configure_file(",
    "    \"${CMAKE_CURRENT_SOURCE_DIR}/resources/app.rc.in\"",
    "    \"${CMAKE_CURRENT_BINARY_DIR}/app.rc\"",
    "    @ONLY",
    ")",
    "set(WINDOWS_RC_FILE \"${CMAKE_CURRENT_BINARY_DIR}/app.rc\")",
]))
sp(2)
S.append(callout(
    "Use an absolute icon path in a generated .rc",
    "The generated file lands in the <b>build</b> directory, so a relative "
    "path such as <b>\"app.ico\"</b> would be resolved against the build tree "
    "and fail. This is why the template above interpolates "
    "<b>@CMAKE_CURRENT_SOURCE_DIR@</b>.", LIGHT, BLUE))

S.append(PageBreak())

# ───────────────────────────────────────────────────────── 4
h1("4 &nbsp; Method B — qmake, for reference")
p("If a project still uses qmake rather than CMake, no .rc file is needed for "
  "the icon alone. One line in the .pro file is enough:")
S.append(code_block([
    "RC_ICONS = resources/app.ico",
]))
sp(2)
p("qmake generates a resource script behind the scenes. Related variables "
  "populate the version block:")
S.append(code_block([
    "VERSION            = 2.1.0",
    "QMAKE_TARGET_COMPANY     = Keynetik Solutions",
    "QMAKE_TARGET_PRODUCT     = KeynetikPOS",
    "QMAKE_TARGET_DESCRIPTION = Professional Point of Sale System",
    "QMAKE_TARGET_COPYRIGHT   = Copyright (C) 2026 Keynetik Solutions",
]))
sp(2)
S.append(callout(
    "Do not mix the two",
    "If you supply your own <b>RC_FILE</b>, qmake stops generating one and "
    "<b>RC_ICONS</b> is ignored. Use one mechanism or the other.",
    WARNBG, WARNBAR))
sp(2)
p("This section is included for completeness. KeynetikPOS is a CMake project "
  "— use Method A.")

sp(4)

# ───────────────────────────────────────────────────────── 5
h1("5 &nbsp; The runtime window icon")
p("The executable icon is embedded in the binary and is not readable as a file "
  "at runtime. For <b>setWindowIcon()</b> you need the image inside Qt's own "
  "resource system, which means a <b>.qrc</b> file and a raster format — "
  "typically PNG.")
sp(2)

h3("resources/resources.qrc")
S.append(code_block([
    "<RCC>",
    "    <qresource prefix=\"/\">",
    "        <file>app.png</file>",
    "    </qresource>",
    "</RCC>",
]))
sp(2)

h3("CMakeLists.txt")
S.append(code_block([
    "set(QT_RESOURCE_FILE \"\")",
    "if(EXISTS \"${CMAKE_CURRENT_SOURCE_DIR}/resources/resources.qrc\")",
    "    set(QT_RESOURCE_FILE \"resources/resources.qrc\")",
    "endif()",
    "# ... then include ${QT_RESOURCE_FILE} in the target sources",
]))
sp(2)

h3("main.cpp — before any window is constructed")
S.append(code_block([
    "QApplication app(argc, argv);",
    "app.setWindowIcon(QIcon(\":/app.png\"));",
]))
sp(2)
S.append(callout(
    "Set it on the application, not on each window",
    "<b>QApplication::setWindowIcon()</b> applies to every top-level window "
    "including dialogs created later. Setting it per-window means every new "
    "dialog is a chance to forget. Call it once, immediately after "
    "constructing the application object and before the first window.",
    LIGHT, ACCENT))
sp(2)
p("Use a reasonably large PNG — 256&nbsp;×&nbsp;256 — and let Qt scale it "
  "down. A 32&nbsp;×&nbsp;32 source will look poor on a high-DPI display.")

S.append(PageBreak())

# ───────────────────────────────────────────────────────── 6
h1("6 &nbsp; The current state of KeynetikPOS")
p("The pieces are half-assembled. The project has both a resource script and a "
  "correct template for generating one, but the two were never connected and "
  "the icon directive is disabled. Verified by reading the files on "
  "19&nbsp;July&nbsp;2026.")
sp(2)
S.append(table([
    ["Component", "State"],
    ["resources/app.ico",
     "Present, but contains only <b>one</b> image. Should hold four sizes "
     "(section 2)."],
    ["resources/icon.ico",
     "<b>Does not exist</b> — yet this is the filename both resource scripts "
     "point at."],
    ["resources/app.rc",
     "Compiled into the binary, but the <b>IDI_ICON1</b> line is commented "
     "out. Supplies version information only."],
    ["resources/app.rc.in",
     "A correct, complete template — but nothing calls <b>configure_file</b> "
     "on it, so it is dead code."],
    ["CMakeLists.txt:31",
     "Points <b>RC_SOURCE</b> at <b>app.rc</b>, the static file, never at the "
     "template."],
    ["resources/resources.qrc",
     "Does not exist. The CMake guard at line 38 therefore never fires."],
    ["setWindowIcon",
     "Not called anywhere in the codebase."],
    ["CMakeLists.txt:215",
     "The <b>installer</b> icon does correctly use app.ico — which is why "
     "Setup.exe looks right."],
]))
sp(3)
S.append(callout(
    "Consequences today",
    "The built executable carries <b>no icon at all</b> — Explorer, the "
    "taskbar and Alt+Tab show the Windows default, and Start Menu and desktop "
    "shortcuts inherit that same blank icon because they point at the "
    "executable. Separately, the embedded version information reads "
    "<b>1.0.0.0</b> while the application is version <b>2.1.0</b>, because "
    "app.rc hardcodes it.", WARNBG, WARNBAR))
sp(2)
p("Only the installer looks correct, which is exactly the trap described in "
  "section 1: a right-looking Setup.exe conceals a blank application icon.")

S.append(PageBreak())

# ───────────────────────────────────────────────────────── 7
h1("7 &nbsp; The repair, step by step")
p("Ordered so that each step can be verified before the next. Steps 1–3 fix "
  "the executable icon and the version drift; steps 4–5 add the runtime window "
  "icon.")
sp(2)

h3("Step 1 — Produce a multi-size icon")
p("Replace the single-image file with one containing all four sizes, keeping "
  "the existing name so the installer configuration at CMakeLists.txt:215 "
  "continues to work:")
S.append(code_block([
    "magick logo.png -define icon:auto-resize=256,48,32,16 resources/app.ico",
]))
sp(2)

h3("Step 2 — Point the template at the file that exists")
p("<b>resources/app.rc.in</b> currently references <b>icon.ico</b>, which is "
  "not present. Change it to <b>app.ico</b>:")
S.append(code_block([
    "IDI_ICON1 ICON \"@CMAKE_CURRENT_SOURCE_DIR@/resources/app.ico\"",
]))
sp(2)

h3("Step 3 — Generate the script instead of using the stale one")
p("In <b>CMakeLists.txt</b>, replace the block at lines 29&ndash;35 so the "
  "template is configured and the generated file is used. This fixes the icon "
  "and the version drift in one change, because the template already "
  "interpolates the real version:")
S.append(code_block([
    "set(WINDOWS_RC_FILE \"\")",
    "if(WIN32)",
    "    configure_file(",
    "        \"${CMAKE_CURRENT_SOURCE_DIR}/resources/app.rc.in\"",
    "        \"${CMAKE_CURRENT_BINARY_DIR}/app.rc\"",
    "        @ONLY",
    "    )",
    "    set(WINDOWS_RC_FILE \"${CMAKE_CURRENT_BINARY_DIR}/app.rc\")",
    "endif()",
]))
sp(2)
p("<b>app.rc</b> then becomes redundant and should be deleted, so no one "
  "later edits the file that is no longer compiled.")
sp(2)
S.append(callout(
    "Reconfigure, do not just rebuild",
    "<b>configure_file</b> runs at CMake configure time. An incremental build "
    "may not pick up an edit to the template — re-run CMake, or simply build "
    "after touching CMakeLists.txt, which triggers a reconfigure.",
    LIGHT, BLUE))
sp(3)

h3("Step 4 — Add a Qt resource for the window icon")
p("Export the logo as <b>resources/app.png</b> at 256&nbsp;×&nbsp;256 and "
  "create <b>resources/resources.qrc</b> as shown in section 5. The existing "
  "guard at CMakeLists.txt:38 detects the file automatically — no CMake change "
  "is needed for this step.")
sp(2)

h3("Step 5 — Set the icon at startup")
p("In <b>main.cpp</b>, immediately after the <b>QApplication</b> is "
  "constructed and before any window is shown:")
S.append(code_block([
    "app.setWindowIcon(QIcon(\":/app.png\"));",
]))
sp(2)
S.append(callout(
    "Order matters here",
    "This project shows a <b>LoginDialog</b> before <b>MainWindow</b> is "
    "constructed. Setting the icon on the application object before either is "
    "created ensures the login window is covered too — a per-window call on "
    "MainWindow would leave the first thing the user sees unbranded.",
    WARNBG, WARNBAR))

S.append(PageBreak())

# ───────────────────────────────────────────────────────── 8
h1("8 &nbsp; Verifying it worked")
p("Check the executable itself, not a shortcut to it, and not the installer.")
sp(2)
S.append(steps([
    "Rebuild, then open the build directory in Explorer and switch to "
    "<b>Large icons</b> view. The .exe should show the artwork.",
    "Right-click the .exe &rarr; <b>Properties</b> &rarr; <b>Details</b>. "
    "File version and product version should now read <b>2.1.0</b>, not "
    "1.0.0.0 — this confirms the generated script is the one being compiled.",
    "Run the application. The title bar and taskbar button should both show "
    "the icon, including on the login dialog.",
    "Repackage with cpack, install, and confirm the Start Menu and desktop "
    "shortcuts show the icon.",
]))
sp(3)
p("The version check can be scripted, and is the quickest confirmation that "
  "the <b>generated</b> resource script replaced the stale one:")
S.append(code_block([
    "$exe = \"build\\Desktop_Qt_6_11_0_MinGW_64_bit-Release\\KeynetikPOS.exe\"",
    "[Diagnostics.FileVersionInfo]::GetVersionInfo($exe) |",
    "    Select-Object FileVersion, ProductVersion, CompanyName",
]))
sp(2)
S.append(callout(
    "Version still 1.0.0.0 after rebuilding?",
    "The old <b>app.rc</b> is still being compiled. Confirm it was deleted and "
    "that <b>WINDOWS_RC_FILE</b> points into the binary directory, then "
    "reconfigure.", WARNBG, WARNBAR))

sp(4)

# ───────────────────────────────────────────────────────── 9
h1("9 &nbsp; Troubleshooting")
S.append(table([
    ["Symptom", "Cause and remedy"],
    ["Icon unchanged in Explorer, correct elsewhere",
     "Windows caches shell icons aggressively. Run <b>ie4uinit.exe -show</b>, "
     "or restart Explorer. Copying the .exe to a new name will also show the "
     "true icon immediately."],
    ["No icon anywhere after a clean build",
     "The .rc is not in the target's source list, or the ICON line is "
     "commented out, or the identifier is not <b>IDI_ICON1</b>. Check all "
     "three."],
    ["windres reports it cannot open the icon",
     "The path in the .rc is resolved relative to the .rc file. For a "
     "generated script in the build directory, use an absolute path via "
     "<b>@CMAKE_CURRENT_SOURCE_DIR@</b> (section 3.3)."],
    ["Title bar icon correct, taskbar wrong",
     "Two different icons. The taskbar generally follows the executable "
     "resource, the title bar follows setWindowIcon. Set both."],
    ["Icon blurred at small sizes",
     "The .ico contains only one large bitmap. Rebuild it with all four sizes "
     "(section 2)."],
    ["Installer looks right, application does not",
     "CPACK_NSIS_MUI_ICON is independent of the executable resource. Fix the "
     ".rc — see section 6."],
    ["Icon missing only on the login dialog",
     "setWindowIcon was called on MainWindow rather than on the QApplication, "
     "after the dialog had already been shown."],
]))
sp(3)
S.append(Paragraph(
    "Guide to setting the application icon in a Qt + CMake project, with the "
    "repair required by KeynetikPOS. Generated by "
    "docs/generate_app_icon_guide.py. Repository state in section 6 verified "
    "on 19 July 2026.", SMALL))


def main():
    doc = Doc(OUT, "Application Icon Guide",
              "KeynetikPOS — build reference")
    doc.build(S)
    print("Wrote %s" % OUT)


if __name__ == "__main__":
    main()
