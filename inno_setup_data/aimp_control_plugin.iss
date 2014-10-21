#define SourceDir "..\"
#define AppName "AIMP Control Plugin"
#define SrcApp "temp_build\Release\aimp_control_plugin.dll"
; use absolute path as GetFileVersion() argument since there is problem with relative path when we execute this script from command line.
#define FileVerStr GetFileVersion(SourcePath + "\" + SourceDir + SrcApp)
#define StripBuild(VerStr) Copy(VerStr, 1, RPos(".", VerStr)-1)
#define AppVerStr StripBuild(FileVerStr)
#define PluginWorkDirectoryName "Control Plugin"
#define AimpPluginsDirectoryName "Plugins"
#define FreeImage_VERSION GetEnv('FreeImage_VERSION')

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
AppCopyright=Copyright © 2014 Alexey Ivanov
AppPublisherURL=https://github.com/a0ivanov/aimp-control-plugin
AppSupportURL=http://code.google.com/p/aimp-control-plugin/w/list
AppUpdatesURL=https://github.com/a0ivanov/aimp-control-plugin/releases/latest

DefaultDirName={#AimpPluginsDirectoryName}
DefaultGroupName={#AppName}
DisableProgramGroupPage=true
OutputDir=temp_build\Release\distrib\
SetupIconFile=inno_setup_data\icons\aimp_control_settings_manager.ico
Compression=lzma/ultra
SolidCompression=true
InternalCompressLevel=ultra
PrivilegesRequired=admin
DirExistsWarning=no
AllowUNCPath=false
AppendDefaultDirName=true

[Languages]
; Language name is used on Donation page as {language} constant.
Name: en; MessagesFile: inno_setup_data\English.isl; LicenseFile: Lisense-English.txt;
Name: ru; MessagesFile: inno_setup_data\Russian.isl; LicenseFile: Lisense-Russian.txt;
Name: cs; MessagesFile: inno_setup_data\Czech.isl;   LicenseFile: Lisense-English.txt;

[Files]
Source: "{#SrcApp}"; DestDir: "{app}"; Flags: ignoreversion
Source: "temp_build\Release\htdocs\*"; DestDir: "{code:GetBrowserScriptsDir}"; Flags: ignoreversion recursesubdirs createallsubdirs
; NOTE: Don't use "Flags: ignoreversion" on any shared system files
Source: "inno_setup_data\default_settings.dat"; DestDir: "{code:GetPluginSettingsDir}"; DestName: "settings.dat"; Flags: onlyifdoesntexist; AfterInstall: AfterInstallSettingsFile( ExpandConstant('{code:GetPluginSettingsDir}\settings.dat') )
Source: "3rd_party\FreeImage\{#FreeImage_VERSION}\Dist\FreeImage.dll"; DestDir: "{code:GetFreeImageDLLsDstDir}"; Flags: ignoreversion
Source: "3rd_party\FreeImage\{#FreeImage_VERSION}\Wrapper\FreeImagePlus\dist\FreeImagePlus.dll"; DestDir: "{code:GetFreeImageDLLsDstDir}"; Flags: ignoreversion
Source: "temp_build\Release\SettingsManager\*"; DestDir: "{code:GetSettingsManagerDir}"; Flags: ignoreversion recursesubdirs createallsubdirs


[Registry]
; etc.

[Code]
var
  BrowserScriptsDirPage: TInputDirWizardPage;
  AimpVersionSelectionPage: TInputOptionWizardPage;
  PluginOptionsPage: TInputOptionWizardPage;
  SettingsManagerInstallationPage: TOutputMsgMemoWizardPage;
  AfterInstallPage: TOutputMsgMemoWizardPage;
  AllowNetworkAccess: Boolean;
  AllowFilesUpload: Boolean;
  AllowFilesDeletion: Boolean;
  EnableScheduler: Boolean;
  SettingsFileDestination: String;
  Port: Integer;

procedure InitDonationPage; forward;
function GetTextForSettingsManagerInstallationMemo(): AnsiString; forward;

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

  InitDonationPage();
  
  PluginOptionsPage := CreateInputOptionPage(BrowserScriptsDirPage.ID,
                                            ExpandConstant('{cm:PluginOptionsTitle}'),
                                            ExpandConstant('{cm:PluginOptionsDescription}'),
                                            ExpandConstant('{cm:PluginOptionsSubDescription}'),
                                            False, False);

  PluginOptionsPage.Add(ExpandConstant('{cm:OptionNetworkCheckBox}'));
  PluginOptionsPage.Add(ExpandConstant('{cm:OptionUploadTracksCheckBox}'));
  PluginOptionsPage.Add(ExpandConstant('{cm:OptionPhysicalTrackDeletionCheckBox}'));
  PluginOptionsPage.Add(ExpandConstant('{cm:OptionSchedulerCheckBox}'));
  
  
  SettingsManagerInstallationPage := CreateOutputMsgMemoPage(PluginOptionsPage.ID,
                                          ExpandConstant('{cm:SettingsManagerInstallationTitle}'),
                                          ExpandConstant('{cm:SettingsManagerInstallationDescription}'),
                                          ExpandConstant('{cm:SettingsManagerInstallationSubDescription}'),
                                          GetTextForSettingsManagerInstallationMemo());
  
  
  AfterInstallPage := CreateOutputMsgMemoPage(wpInfoAfter,
   SetupMessage(msgWizardInfoAfter),
   SetupMessage(msgInfoAfterLabel),
   SetupMessage(msgInfoAfterClickLabel),
   '' // memo text will be set later by AfterInstallPage.RichEditViewer.RTFText.
  );
  
  Port := 3333;
end;

procedure TerminateAimpExecutionIfNeedeed(); forward;
function GetInfoAfterMemoText() : String; forward;
function DefaultWritableBrowserScriptsDir(): String; forward;

procedure CurPageChanged(CurPageID: Integer);
begin
  if CurPageID = wpSelectDir then
    begin
    WizardForm.DirEdit.Text := ExpandConstant('{pf}\{code:GetAimpPlayerAppDirName}\{code:AimpPluginsDirName}')

    TerminateAimpExecutionIfNeedeed(); { must do this after WizardForm.DirEdit.Text set. }
    end
  else if CurPageID = BrowserScriptsDirPage.ID then
    begin
    SettingsFileDestination := ExpandConstant('{code:GetPluginSettingsDir}\settings.dat');
  
    { Set default values, using settings that were stored last time if possible }
    BrowserScriptsDirPage.Values[0] := DefaultWritableBrowserScriptsDir();
    end
  else if CurPageID = wpReady then
     begin
     AllowNetworkAccess := PluginOptionsPage.Values[0];
     AllowFilesUpload := PluginOptionsPage.Values[1];
     AllowFilesDeletion := PluginOptionsPage.Values[2];
     EnableScheduler := PluginOptionsPage.Values[3];
     AfterInstallPage.RichEditViewer.RTFText := GetInfoAfterMemoText();
     end
end;

function IsDotNetDetected(version: string; service: cardinal): boolean; forward;

function ShouldSkipPage(PageID: Integer): Boolean;
begin
  // initialize result to not skip any page (not necessary, but safer)
  Result := False;
  
  // if the page that is asked to be skipped is your custom page, then...
  if PageID = SettingsManagerInstallationPage.ID then
    Result := IsDotNetDetected('v4\Full', 0)
end;

function GetTextForSettingsManagerInstallationMemo(): AnsiString;
begin
   Result := ExpandConstant('{cm:SettingsManagerInstallationMessage}');
end;

function DefaultWritableBrowserScriptsDir(): String;
begin
  //GetPreviousData( 'BrowserScriptsDir',
  //                  ExpandConstant('{code:GetPluginWorkDir}\htdocs')
  //                );
  
  Result := ExpandConstant('{userappdata}\{code:GetAimpPlayerAppDataDirName}\{#PluginWorkDirectoryName}\htdocs');
end;

function IsAimp2(): Boolean; forward;

function GetAimpPlayerAppDataDirName(): String;
begin
  { Return the name of AppData directory for selected AIMP player version }
  if IsAimp2() then
	  Result := 'AIMP'
  else
	  Result := 'AIMP3';
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

function GetPluginSettingsDir(Param: String): String;
begin
  Result := ExpandConstant('{app}\{#PluginWorkDirectoryName}')
end;

function GetFreeImageDLLsDstDir(Param: String): String;
begin
  Result := ExpandConstant('{app}\{#PluginWorkDirectoryName}')
end;

function GetSettingsManagerDir(Param: String): String;
begin
  Result := ExpandConstant('{app}\{#PluginWorkDirectoryName}\SettingsManager')
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

function BooleanToString(condition: Boolean): String;
begin
  if (condition) then
    Result := 'true'
  else
    Result := 'false';
end;

procedure AfterInstallSettingsFile(Path: String);
var
  // See http://msdn.microsoft.com/en-us/library/ms757878(VS.85).aspx for details about
  XMLDoc : Variant;       // IXMLDOMDocument
  DocRootNodes : Variant; // IXMLDOMNodeList
  DocRootNode : Variant;  // IXMLDOMNode
  HttpServerNode : Variant;  // IXMLDOMNode
  AllInterfacesNode : Variant;  // IXMLDOMNode
  MiscNode : Variant;  // IXMLDOMNode
  Nodes : Variant;        // IXMLDOMNode
  Node : Variant;        // IXMLDOMNode
begin
  //MsgBox('Try to load the XML file: ' + Path, mbInformation, mb_Ok);

  { Load the XML File }
  XMLDoc := CreateOleObject('Msxml2.DOMDocument');
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

  Nodes := XMLDoc.selectNodes('//httpserver/port');
  if (Nodes.length > 0) then
    Port := Nodes.item(0).Text;

  HttpServerNode := XMLDoc.selectNodes('//httpserver').item(0);
  if (AllowNetworkAccess) then
    begin
    Nodes := XMLDoc.selectNodes('//httpserver/ip_to_bind');
    if (Nodes.length > 0) then
       HttpServerNode.removeChild(Nodes.item(0));
       
    Nodes := XMLDoc.selectNodes('//httpserver/port');
    if (Nodes.length > 0) then
       HttpServerNode.removeChild(Nodes.item(0));
    
    Nodes := XMLDoc.selectNodes('//httpserver/all_interfaces');
    if (Nodes.length > 0) then
      begin
      AllInterfacesNode := Nodes.item(0);
      Port := AllInterfacesNode.getAttribute('port');
      end
    else
      begin
      AllInterfacesNode := XMLDoc.createElement('all_interfaces')
      AllInterfacesNode.setAttribute('port', IntToStr(Port));
      HttpServerNode.appendChild(AllInterfacesNode);
      end;
    end;
  
  MiscNode := XMLDoc.selectNodes('//misc').item(0);
  
  Nodes := XMLDoc.selectNodes('//misc/enable_track_upload');
  if (Nodes.length > 0) then
    Node := Nodes.item(0)
  else
    begin
    Node := XMLDoc.createElement('enable_track_upload');
    MiscNode.appendChild(Node);
    Node := XMLDoc.selectNodes('//misc/enable_track_upload').item(0);
    end;
  Node.Text := BooleanToString(AllowFilesDeletion);
    
  Nodes := XMLDoc.selectNodes('//misc/enable_physical_track_deletion');
  if (Nodes.length > 0) then
    Node := Nodes.item(0)
  else
    begin
    Node := XMLDoc.createElement('enable_physical_track_deletion');
    MiscNode.appendChild(Node);
    Node := XMLDoc.selectNodes('//misc/enable_physical_track_deletion').item(0);
    end;
  Node.Text := BooleanToString(AllowFilesDeletion);
      
  Nodes := XMLDoc.selectNodes('//misc/enable_scheduler');
  if (Nodes.length > 0) then
    Node := Nodes.item(0)
  else
    begin
    Node := XMLDoc.createElement('enable_scheduler');
    MiscNode.appendChild(Node);
    Node := XMLDoc.selectNodes('//misc/enable_scheduler').item(0);
    end;
  Node.Text := BooleanToString(EnableScheduler);
    
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
  S := S + ExpandConstant('{cm:SettingsManager}') + ':' + NewLine + Space + GetSettingsManagerDir('') + '\SettingsManager.exe' + NewLine;
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

function GetBrowserScriptsStringDirectlyFromRegistry(): String;
var
  sUnInstPath: String;
  sBrowserScriptsString: String;
begin
  sUnInstPath := ExpandConstant('Software\Microsoft\Windows\CurrentVersion\Uninstall\{#emit SetupSetting("AppId")}_is1');
  sBrowserScriptsString := '';
  if not RegQueryStringValue(HKLM, sUnInstPath, 'Inno Setup CodeFile: BrowserScriptsDir', sBrowserScriptsString) then
    RegQueryStringValue(HKCU, sUnInstPath, 'Inno Setup CodeFile: BrowserScriptsDir', sBrowserScriptsString);
    
  Result := sBrowserScriptsString;
end;

function GetBrowserScriptsString(): String;
begin
  Result := GetPreviousData('BrowserScriptsDir', '');
end;

function IsUpgrade(): Boolean;
begin
  Result := (GetUninstallString() <> '');
end;

function UnInstallOldVersion(): Integer;
var
  sUnInstallString: String;
  iResultCode: Integer;
begin
// Return Values:
// 1 - uninstall string is empty
// 2 - error executing the UnInstallString
// 3 - successfully executed the UnInstallString

  // default return value
  Result := 0;

  // get the uninstall string of the old app
  sUnInstallString := GetUninstallString();
  if sUnInstallString <> '' then begin
    sUnInstallString := RemoveQuotes(sUnInstallString);
    if Exec(sUnInstallString, '/SILENT /NORESTART /SUPPRESSMSGBOXES','', SW_HIDE, ewWaitUntilTerminated, iResultCode) then
      Result := 3
    else
      Result := 2;
  end else
    Result := 1;
end;

function RemoveInstalledBrowserScripts(): Integer;
var
  sBrowserScriptsDir: String;
begin
// Return Values:
// 1 - Browser scripts dir string is empty
// 2 - error executing the DelTree
// 3 - successfully executed the DelTree

  // default return value
  Result := 0;

  // get the uninstall string of the old app.
  sBrowserScriptsDir := GetBrowserScriptsString();
  if sBrowserScriptsDir <> '' then begin
    sBrowserScriptsDir := RemoveQuotes(sBrowserScriptsDir);
    if DelTree(sBrowserScriptsDir, True, True, True) then
      Result := 3
    else
      Result := 2;
  end else
    Result := 1;
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if (CurStep=ssInstall) then
  begin
    if (IsUpgrade()) then
    begin
      // Do not uninstall previous version till it will be possible remain settings file.
      // Instead old remove browser scripts.
      // UnInstallOldVersion();
      RemoveInstalledBrowserScripts();
    end;
  end;
end;

procedure DonationButtonOnClick(Sender: TObject);
var
  ErrorCode: Integer;
  URL: String;
begin
  URL := 'http://www.a0ivanov.ru/aimp/donate_' + ExpandConstant('{language}') + '.html';

  if not ShellExec('', URL, '', '', SW_SHOW, ewNoWait, ErrorCode) then
  begin
    // handle failure if necessary
  end;
end;
  
procedure InitDonationPage;
var
  Page: TWizardPage;
  Button: TNewButton;
  Label1: TLabel;
begin
  Page := CreateCustomPage(wpInfoAfter, ExpandConstant('{cm:DonatePageTitle}'), ExpandConstant('{cm:DonateMsg}')); // cm:DonatePageTitle2

  Label1 := TLabel.Create(Page);
  Label1.AutoSize := False;
  Label1.Top := ScaleY(15);
  Label1.Left := ScaleX(0);
  Label1.Width := Page.SurfaceWidth;
  Label1.Alignment := taCenter;
  Label1.Caption := ExpandConstant('{cm:DonateMsg}');
  Label1.WordWrap := True;
  Label1.Parent := Page.Surface;
  

  Button := TNewButton.Create(Page);
  Button.Width := ScaleX(80);
  Button.Height := ScaleY(26);
  Button.Caption := ExpandConstant('{cm:DonateButton}');
  Button.OnClick := @DonationButtonOnClick;
  Button.Parent := Page.Surface;
  Button.Top := Label1.Top + Label1.Height + ScaleY(8);
  Button.Left := Label1.Left + (Label1.Width - Button.Width) / 2;
end;

procedure GetIpAddresses(Addresses : TStringList); forward;
function GetInfoAfterMemoText(): String;
var
  SL : TStringList;
  I: Integer;
  Msg: String;
begin
  if (AllowNetworkAccess) then
     begin
     Msg := ExpandConstant('{cm:InfoAfterPageMemoTextRemoteBegin}'); 
     SL := TStringList.Create;
     GetIpAddresses(SL);
     For I := 0 to SL.Count - 1 do
       begin
        Msg := Msg + #13#10 + FmtMessage('http://%1:%2/index.htm', [SL.Strings[I], IntToStr(Port)]);
       end;
     SL.Free;
     Msg := Msg + #13#10 + ExpandConstant('{cm:InfoAfterPageMemoTextRemoteEnd}'); 
     end
   else
     Msg := ExpandConstant('{cm:InfoAfterPageMemoTextLocal}');
     
   Msg := Msg + #13#10 + #13#10 + ExpandConstant('{cm:SettingsManagerInfo}');
   Result := Msg;
end;

const
 ERROR_INSUFFICIENT_BUFFER = 122;

function GetIpAddrTable( pIpAddrTable: Array of Byte;
  var pdwSize: Cardinal; bOrder: WordBool ): DWORD;
external 'GetIpAddrTable@IpHlpApi.dll stdcall';

procedure GetIpAddresses(Addresses : TStringList);
var 
 Size : Cardinal;
 Buffer : Array of Byte;
 IpAddr : String;
 AddrCount : Integer;
 I, J : Integer;
begin
  // Find Size
  if GetIpAddrTable(Buffer,Size,False) = ERROR_INSUFFICIENT_BUFFER then
  begin
     // Allocate Buffer with large enough size
     SetLength(Buffer,Size);
     // Get List of IP Addresses into Buffer
     if GetIpAddrTable(Buffer,Size,True) = 0 then
     begin
       // Find out how many addresses will be returned.
       AddrCount := (Buffer[1] * 256) + Buffer[0];
       // Loop through addresses.
       For I := 0 to AddrCount -1 do
       begin
         IpAddr := '';
         // Loop through each byte of the address
         For J := 0 to 3 do
         begin
           if J > 0 then
             IpAddr := IpAddr + '.';
           // Navigagte through record structure to find correct byte of Addr
           IpAddr := IpAddr + IntToStr(Buffer[I*24+J+4]);
         end;
         Addresses.Add(IpAddr);
       end;
     end;
  end;
end;

function IsDotNetDetected(version: string; service: cardinal): boolean;
// Indicates whether the specified version and service pack of the .NET Framework is installed.
//
// version -- Specify one of these strings for the required .NET Framework version:
//    'v1.1.4322'     .NET Framework 1.1
//    'v2.0.50727'    .NET Framework 2.0
//    'v3.0'          .NET Framework 3.0
//    'v3.5'          .NET Framework 3.5
//    'v4\Client'     .NET Framework 4.0 Client Profile
//    'v4\Full'       .NET Framework 4.0 Full Installation
//    'v4.5'          .NET Framework 4.5
//
// service -- Specify any non-negative integer for the required service pack level:
//    0               No service packs required
//    1, 2, etc.      Service pack 1, 2, etc. required
var
    key: string;
    install, release, serviceCount: cardinal;
    check45, success: boolean;
begin
    // .NET 4.5 installs as update to .NET 4.0 Full
    if version = 'v4.5' then begin
        version := 'v4\Full';
        check45 := true;
    end else
        check45 := false;

    // installation key group for all .NET versions
    key := 'SOFTWARE\Microsoft\NET Framework Setup\NDP\' + version;

    // .NET 3.0 uses value InstallSuccess in subkey Setup
    if Pos('v3.0', version) = 1 then begin
        success := RegQueryDWordValue(HKLM, key + '\Setup', 'InstallSuccess', install);
    end else begin
        success := RegQueryDWordValue(HKLM, key, 'Install', install);
    end;

    // .NET 4.0/4.5 uses value Servicing instead of SP
    if Pos('v4', version) = 1 then begin
        success := success and RegQueryDWordValue(HKLM, key, 'Servicing', serviceCount);
    end else begin
        success := success and RegQueryDWordValue(HKLM, key, 'SP', serviceCount);
    end;

    // .NET 4.5 uses additional value Release
    if check45 then begin
        success := success and RegQueryDWordValue(HKLM, key, 'Release', release);
        success := success and (release >= 378389);
    end;

    result := success and (install = 1) and (serviceCount >= service);
end;
