!define APP_NAME        "KeynetikPOS"
!define APP_VERSION     "2.5.0"
!define APP_PUBLISHER   "Keynetik Solutions"
!define APP_EXE         "KeynetikPOS.exe"
!define APP_ICON        "resources\icon.ico"
!define INSTALL_DIR     "$PROGRAMFILES64\${APP_NAME}"
!define REG_KEY         "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}"
!define LICENSE_REG_KEY "Software\KeynetikPOS\License"
!define TRIAL_DAYS      30

Name            "${APP_NAME} ${APP_VERSION}"
OutFile         "${APP_NAME}-${APP_VERSION}-Setup.exe"
InstallDir      "${INSTALL_DIR}"
InstallDirRegKey HKLM "${REG_KEY}" "InstallLocation"
RequestExecutionLevel admin

!include "MUI2.nsh"
!include "nsDialogs.nsh"    ; ← FIX 2: added

Var CDKeyInput
Var CDKeyValue              ; ← FIX 1: was CDKeyValu

!define MUI_ABORTWARNING
!define MUI_ICON   "${APP_ICON}"
!define MUI_UNICON "${APP_ICON}"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "LICENSE.txt"
Page custom CDKeyPage CDKeyPageLeave
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

; ── FIX 3: Functions defined BEFORE Section blocks ─────────────
Function CDKeyPage
    nsDialogs::Create 1018
    Pop $0
    ${NSD_CreateLabel} 0 0 100% 40u "Enter your CD Key to activate KeynetikPOS.$\r$\nLeave blank to start a ${TRIAL_DAYS}-day free trial."
    ${NSD_CreateText}  0 50u 60% 14u ""
    Pop $CDKeyInput
    nsDialogs::Show
FunctionEnd

Function CDKeyPageLeave
    ${NSD_GetText} $CDKeyInput $CDKeyValue
    WriteRegStr HKCU "${LICENSE_REG_KEY}" "CDKey"       "$CDKeyValue"
    WriteRegStr HKCU "${LICENSE_REG_KEY}" "InstallDate" ""
FunctionEnd

;----------------------------------------------------------
Section "MainSection" SEC01
    SetOutPath "$INSTDIR"
    File /r "installer\app\*.*"

    WriteRegStr   HKLM "${REG_KEY}" "DisplayName"     "${APP_NAME}"
    WriteRegStr   HKLM "${REG_KEY}" "DisplayVersion"  "${APP_VERSION}"
    WriteRegStr   HKLM "${REG_KEY}" "Publisher"       "${APP_PUBLISHER}"
    WriteRegStr   HKLM "${REG_KEY}" "InstallLocation" "$INSTDIR"
    WriteRegStr   HKLM "${REG_KEY}" "UninstallString" "$INSTDIR\Uninstall.exe"
    WriteRegStr   HKLM "${REG_KEY}" "DisplayIcon"     "$INSTDIR\${APP_EXE}"
    WriteRegDWORD HKLM "${REG_KEY}" "NoModify"        1
    WriteRegDWORD HKLM "${REG_KEY}" "NoRepair"        1

    CreateShortCut "$DESKTOP\${APP_NAME}.lnk" "$INSTDIR\${APP_EXE}" "" "$INSTDIR\${APP_EXE}" 0
    CreateDirectory "$SMPROGRAMS\${APP_NAME}"
    CreateShortCut  "$SMPROGRAMS\${APP_NAME}\${APP_NAME}.lnk" "$INSTDIR\${APP_EXE}"
    CreateShortCut  "$SMPROGRAMS\${APP_NAME}\Uninstall.lnk"   "$INSTDIR\Uninstall.exe"

    WriteUninstaller "$INSTDIR\Uninstall.exe"
SectionEnd

;----------------------------------------------------------
Section "Uninstall"
    RMDir /r "$INSTDIR"
    Delete "$DESKTOP\${APP_NAME}.lnk"
    RMDir /r "$SMPROGRAMS\${APP_NAME}"
    DeleteRegKey HKLM "${REG_KEY}"
SectionEnd