#define AppName "Cosmic"
#define AppVersion GetEnv("APP_VERSION")

#if AppVersion == ""
  #error "APP_VERSION must be set when compiling the installer"
#endif

[Setup]
AppId={{6E9FD37E-A786-4B60-9D8F-E6D3AB30E562}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher=Cosmic
DefaultDirName={localappdata}\Programs\Cosmic
DefaultGroupName=Cosmic
PrivilegesRequired=lowest
ArchitecturesInstallIn64BitMode=x64
OutputDir=..
OutputBaseFilename=cosmic-{#AppVersion}-windows-x64
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayIcon={app}\bin\cosmic.exe

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Additional shortcuts:"

[Files]
Source: "..\dist\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\Cosmic"; Filename: "{app}\bin\cosmic.exe"
Name: "{autodesktop}\Cosmic"; Filename: "{app}\bin\cosmic.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\bin\cosmic.exe"; Description: "Launch Cosmic"; Flags: nowait postinstall skipifsilent
