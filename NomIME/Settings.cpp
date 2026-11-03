#include "TextService.h"
#include "SettingsResource.h"

namespace {
constexpr wchar_t settingsKey[] = L"Software\\NomKeyboard";
struct Option { int control; const wchar_t* name; bool initial; };
constexpr Option tone{IDC_TONE, L"ToneStyleOld", false};
constexpr Option shorthand{IDC_SHORT, L"ShorthandEnabled", true};
constexpr Option segment{IDC_SEGMENT, L"SegmentMode", true};

bool ReadOption(const Option& option) {
    DWORD value{}, size = sizeof(value);
    return RegGetValueW(HKEY_CURRENT_USER, settingsKey, option.name, RRF_RT_REG_DWORD,
        nullptr, &value, &size) == ERROR_SUCCESS ? value != 0 : option.initial;
}

INT_PTR CALLBACK ConfigureDialog(HWND hwnd, UINT msg, WPARAM wp, LPARAM) {
    if (msg == WM_INITDIALOG) {
        for (auto option : {tone, shorthand, segment})
            CheckDlgButton(hwnd, option.control, ReadOption(option) ? BST_CHECKED : BST_UNCHECKED);
        return TRUE;
    }
    if (msg != WM_COMMAND) return FALSE;
    if (LOWORD(wp) == IDOK) {
        for (auto option : {tone, shorthand, segment}) {
            DWORD value = IsDlgButtonChecked(hwnd, option.control) == BST_CHECKED;
            if (RegSetKeyValueW(HKEY_CURRENT_USER, settingsKey, option.name, REG_DWORD,
                &value, sizeof(value)) != ERROR_SUCCESS) {
                ShowWindow(GetDlgItem(hwnd, IDC_SAVE_ERROR), SW_SHOW);
                return TRUE;
            }
        }
    } else if (LOWORD(wp) != IDCANCEL) return FALSE;
    EndDialog(hwnd, LOWORD(wp));
    return TRUE;
}

INT_PTR ShowSettings(HWND parent) {
    return DialogBoxParamW(g_hInst, MAKEINTRESOURCEW(IDD_SETTINGS), parent, ConfigureDialog, 0);
}
}

void NomTextService::LoadPreferences() {
    toneStyleOld_ = ReadOption(tone);
    shorthandEnabled_ = ReadOption(shorthand);
    segmentMode_ = ReadOption(segment);
}

STDMETHODIMP NomTextService::GetDisplayName(BSTR* name) {
    if (!name) return E_POINTER;
    *name = SysAllocString(L"Nom Keyboard");
    return *name ? S_OK : E_OUTOFMEMORY;
}

STDMETHODIMP NomTextService::Show(HWND parent, LANGID, REFGUID) {
    if (ShowSettings(parent) == -1) return HRESULT_FROM_WIN32(GetLastError());
    LoadPreferences();
    return S_OK;
}

// Entry point for the Start menu shortcut when the host offers no options button.
extern "C" void CALLBACK NomKeyboardSettings(HWND parent, HINSTANCE, LPSTR, int) {
    ShowSettings(parent);
}
