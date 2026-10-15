; ---------------------------------------------------------------------------
; Winnow Windows installer (Inno Setup 6).
;
; Driven by packaging\windows\deploy.ps1, which passes:
;   /DMyAppVersion=<version>   single-sourced from CMake project(VERSION ...)
;   /DSourceDir=<dir>          the deployed build output (build\win-release\app)
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
  #define SourceDir "..\..\build\win-release\app"
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
; In-app update (Utilities/appinstaller.cpp) runs this installer with
;   /SILENT /SUPPRESSMSGBOXES /NORESTART /WAITPID=<Winnow pid> /RELAUNCH=1
; Relaunch Winnow after that silent install. postinstall runs it as the original,
; non-elevated user.
Filename: "{app}\{#MyAppExeName}"; Flags: nowait postinstall skipifnotsilent; Check: ShouldRelaunch

[Code]
const
  SYNCHRONIZE = $00100000;
  INFINITE = $FFFFFFFF;

function OpenProcess(dwDesiredAccess: DWORD; bInheritHandle: BOOL; dwProcessId: DWORD): THandle;
  external 'OpenProcess@kernel32.dll stdcall';
function WaitForSingleObject(hHandle: THandle; dwMilliseconds: DWORD): DWORD;
  external 'WaitForSingleObject@kernel32.dll stdcall';
function CloseHandle(hObject: THandle): BOOL;
  external 'CloseHandle@kernel32.dll stdcall';

{ /WAITPID=<pid>: the in-app updater starts this installer and then quits Winnow.
  Wait for that Winnow process to exit before touching its files. If the user
  declines the quit, the install simply happens whenever Winnow does quit. }
function InitializeSetup(): Boolean;
var
  Pid: Integer;
  H: THandle;
begin
  Pid := StrToIntDef(ExpandConstant('{param:waitpid|0}'), 0);
  if Pid > 0 then begin
    H := OpenProcess(SYNCHRONIZE, False, Pid);
    if H <> 0 then begin
      WaitForSingleObject(H, INFINITE);
      CloseHandle(H);
    end;
  end;
  Result := True;
end;

function ShouldRelaunch(): Boolean;
begin
  Result := ExpandConstant('{param:relaunch|0}') = '1';
end;
