#define SourceDir "..\"
#define AppName "AIMP Control Plugin"
#define SrcApp "temp_build\Release\aimp_control_plugin.dll"
; use absolute path as GetFileVersion() argument since there is problem with relative path when we execute this script from command line.
#define FileVerStr GetFileVersion(SourcePath + "\" + SourceDir + SrcApp)
#define StripBuild(VerStr) Copy(VerStr, 1, RPos(".", VerStr)-1)
#define AppVerStr StripBuild(FileVerStr)
#define PluginWorkDirectoryName "Control Plugin"
#define AimpPluginsDirectoryName "Plugins"

[Setup]

SourceDir={#SourceDir}
AppName={#AppName}
AppVersion={#AppVerStr}
AppVerName={#AppName} {#AppVerStr}
UninstallDisplayName={#AppName} {#AppVerStr}
UninstallFilesDir={app}\{#PluginWorkDirectoryName}
VersionInfoVersion={#FileVerStr}
VersionInfoTextVersion={#AppVerStr}
VersionInfoDescription=AIMP player plugin. Provides network access to AIMP player.
VersionInfoProductName={#AppName}
VersionInfoProductVersion={#FileVerStr}
OutputBaseFilename=aimp_control_plugin-{#FileVerStr}-setup

; NOTE: The value of AppId uniquely identifies this application.
; Do not use the same AppId value in installers for other applications.
; (To generate a new GUID, click Tools | Generate GUID inside the IDE.)
AppId={{F171581D-00CD-4E77-8982-B1B68FDCAAFA}
AppPublisher=Alexey Ivanov
AppCopyright=Copyright © 2012 Alexey Ivanov
AppPublisherURL=http://code.google.com/p/aimp-control-plugin/
AppSupportURL=http://code.google.com/p/aimp-control-plugin/w/list
AppUpdatesURL=http://code.google.com/p/aimp-control-plugin/downloads/list

DefaultDirName={#AimpPluginsDirectoryName}
DefaultGroupName={#AppName}
DisableProgramGroupPage=true
OutputDir=temp_build\Release\distrib\
SetupIconFile=inno_setup_data\icons\icon_aimp3_32x32.ico
Compression=lzma/ultra
SolidCompression=true
InternalCompressLevel=ultra
PrivilegesRequired=admin
DirExistsWarning=no
AllowUNCPath=false
AppendDefaultDirName=true

[Languages]
Name: english; MessagesFile: inno_setup_data\English.isl; LicenseFile: Lisense-English.txt; InfoAfterFile: inno_setup_data\InfoAfterInstall-English.txt
Name: russian; MessagesFile: inno_setup_data\Russian.isl; LicenseFile: Lisense-Russian.txt; InfoAfterFile: inno_setup_data\InfoAfterInstall-Russian.txt

[Files]
Source: {#SrcApp}; DestDir: {app}; Flags: ignoreversion
Source: temp_build\Release\htdocs\*; DestDir: {code:GetBrowserScriptsDir}; Flags: ignoreversion recursesubdirs createallsubdirs
; NOTE: Don't use "Flags: ignoreversion" on any shared system files
Source: inno_setup_data\default_settings.dat; DestDir: {code:GetPluginWorkDir}; DestName: settings.dat; Flags: onlyifdoesntexist; AfterInstall: "AfterInstallSettingsFile( ExpandConstant('{code:GetPluginWorkDir}\settings.dat') )"; 

[Registry]
; etc.

[Code]
var
  BrowserScriptsDirPage: TInputDirWizardPage;
  AimpVersionSelectionPage: TInputOptionWizardPage;
  SettingsFileDestination: String;

procedure InitializeWizard;
begin
  { Set AIMP player version, used to determine default path of Aimp's Plugins directory. }
  AimpVersionSelectionPage := CreateInputOptionPage(wpLicense,
                                                    ExpandConstant('{cm:AimpVerionSelectionMsg1}'),
                                                    ExpandConstant('{cm:AimpVerionSelectionMsg2}'),
                                                    ExpandConstant('{cm:AimpVerionSelectionMsg3}'),
                                                    True,
                                                    False
                                                    );
  AimpVersionSelectionPage.Add('AIMP2');
  AimpVersionSelectionPage.Add('AIMP3');

  AimpVersionSelectionPage.SelectedValueIndex := 1; { default is AIMP3 }

  BrowserScriptsDirPage := CreateInputDirPage(wpSelectDir,
                                              ExpandConstant('{cm:BrowserScriptsDirSelectionMsg1}'),
                                              ExpandConstant('{cm:BrowserScriptsDirSelectionMsg2}'),
                                              ExpandConstant('{cm:BrowserScriptsDirSelectionMsg3}'),
                                              False,
                                              ''
                                              );
  BrowserScriptsDirPage.Add('');
end;

procedure TerminateAimpExecutionIfNeedeed(); forward;

procedure CurPageChanged(CurPageID: Integer);
begin
  if CurPageID = wpSelectDir then
    begin
    WizardForm.DirEdit.Text := ExpandConstant('{pf}\{code:GetAimpPlayerAppDirName}\{code:AimpPluginsDirName}')

    TerminateAimpExecutionIfNeedeed(); { must do this after WizardForm.DirEdit.Text set. }
    end
  else if CurPageID = BrowserScriptsDirPage.ID then
    begin
    SettingsFileDestination := ExpandConstant('{code:GetPluginWorkDir}\settings.dat');
  
    { Set default values, using settings that were stored last time if possible }
    BrowserScriptsDirPage.Values[0] := GetPreviousData( 'BrowserScriptsDir',
                                                         ExpandConstant('{code:GetPluginWorkDir}\htdocs')
                                                       );
    end
end;

function GetBrowserScriptsDir(Param: String): String;
begin
  { Return the selected BrowserScriptsDir }
  Result := BrowserScriptsDirPage.Values[0];
end;

function IsAimp2(): Boolean;
begin
   Result := AimpVersionSelectionPage.SelectedValueIndex = 0;
end;

function GetPluginWorkDir(Param: String): String;
begin
  Result := ExpandConstant('{app}\{#PluginWorkDirectoryName}')
end;

function AimpPluginsDirName(Param: String): String;
begin
  { Return name of AIMP's Plugins directory. }
  Result := ExpandConstant('{#AimpPluginsDirectoryName}')
  { We do not distinguish between AIMP2/3 Plugins directory names 
    since supporting of this difference is hard
    while Windows is case insensitive.
  if IsAimp2() then
    Result := 'PlugIns'
  else
    Result := 'Plugins';
  }
end;

function GetAimpPlayerAppDirName(Param: String): String;
begin
  { Return the name of program directory for selected AIMP player version }
  if IsAimp2() then
    Result := 'AIMP2'
  else
    Result := 'AIMP3';
end;

procedure RegisterPreviousData(PreviousDataKey: Integer);
begin
  SetPreviousData(PreviousDataKey, 'BrowserScriptsDir', BrowserScriptsDirPage.Values[0]);
end;

procedure AfterInstallSettingsFile(Path: String);
var
  // See http://msdn.microsoft.com/en-us/library/ms757878(VS.85).aspx for details about
  XMLDoc : Variant;       // IXMLDOMDocument
  DocRootNodes : Variant; // IXMLDOMNodeList
  DocRootNode : Variant;  // IXMLDOMNode
begin
  //MsgBox('Try to load the XML file: ' + Path, mbInformation, mb_Ok);

  { Load the XML File }
  XMLDoc := CreateOleObject('MSXML2.DOMDocument');
  XMLDoc.async := False;
  XMLDoc.resolveExternals := False;
  XMLDoc.setProperty('SelectionLanguage', 'XPath');
  XMLDoc.load(Path);
  if XMLDoc.parseError.errorCode <> 0 then
    RaiseException('Error in xml file "' + Path + '" on line ' + IntToStr(XMLDoc.parseError.line) + ', position ' + IntToStr(XMLDoc.parseError.linepos) + ': ' + XMLDoc.parseError.reason);

  //MsgBox('Loaded the XML file.', mbInformation, mb_Ok);

  { Modify the XML document }
  DocRootNodes := XMLDoc.selectNodes('//httpserver/document_root');
  //MsgBox('Mathing nodes: ' + IntToStr(DocRootNodes.length), mbInformation, mb_Ok);
  DocRootNode := DocRootNodes.item(0);
  DocRootNode.Text := GetBrowserScriptsDir('');

  { Save the XML document }
  XMLDoc.Save(Path);
  //MsgBox('Saved the modified XML as ''' + Path + '''.', mbInformation, mb_Ok);
end;


function UpdateReadyMemo(Space, NewLine, MemoUserInfoInfo, MemoDirInfo, MemoTypeInfo,
  MemoComponentsInfo, MemoGroupInfo, MemoTasksInfo: String): String;
var
  S: String;
begin
  { Fill the 'Ready Memo' with the normal settings and the custom settings }
  S := '';
  S := S + MemoDirInfo + NewLine;
  S := S + ExpandConstant('{cm:BrowserScriptsDirDestination}') + NewLine + Space + GetBrowserScriptsDir('') + NewLine;
  S := S + ExpandConstant('{cm:SettingsFileDestination}') + NewLine + Space + SettingsFileDestination + NewLine;
  Result := S;
end;

function GetAimpHWND(): HWND;
begin
  Result := FindWindowByClassName('AIMP2_RemoteInfo');
end;

function IsAimpLaunched(): boolean;
begin
  Result := GetAimpHWND() <> 0;
end;

const
  WM_AIMP_COMMAND = 1024 + $75;
  WM_AIMP_CALLFUNC = 3;
  AIMP_QUIT = 10;
procedure KillAimp();
begin
  SendMessage(GetAimpHWND(), WM_AIMP_COMMAND, WM_AIMP_CALLFUNC, AIMP_QUIT);
end;

procedure TerminateAimpExecutionIfNeedeed();
begin
  if IsAimpLaunched() then
    if MsgBox(ExpandConstant('{cm:AimpApplicationTerminateQuery}'),
          mbConfirmation,
          MB_YESNO) = IDYES then
      KillAimp();
end;
