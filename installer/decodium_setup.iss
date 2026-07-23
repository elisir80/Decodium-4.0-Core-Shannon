; Decodium Installer Script per Inno Setup 6.x
; Pensato per essere pilotato sia localmente sia da GitHub Actions.

#ifndef AppName
  #define AppName "Decodium"
#endif
#ifndef AppVersion
  #define AppVersion "1.0.472"
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
; 1.0.491 — FIX errore uninstaller "file in uso": senza questo Inno non chiudeva
; Decodium (ne' i suoi processi Qt / DLL / cache in uso) prima di rimuovere i file,
; e la disinstallazione (e l'auto-disinstallazione a ogni update) falliva con
; l'errore di rimozione. Il Restart Manager di Windows ora chiude l'app in modo
; pulito (e la force-killa se e' bloccata/freezata) prima di install e uninstall.
CloseApplications=yes
RestartApplications=no
ShowLanguageDialog=auto
WizardStyle=modern
LicenseFile={#SourceRoot}\COPYING

[Languages]
; 1.0.430 — tutte le 13 lingue supportate dall'app Decodium (translations/decodium_*.ts).
; Le ultime 3 (cinese sempl./trad., lettone) non sono nel set ufficiale di Inno: i .isl
; sono inclusi nel repo sotto installer\languages\ (UTF-8 con BOM, da jrsoftware/issrc).
Name: "italian";            MessagesFile: "compiler:Languages\Italian.isl"
Name: "dutch";              MessagesFile: "compiler:Languages\Dutch.isl"
Name: "english";            MessagesFile: "compiler:Default.isl"
Name: "catalan";            MessagesFile: "compiler:Languages\Catalan.isl"
Name: "danish";             MessagesFile: "compiler:Languages\Danish.isl"
Name: "german";             MessagesFile: "compiler:Languages\German.isl"
Name: "spanish";            MessagesFile: "compiler:Languages\Spanish.isl"
Name: "french";             MessagesFile: "compiler:Languages\French.isl"
Name: "hungarian";          MessagesFile: "compiler:Languages\Hungarian.isl"
Name: "japanese";           MessagesFile: "compiler:Languages\Japanese.isl"
Name: "russian";            MessagesFile: "compiler:Languages\Russian.isl"
Name: "chinesesimplified";  MessagesFile: "languages\ChineseSimplified.isl"
Name: "chinesetraditional"; MessagesFile: "languages\ChineseTraditional.isl"
Name: "latvian";            MessagesFile: "languages\Latvian.isl"

[CustomMessages]
; 1.0.430 — messaggi del riavvio post-installazione, tradotti in tutte le lingue
; dell'app. RebootPrompt = avviso/domanda (MsgBox); RebootCountdown = testo mostrato
; da Windows durante il conto alla rovescia di shutdown.exe. Vedi [Code]/CurStepChanged.
english.RebootPrompt=Decodium was updated successfully.%n%nFor best performance it is STRONGLY RECOMMENDED to restart your computer now: audio, CAT and caches always start clean and you avoid malfunctions after the update.%n%nDo you want to restart now? (recommended)
italian.RebootPrompt=Decodium è stato aggiornato con successo.%n%nPer un funzionamento ottimale si CONSIGLIA VIVAMENTE di riavviare ora il computer: audio, CAT e cache ripartono sempre puliti e si evitano malfunzionamenti dopo l'aggiornamento.%n%nVuoi riavviare adesso? (consigliato)
catalan.RebootPrompt=Decodium s'ha actualitzat correctament.%n%nPer a un funcionament òptim es RECOMANA ENCARIDAMENT reiniciar ara l'ordinador: l'àudio, el CAT i les memòries cau sempre s'inicien netes i s'eviten errades després de l'actualització.%n%nVoleu reiniciar ara? (recomanat)
danish.RebootPrompt=Decodium blev opdateret.%n%nFor at opnå den bedste ydeevne ANBEFALES det KRAFTIGT at genstarte computeren nu: lyd, CAT og cache starter altid rene, og du undgår fejl efter opdateringen.%n%nVil du genstarte nu? (anbefales)
german.RebootPrompt=Decodium wurde erfolgreich aktualisiert.%n%nFür eine optimale Funktion wird DRINGEND EMPFOHLEN, den Computer jetzt neu zu starten: Audio, CAT und Caches starten immer sauber und Fehlfunktionen nach dem Update werden vermieden.%n%nMöchten Sie jetzt neu starten? (empfohlen)
spanish.RebootPrompt=Decodium se ha actualizado correctamente.%n%nPara un funcionamiento óptimo se RECOMIENDA ENCARECIDAMENTE reiniciar ahora el ordenador: el audio, el CAT y las cachés siempre arrancan limpios y se evitan fallos tras la actualización.%n%n¿Quieres reiniciar ahora? (recomendado)
french.RebootPrompt=Decodium a été mis à jour avec succès.%n%nPour un fonctionnement optimal, il est FORTEMENT RECOMMANDÉ de redémarrer l'ordinateur maintenant : l'audio, le CAT et les caches redémarrent toujours propres et vous évitez les dysfonctionnements après la mise à jour.%n%nVoulez-vous redémarrer maintenant ? (recommandé)
hungarian.RebootPrompt=A Decodium frissítése sikerült.%n%nAz optimális működés érdekében NYOMATÉKOSAN AJÁNLOTT most újraindítani a számítógépet: a hang, a CAT és a gyorsítótárak mindig tisztán indulnak, és elkerülhetők a frissítés utáni hibák.%n%nÚjraindítja most? (ajánlott)
japanese.RebootPrompt=Decodium のアップデートが完了しました。%n%n最適な動作のために、今すぐパソコンを再起動することを強くおすすめします。オーディオ・CAT・キャッシュが常にクリーンな状態で起動し、アップデート後の不具合を防げます。%n%n今すぐ再起動しますか？（推奨）
russian.RebootPrompt=Decodium успешно обновлён.%n%nДля оптимальной работы НАСТОЯТЕЛЬНО РЕКОМЕНДУЕТСЯ перезагрузить компьютер сейчас: звук, CAT и кэш всегда запускаются с чистого состояния, и вы избежите сбоев после обновления.%n%nПерезагрузить сейчас? (рекомендуется)
chinesesimplified.RebootPrompt=Decodium 已成功更新。%n%n为获得最佳性能，强烈建议立即重新启动计算机：音频、CAT 和缓存始终以干净状态启动，可避免更新后出现故障。%n%n现在重新启动吗？（推荐）
chinesetraditional.RebootPrompt=Decodium 已成功更新。%n%n為獲得最佳效能，強烈建議立即重新啟動電腦：音訊、CAT 與快取始終以乾淨狀態啟動，可避免更新後發生故障。%n%n現在重新啟動嗎？（建議）
latvian.RebootPrompt=Decodium tika veiksmīgi atjaunināts.%n%nLai nodrošinātu optimālu darbību, ĻOTI IETEICAMS tagad restartēt datoru: audio, CAT un kešatmiņa vienmēr startē tīri, un jūs izvairīsities no kļūdām pēc atjaunināšanas.%n%nVai restartēt tagad? (ieteicams)
dutch.RebootPrompt=Decodium is succesvol bijgewerkt.%n%nVoor de beste prestaties wordt STERK AANGERADEN om nu de computer opnieuw op te starten: audio, CAT en caches starten altijd schoon en zo voorkomt u storingen na de update.%n%nWilt u nu opnieuw opstarten? (aanbevolen)
english.RebootCountdown=Decodium updated. The PC will restart in 15 seconds. Save your open work.
italian.RebootCountdown=Decodium aggiornato. Il PC verrà riavviato tra 15 secondi. Salva il lavoro aperto.
catalan.RebootCountdown=Decodium actualitzat. L'ordinador es reiniciarà d'aquí a 15 segons. Deseu la feina oberta.
danish.RebootCountdown=Decodium opdateret. Pc'en genstarter om 15 sekunder. Gem dit åbne arbejde.
german.RebootCountdown=Decodium aktualisiert. Der PC wird in 15 Sekunden neu gestartet. Speichern Sie Ihre offene Arbeit.
spanish.RebootCountdown=Decodium actualizado. El PC se reiniciará en 15 segundos. Guarda tu trabajo abierto.
french.RebootCountdown=Decodium mis à jour. Le PC redémarrera dans 15 secondes. Enregistrez votre travail ouvert.
hungarian.RebootCountdown=A Decodium frissítve. A számítógép 15 másodperc múlva újraindul. Mentse a megnyitott munkát.
japanese.RebootCountdown=Decodium を更新しました。15 秒後に PC が再起動します。開いている作業を保存してください。
russian.RebootCountdown=Decodium обновлён. Компьютер перезагрузится через 15 секунд. Сохраните открытую работу.
chinesesimplified.RebootCountdown=Decodium 已更新。计算机将在 15 秒后重新启动。请保存正在进行的工作。
chinesetraditional.RebootCountdown=Decodium 已更新。電腦將在 15 秒後重新啟動。請儲存正在進行的工作。
latvian.RebootCountdown=Decodium atjaunināts. Dators tiks restartēts pēc 15 sekundēm. Saglabājiet atvērto darbu.
dutch.RebootCountdown=Decodium bijgewerkt. De pc wordt over 15 seconden opnieuw opgestart. Sla uw geopende werk op.
; 1.0.482 — disinstallazione senza residui. Le impostazioni vengono sempre rimosse;
; i DATI personali (database QSO, cache, log) solo su conferma esplicita. Il default
; e' NO per non far perdere il log a chi disinstalla per sbaglio. Vedi [Code]/CurUninstallStepChanged.
english.RemoveDataPrompt=Decodium settings have been removed.%n%nDo you also want to delete your personal data: the QSO log database, caches and logs?%n%nChoose NO to keep your QSO log (recommended if you plan to reinstall Decodium).
italian.RemoveDataPrompt=Le impostazioni di Decodium sono state rimosse.%n%nVuoi eliminare anche i tuoi dati personali: database del log QSO, cache e log?%n%nScegli NO per conservare il log dei QSO (consigliato se pensi di reinstallare Decodium).
catalan.RemoveDataPrompt=S'han eliminat els paràmetres de Decodium.%n%nVoleu eliminar també les vostres dades personals: la base de dades del log de QSO, les memòries cau i els registres?%n%nTrieu NO per conservar el log de QSO (recomanat si penseu reinstal·lar Decodium).
danish.RemoveDataPrompt=Decodiums indstillinger er fjernet.%n%nVil du også slette dine personlige data: QSO-logdatabasen, cache og logfiler?%n%nVælg NEJ for at beholde din QSO-log (anbefales, hvis du vil geninstallere Decodium).
german.RemoveDataPrompt=Die Einstellungen von Decodium wurden entfernt.%n%nMöchten Sie auch Ihre persönlichen Daten löschen: die QSO-Log-Datenbank, Caches und Protokolle?%n%nWählen Sie NEIN, um Ihr QSO-Log zu behalten (empfohlen, wenn Sie Decodium neu installieren möchten).
spanish.RemoveDataPrompt=Se han eliminado los ajustes de Decodium.%n%n¿Quieres borrar también tus datos personales: la base de datos del log de QSO, las cachés y los registros?%n%nElige NO para conservar tu log de QSO (recomendado si piensas reinstalar Decodium).
french.RemoveDataPrompt=Les réglages de Decodium ont été supprimés.%n%nVoulez-vous aussi supprimer vos données personnelles : la base de données du log QSO, les caches et les journaux ?%n%nChoisissez NON pour conserver votre log QSO (recommandé si vous prévoyez de réinstaller Decodium).
hungarian.RemoveDataPrompt=A Decodium beállításai eltávolítva.%n%nTörölni szeretné a személyes adatait is: a QSO-napló adatbázisát, a gyorsítótárakat és a naplókat?%n%nVálassza a NEM lehetőséget a QSO-napló megtartásához (ajánlott, ha újratelepíti a Decodiumot).
japanese.RemoveDataPrompt=Decodium の設定を削除しました。%n%n個人データ（QSO ログのデータベース・キャッシュ・ログ）も削除しますか？%n%nQSO ログを残す場合は「いいえ」を選んでください（Decodium を再インストールする予定がある場合におすすめします）。
russian.RemoveDataPrompt=Настройки Decodium удалены.%n%nУдалить также ваши личные данные: базу данных журнала QSO, кэш и логи?%n%nВыберите НЕТ, чтобы сохранить журнал QSO (рекомендуется, если вы планируете переустановить Decodium).
chinesesimplified.RemoveDataPrompt=Decodium 的设置已删除。%n%n是否同时删除您的个人数据：QSO 日志数据库、缓存和日志？%n%n选择“否”可保留您的 QSO 日志（如果打算重新安装 Decodium，建议保留）。
chinesetraditional.RemoveDataPrompt=Decodium 的設定已刪除。%n%n是否同時刪除您的個人資料：QSO 日誌資料庫、快取與日誌？%n%n選擇「否」可保留您的 QSO 日誌（若打算重新安裝 Decodium，建議保留）。
latvian.RemoveDataPrompt=Decodium iestatījumi ir noņemti.%n%nVai vēlaties dzēst arī savus personiskos datus: QSO žurnāla datubāzi, kešatmiņu un žurnālus?%n%nIzvēlieties NĒ, lai saglabātu savu QSO žurnālu (ieteicams, ja plānojat Decodium instalēt no jauna).
dutch.RemoveDataPrompt=De instellingen van Decodium zijn verwijderd.%n%nWilt u ook uw persoonlijke gegevens verwijderen: de QSO-logdatabase, caches en logbestanden?%n%nKies NEE om uw QSO-log te behouden (aanbevolen als u Decodium opnieuw wilt installeren).

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

; 1.0.497 Fase 3 alleggerimento — installer modulare. NB: Live Map, Full
; Spectrum, FT2-Link e i modi extra sono COMPILATI nell'exe → non rimovibili
; come file (per quelli vale la Modalità PC lento a runtime). I componenti qui
; rimuovono solo ASSET: suoni e pacchetti-lingua aggiuntivi. Inglese e italiano
; (lingue base del fork) restano sempre installati.
[Types]
Name: "full";   Description: "Completa (tutte le 14 lingue e i suoni)"
Name: "lite";   Description: "Leggera per PC lenti (solo inglese/italiano, senza suoni)"
Name: "custom"; Description: "Personalizzata"; Flags: iscustom

[Components]
Name: "sounds"; Description: "Suoni di avviso (CQ, chiamata, band, ecc. ~3,5 MB)"; Types: full custom
Name: "langs";  Description: "Lingue aggiuntive oltre inglese/italiano (~10 MB)"; Types: full custom

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
; 1.0.497 — TAGLIO PESO MORTO (~53MB): 6 codec immagine/video trascinati dal
; deploy MSYS2 ma ORFANI (verificato con tabella import: nessun file
; dell'install li carica; imageformats plugin = solo gif/ico/jpeg/svg; 0
; riferimenti nell'exe). Stesso caso di ffmpeg (--no-ffmpeg). Nessun cambio
; funzionale: Decodium usa PNG/JPEG/GIF/ICO/SVG, non HEVC/AV1/VP9/JPEG-XL/rsvg.
; 1.0.497 Fase 3 — bundle base (SEMPRE installato): esclude anche suoni e
; lingue-extra, che vengono ri-aggiunti sotto come componenti opzionali. Le
; lingue base inglese/italiano (decodium_en/it.qm) NON sono escluse qui → sempre
; presenti; solo i .qm delle altre 12 lingue e i qt_*.qm non-base sono opzionali.
Source: "{#BuildDir}\*"; DestDir: "{app}"; \
  Excludes: "wsjtx*,CMakeFiles\*,deploy\*,deploy_staging\*,map65\*,qmap\*,Testing\*,NVIDIA Corporation\*,.claude\*,build_mingw64\*,CMakeCache.txt,cmake_install.cmake,CTestTestfile.cmake,Makefile,build.ninja,.ninja_*,compile_commands.json,*.obj,*.d,*.a,*.rc,*_autogen\*,.qt\*,tests\*,tools\*,bundle_fixup\*,qrc_*.cpp,qrc_*.cpp.depends,*.qrc.depends,*.cmake,VersionInfo_*.h,VersionResource_*.rc,DartConfiguration.tcl,CPack*.cmake,libx265-215.dll,libaom.dll,librsvg-2-2.dll,libjxl.dll,libvpx-1.dll,libgdk_pixbuf-2.0-0.dll,libcairo-2.dll,libcairo-gobject-2.dll,libpango-1.0-0.dll,libpangocairo-1.0-0.dll,libpangoft2-1.0-0.dll,libpangowin32-1.0-0.dll,libpixman-1-0.dll,libdatrie-1.dll,libthai-0.dll,libfribidi-0.dll,sounds\*,decodium_ca.qm,decodium_da.qm,decodium_de.qm,decodium_es.qm,decodium_fr.qm,decodium_hu.qm,decodium_ja.qm,decodium_lv.qm,decodium_nl.qm,decodium_ru.qm,decodium_zh.qm,decodium_zh_TW.qm,translations\qt_*.qm"; \
  Flags: ignoreversion recursesubdirs

; Lingue base (sempre): i qt_*.qm inglese/italiano ri-aggiunti (gli altri sono nel componente 'langs')
Source: "{#BuildDir}\translations\qt_en.qm"; DestDir: "{app}\translations"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#BuildDir}\translations\qt_it.qm"; DestDir: "{app}\translations"; Flags: ignoreversion skipifsourcedoesntexist

; COMPONENTE 'sounds' — suoni di avviso (~3,5 MB)
Source: "{#BuildDir}\sounds\*"; DestDir: "{app}\sounds"; Components: sounds; Flags: ignoreversion recursesubdirs skipifsourcedoesntexist

; COMPONENTE 'langs' — lingue aggiuntive: .qm Decodium delle altre 12 lingue + qt_*.qm non-base
Source: "{#BuildDir}\decodium_ca.qm";    DestDir: "{app}"; Components: langs; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#BuildDir}\decodium_da.qm";    DestDir: "{app}"; Components: langs; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#BuildDir}\decodium_de.qm";    DestDir: "{app}"; Components: langs; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#BuildDir}\decodium_es.qm";    DestDir: "{app}"; Components: langs; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#BuildDir}\decodium_fr.qm";    DestDir: "{app}"; Components: langs; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#BuildDir}\decodium_hu.qm";    DestDir: "{app}"; Components: langs; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#BuildDir}\decodium_ja.qm";    DestDir: "{app}"; Components: langs; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#BuildDir}\decodium_lv.qm";    DestDir: "{app}"; Components: langs; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#BuildDir}\decodium_nl.qm";    DestDir: "{app}"; Components: langs; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#BuildDir}\decodium_ru.qm";    DestDir: "{app}"; Components: langs; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#BuildDir}\decodium_zh.qm";    DestDir: "{app}"; Components: langs; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#BuildDir}\decodium_zh_TW.qm"; DestDir: "{app}"; Components: langs; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#BuildDir}\translations\qt_*.qm"; DestDir: "{app}\translations"; Excludes: "qt_en.qm,qt_it.qm"; Components: langs; Flags: ignoreversion skipifsourcedoesntexist

; File licenza fuori dal bundle
Source: "{#SourceRoot}\COPYING"; DestDir: "{app}"; DestName: "COPYING.txt"; Flags: ignoreversion skipifsourcedoesntexist

[Icons]
Name: "{group}\{#AppName}";              Filename: "{app}\{#AppExeName}"; IconFilename: "{app}\{#AppExeName}"
Name: "{group}\Disinstalla {#AppName}";  Filename: "{uninstallexe}"
Name: "{autodesktop}\{#AppName}";        Filename: "{app}\{#AppExeName}"; IconFilename: "{app}\{#AppExeName}"; Tasks: desktopicon

; 1.0.482 — [Registry] rimossa di proposito: Decodium non scrive PIU' nulla di suo
; nel registro (richiesta utente). Le vecchie voci InstallPath/Version sotto
; HKCU\Software\IU8LMC\Decodium non erano lette da nessuna parte del codice, e
; ricrearle avrebbe vanificato la bonifica fatta da DecodiumStorageMigration.
; L'unica chiave che resta e' quella di Windows in ...\CurrentVersion\Uninstall,
; gestita da Inno stesso: e' il meccanismo di "App e funzionalita'", non roba nostra.

[Run]
; 1.0.430 — avvio automatico DISABILITATO: a fine installazione interattiva viene
; PROPOSTO (non forzato) il riavvio del PC con avviso consigliato (vedi [Code],
; CurStepChanged). Lanciare l'app qui e poi proporre il reboot sarebbe confuso:
; l'utente riapre Decodium dall'icona desktop / menu Start quando preferisce. Per
; tornare al comportamento classico (avvio app subito) riabilita la riga sottostante.
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
  (il reboot viene proposto a fine installazione). L'uninstaller di Inno si auto-copia
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

{ 1.0.482 — Disinstallazione senza residui (richiesta utente: niente valori zombie
  che sopravvivono alla disinstallazione, come l'uiSpectrumHeight=2808 che rendeva
  inservibile la barra del waterfall).

  ATTENZIONE, TRAPPOLA: questa procedura gira ANCHE durante l'auto-disinstallazione
  che UninstallPreviousVersion esegue a OGNI aggiornamento (con /VERYSILENT). Senza
  la guardia UninstallSilent ogni update azzererebbe impostazioni e log QSO di tutti
  gli utenti. Percio': silenzioso = aggiornamento = non si tocca NULLA.

  Le impostazioni (.ini) vengono rimosse sempre; i DATI personali (database QSO,
  cache, log) solo se l'utente conferma. Default del MsgBox = NO. }
procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  ResultCode: Integer;
begin
  { 1.0.491 — cintura+bretelle a CloseApplications: PRIMA di rimuovere i file
    chiudi a forza Decodium e i suoi processi figli (anche se freezato), cosi'
    nessun file resta bloccato e l'uninstall non fallisce. Vale ANCHE
    nell'auto-uninstall silenzioso degli update (per questo NON e' sotto la
    guardia UninstallSilent). }
  if CurUninstallStep = usUninstall then
  begin
    Exec(ExpandConstant('{sys}\taskkill.exe'), '/F /IM {#AppExeName} /T',
         '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
    Exit;
  end;

  if CurUninstallStep <> usPostUninstall then
    Exit;
  if UninstallSilent then
    Exit;

  { Impostazioni: sono la fonte dei valori zombie -> via sempre. }
  DeleteFile(ExpandConstant('{userappdata}\Decodium\Decodium3.ini'));
  DeleteFile(ExpandConstant('{userappdata}\Decodium\Decodium.ini'));

  { Registro legacy (pre-1.0.482 le impostazioni stavano qui). }
  RegDeleteKeyIncludingSubkeys(HKCU, 'Software\Decodium');
  RegDeleteKeyIncludingSubkeys(HKCU, 'Software\IU8LMC');

  { Dati personali: solo su conferma esplicita. }
  if MsgBox(ExpandConstant('{cm:RemoveDataPrompt}'), mbConfirmation,
            MB_YESNO or MB_DEFBUTTON2) = IDYES then
  begin
    DelTree(ExpandConstant('{userappdata}\Decodium'), True, True, True);
    DelTree(ExpandConstant('{localappdata}\Decodium'), True, True, True);
    { layout legacy, se la migrazione non era mai girata }
    DelTree(ExpandConstant('{userappdata}\IU8LMC'), True, True, True);
    DelTree(ExpandConstant('{localappdata}\IU8LMC'), True, True, True);
    { log sparsi nella radice di %LOCALAPPDATA% (path del log corretto in 1.0.482) }
    DelTree(ExpandConstant('{localappdata}\decodium_*.log'), False, True, False);
    DelTree(ExpandConstant('{localappdata}\decodium_diagnostic*.bak'), False, True, False);
  end;
end;

procedure CurStepChanged(CurStep: TSetupStep);
var
  ResultCode: Integer;
begin
  { Prima di copiare i file: disinstalla automaticamente la versione precedente. }
  if CurStep = ssInstall then
    UninstallPreviousVersion;

  { 1.0.430 — Riavvio PC OPZIONALE a fine installazione interattiva (era forzato
    nella 1.0.428). Mostriamo un avviso che RACCOMANDA VIVAMENTE il riavvio (audio,
    CAT e cache puliti); il pulsante predefinito e' "Si". Se l'utente accetta,
    riavvio con conto alla rovescia 15s; se rifiuta, nessun riavvio (riapre Decodium
    dall'icona quando vuole). Saltato in /SILENT e /VERYSILENT per non sorprendere
    flussi automatici/CI. shutdown /r funziona anche per utente non-admin
    (sessione interattiva = SeShutdownPrivilege). I testi (RebootPrompt/RebootCountdown)
    sono localizzati in tutte le 13 lingue via [CustomMessages] e seguono la lingua
    scelta nell'installer. }
  if (CurStep = ssDone) and (not WizardSilent) then
  begin
    if MsgBox(ExpandConstant('{cm:RebootPrompt}'),
              mbConfirmation, MB_YESNO or MB_DEFBUTTON1) = IDYES then
      Exec(ExpandConstant('{sys}\shutdown.exe'),
           '/r /t 15 /c "' + ExpandConstant('{cm:RebootCountdown}') + '"',
           '', SW_HIDE, ewNoWait, ResultCode);
  end;
end;
