; openFPGALoader Windows Installer - Inno Setup Script
; SPDX-License-Identifier: Apache-2.0

#define MyAppName "openFPGALoader"
#ifndef MyAppVersion
#define MyAppVersion "0.0.0"
#endif
#define MyAppPublisher "openFPGALoader"
#define MyAppURL "https://github.com/xsession/openFPGALoader"
#define MyAppExeName "openFPGALoader.exe"

[Setup]
AppId={{A8F5B2D1-3C94-4E7A-B6D2-1F8E3A5C7D90}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={autopf}\openFPGALoader
DefaultGroupName={#MyAppName}
AllowNoIcons=yes
OutputDir=Z:\dist
OutputBaseFilename=openFPGALoader-{#MyAppVersion}-win64-setup
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
ChangesEnvironment=yes
DisableProgramGroupPage=yes
LicenseFile=Z:\license

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
Name: "addpath"; Description: "Add openFPGALoader to system PATH"; GroupDescription: "Installation options:"; Flags: unchecked
Name: "installdrivers"; Description: "Install Xilinx Platform Cable USB drivers"; GroupDescription: "Driver installation:"; Flags: unchecked

[Files]
Source: "Z:\dist\install\bin\openFPGALoader.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "Z:\dist\install\share\openFPGALoader\*"; DestDir: "{app}\share\openFPGALoader"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "Z:\dist\installer\xilinx-platform-cable-windows\*"; DestDir: "{app}\drivers\xilinx"; Flags: ignoreversion recursesubdirs createallsubdirs; Tasks: installdrivers

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\{cm:UninstallProgram,{#MyAppName}}"; Filename: "{uninstallexe}"
Name: "{commondesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
; libwdi generates the signed-driver-compatible INF/CAT payload for each live
; XPCU identity, then installs it through the elevated PowerShell helper.
Filename: "{sys}\WindowsPowerShell\v1.0\powershell.exe"; Parameters: "-NoProfile -ExecutionPolicy Bypass -File ""{app}\drivers\xilinx\install.ps1"" -Silent"; StatusMsg: "Installing Xilinx Platform Cable USB drivers..."; Tasks: installdrivers; Flags: runhidden waituntilterminated

[Code]
function AddToPath(Path: string): Boolean;
var
  oldPath: string;
begin
  if RegQueryStringValue(HKEY_LOCAL_MACHINE,
    'SYSTEM\CurrentControlSet\Control\Session Manager\Environment',
    'Path', oldPath) then
  begin
    if not Pos(';' + ExpandConstant('{app}') + ';', ';' + oldPath + ';') > 0 then
    begin
      oldPath := oldPath + ';' + ExpandConstant('{app}');
      RegWriteStringValue(HKEY_LOCAL_MACHINE,
        'SYSTEM\CurrentControlSet\Control\Session Manager\Environment',
        'Path', oldPath);
    end;
    Result := True;
  end
  else
    Result := False;
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssPostInstall then
  begin
    if IsTaskSelected('addpath') then
    begin
      AddToPath(ExpandConstant('{app}'));
    end;
  end;
end;
