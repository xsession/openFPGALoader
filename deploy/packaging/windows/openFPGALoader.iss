; openFPGALoader Windows Installer - Inno Setup Script
; SPDX-License-Identifier: Apache-2.0

#define MyAppName "openFPGALoader"
#define MyAppVersion "1.1.2"
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
Name: "installdrivers"; Description: "Install USB device drivers (Xilinx Platform Cable, Digilent HS3)"; GroupDescription: "Driver installation:"; Flags: unchecked

[Files]
Source: "Z:\dist\install\bin\openFPGALoader.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "Z:\dist\install\share\openFPGALoader\*"; DestDir: "{app}\share\openFPGALoader"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "Z:\externals\xilinx-usb-driver\dist\xpcu-driver\xilinx-platform-cable-windows\driver\*"; DestDir: "{app}\drivers\xilinx"; Flags: ignoreversion recursesubdirs createallsubdirs; Tasks: installdrivers

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\{cm:UninstallProgram,{#MyAppName}}"; Filename: "{uninstallexe}"
Name: "{commondesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
; Install Xilinx USB drivers using pnputil
Filename: "pnputil.exe"; Parameters: "-i -a ""{app}\drivers\xilinx\03fd_0008\xpcu_0008.inf"""; StatusMsg: "Installing Xilinx Platform Cable USB driver..."; Tasks: installdrivers; Flags: runhidden
Filename: "pnputil.exe"; Parameters: "-i -a ""{app}\drivers\xilinx\03fd_000d\xpcu_000d.inf"""; StatusMsg: "Installing Xilinx Platform Cable USB driver (alt)..."; Tasks: installdrivers; Flags: runhidden
Filename: "pnputil.exe"; Parameters: "-i -a ""{app}\drivers\xilinx\03fd_0013\xpcu_0013.inf"""; StatusMsg: "Installing Xilinx Platform Cable USB driver (alt)..."; Tasks: installdrivers; Flags: runhidden

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