; ---------------------------------------------------------------------------
; Winnow Windows installer (Inno Setup 6).
;
; Driven by packaging\windows\deploy.ps1, which passes:
;   /DMyAppVersion=<version>   single-sourced from CMake project(VERSION ...)
;   /DSourceDir=<dir>          the deployed build output (build\win-release\Release)
;   /DOutputDir=<dir>          where to write WinnowSetup-<version>.exe (repo out\)
;
; SourceDir already contains Winnow.exe, the Qt runtime + plugins (windeployqt),
; the third-party DLLs, the ONNX model, ExifTool, and Winnet — assembled by the
; CMake POST_BUILD steps. We package that whole tree recursively.
; ---------------------------------------------------------------------------

#ifndef MyAppVersion
  #define MyAppVersion "0.0.0"
#endif
#ifndef SourceDir
  #define SourceDir "..\..\build\win-release\Release"
#endif
#ifndef OutputDir
  #define OutputDir "..\..\out"
#endif

#define MyAppName "Winnow"
#define MyAppPublisher "Robert Hill"
#define MyAppExeName "Winnow.exe"
#define MyAppURL "https://winnow.ca"

[Setup]
AppId={{2663CS48-9R00-4000-8000-77696E6E6F77}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
UninstallDisplayIcon={app}\{#MyAppExeName}
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir={#OutputDir}
OutputBaseFilename=WinnowSetup-{#MyAppVersion}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
; Whole deployed tree, recursively.
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: recursesubdirs createallsubdirs ignoreversion

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\{cm:UninstallProgram,{#MyAppName}}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#MyAppName}}"; Flags: nowait postinstall skipifsilent
