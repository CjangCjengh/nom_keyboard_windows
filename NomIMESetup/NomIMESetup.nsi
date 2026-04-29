; NomIME NSIS Installer Script
; Generates setup.exe for Nom Keyboard

!include "MUI2.nsh"
!include "x64.nsh"
!include "LogicLib.nsh"

!ifndef SOLUTION_DIR
    !define SOLUTION_DIR ".."
!endif

Name "Nom Keyboard"
OutFile "setup.exe"
Unicode true
RequestExecutionLevel admin
InstallDir "$PROGRAMFILES\NomKeyboard"

VIProductVersion "1.0.0.0"
VIAddVersionKey "ProductName" "Nom Keyboard"
VIAddVersionKey "FileDescription" "Nom Keyboard IME Installer"
VIAddVersionKey "FileVersion" "1.0.0.0"
VIAddVersionKey "ProductVersion" "1.0.0.0"
VIAddVersionKey "LegalCopyright" "Copyright (C) 2025 Nom Keyboard Project"

!define MUI_ABORTWARNING
!define MUI_WELCOMEPAGE_TITLE "Welcome to Nom Keyboard Setup"
!define MUI_WELCOMEPAGE_TEXT "This wizard will install Nom Keyboard - a Han Nom input method for Windows."
!define MUI_FINISHPAGE_TEXT "Nom Keyboard has been installed.$\n$\nIf upgrading, you may need to restart your computer for the new version to take effect."

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

Var NeedReboot

Section "Nom Keyboard" SecMain
    SectionIn RO
    SetOutPath "$INSTDIR"
    StrCpy $NeedReboot "0"

    ; Unregister old DLLs if upgrading
    IfFileExists "$INSTDIR\NomIME64.dll" 0 skip_unreg64
        ExecWait '"$SYSDIR\regsvr32.exe" /u /s "$INSTDIR\NomIME64.dll"'
    skip_unreg64:
    IfFileExists "$INSTDIR\NomIME32.dll" 0 skip_unreg32
        ExecWait '"$WINDIR\SysWOW64\regsvr32.exe" /u /s "$INSTDIR\NomIME32.dll"'
    skip_unreg32:

    ; Gently stop ctfmon and TextInputHost (not explorer!)
    ExecWait 'cmd /c taskkill /f /im ctfmon.exe 2>nul'
    ExecWait 'cmd /c taskkill /f /im TextInputHost.exe 2>nul'
    Sleep 800

    ; Try to copy DLLs. Use /r flag (overwrite) with retry.
    ; If the file is still locked, NSIS will use the built-in overwrite retry.
    SetOverwrite try
    ClearErrors
    File /oname=NomIME64.dll "${SOLUTION_DIR}\bin\Release\x64\NomIME.dll"
    IfErrors 0 ok64
        ; File locked (explorer etc still holds it) - schedule replace on reboot
        SetOutPath "$INSTDIR"
        Rename "$INSTDIR\NomIME64.dll" "$INSTDIR\NomIME64.dll.old"
        File /oname=NomIME64.dll "${SOLUTION_DIR}\bin\Release\x64\NomIME.dll"
        IfErrors 0 ok64
            ; Still locked - use temp file + reboot replace
            File /oname=NomIME64.dll.new "${SOLUTION_DIR}\bin\Release\x64\NomIME.dll"
            Delete /REBOOTOK "$INSTDIR\NomIME64.dll"
            Rename "$INSTDIR\NomIME64.dll.new" "$INSTDIR\NomIME64.dll"
            StrCpy $NeedReboot "1"
    ok64:

    ClearErrors
    File /oname=NomIME32.dll "${SOLUTION_DIR}\bin\Release\Win32\NomIME.dll"
    IfErrors 0 ok32
        Rename "$INSTDIR\NomIME32.dll" "$INSTDIR\NomIME32.dll.old"
        File /oname=NomIME32.dll "${SOLUTION_DIR}\bin\Release\Win32\NomIME.dll"
        IfErrors 0 ok32
            File /oname=NomIME32.dll.new "${SOLUTION_DIR}\bin\Release\Win32\NomIME.dll"
            Delete /REBOOTOK "$INSTDIR\NomIME32.dll"
            Rename "$INSTDIR\NomIME32.dll.new" "$INSTDIR\NomIME32.dll"
            StrCpy $NeedReboot "1"
    ok32:

    ; Dictionary data (these are never locked)
    SetOverwrite on
    File "${SOLUTION_DIR}\data\nom_dict_single.tsv"
    File "${SOLUTION_DIR}\data\nom_dict_word.tsv"

    WriteUninstaller "$INSTDIR\uninstall.exe"

    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\NomKeyboard" "DisplayName" "Nom Keyboard"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\NomKeyboard" "UninstallString" "$\"$INSTDIR\uninstall.exe$\""
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\NomKeyboard" "DisplayVersion" "1.0.0.0"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\NomKeyboard" "Publisher" "Nom Keyboard Project"
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\NomKeyboard" "NoModify" 1
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\NomKeyboard" "NoRepair" 1

    WriteRegDWORD HKCU "Software\NomKeyboard" "ToneStyleOld" 0
    WriteRegDWORD HKCU "Software\NomKeyboard" "ShorthandEnabled" 1
    WriteRegDWORD HKCU "Software\NomKeyboard" "SegmentMode" 1

    ; Register new DLLs
    ${If} ${RunningX64}
        ExecWait '"$SYSDIR\regsvr32.exe" /s "$INSTDIR\NomIME64.dll"'
        ExecWait '"$WINDIR\SysWOW64\regsvr32.exe" /s "$INSTDIR\NomIME32.dll"'
    ${Else}
        ExecWait '"$SYSDIR\regsvr32.exe" /s "$INSTDIR\NomIME32.dll"'
    ${EndIf}

    ; Restart ctfmon (NOT explorer)
    Exec '"$SYSDIR\ctfmon.exe"'

    ; Clean up old files if rename succeeded
    Delete "$INSTDIR\NomIME64.dll.old"
    Delete "$INSTDIR\NomIME32.dll.old"

    ; If reboot needed, tell the user
    StrCmp $NeedReboot "1" 0 done
        MessageBox MB_YESNO "Some files were in use. A reboot is needed to complete the update.$\n$\nRestart now?" IDYES reboot_now
        Goto done
    reboot_now:
        Reboot
    done:

SectionEnd

Section "Uninstall"

    ${If} ${RunningX64}
        ExecWait '"$SYSDIR\regsvr32.exe" /u /s "$INSTDIR\NomIME64.dll"'
        ExecWait '"$WINDIR\SysWOW64\regsvr32.exe" /u /s "$INSTDIR\NomIME32.dll"'
    ${Else}
        ExecWait '"$SYSDIR\regsvr32.exe" /u /s "$INSTDIR\NomIME32.dll"'
    ${EndIf}

    ExecWait 'cmd /c taskkill /f /im ctfmon.exe 2>nul'
    ExecWait 'cmd /c taskkill /f /im TextInputHost.exe 2>nul'
    Sleep 800

    Delete /REBOOTOK "$INSTDIR\NomIME64.dll"
    Delete /REBOOTOK "$INSTDIR\NomIME32.dll"
    Delete "$INSTDIR\NomIME64.dll.old"
    Delete "$INSTDIR\NomIME32.dll.old"
    Delete "$INSTDIR\NomIME64.dll.new"
    Delete "$INSTDIR\NomIME32.dll.new"
    Delete "$INSTDIR\nom_dict_single.tsv"
    Delete "$INSTDIR\nom_dict_word.tsv"
    Delete "$INSTDIR\uninstall.exe"
    RMDir "$INSTDIR"

    DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\NomKeyboard"
    DeleteRegKey HKCU "Software\NomKeyboard"

    Exec '"$SYSDIR\ctfmon.exe"'

    IfRebootFlag 0 un_done
        MessageBox MB_YESNO "Some files were in use. A reboot is needed to complete removal.$\n$\nRestart now?" IDYES un_reboot
        Goto un_done
    un_reboot:
        Reboot
    un_done:

SectionEnd
