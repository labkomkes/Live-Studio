; Live Studio - skrip Inno Setup 6.
; Dipanggil oleh scripts/package-windows.ps1 dengan:
;   /DAppVersion=0.4.1 /DStageDir=<folder hasil cmake --install> /DOutputDir=<dist> /DRedistFile=<vc_redist.x64.exe>
; STATUS: belum dikompilasi/diuji oleh penulis (tidak ada Windows/Inno Setup di lingkungan pengembangan).

#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif
#ifndef StageDir
  #error StageDir belum didefinisikan (jalankan lewat scripts/package-windows.ps1)
#endif
#ifndef OutputDir
  #define OutputDir "..\dist"
#endif
#ifndef RedistFile
  #define RedistFile ""
#endif

[Setup]
AppId={{6B1D5E0A-3C0F-4E4B-9C55-4F0A7E2D91C3}
AppName=Live Studio
AppVersion={#AppVersion}
AppVerName=Live Studio {#AppVersion}
AppPublisher=Live Studio
DefaultDirName={autopf}\Live Studio
DefaultGroupName=Live Studio
DisableProgramGroupPage=yes
OutputDir={#OutputDir}
OutputBaseFilename=LiveStudio-Setup-{#AppVersion}
SetupIconFile=..\resources\livestudio.ico
UninstallDisplayIcon={app}\bin\64bit\LiveStudio.exe
LicenseFile=..\LICENSE.txt
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0.17763
PrivilegesRequired=admin
PrivilegesRequiredOverridesAllowed=dialog
CloseApplications=yes

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Buat pintasan di Desktop"; GroupDescription: "Pintasan:"; Flags: unchecked

[Files]
Source: "{#StageDir}\*"; DestDir: "{app}"; Flags: recursesubdirs createallsubdirs ignoreversion
#if RedistFile != ""
Source: "{#RedistFile}"; DestDir: "{tmp}"; Flags: deleteafterinstall; Check: NeedsVCRedist
#endif

[Icons]
Name: "{autoprograms}\Live Studio"; Filename: "{app}\bin\64bit\LiveStudio.exe"; WorkingDir: "{app}\bin\64bit"
Name: "{autoprograms}\Live Studio - Uji Mandiri"; Filename: "{app}\bin\64bit\LiveStudio.exe"; Parameters: "--selftest"; WorkingDir: "{app}\bin\64bit"
Name: "{autodesktop}\Live Studio"; Filename: "{app}\bin\64bit\LiveStudio.exe"; WorkingDir: "{app}\bin\64bit"; Tasks: desktopicon

[Run]
#if RedistFile != ""
Filename: "{tmp}\vc_redist.x64.exe"; Parameters: "/install /quiet /norestart"; StatusMsg: "Memasang Microsoft Visual C++ Runtime..."; Check: NeedsVCRedist; Flags: waituntilterminated
#endif
Filename: "{app}\bin\64bit\LiveStudio.exe"; Description: "Jalankan Live Studio"; WorkingDir: "{app}\bin\64bit"; Flags: nowait postinstall skipifsilent

[Code]
// Data pengguna (%APPDATA%\LiveStudio: scenes.json, log) sengaja TIDAK dihapus saat uninstall.
function NeedsVCRedist: Boolean;
var
  Installed: Cardinal;
begin
  Result := True;
  if RegQueryDWordValue(HKLM64, 'SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64', 'Installed', Installed) then
    Result := (Installed <> 1);
end;
