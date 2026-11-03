; Return the owner names and PIDs for the exact path in $LockPath.
; Restart Manager only enumerates applications. The user controls when to close them.
Var LockPath
Var LockOwners
!macro LockOwnersFunction PREFIX
Function ${PREFIX}FindLockOwners
    StrCpy $LockOwners ""
    System::Call 'rstrtmgr::RmStartSession(*i .r1, i 0, w .r2)i.r0'
    ${If} $0 != 0
        Return
    ${EndIf}
    StrCpy $0 $LockPath
    System::Call '*(&w${NSIS_MAX_STRLEN} r0)p.r2'
    System::Call 'rstrtmgr::RmRegisterResources(i r1, i 1, *p r2, i 0, p 0, i 0, p 0)i.r0'
    System::Free $2
    ${If} $0 = 0
        System::Call 'rstrtmgr::RmGetList(i r1, *i .r2, *i 0, p 0, *i .r3)i.r0'
        ${If} $0 = 234 ; ERROR_MORE_DATA
        ${AndIf} $2 > 0
            ; RM_PROCESS_INFO is 668 bytes, with the application name at offset 12.
            IntOp $3 $2 * 668
            System::Alloc $3
            Pop $4
            ${If} $4 P<> 0
                System::Call 'rstrtmgr::RmGetList(i r1, *i .r3, *i r2 r5, p r4, *i .r6)i.r0'
                ${If} $0 = 0
                    StrCpy $6 $4
                    ${DoWhile} $5 > 0
                        System::Call '*$6(i .r7)'
                        IntOp $8 $6 + 12
                        System::Call '*$8(&w256 .r9)'
                        StrCpy $LockOwners "$LockOwners$\r$\n$9 (PID $7)"
                        IntOp $6 $6 + 668
                        IntOp $5 $5 - 1
                    ${Loop}
                ${EndIf}
                System::Free $4
            ${EndIf}
        ${EndIf}
    ${EndIf}
    System::Call 'rstrtmgr::RmEndSession(i r1)'
FunctionEnd
!macroend

!insertmacro LockOwnersFunction ""
!insertmacro LockOwnersFunction "un."

; An exclusive write handle cannot be opened while a DLL is mapped as an image.
; Check before replacing either DLL. Keep the uninstaller available on cancel.
!macro WaitForFile PREFIX NAME
    StrCpy $LockPath "$INSTDIR\${NAME}"
    ${DoWhile} ${FileExists} "$LockPath"
        System::Call 'kernel32::CreateFileW(w "$LockPath", i 0x40000000, i 0, p 0, i 3, i 0, p 0)p.r0'
        ${If} $0 P<> -1
            System::Call 'kernel32::CloseHandle(p r0)'
            ${ExitDo}
        ${EndIf}
        Call ${PREFIX}FindLockOwners
        DetailPrint "Cannot replace $LockPath. $LockOwners"
        MessageBox MB_RETRYCANCEL|MB_ICONEXCLAMATION "Cannot change $LockPath.$\r$\nSwitch to another keyboard, save your work and close the applications using this file. Then choose Retry.$\r$\n$LockOwners" /SD IDCANCEL IDRETRY +3
        SetErrorLevel 32
        Abort
    ${Loop}
!macroend
