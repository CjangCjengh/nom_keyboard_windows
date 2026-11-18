; Nom Keyboard TSF installer
Unicode true
!include "MUI2.nsh"
!include "x64.nsh"
!include "LogicLib.nsh"
!include "FileFunc.nsh"
!ifndef SOLUTION_DIR
    !define SOLUTION_DIR ".."
!endif
!define ARP "Software\Microsoft\Windows\CurrentVersion\Uninstall\NomKeyboard"
!define LEGACY_TIP "SOFTWARE\Microsoft\CTF\TIP\{7A8B9C0D-1E2F-3A4B-5C6D-7E8F9A0B1C2D}"

Name "Nom Keyboard"
OutFile "NomKeyboardSetup.exe"
RequestExecutionLevel admin
InstallDir "$PROGRAMFILES\NomKeyboard"
InstallDirRegKey HKLM "${ARP}" "InstallLocation"
AllowSkipFiles off
VIProductVersion "1.1.5.0"
VIAddVersionKey "ProductName" "Nom Keyboard"
VIAddVersionKey "FileDescription" "Nom Keyboard IME Installer"
VIAddVersionKey "FileVersion" "1.1.5.0"
VIAddVersionKey "LegalCopyright" "Copyright (C) Nom Keyboard Project"

!define MUI_ABORTWARNING
!define MUI_FINISHPAGE_TEXT "Nom Keyboard is ready. Select the Vietnamese keyboard with Win+Space. To change input preferences, open Nom Keyboard Settings from Start."
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"
!include "FileUse.nsh"

Function .onInit
    ; v1.0 wrote only a quoted UninstallString. Reuse its custom installation
    ; directory so upgrading does not strand an old DLL and uninstaller.
    ReadRegStr $0 HKLM "${ARP}" "InstallLocation"
    ${If} $0 == ""
        ReadRegStr $0 HKLM "${ARP}" "UninstallString"
        StrCpy $1 $0 1
        ${If} $1 == '$\"'
            StrCpy $0 $0 -1 1
        ${EndIf}
        ${GetFileName} "$0" $1
        ${If} $1 == "uninstall.exe"
        ${AndIf} ${FileExists} "$0"
            ${GetParent} "$0" $INSTDIR
        ${EndIf}
    ${EndIf}
FunctionEnd

!macro RegisterDLL TOOL SWITCH DLL
    ${Do}
        ClearErrors
        StrCpy $0 -1
        ExecWait '"${TOOL}" /s ${SWITCH} "$INSTDIR\${DLL}"' $0
        ${IfNot} ${Errors}
        ${AndIf} $0 = 0
            ${ExitDo}
        ${EndIf}
        MessageBox MB_RETRYCANCEL|MB_ICONSTOP "Registration command failed for ${DLL} (code $0). See the setup details." /SD IDCANCEL IDRETRY +3
        SetErrorLevel 1
        Abort
    ${Loop}
!macroend

!macro RegisterBoth SWITCH
    ${If} ${RunningX64}
        ${If} ${FileExists} "$INSTDIR\NomIME64.dll"
            !insertmacro RegisterDLL "$WINDIR\Sysnative\regsvr32.exe" "${SWITCH}" "NomIME64.dll"
        ${EndIf}
    ${EndIf}
    ${If} ${FileExists} "$INSTDIR\NomIME32.dll"
        !insertmacro RegisterDLL "$SYSDIR\regsvr32.exe" "${SWITCH}" "NomIME32.dll"
    ${EndIf}
!macroend

Section "Nom Keyboard"
    SetOutPath "$INSTDIR"
    !insertmacro WaitForFile "" "NomIME64.dll"
    !insertmacro WaitForFile "" "NomIME32.dll"
    ; Only remove the legacy TIP, never the colliding system COM class.
    ${If} ${RunningX64}
        SetRegView 64
        DeleteRegKey HKLM "${LEGACY_TIP}"
        SetRegView 32
        File /oname=NomIME64.dll "${SOLUTION_DIR}\bin\Release\x64\NomIME.dll"
    ${EndIf}
    DeleteRegKey HKLM "${LEGACY_TIP}"
    File /oname=NomIME32.dll "${SOLUTION_DIR}\bin\Release\Win32\NomIME.dll"
    File "${SOLUTION_DIR}\data\nom_dict_single.tsv"
    File "${SOLUTION_DIR}\data\nom_dict_word.tsv"
    File "${SOLUTION_DIR}\data\NomKeyboardSerif.ttf"
    File "${SOLUTION_DIR}\data\CandidateFont-LICENSE.txt"

    WriteUninstaller "$INSTDIR\uninstall.exe"
    WriteRegStr HKLM "${ARP}" "DisplayName" "Nom Keyboard"
    WriteRegStr HKLM "${ARP}" "InstallLocation" "$INSTDIR"
    WriteRegStr HKLM "${ARP}" "UninstallString" '$\"$INSTDIR\uninstall.exe$\"'
    WriteRegStr HKLM "${ARP}" "DisplayVersion" "1.1.5"
    WriteRegStr HKLM "${ARP}" "Publisher" "Nom Keyboard Project"
    WriteRegDWORD HKLM "${ARP}" "NoModify" 1
    WriteRegDWORD HKLM "${ARP}" "NoRepair" 1
    !insertmacro RegisterBoth ""
    Delete "$INSTDIR\NomNaTong-Regular.ttf"
    Delete "$INSTDIR\NomNaTong-LICENSE.txt"
    SetShellVarContext all
    ${If} ${RunningX64}
        CreateShortcut "$SMPROGRAMS\Nom Keyboard Settings.lnk" "$WINDIR\SysWOW64\rundll32.exe" '"$INSTDIR\NomIME32.dll",NomKeyboardSettings'
    ${Else}
        CreateShortcut "$SMPROGRAMS\Nom Keyboard Settings.lnk" "$SYSDIR\rundll32.exe" '"$INSTDIR\NomIME32.dll",NomKeyboardSettings'
    ${EndIf}
SectionEnd

!macro RemoveFile NAME
    ${DoWhile} ${FileExists} "$INSTDIR\${NAME}"
        ClearErrors
        Delete "$INSTDIR\${NAME}"
        ${If} ${Errors}
            StrCpy $LockPath "$INSTDIR\${NAME}"
            Call un.FindLockOwners
            DetailPrint "Could not remove $LockPath. $LockOwners"
            MessageBox MB_RETRYCANCEL|MB_ICONEXCLAMATION "Could not remove $LockPath. Close the application using it and retry.$\r$\n$LockOwners" /SD IDCANCEL IDRETRY +3
            SetErrorLevel 32
            Abort
        ${EndIf}
    ${Loop}
!macroend

Section "Uninstall"
    ; Unregister first so new applications stop loading the DLL while we wait.
    !insertmacro RegisterBoth "/u"
    !insertmacro RemoveFile "NomIME64.dll"
    !insertmacro RemoveFile "NomIME32.dll"
    !insertmacro RemoveFile "NomIME64.dll.old"
    !insertmacro RemoveFile "NomIME32.dll.old"
    !insertmacro RemoveFile "NomIME64.dll.new"
    !insertmacro RemoveFile "NomIME32.dll.new"
    !insertmacro RemoveFile "nom_dict_single.tsv"
    !insertmacro RemoveFile "nom_dict_word.tsv"
    !insertmacro RemoveFile "NomKeyboardSerif.ttf"
    !insertmacro RemoveFile "CandidateFont-LICENSE.txt"
    SetShellVarContext all
    Delete "$SMPROGRAMS\Nom Keyboard Settings.lnk"
    DeleteRegKey HKLM "${ARP}"
    DeleteRegKey HKCU "Software\NomKeyboard"
    Delete "$INSTDIR\uninstall.exe"
    RMDir "$INSTDIR"
SectionEnd
