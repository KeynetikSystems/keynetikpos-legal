; KeynetikPOS Inno Setup installer script.
;
; Packages the windeployqt-staged Release build (build-mingw\windeployqt_stage)
; — build that first:
;   cmake --build build-mingw --config Release --target KeynetikPOS
; then compile this script (ISCC.exe installer.iss, or open it in the Inno
; Setup Compiler GUI).
;
; This is a second, independent installer alongside the existing CPack/NSIS
; one in CMakeLists.txt (CPACK_GENERATOR "NSIS") — metadata below is kept in
; sync with APP_NAME/APP_VERSION/APP_PUBLISHER/APP_URL there by hand, since
; Inno Setup scripts aren't CMake-configured.

#define AppName "KeynetikPOS"
#define AppVersion "2.1.0"
#define AppPublisher "Keynetik Solutions"
#define AppURL "https://www.keynetik.com"
#define AppExeName "KeynetikPOS.exe"
#define DeployDir "build-mingw\windeployqt_stage"

[Setup]
; Fixed AppId (generated once) — keep this stable across versions so Windows
; recognizes upgrades/uninstalls as the same product instead of a new one.
AppId={{39365AF0-4CA0-4010-A74D-2F1ED0D0B0A3}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
AppPublisherURL={#AppURL}
AppSupportURL={#AppURL}
AppUpdatesURL={#AppURL}
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
UninstallDisplayIcon={app}\{#AppExeName}
LicenseFile=LICENSE.txt
SetupIconFile=resources\app.ico
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
OutputDir=dist
OutputBaseFilename={#AppName}-{#AppVersion}-Setup
ArchitecturesInstallIn64BitMode=x64compatible
DisableProgramGroupPage=yes
; The deployed tree doesn't exist until the Release build has run.
SourceDir=.

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Create a &desktop shortcut"; GroupDescription: "Additional shortcuts:"

[Files]
; Whole deployed tree (exe + Qt DLLs + plugin subfolders) — recursesubdirs
; picks up generic/iconengines/imageformats/platforms/sqldrivers/styles/tls
; etc. without listing each one.
Source: "{#DeployDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\{#AppExeName}"
Name: "{group}\Uninstall {#AppName}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#AppExeName}"; Description: "Launch {#AppName}"; Flags: nowait postinstall skipifsilent
