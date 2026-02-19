#define MyAppName "SBaGenX"
#define MyAppVersion "2.0.0"
#define MyAppPublisher "Lech Madrzyk"
#define MyAppURL "https://github.com/lm7137/SBaGenX"
#define MyAppExeName "sbagenx.exe"
#define MyAppIcon "assets\sbagenx.ico"
#define MyAppAssocName "SBaGenX Sequence File"
#define MyAppAssocExt ".sbg"
#define MyAppAssocKey StringChange(MyAppAssocName, " ", "") + MyAppAssocExt
#define MyAppUserDocsDir "{userdocs}\SBaGenX"

[Setup]
; NOTE: The value of AppId uniquely identifies this application.
AppId={{F4E7F182-5437-4F54-A4FE-B8F2891AE7E2}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
OutputDir=dist
OutputBaseFilename=sbagenx-windows-setup
Compression=lzma2/fast
LZMAUseSeparateProcess=yes
LZMANumBlockThreads=1
SolidCompression=yes
ArchitecturesAllowed=x86 x64 arm64
MinVersion=6.1.7601
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=commandline
SetupIconFile={#MyAppIcon}
UninstallDisplayIcon={app}\{#MyAppExeName}
WizardStyle=modern
LicenseFile=COPYING.txt
DisableWelcomePage=no
ChangesAssociations=yes

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Messages]
; Override some default messages to ensure they're in English
BeveledLabel=Installation
ButtonBack=< &Back
ButtonCancel=Cancel
ButtonFinish=&Finish
ButtonInstall=&Install
ButtonNext=&Next >
ClickNext=Click Next to continue, or Cancel to exit Setup.
WizardLicense=License Agreement
LicenseLabel={#MyAppName} license agreement.
LicenseAccepted=I &accept the agreement
LicenseNotAccepted=I &do not accept the agreement

[CustomMessages]
NoticeCaption=Important Notice
NoticeDescription=Please read this important notice before continuing:

[Tasks]
Name: "associatewithfiles"; Description: "Associate .sbg files with {#MyAppName}"; GroupDescription: "File associations:";
Name: "addtopath"; Description: "Add {#MyAppName} to PATH environment variable"; GroupDescription: "System integration:"; Flags: unchecked

[Files]
; Include both 32-bit and 64-bit versions
Source: "dist\sbagenx-win32.exe"; DestDir: "{app}"; DestName: "sbagenx-win32.exe"; Flags: ignoreversion
Source: "dist\sbagenx-win64.exe"; DestDir: "{app}"; DestName: "sbagenx-win64.exe"; Flags: ignoreversion
; Bundled ambient mix tracks (kept beside sbagenx.exe for direct -m usage)
Source: "assets\river1.ogg"; DestDir: "{app}"; DestName: "river1.ogg"; Flags: ignoreversion
Source: "assets\river2.ogg"; DestDir: "{app}"; DestName: "river2.ogg"; Flags: ignoreversion
; Optional encoder runtime DLL bundles (added by windows-build-sbagenx.sh when available)
Source: "dist\libsndfile-win32.dll"; DestDir: "{app}"; DestName: "libsndfile-win32.dll"; Flags: ignoreversion skipifsourcedoesntexist
Source: "dist\libsndfile-win64.dll"; DestDir: "{app}"; DestName: "libsndfile-win64.dll"; Flags: ignoreversion skipifsourcedoesntexist
Source: "dist\libmp3lame-win32.dll"; DestDir: "{app}"; DestName: "libmp3lame-win32.dll"; Flags: ignoreversion skipifsourcedoesntexist
Source: "dist\libmp3lame-win64.dll"; DestDir: "{app}"; DestName: "libmp3lame-win64.dll"; Flags: ignoreversion skipifsourcedoesntexist
Source: "dist\libFLAC-win32.dll"; DestDir: "{app}"; DestName: "libFLAC-win32.dll"; Flags: ignoreversion skipifsourcedoesntexist
Source: "dist\libFLAC-win64.dll"; DestDir: "{app}"; DestName: "libFLAC-win64.dll"; Flags: ignoreversion skipifsourcedoesntexist
Source: "dist\libmpg123-0-win32.dll"; DestDir: "{app}"; DestName: "libmpg123-0-win32.dll"; Flags: ignoreversion skipifsourcedoesntexist
Source: "dist\libmpg123-0-win64.dll"; DestDir: "{app}"; DestName: "libmpg123-0-win64.dll"; Flags: ignoreversion skipifsourcedoesntexist
Source: "dist\libogg-0-win32.dll"; DestDir: "{app}"; DestName: "libogg-0-win32.dll"; Flags: ignoreversion skipifsourcedoesntexist
Source: "dist\libogg-0-win64.dll"; DestDir: "{app}"; DestName: "libogg-0-win64.dll"; Flags: ignoreversion skipifsourcedoesntexist
Source: "dist\libopus-0-win32.dll"; DestDir: "{app}"; DestName: "libopus-0-win32.dll"; Flags: ignoreversion skipifsourcedoesntexist
Source: "dist\libopus-0-win64.dll"; DestDir: "{app}"; DestName: "libopus-0-win64.dll"; Flags: ignoreversion skipifsourcedoesntexist
Source: "dist\libvorbis-0-win32.dll"; DestDir: "{app}"; DestName: "libvorbis-0-win32.dll"; Flags: ignoreversion skipifsourcedoesntexist
Source: "dist\libvorbis-0-win64.dll"; DestDir: "{app}"; DestName: "libvorbis-0-win64.dll"; Flags: ignoreversion skipifsourcedoesntexist
Source: "dist\libvorbisenc-2-win32.dll"; DestDir: "{app}"; DestName: "libvorbisenc-2-win32.dll"; Flags: ignoreversion skipifsourcedoesntexist
Source: "dist\libvorbisenc-2-win64.dll"; DestDir: "{app}"; DestName: "libvorbisenc-2-win64.dll"; Flags: ignoreversion skipifsourcedoesntexist
Source: "dist\libwinpthread-1-win32.dll"; DestDir: "{app}"; DestName: "libwinpthread-1-win32.dll"; Flags: ignoreversion skipifsourcedoesntexist
Source: "dist\libwinpthread-1-win64.dll"; DestDir: "{app}"; DestName: "libwinpthread-1-win64.dll"; Flags: ignoreversion skipifsourcedoesntexist
Source: "dist\libgcc_s_dw2-1-win32.dll"; DestDir: "{app}"; DestName: "libgcc_s_dw2-1-win32.dll"; Flags: ignoreversion skipifsourcedoesntexist
; Documentation
Source: "COPYING.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "NOTICE.txt"; DestDir: "{app}"; Flags: ignoreversion dontcopy
Source: "licenses\*"; DestDir: "{app}\licenses"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "docs\*"; DestDir: "{app}\docs"; Flags: ignoreversion recursesubdirs createallsubdirs
; Example files
Source: "examples\*"; DestDir: "{app}\examples"; Flags: ignoreversion recursesubdirs createallsubdirs
; Scripts directory
Source: "scripts\*"; DestDir: "{app}\scripts"; Flags: ignoreversion recursesubdirs createallsubdirs
; Changelog
Source: "ChangeLog.txt"; DestDir: "{app}"; Flags: ignoreversion
; Top-level Markdown docs (preserved as Markdown)
Source: "README.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "USAGE.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "RESEARCH.md"; DestDir: "{app}"; Flags: ignoreversion
; Documentation files in user's Documents folder
Source: "docs\*"; DestDir: "{#MyAppUserDocsDir}\Documentation"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "licenses\*"; DestDir: "{#MyAppUserDocsDir}\Licenses"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "examples\*"; DestDir: "{#MyAppUserDocsDir}\Examples"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "COPYING.txt"; DestDir: "{#MyAppUserDocsDir}"; DestName: "License.txt"; Flags: ignoreversion
Source: "NOTICE.txt"; DestDir: "{#MyAppUserDocsDir}"; DestName: "Notice.txt"; Flags: ignoreversion
Source: "README.md"; DestDir: "{#MyAppUserDocsDir}"; DestName: "README.md"; Flags: ignoreversion
Source: "USAGE.md"; DestDir: "{#MyAppUserDocsDir}"; DestName: "USAGE.md"; Flags: ignoreversion
Source: "RESEARCH.md"; DestDir: "{#MyAppUserDocsDir}"; DestName: "RESEARCH.md"; Flags: ignoreversion

[Dirs]
Name: "{#MyAppUserDocsDir}"; Flags: uninsalwaysuninstall

[Icons]
; Program shortcuts
Name: "{group}\{cm:UninstallProgram,{#MyAppName}}"; Filename: "{uninstallexe}"

; Desktop shortcut to Documents folder
Name: "{autodesktop}\SBaGenX Files"; Filename: "{#MyAppUserDocsDir}"

[Registry]
; File association for .sbg files (User Level)
Root: HKCU; Subkey: "Software\Classes\{#MyAppAssocExt}\OpenWithProgids"; ValueType: string; ValueName: "{#MyAppAssocKey}"; ValueData: ""; Flags: uninsdeletevalue; Tasks: associatewithfiles
Root: HKCU; Subkey: "Software\Classes\{#MyAppAssocKey}"; ValueType: string; ValueName: ""; ValueData: "{#MyAppAssocName}"; Flags: uninsdeletekey; Tasks: associatewithfiles
Root: HKCU; Subkey: "Software\Classes\{#MyAppAssocKey}\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: associatewithfiles
Root: HKCU; Subkey: "Software\Classes\{#MyAppAssocKey}\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: associatewithfiles

; Context menu for .sbg files - Edit option
Root: HKCU; Subkey: "Software\Classes\{#MyAppAssocKey}\shell\edit"; ValueType: string; ValueName: ""; ValueData: "Edit sequence file"; Flags: uninsdeletekey; Tasks: associatewithfiles
Root: HKCU; Subkey: "Software\Classes\{#MyAppAssocKey}\shell\edit"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\{#MyAppExeName}"""; Flags: uninsdeletekey; Tasks: associatewithfiles
Root: HKCU; Subkey: "Software\Classes\{#MyAppAssocKey}\shell\edit\command"; ValueType: string; ValueName: ""; ValueData: "notepad.exe ""%1"""; Flags: uninsdeletekey; Tasks: associatewithfiles

; Context menu for .sbg files - Write to WAV option
Root: HKCU; Subkey: "Software\Classes\{#MyAppAssocKey}\shell\writetoWAV"; ValueType: string; ValueName: ""; ValueData: "Write file to WAV"; Flags: uninsdeletekey; Tasks: associatewithfiles
Root: HKCU; Subkey: "Software\Classes\{#MyAppAssocKey}\shell\writetoWAV"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\{#MyAppExeName}"""; Flags: uninsdeletekey; Tasks: associatewithfiles
Root: HKCU; Subkey: "Software\Classes\{#MyAppAssocKey}\shell\writetoWAV\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" -Wo ""output.wav"" ""%1"""; Flags: uninsdeletekey; Tasks: associatewithfiles

; Context menu for .sbg files - Write to WAV option (30 minutes)
Root: HKCU; Subkey: "Software\Classes\{#MyAppAssocKey}\shell\writetoWAV30"; ValueType: string; ValueName: ""; ValueData: "Write file to WAV (30 minutes)"; Flags: uninsdeletekey; Tasks: associatewithfiles
Root: HKCU; Subkey: "Software\Classes\{#MyAppAssocKey}\shell\writetoWAV30"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\{#MyAppExeName}"""; Flags: uninsdeletekey; Tasks: associatewithfiles
Root: HKCU; Subkey: "Software\Classes\{#MyAppAssocKey}\shell\writetoWAV30\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" -L 00:30:00 -Wo ""output.wav"" ""%1"""; Flags: uninsdeletekey; Tasks: associatewithfiles

; Force Windows to refresh shell icons
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Explorer\FileExts\{#MyAppAssocExt}"; ValueType: none; ValueName: ""; Flags: deletekey; Tasks: associatewithfiles

; Add installation directory to user PATH
Root: HKCU; Subkey: "Environment"; ValueType: expandsz; ValueName: "PATH"; ValueData: "{olddata};{app}"; Check: NeedsAddPath('{app}'); Tasks: addtopath

[Run]
Filename: "{win}\explorer.exe"; Parameters: """{#MyAppUserDocsDir}"""; Description: "Open SBaGenX folder"; Flags: postinstall nowait skipifsilent shellexec

[Code]
var
  NoticePage: TOutputMsgMemoWizardPage;

function NeedsAddPath(Param: string): boolean;
var
  OrigPath: string;
begin
  if RegQueryStringValue(HKEY_CURRENT_USER, 'Environment', 'PATH', OrigPath) then
  begin
    { look for the path with leading and trailing semicolon }
    { Pos() returns 0 if not found }
    Result := Pos(';' + Param + ';', ';' + OrigPath + ';') = 0;
  end
  else
  begin
    Result := True;
  end;
end;

procedure RemovePath(Path: string);
var
  Paths: string;
  P: Integer;
begin
  if RegQueryStringValue(HKEY_CURRENT_USER, 'Environment', 'PATH', Paths) then
  begin
    { Remove path from the string }
    P := Pos(';' + Path + ';', ';' + Paths + ';');
    if P > 0 then
    begin
      Delete(Paths, P - 1, Length(Path) + 1);
      RegWriteStringValue(HKEY_CURRENT_USER, 'Environment', 'PATH', Paths);
    end;
  end;
end;

procedure InitializeWizard;
var
  NoticeLines: TStringList;
  NoticeText: AnsiString;
begin
  { Create the notice page }
  NoticePage := CreateOutputMsgMemoPage(wpWelcome,
    ExpandConstant('{cm:NoticeCaption}'),
    ExpandConstant('{cm:NoticeDescription}'),
    '', '');

  { Load and display NOTICE.txt content }
  NoticeLines := TStringList.Create;
  try
    ExtractTemporaryFile('NOTICE.txt');
    NoticeLines.LoadFromFile(ExpandConstant('{tmp}\NOTICE.txt'));
    NoticeText := NoticeLines.Text;
    NoticePage.RichEditViewer.Lines.Text := NoticeText;
  finally
    NoticeLines.Free;
  end;
end;

function GetUninstallString(): String;
var
  sUnInstPath: String;
  sUnInstallString: String;
begin
  sUnInstPath := ExpandConstant('Software\Microsoft\Windows\CurrentVersion\Uninstall\{#emit SetupSetting("AppId")}_is1');
  sUnInstallString := '';
  if not RegQueryStringValue(HKLM, sUnInstPath, 'UninstallString', sUnInstallString) then
    RegQueryStringValue(HKCU, sUnInstPath, 'UninstallString', sUnInstallString);
  Result := sUnInstallString;
end;

function IsUpgrade(): Boolean;
begin
  Result := (GetUninstallString() <> '');
end;

function InitializeSetup(): Boolean;
var
  V: Integer;
  iResultCode: Integer;
  sUnInstallString: String;
begin
  Result := True;
  
  if IsUpgrade() then
  begin
    V := MsgBox('A previous version of ' + '{#MyAppName}' + ' was detected. Would you like to uninstall it before continuing?', mbInformation, MB_YESNO);
    if V = IDYES then
    begin
      sUnInstallString := GetUninstallString();
      sUnInstallString := RemoveQuotes(sUnInstallString);
      Exec(ExpandConstant(sUnInstallString), '/SILENT /NORESTART /SUPPRESSMSGBOXES','', SW_HIDE, ewWaitUntilTerminated, iResultCode);
      Result := True;
    end
    else
      Result := False;
  end;
end;

procedure PromoteBundledRuntimeDll(const ArchTag: String; const SourceBase: String; const DestName: String);
var
  SourceFile: String;
  DestFile: String;
begin
  SourceFile := ExpandConstant('{app}\' + SourceBase + '-' + ArchTag + '.dll');
  DestFile := ExpandConstant('{app}\' + DestName);
  if FileExists(SourceFile) then
    FileCopy(SourceFile, DestFile, False);
end;

procedure DeleteBundledRuntimeDllCopies(const SourceBase: String);
begin
  DeleteFile(ExpandConstant('{app}\' + SourceBase + '-win32.dll'));
  DeleteFile(ExpandConstant('{app}\' + SourceBase + '-win64.dll'));
end;

procedure CurStepChanged(CurStep: TSetupStep);
var
  SourceFile: String;
  DestFile: String;
  ArchTag: String;
begin
  if CurStep = ssPostInstall then
  begin
    { Check system architecture and copy appropriate executable }
    if IsWin64 then
    begin
      ArchTag := 'win64';
      SourceFile := ExpandConstant('{app}\sbagenx-win64.exe');
      DestFile := ExpandConstant('{app}\sbagenx.exe');
    end
    else
    begin
      ArchTag := 'win32';
      SourceFile := ExpandConstant('{app}\sbagenx-win32.exe');
      DestFile := ExpandConstant('{app}\sbagenx.exe');
    end;

    { Copy the appropriate executable }
    if FileCopy(SourceFile, DestFile, False) then
    begin
      { Delete the original files }
      DeleteFile(ExpandConstant('{app}\sbagenx-win32.exe'));
      DeleteFile(ExpandConstant('{app}\sbagenx-win64.exe'));
    end;

    { Promote bundled runtime DLLs to canonical names used by the loader }
    PromoteBundledRuntimeDll(ArchTag, 'libsndfile', 'libsndfile-1.dll');
    PromoteBundledRuntimeDll(ArchTag, 'libmp3lame', 'libmp3lame-0.dll');
    PromoteBundledRuntimeDll(ArchTag, 'libFLAC', 'libFLAC.dll');
    PromoteBundledRuntimeDll(ArchTag, 'libmpg123-0', 'libmpg123-0.dll');
    PromoteBundledRuntimeDll(ArchTag, 'libogg-0', 'libogg-0.dll');
    PromoteBundledRuntimeDll(ArchTag, 'libopus-0', 'libopus-0.dll');
    PromoteBundledRuntimeDll(ArchTag, 'libvorbis-0', 'libvorbis-0.dll');
    PromoteBundledRuntimeDll(ArchTag, 'libvorbisenc-2', 'libvorbisenc-2.dll');
    PromoteBundledRuntimeDll(ArchTag, 'libwinpthread-1', 'libwinpthread-1.dll');
    if ArchTag = 'win32' then
      PromoteBundledRuntimeDll(ArchTag, 'libgcc_s_dw2-1', 'libgcc_s_dw2-1.dll');

    { Remove architecture-tagged DLL copies after promotion }
    DeleteBundledRuntimeDllCopies('libsndfile');
    DeleteBundledRuntimeDllCopies('libmp3lame');
    DeleteBundledRuntimeDllCopies('libFLAC');
    DeleteBundledRuntimeDllCopies('libmpg123-0');
    DeleteBundledRuntimeDllCopies('libogg-0');
    DeleteBundledRuntimeDllCopies('libopus-0');
    DeleteBundledRuntimeDllCopies('libvorbis-0');
    DeleteBundledRuntimeDllCopies('libvorbisenc-2');
    DeleteBundledRuntimeDllCopies('libwinpthread-1');
    DeleteBundledRuntimeDllCopies('libgcc_s_dw2-1');

    { Notify system about PATH changes }
    if WizardIsTaskSelected('addtopath') then
    begin
      { Broadcast WM_SETTINGCHANGE message }
      MsgBox('SBaGenX has been added to the PATH environment variable. ' +
             'You may need to restart running applications for them to ' +
             'recognize the change.', mbInformation, MB_OK);
    end;
  end;
end;

{ Handle uninstallation - Remove dynamically created files }
procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if CurUninstallStep = usPostUninstall then
  begin
    { Remove the dynamically created sbagenx.exe file }
    DeleteFile(ExpandConstant('{app}\sbagenx.exe'));
    { Remove dynamically promoted runtime encoder DLLs }
    DeleteFile(ExpandConstant('{app}\libsndfile-1.dll'));
    DeleteFile(ExpandConstant('{app}\libmp3lame-0.dll'));
    DeleteFile(ExpandConstant('{app}\libFLAC.dll'));
    DeleteFile(ExpandConstant('{app}\libmpg123-0.dll'));
    DeleteFile(ExpandConstant('{app}\libogg-0.dll'));
    DeleteFile(ExpandConstant('{app}\libopus-0.dll'));
    DeleteFile(ExpandConstant('{app}\libvorbis-0.dll'));
    DeleteFile(ExpandConstant('{app}\libvorbisenc-2.dll'));
    DeleteFile(ExpandConstant('{app}\libwinpthread-1.dll'));
    DeleteFile(ExpandConstant('{app}\libgcc_s_dw2-1.dll'));
  end;
end; 
