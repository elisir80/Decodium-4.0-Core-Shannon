; Decodium Installer Script per Inno Setup 6.x
; Pensato per essere pilotato sia localmente sia da GitHub Actions.

#ifndef AppName
  #define AppName "Decodium"
#endif
#ifndef AppVersion
  #define AppVersion "1.0.427"
#endif
#ifndef AppPublisher
  #define AppPublisher "IU8LMC"
#endif
#ifndef AppExeName
  #define AppExeName "decodium.exe"
#endif
#ifndef BuildDir
  #define BuildDir "..\build_mingw64"
#endif
#ifndef SourceRoot
  #define SourceRoot ".."
#endif
#ifndef OutputDir
  #define OutputDir ".\output"
#endif
#ifndef OutputBaseFilename
  #define OutputBaseFilename "Decodium_" + AppVersion + "_Setup_x64"
#endif

[Setup]
AppId={{D3C0D1A4-4000-4A10-8C64-6F7C3A6F4000}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher={#AppPublisher}
AppPublisherURL=https://github.com/iu8lmc/Decodium-4.0-Core-Shannon
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
AllowNoIcons=yes
OutputDir={#OutputDir}
OutputBaseFilename={#OutputBaseFilename}
SetupIconFile={#SourceRoot}\icons\windows-icons\decodium.ico
WizardImageFile=compiler:WizClassicImage-IS.bmp
WizardSmallImageFile=compiler:WizClassicSmallImage-IS.bmp
Compression=lzma2/ultra64
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
UninstallDisplayIcon={app}\{#AppExeName}
ShowLanguageDialog=auto
WizardStyle=modern
LicenseFile={#SourceRoot}\COPYING

[Languages]
Name: "italian"; MessagesFile: "compiler:Languages\Italian.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[InstallDelete]
; 1.0.428 — PULIZIA OBBLIGATORIA della cartella di installazione: rimuove tutto il
; contenuto della versione precedente PRIMA di copiare i nuovi file (in aggiunta
; alla disinstallazione automatica in [Code]). Elimina anche eventuali cartelle
; vuote/residui (es. vecchie wsjtx_*_autogen) da installazioni precedenti.
Type: filesandordirs; Name: "{app}\*"
; Pulisci cache QML compilata da versioni precedenti — Qt6 la rigenera
; automaticamente dal .qml sorgente aggiornato. Senza questo, il vecchio
; .qmlc può essere caricato al posto del .qml nuovo, causando bug fantasma.
Type: filesandordirs; Name: "{app}\qml\decodium\*.qmlc"
Type: filesandordirs; Name: "{app}\qml\decodium\components\*.qmlc"
Type: filesandordirs; Name: "{app}\qml\decodium\qmlcache"
Type: filesandordirs; Name: "{app}\qmlcache"
; Pulisci anche le vecchie cache Qt/QML utente condivise da installazioni
; precedenti. Decodium usa una cache isolata per versione+path.
Type: filesandordirs; Name: "{localappdata}\IU8LMC\Decodium\cache\qmlcache"
Type: filesandordirs; Name: "{localappdata}\IU8LMC\Decodium\qmlcache"
Type: filesandordirs; Name: "{localappdata}\Decodium\cache\qmlcache"
Type: filesandordirs; Name: "{localappdata}\decodium4\cache\qmlcache"

[UninstallDelete]
; Rimuovi anche eventuali file generati dopo l'installazione dentro la
; directory dell'app. Con installazione per-utente, {app} corrisponde a:
; C:\Users\<utente>\AppData\Local\Programs\Decodium.
Type: filesandordirs; Name: "{app}\*"
Type: dirifempty; Name: "{app}"

[Files]
; Copia l'intero bundle portabile già preparato da windeployqt.
; In questo modo installer e portable restano sempre allineati 1:1.
; Esclude pollution di build CMake (oggetti, autogen, static libs, test, tools sperimentali).
; 1.0.428 — Esclusioni rinforzate: "wsjtx*" rimuove OGNI artefatto wsjtx residuo
; (l'exe build wsjtx_app_version.exe, wsjtx_config.h, libwsjtx_udp.a e le cartelle
; vuote wsjtx_*_autogen). Aggiunte anche Testing/, NVIDIA Corporation/, .claude/ e
; build_mingw64/ annidata = spazzatura di build. Niente DLL wsjtx → sicuro.
; Rimosso 'createallsubdirs': senza, Inno NON crea le cartelle che restano vuote
; dopo l'esclusione dei contenuti (es. *_autogen) — risolve le "cartelle vuote".
Source: "{#BuildDir}\*"; DestDir: "{app}"; \
  Excludes: "wsjtx*,CMakeFiles\*,deploy\*,deploy_staging\*,map65\*,qmap\*,Testing\*,NVIDIA Corporation\*,.claude\*,build_mingw64\*,CMakeCache.txt,cmake_install.cmake,CTestTestfile.cmake,Makefile,build.ninja,.ninja_*,compile_commands.json,*.obj,*.d,*.a,*.rc,*_autogen\*,.qt\*,tests\*,tools\*,bundle_fixup\*,qrc_*.cpp,qrc_*.cpp.depends,*.qrc.depends,*.cmake,VersionInfo_*.h,VersionResource_*.rc,DartConfiguration.tcl,CPack*.cmake"; \
  Flags: ignoreversion recursesubdirs

; File licenza fuori dal bundle
Source: "{#SourceRoot}\COPYING"; DestDir: "{app}"; DestName: "COPYING.txt"; Flags: ignoreversion skipifsourcedoesntexist

[Icons]
Name: "{group}\{#AppName}";              Filename: "{app}\{#AppExeName}"; IconFilename: "{app}\{#AppExeName}"
Name: "{group}\Disinstalla {#AppName}";  Filename: "{uninstallexe}"
Name: "{autodesktop}\{#AppName}";        Filename: "{app}\{#AppExeName}"; IconFilename: "{app}\{#AppExeName}"; Tasks: desktopicon

[Registry]
Root: HKCU; Subkey: "Software\{#AppPublisher}\{#AppName}"; ValueType: string; ValueName: "InstallPath"; ValueData: "{app}"; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\{#AppPublisher}\{#AppName}"; ValueType: string; ValueName: "Version"; ValueData: "{#AppVersion}"

[Run]
; 1.0.428 — avvio automatico DISABILITATO: dopo un'installazione interattiva il PC
; viene riavviato (vedi sezione [Code], CurStepChanged). Lanciare l'app per poi
; riavviare subito sarebbe inutile/confuso: l'utente riapre Decodium dopo il reboot
; (icona desktop / menu Start). Per tornare al comportamento classico (avvio app, NO
; reboot) riabilita la riga sottostante e rimuovi la sezione [Code].
; Filename: "{app}\{#AppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(AppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent

[Code]
const
  AppUninstallKey =
    'Software\Microsoft\Windows\CurrentVersion\Uninstall\{D3C0D1A4-4000-4A10-8C64-6F7C3A6F4000}_is1';

{ Legge la UninstallString della versione gia' installata (prima HKCU = install
  per-utente, poi HKLM = eventuale install per-macchina). '' se non installata. }
function GetUninstallString: String;
var
  S: String;
begin
  S := '';
  if not RegQueryStringValue(HKCU, AppUninstallKey, 'UninstallString', S) then
    RegQueryStringValue(HKLM, AppUninstallKey, 'UninstallString', S);
  Result := S;
end;

{ 1.0.428 — Disinstallazione OBBLIGATORIA in automatico della versione precedente
  PRIMA di copiare i nuovi file (scelta utente). Gira in silenzio e senza riavvio
  (il reboot lo forziamo a fine installazione). L'uninstaller di Inno si auto-copia
  in temp e ritorna subito: dopo Exec attendiamo che la chiave di disinstallazione
  sparisca (max ~30s) cosi' la copia dei nuovi file non va in conflitto con la
  cancellazione dei vecchi. La pulizia della cartella e' rinforzata anche dalla
  sezione InstallDelete sulla cartella app. }
procedure UninstallPreviousVersion;
var
  UninstStr: String;
  ResultCode, Waited: Integer;
begin
  UninstStr := GetUninstallString;
  if UninstStr = '' then
    Exit;
  UninstStr := RemoveQuotes(UninstStr);
  Exec(UninstStr, '/VERYSILENT /SUPPRESSMSGBOXES /NORESTART', '', SW_HIDE,
       ewWaitUntilTerminated, ResultCode);
  Waited := 0;
  while (GetUninstallString <> '') and (Waited < 60) do
  begin
    Sleep(500);
    Waited := Waited + 1;
  end;
end;

procedure CurStepChanged(CurStep: TSetupStep);
var
  ResultCode: Integer;
begin
  { Prima di copiare i file: disinstalla automaticamente la versione precedente. }
  if CurStep = ssInstall then
    UninstallPreviousVersion;

  { 1.0.428 — Riavvio PC FORZATO a fine installazione interattiva (scelta utente:
    "reboot obbligatorio"). Conta alla rovescia 15s con messaggio. Saltato in
    /SILENT e /VERYSILENT per non sorprendere flussi automatici/CI. shutdown /r
    funziona anche per utente non-admin (sessione interattiva = SeShutdownPrivilege). }
  if (CurStep = ssDone) and (not WizardSilent) then
    Exec(ExpandConstant('{sys}\shutdown.exe'),
         '/r /t 15 /c "Decodium aggiornato. Il PC verra'' riavviato tra 15 secondi per completare l''installazione. Salva il lavoro aperto."',
         '', SW_HIDE, ewNoWait, ResultCode);
end;
