; NomIME NSIS Installer Script
; Generates setup.exe for Nom Keyboard
; -------------------------------------------------------
; IMPORTANT: Build NomIME DLLs (Release|x64 and Release|Win32) BEFORE running this.

!include "MUI2.nsh"
!include "x64.nsh"
!include "LogicLib.nsh"

; ---- Configurable paths ----
!ifndef SOLUTION_DIR
    !define SOLUTION_DIR ".."
!endif

; ---- General ----
Name "Nom Keyboard"
OutFile "setup.exe"
Unicode true
RequestExecutionLevel admin
InstallDir "$PROGRAMFILES\NomKeyboard"

; ---- Version info ----
VIProductVersion "1.0.0.0"
VIAddVersionKey "ProductName" "Nom Keyboard"
VIAddVersionKey "FileDescription" "Nom Keyboard IME Installer"
VIAddVersionKey "FileVersion" "1.0.0.0"
VIAddVersionKey "ProductVersion" "1.0.0.0"
VIAddVersionKey "LegalCopyright" "Copyright (C) 2025 Nom Keyboard Project"

; ---- Interface Settings ----
!define MUI_ABORTWARNING
!define MUI_WELCOMEPAGE_TITLE "Welcome to Nom Keyboard Setup"
!define MUI_WELCOMEPAGE_TEXT "This wizard will install Nom Keyboard - a Han Nom input method for Windows.$\n$\nBan phim Han Nom cho Windows."

; ---- Pages ----
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

; ---- Languages ----
!insertmacro MUI_LANGUAGE "English"

; ---- Sections ----

Section "Nom Keyboard" SecMain
    SectionIn RO
    SetOutPath "$INSTDIR"

    ; DLLs
    File /oname=NomIME64.dll "${SOLUTION_DIR}\bin\Release\x64\NomIME.dll"
    File /oname=NomIME32.dll "${SOLUTION_DIR}\bin\Release\Win32\NomIME.dll"

    ; Dictionary data files
    File "${SOLUTION_DIR}\data\nom_dict_single.tsv"
    File "${SOLUTION_DIR}\data\nom_dict_word.tsv"

    ; Write uninstaller
    WriteUninstaller "$INSTDIR\uninstall.exe"

    ; Registry entries for Add/Remove Programs
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\NomKeyboard" \
        "DisplayName" "Nom Keyboard"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\NomKeyboard" \
        "UninstallString" "$\"$INSTDIR\uninstall.exe$\""
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\NomKeyboard" \
        "DisplayVersion" "1.0.0.0"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\NomKeyboard" \
        "Publisher" "Nom Keyboard Project"
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\NomKeyboard" \
        "NoModify" 1
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\NomKeyboard" \
        "NoRepair" 1

    ; Default preferences
    WriteRegDWORD HKCU "Software\NomKeyboard" "ToneStyleOld" 0
    WriteRegDWORD HKCU "Software\NomKeyboard" "ShorthandEnabled" 1
    WriteRegDWORD HKCU "Software\NomKeyboard" "SegmentMode" 1

    ; Register the IME DLL(s)
    ${If} ${RunningX64}
        ExecWait '"$SYSDIR\regsvr32.exe" /s "$INSTDIR\NomIME64.dll"'
        ExecWait '"$WINDIR\SysWOW64\regsvr32.exe" /s "$INSTDIR\NomIME32.dll"'
    ${Else}
        ExecWait '"$SYSDIR\regsvr32.exe" /s "$INSTDIR\NomIME32.dll"'
    ${EndIf}

SectionEnd

; ---- Uninstaller ----

Section "Uninstall"

    ; Unregister the IME DLL(s)
    ${If} ${RunningX64}
        ExecWait '"$SYSDIR\regsvr32.exe" /u /s "$INSTDIR\NomIME64.dll"'
        ExecWait '"$WINDIR\SysWOW64\regsvr32.exe" /u /s "$INSTDIR\NomIME32.dll"'
    ${Else}
        ExecWait '"$SYSDIR\regsvr32.exe" /u /s "$INSTDIR\NomIME32.dll"'
    ${EndIf}

    ; Remove files
    Delete "$INSTDIR\NomIME64.dll"
    Delete "$INSTDIR\NomIME32.dll"
    Delete "$INSTDIR\nom_dict_single.tsv"
    Delete "$INSTDIR\nom_dict_word.tsv"
    Delete "$INSTDIR\uninstall.exe"

    ; Remove directory
    RMDir "$INSTDIR"

    ; Remove registry entries
    DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\NomKeyboard"
    DeleteRegKey HKCU "Software\NomKeyboard"

SectionEnd
