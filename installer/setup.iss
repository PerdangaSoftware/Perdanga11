#define MyAppName "Perdanga11"
#define MyAppVersion "1.1"
#define MyAppPublisher "Perdanga Software"
#define MyAppURL "https://gitlab.com/perdanga/perdanga11"
#define MyAppExeName "Perdanga11.exe"

[Setup]
AppId={{E589417A-7241-4BD4-A42C-8C127D95B309}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}

; Directory selection page enabled
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableDirPage=no

UninstallDisplayIcon={app}\ico\perdanga11.ico
OutputDir=..\dist
OutputBaseFilename=Perdanga11_Setup
SetupIconFile=..\assets\ico\perdanga11.ico

; Custom artwork generated from assets\logo\perdanga11.png
WizardImageFile=..\assets\logo\wizard_banner.bmp
WizardSmallImageFile=..\assets\logo\logo_small.bmp

Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin

; Language selection dialog
ShowLanguageDialog=yes
LanguageDetectionMethod=none
UsePreviousLanguage=no

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "russian"; MessagesFile: "compiler:Languages\Russian.isl"

[CustomMessages]
; English
english.CreateDesktopIcon=Create a desktop shortcut
english.AutoStartTask=Start Perdanga11 automatically when Windows starts
english.VisitGitLabRun=View source code on GitLab
english.DevelopedBy=Developed by Perdanga Software
english.GitLabLink=GitLab

; Russian
russian.CreateDesktopIcon=Создать ярлык на Рабочем столе
russian.AutoStartTask=Запускать Perdanga11 автоматически при входе в Windows
russian.VisitGitLabRun=Посетить репозиторий проекта
russian.DevelopedBy=Разработано Perdanga Software
russian.GitLabLink=Репозиторий

[Messages]
english.FinishedHeadingLabel=Perdanga Forever!
english.FinishedLabel=[name] has been successfully installed on your computer.
russian.FinishedHeadingLabel=Перданга Навсегда!
russian.FinishedLabel=Программа [name] успешно установлена на ваш компьютер.

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"
Name: "autostart"; Description: "{cm:AutoStartTask}"; GroupDescription: "{cm:AdditionalIcons}"

[Files]
Source: "..\bin\{#MyAppExeName}"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\assets\ico\perdanga11.ico"; DestDir: "{app}\ico"; Flags: ignoreversion
Source: "..\assets\logo\perdanga11.png"; DestDir: "{app}\logo"; Flags: ignoreversion
Source: "..\bin\config.ini"; DestDir: "{app}"; Flags: onlyifdoesntexist

[Icons]
Name: "{autoprograms}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; IconFilename: "{app}\ico\perdanga11.ico"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; IconFilename: "{app}\ico\perdanga11.ico"; Tasks: desktopicon
Name: "{autostartup}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; IconFilename: "{app}\ico\perdanga11.ico"; Tasks: autostart

[Run]
; Run as non-elevated original user so UIPI does not block shell context menu messages
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent runasoriginaluser
Filename: "{#MyAppURL}"; Description: "{cm:VisitGitLabRun}"; Flags: shellexec postinstall unchecked

[Code]
// Fluent Midnight Navy & Electric Azure Palette (Delphi BGR format)
const
  COLOR_BG_DARK    = $280C03; // Deep Midnight Navy background (#030C28)
  COLOR_SURFACE    = $361507; // Rich Deep Sapphire Surface (#071536)
  COLOR_ACCENT     = $FFA21E; // Vibrant Electric Azure Blue (#1EA2FF)
  COLOR_SAGE       = $FFD0B0; // Soft Ice Blue Subtext (#B0D0FF)
  COLOR_TEXT_WHITE = $FFFFFF; // Crisp Pure White (#FFFFFF)
  COLOR_MUTED      = $A88B78; // Subtle Slate Blue-Gray (#788BA8)

procedure GitLabLabelOnClick(Sender: TObject);
var
  ErrorCode: Integer;
begin
  ShellExec('open', '{#MyAppURL}', '', '', SW_SHOWNORMAL, ewNoWait, ErrorCode);
end;

// Terminate running instance before installing
function PrepareToInstall(var NeedsRestart: Boolean): String;
var
  ResultCode: Integer;
begin
  Exec('taskkill.exe', '/f /im {#MyAppExeName}', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
  Sleep(250);
  Result := '';
end;

// Terminate running instance before uninstalling
procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  ResultCode: Integer;
begin
  if CurUninstallStep = usUninstall then
  begin
    Exec('taskkill.exe', '/f /im {#MyAppExeName}', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
    Sleep(250);
  end;
end;

procedure ApplyCustomArtworkTheme();
begin
  WizardForm.Color := COLOR_BG_DARK;
  WizardForm.InnerPage.Color := COLOR_BG_DARK;
  WizardForm.MainPanel.Color := COLOR_SURFACE;

  WizardForm.WelcomePage.Color := COLOR_BG_DARK;
  WizardForm.FinishedPage.Color := COLOR_BG_DARK;

  WizardForm.Bevel.Visible := False;

  WizardForm.PageNameLabel.Font.Color := COLOR_ACCENT;
  WizardForm.PageNameLabel.Font.Style := [fsBold];
  WizardForm.PageDescriptionLabel.Font.Color := COLOR_SAGE;

  WizardForm.WelcomeLabel1.Font.Color := COLOR_ACCENT;
  WizardForm.WelcomeLabel1.Font.Style := [fsBold];
  WizardForm.WelcomeLabel2.Font.Color := COLOR_TEXT_WHITE;

  WizardForm.FinishedHeadingLabel.Font.Color := COLOR_ACCENT;
  WizardForm.FinishedHeadingLabel.Font.Style := [fsBold];
  WizardForm.FinishedLabel.Font.Color := COLOR_TEXT_WHITE;
  WizardForm.RunList.Color := COLOR_BG_DARK;
  WizardForm.RunList.Font.Color := COLOR_TEXT_WHITE;
  WizardForm.RunList.BorderStyle := bsNone;

  WizardForm.SelectDirLabel.Font.Color := COLOR_TEXT_WHITE;
  WizardForm.SelectDirBrowseLabel.Font.Color := COLOR_SAGE;
  WizardForm.DirEdit.Color := COLOR_SURFACE;
  WizardForm.DirEdit.Font.Color := COLOR_TEXT_WHITE;
  WizardForm.DiskSpaceLabel.Font.Color := COLOR_SAGE;

  WizardForm.SelectStartMenuFolderLabel.Font.Color := COLOR_TEXT_WHITE;
  WizardForm.SelectStartMenuFolderBrowseLabel.Font.Color := COLOR_SAGE;
  WizardForm.GroupEdit.Color := COLOR_SURFACE;
  WizardForm.GroupEdit.Font.Color := COLOR_TEXT_WHITE;
  WizardForm.NoIconsCheck.Font.Color := COLOR_TEXT_WHITE;

  WizardForm.SelectTasksLabel.Font.Color := COLOR_TEXT_WHITE;
  WizardForm.TasksList.Color := COLOR_SURFACE;
  WizardForm.TasksList.Font.Color := COLOR_TEXT_WHITE;

  WizardForm.ReadyLabel.Font.Color := COLOR_TEXT_WHITE;
  WizardForm.ReadyMemo.Color := COLOR_SURFACE;
  WizardForm.ReadyMemo.Font.Color := COLOR_TEXT_WHITE;

  WizardForm.StatusLabel.Font.Color := COLOR_TEXT_WHITE;
  WizardForm.FileNameLabel.Font.Color := COLOR_SAGE;
end;

procedure CurPageChanged(CurPageID: Integer);
begin
  if CurPageID = wpSelectDir then
  begin
    WizardForm.DiskSpaceLabel.Font.Color := COLOR_SAGE;
  end
  else if CurPageID = wpSelectProgramGroup then
  begin
    WizardForm.SelectStartMenuFolderLabel.Font.Color := COLOR_TEXT_WHITE;
    WizardForm.SelectStartMenuFolderBrowseLabel.Font.Color := COLOR_SAGE;
  end
  else if CurPageID = wpFinished then
  begin
    WizardForm.FinishedPage.Color := COLOR_BG_DARK;
    WizardForm.RunList.Color := COLOR_BG_DARK;
    WizardForm.RunList.Font.Color := COLOR_TEXT_WHITE;
  end
  else if CurPageID = wpWelcome then
  begin
    WizardForm.WelcomePage.Color := COLOR_BG_DARK;
  end;
end;

procedure InitializeWizard();
var
  DevLabel, Sep1Label, GitLabLabel: TLabel;
begin
  ApplyCustomArtworkTheme();

  DevLabel := TLabel.Create(WizardForm);
  DevLabel.Parent := WizardForm;
  DevLabel.Left := ScaleX(18);
  DevLabel.Top := WizardForm.CancelButton.Top + ScaleY(5);
  DevLabel.Caption := ExpandConstant('{cm:DevelopedBy}');
  DevLabel.Font.Color := COLOR_MUTED;
  DevLabel.Font.Size := 8;

  Sep1Label := TLabel.Create(WizardForm);
  Sep1Label.Parent := WizardForm;
  Sep1Label.Left := DevLabel.Left + DevLabel.Width + ScaleX(6);
  Sep1Label.Top := DevLabel.Top;
  Sep1Label.Caption := #$2022;
  Sep1Label.Font.Color := COLOR_MUTED;
  Sep1Label.Font.Size := 8;

  GitLabLabel := TLabel.Create(WizardForm);
  GitLabLabel.Parent := WizardForm;
  GitLabLabel.Left := Sep1Label.Left + Sep1Label.Width + ScaleX(6);
  GitLabLabel.Top := DevLabel.Top;
  GitLabLabel.Caption := ExpandConstant('{cm:GitLabLink}');
  GitLabLabel.Cursor := crHand;
  GitLabLabel.Font.Color := COLOR_ACCENT;
  GitLabLabel.Font.Style := [fsUnderline];
  GitLabLabel.Font.Size := 8;
  GitLabLabel.OnClick := @GitLabLabelOnClick;
end;