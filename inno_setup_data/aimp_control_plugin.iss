#define SourceDir "..\"
#define AppName "AIMP Control Plugin"
#define SrcApp "temp_build\Release\aimp_control_plugin.dll"
; use absolute path as GetFileVersion() argument since there is problem with relative path when we execute this script from command line.
#define FileVerStr GetFileVersion(SourcePath + "\" + SourceDir + SrcApp)
#define StripBuild(VerStr) Copy(VerStr, 1, RPos(".", VerStr)-1)
#define AppVerStr StripBuild(FileVerStr)
#define WorkDirRelativePath  "AIMP\Control Plugin"

[Setup]

SourceDir={#SourceDir}
AppName={#AppName}
AppVersion={#AppVerStr}
AppVerName={#AppName} {#AppVerStr}
UninstallDisplayName={#AppName} {#AppVerStr}
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
AppCopyright=Copyright © 2011 Alexey Ivanov
AppPublisherURL=http://www.example.com/
AppSupportURL=http://www.example.com/
AppUpdatesURL=http://www.example.com/

DefaultDirName={pf}\AIMP2\PlugIns
DefaultGroupName={#AppName}
DisableProgramGroupPage=true
OutputDir=temp_build\Release\distrib\
SetupIconFile=inno_setup_data\icons\icon_aimp3_32x32.ico
Compression=lzma/ultra
SolidCompression=true
InternalCompressLevel=ultra
PrivilegesRequired=none
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
Source: inno_setup_data\default_settings.dat; DestDir: {userappdata}\{#WorkDirRelativePath}; DestName: settings.dat; Flags: onlyifdoesntexist; AfterInstall: AfterInstallSettingsFile( ExpandConstant('{userappdata}\{#WorkDirRelativePath}\settings.dat') )

[Registry]
; etc.

[Code]
var
  BrowserScriptsDirPage: TInputDirWizardPage;
  SettingsFileDestination: String;

procedure InitializeWizard;
begin
  { Create the pages }
  BrowserScriptsDirPage := CreateInputDirPage(wpSelectDir,
	ExpandConstant('{cm:BrowserScriptsDirSelectionMsg1}'),
	ExpandConstant('{cm:BrowserScriptsDirSelectionMsg2}'),
	ExpandConstant('{cm:BrowserScriptsDirSelectionMsg3}'),
    False, '');
  BrowserScriptsDirPage.Add('');

  { Set default values, using settings that were stored last time if possible }
  BrowserScriptsDirPage.Values[0] := GetPreviousData( 'BrowserScriptsDir', ExpandConstant('{userappdata}') + '\{#WorkDirRelativePath}\htdocs\' );

  SettingsFileDestination := ExpandConstant('{userappdata}\{#WorkDirRelativePath}\settings.dat');
end;


function GetBrowserScriptsDir(Param: String): String;
begin
  { Return the selected BrowserScriptsDir }
  Result := BrowserScriptsDirPage.Values[0];
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
