; مثبت نظم المستقل لويندوز.

#define MyAppId "{{8D3D57AE-41CF-4B8A-95E9-270E4564E2A1}"
#define MyAppName "نظم"
#ifndef MyAppVersion
  #define MyAppVersion "0.4.0"
#endif
#ifndef NazmBinaryDir
  #define NazmBinaryDir "build\windows-release"
#endif
#define MyAppPublisher "Omar Aglan"
#define MyAppURL "https://github.com/OmarAglan/Nazm"
#define MyAppExeName "نظم.exe"

[Setup]
AppId={#MyAppId}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={autopf}\Nazm
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
AllowNoIcons=yes
OutputDir=dist\installer
OutputBaseFilename=nazm-setup-{#MyAppVersion}-x64
#ifdef InstallerSignTool
SignTool={#InstallerSignTool}
SignedUninstaller=yes
#endif
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
PrivilegesRequired=admin
PrivilegesRequiredOverridesAllowed=dialog commandline
ChangesEnvironment=yes
SetupLogging=yes
UsePreviousAppDir=yes
UsePreviousLanguage=yes
UsePreviousTasks=yes
CloseApplications=yes
RestartApplications=no
UninstallDisplayIcon={app}\bin\{#MyAppExeName}
UninstallDisplayName={#MyAppName} {#MyAppVersion}

[Languages]
Name: "arabic"; MessagesFile: "compiler:Languages\Arabic.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Types]
Name: "full"; Description: "تثبيت كامل"
Name: "compact"; Description: "أداة سطر الأوامر"
Name: "custom"; Description: "تثبيت مخصص"; Flags: iscustom

[Components]
Name: "cli"; Description: "أداة نظم"; Types: full compact custom; Flags: fixed
Name: "developer"; Description: "رأس C والمكتبة الثابتة"; Types: full

[Files]
Source: "{#NazmBinaryDir}\نظم.exe"; DestDir: "{app}\bin"; Components: cli; Flags: ignoreversion
Source: "{#NazmBinaryDir}\nazm.exe"; DestDir: "{app}\bin"; Components: cli; Flags: ignoreversion
Source: "{#NazmBinaryDir}\libnazm.a"; DestDir: "{app}\lib"; Components: developer; Flags: ignoreversion
Source: "include\nazm.h"; DestDir: "{app}\include"; Components: developer; Flags: ignoreversion
Source: "Docs\*"; DestDir: "{app}\docs"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "examples\*"; DestDir: "{app}\examples"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "README.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "CHANGELOG.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "ROADMAP.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "LICENSE"; DestDir: "{app}"; Flags: ignoreversion

[InstallDelete]
Type: filesandordirs; Name: "{app}\bin"
Type: filesandordirs; Name: "{app}\lib"
Type: filesandordirs; Name: "{app}\include"

[Icons]
Name: "{autoprograms}\نظم\دليل نظم"; Filename: "{app}\README.md"
Name: "{autoprograms}\نظم\أمثلة نظم"; Filename: "{app}\examples"
Name: "{autoprograms}\نظم\إزالة نظم"; Filename: "{uninstallexe}"

[Run]
Filename: "{app}\bin\{#MyAppExeName}"; Parameters: "--إصدار"; Description: "التحقق من إصدار نظم"; Flags: postinstall skipifsilent unchecked runhidden

[Code]
#include "installer\windows_environment.iss"

const
  NAZM_INSTALLER_KEY = 'Software\BaaEcosystem\Nazm';
  NAZM_PATH_OWNED_VALUE = 'PathOwned';

procedure NazmRegistryRoot(var Root: Integer);
begin
  if IsAdminInstallMode then
    Root := HKLM
  else
    Root := HKCU;
end;

function NazmOwnsPath: Boolean;
var
  Root: Integer;
  Value: Cardinal;
begin
  NazmRegistryRoot(Root);
  Result := RegQueryDWordValue(Root, NAZM_INSTALLER_KEY,
    NAZM_PATH_OWNED_VALUE, Value) and (Value = 1);
end;

procedure NazmSetPathOwned(const Owned: Boolean);
var
  Root: Integer;
begin
  NazmRegistryRoot(Root);
  if Owned then
    RegWriteDWordValue(Root, NAZM_INSTALLER_KEY, NAZM_PATH_OWNED_VALUE, 1)
  else
    RegDeleteValue(Root, NAZM_INSTALLER_KEY, NAZM_PATH_OWNED_VALUE);
end;

procedure ApplyNazmEnvironment;
var
  Root: Integer;
  BinDirectory: string;
begin
  BinDirectory := ExpandConstant('{app}\bin');
  Log('Nazm installer: applying PATH entry ' + BinDirectory);
  if EcoEnsurePathContains(BinDirectory) then
  begin
    Log('Nazm installer: PATH entry added and owned.');
    NazmSetPathOwned(True);
  end
  else
    Log('Nazm installer: PATH entry already present.');
  NazmRegistryRoot(Root);
  RegWriteStringValue(Root, NAZM_INSTALLER_KEY, 'InstallLocation',
    ExpandConstant('{app}'));
  RegWriteStringValue(Root, NAZM_INSTALLER_KEY, 'Version',
    '{#MyAppVersion}');
end;

function RunNazmVersionProbe: Boolean;
var
  ExitCode: Integer;
begin
  Result :=
    FileExists(ExpandConstant('{app}\bin\{#MyAppExeName}')) and
    FileExists(ExpandConstant('{app}\bin\nazm.exe')) and
    Exec(ExpandConstant('{app}\bin\{#MyAppExeName}'), '--إصدار',
      ExpandConstant('{app}\bin'), SW_HIDE, ewWaitUntilTerminated, ExitCode) and
    (ExitCode = 0);
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  Log('Nazm installer: CurStepChanged called.');
  if CurStep = ssPostInstall then
  begin
    Log('Nazm installer: entering post-install checks.');
    ApplyNazmEnvironment;
    EcoBroadcastEnvironmentChange;
    if not RunNazmVersionProbe then
      RaiseException('فشل فحص صحة نظم بعد التثبيت. راجع سجل المثبت.');
    if not WizardSilent then
      MsgBox('اكتمل تثبيت نظم. افتح طرفية جديدة لاستخدام الأمر نظم.',
        mbInformation, MB_OK);
  end;
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  Root: Integer;
begin
  if CurUninstallStep = usPostUninstall then
  begin
    if NazmOwnsPath then
      EcoEnsurePathRemoved(ExpandConstant('{app}\bin'));
    NazmRegistryRoot(Root);
    RegDeleteKeyIncludingSubkeys(Root, NAZM_INSTALLER_KEY);
    EcoBroadcastEnvironmentChange;
  end;
end;
