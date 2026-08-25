!ifndef APP_VERSION
!define APP_VERSION "0.3.0"
!endif

!ifndef STAGE_DIR
!define STAGE_DIR "..\..\dist\stage\Proxmark3GUI Modern"
!endif

!ifndef OUT_FILE
!define OUT_FILE "..\..\dist\Proxmark3GUI-Modern-${APP_VERSION}-setup.exe"
!endif

Unicode true
RequestExecutionLevel admin

!include "MUI2.nsh"

Name "Proxmark3GUI Modern"
OutFile "${OUT_FILE}"
InstallDir "$PROGRAMFILES64\Proxmark3GUI Modern"
InstallDirRegKey HKLM "Software\Proxmark3GUI Modern" "InstallDir"

!define MUI_ABORTWARNING
!define MUI_ICON "..\..\src\modern\app_icon.ico"
!define MUI_UNICON "..\..\src\modern\app_icon.ico"
!define MUI_BGCOLOR "FFFFFF"
!define MUI_TEXTCOLOR "111111"
!define MUI_WELCOMEFINISHPAGE_BITMAP "sidebar.bmp"
!define MUI_HEADERIMAGE
!define MUI_HEADERIMAGE_RIGHT
!define MUI_HEADERIMAGE_BITMAP "header.bmp"
!define MUI_WELCOMEPAGE_TITLE "Install Proxmark3GUI Modern"
!define MUI_WELCOMEPAGE_TEXT "This setup will install Proxmark3GUI Modern, a Windows-focused GUI for the Proxmark3 RRG/Iceman client.$\r$\n$\r$\nUse card reading and cloning features only on systems you own or are authorized to test."
!define MUI_FINISHPAGE_RUN "$INSTDIR\Proxmark3GUI.exe"
!define MUI_FINISHPAGE_RUN_TEXT "Launch Proxmark3GUI Modern"
!define MUI_FINISHPAGE_LINK "Proxmark3GUI Modern on GitHub"
!define MUI_FINISHPAGE_LINK_LOCATION "https://github.com/unicastbg/Proxmark3GUI-Modern"

VIProductVersion "${APP_VERSION}.0"
VIAddVersionKey "ProductName" "Proxmark3GUI Modern"
VIAddVersionKey "CompanyName" "Proxmark3GUI Modern contributors"
VIAddVersionKey "FileDescription" "Modern Windows GUI for Proxmark3 RRG/Iceman"
VIAddVersionKey "FileVersion" "${APP_VERSION}"
VIAddVersionKey "ProductVersion" "${APP_VERSION}"
VIAddVersionKey "LegalCopyright" "LGPL-2.1"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

Section "Install"
    SetRegView 64
    SetOutPath "$INSTDIR"
    File /r "${STAGE_DIR}\*.*"
    WriteRegStr HKLM "Software\Proxmark3GUI Modern" "InstallDir" "$INSTDIR"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Proxmark3GUI Modern" "DisplayName" "Proxmark3GUI Modern"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Proxmark3GUI Modern" "DisplayVersion" "${APP_VERSION}"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Proxmark3GUI Modern" "Publisher" "Proxmark3GUI Modern contributors"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Proxmark3GUI Modern" "InstallLocation" "$INSTDIR"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Proxmark3GUI Modern" "DisplayIcon" "$INSTDIR\Proxmark3GUI.exe"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Proxmark3GUI Modern" "UninstallString" "$INSTDIR\Uninstall.exe"
    WriteUninstaller "$INSTDIR\Uninstall.exe"

    CreateDirectory "$SMPROGRAMS\Proxmark3GUI Modern"
    CreateShortcut "$SMPROGRAMS\Proxmark3GUI Modern\Proxmark3GUI Modern.lnk" "$INSTDIR\Proxmark3GUI.exe"
    CreateShortcut "$SMPROGRAMS\Proxmark3GUI Modern\Uninstall.lnk" "$INSTDIR\Uninstall.exe"
    CreateShortcut "$DESKTOP\Proxmark3GUI Modern.lnk" "$INSTDIR\Proxmark3GUI.exe"
SectionEnd

Section "Uninstall"
    SetRegView 64
    Delete "$DESKTOP\Proxmark3GUI Modern.lnk"
    Delete "$SMPROGRAMS\Proxmark3GUI Modern\Proxmark3GUI Modern.lnk"
    Delete "$SMPROGRAMS\Proxmark3GUI Modern\Uninstall.lnk"
    RMDir "$SMPROGRAMS\Proxmark3GUI Modern"
    RMDir /r "$INSTDIR"
    DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Proxmark3GUI Modern"
    DeleteRegKey HKLM "Software\Proxmark3GUI Modern"
SectionEnd
