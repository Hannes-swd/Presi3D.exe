; Presi3D installer
;
; Ships only the small Presi3D.exe in the installer itself. The Qt runtime
; DLLs (~27 MB) are downloaded from the GitHub Release at install time and
; extracted automatically, so the setup.exe you hand out stays tiny.

#define MyAppName "Presi 3D"
#define MyAppVersion "1.4"
#define MyAppExeName "Presi3D.exe"
#define DepsUrl "https://github.com/Hannes-swd/Presi3D.exe/releases/download/deps-v3/Presi3D-deps.zip"
#define DepsFileName "Presi3D-deps.zip"
#define DepsSHA256 "e2c6f84406f16efaab1fa7b1ce8cd8f243befaf8e5eb315d5726a5c386c094a6"

[Setup]
AppId={{82A24654-F772-4B0A-8DF7-86A06925ABC0}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
DefaultDirName={autopf}\Presi3D
DefaultGroupName={#MyAppName}
UninstallDisplayIcon={app}\{#MyAppExeName}
ArchiveExtraction=auto
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
OutputDir=Output
OutputBaseFilename=Presi3DSetup
PrivilegesRequiredOverridesAllowed=dialog

[Languages]
Name: "german"; MessagesFile: "compiler:Languages\German.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"

[Files]
Source: "..\build\Release\Presi3D.exe"; DestDir: "{app}"; Flags: ignoreversion
; Downloaded at install time (see [Code]) then auto-extracted here.
Source: "{tmp}\{#DepsFileName}"; DestDir: "{app}"; Flags: external extractarchive recursesubdirs ignoreversion

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\{cm:UninstallProgram,{#MyAppName}}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent

[Code]
var
  DownloadPage: TDownloadWizardPage;

procedure InitializeWizard;
begin
  DownloadPage := CreateDownloadPage(SetupMessage(msgWizardPreparing), SetupMessage(msgPreparingDesc), nil);
end;

function NextButtonClick(CurPageID: Integer): Boolean;
begin
  if CurPageID = wpReady then begin
    DownloadPage.Clear;
    DownloadPage.Add('{#DepsUrl}', '{#DepsFileName}', '{#DepsSHA256}');
    DownloadPage.Show;
    try
      try
        DownloadPage.Download;
        Result := True;
      except
        if DownloadPage.AbortedByUser then
          Log('Aborted by user.')
        else
          SuppressibleMsgBox(AddPeriod(GetExceptionMessage), mbCriticalError, MB_OK, IDOK);
        Result := False;
      end;
    finally
      DownloadPage.Hide;
    end;
  end else
    Result := True;
end;
