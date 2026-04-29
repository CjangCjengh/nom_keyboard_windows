; NomIME NSIS Installer Script
; Generates setup.exe for Nom Keyboard (Windows IME)
; -------------------------------------------------------
; Prerequisites: NSIS 3.x (https://nsis.sourceforge.io/)
;   Install NSIS and add to PATH, then build from VS or run:
;   makensis NomIMESetup.nsi

!include "MUI2.nsh"
!include "x64.nsh"
!include "LogicLib.nsh"

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
!define MUI_ICON "${NSISDIR}\Contrib\Graphics\Icons\modern-install.ico"
!define MUI_UNICON "${NSISDIR}\Contrib\Graphics\Icons\modern-uninstall.ico"
!define MUI_WELCOMEPAGE_TITLE "Welcome to Nom Keyboard Setup"
!define MUI_WELCOMEPAGE_TEXT "This wizard will install Nom Keyboard - a Hán Nôm input method for Windows.$\n$\nBàn phím Nôm cho Windows - nhập chữ Nôm qua bộ gõ Telex."

; ---- Pages ----
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

; ---- Languages ----
!insertmacro MUI_LANGUAGE "English"
!insertmacro MUI_LANGUAGE "Vietnamese"

; ---- Sections ----

Section "Nom Keyboard" SecMain
    SectionIn RO

    SetOutPath "$INSTDIR"

    ; Determine architecture and copy the right DLL
    ${If} ${RunningX64}
        ; 64-bit system: install both 32-bit and 64-bit DLLs
        ; The 64-bit DLL is used by 64-bit applications
        ; The 32-bit DLL is used by 32-bit applications (WOW64)
        File /oname=NomIME64.dll "..\bin\Release\x64\NomIME.dll"
        File /oname=NomIME32.dll "..\bin\Release\Win32\NomIME.dll"
    ${Else}
        ; 32-bit system: only 32-bit DLL
        File /oname=NomIME32.dll "..\bin\Release\Win32\NomIME.dll"
    ${EndIf}

    ; Dictionary data files
    File "..\data\nom_dict_single.tsv"
    File "..\data\nom_dict_word.tsv"

    ; Write uninstaller
    WriteUninstaller "$INSTDIR\uninstall.exe"

    ; Registry entries for Add/Remove Programs
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\NomKeyboard" \
        "DisplayName" "Nom Keyboard (Bàn phím Hán Nôm)"
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
        ; Register 64-bit DLL
        ExecWait '"$SYSDIR\regsvr32.exe" /s "$INSTDIR\NomIME64.dll"'
        ; Register 32-bit DLL for WOW64 apps
        ; On 64-bit Windows, $SYSDIR is System32 (64-bit) and we need SysWOW64 for 32-bit
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

    ; Remove user data (optional - commented out to preserve user dictionary)
    ; RMDir /r "$LOCALAPPDATA\NomKeyboard"

    ; Remove registry entries
    DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\NomKeyboard"
    DeleteRegKey HKCU "Software\NomKeyboard"

SectionEnd
