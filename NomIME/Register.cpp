// Register.cpp — DLL registration and IME profile management
#include "Register.h"

static const wchar_t c_szInfoKeyPrefix[] = L"CLSID\\";
static const wchar_t c_szInProcSvr32[]   = L"InProcServer32";
static const wchar_t c_szModelName[]     = L"ThreadingModel";

BOOL RegisterServer() {
    wchar_t szModule[MAX_PATH] = {};
    GetModuleFileNameW(g_hInst, szModule, MAX_PATH);

    // Build the CLSID string
    LPOLESTR pszCLSID = nullptr;
    if (FAILED(StringFromCLSID(CLSID_NomTextService, &pszCLSID))) return FALSE;

    std::wstring keyPath = c_szInfoKeyPrefix;
    keyPath += pszCLSID;

    HKEY hKey = nullptr;
    DWORD dw;
    if (RegCreateKeyExW(HKEY_CLASSES_ROOT, keyPath.c_str(), 0, NULL,
        REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, &dw) != ERROR_SUCCESS)
    {
        CoTaskMemFree(pszCLSID);
        return FALSE;
    }
    RegSetValueExW(hKey, NULL, 0, REG_SZ, (const BYTE*)TEXTSERVICE_DESC,
        (DWORD)(wcslen(TEXTSERVICE_DESC) + 1) * sizeof(wchar_t));

    HKEY hSubKey = nullptr;
    if (RegCreateKeyExW(hKey, c_szInProcSvr32, 0, NULL,
        REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hSubKey, &dw) == ERROR_SUCCESS)
    {
        RegSetValueExW(hSubKey, NULL, 0, REG_SZ, (const BYTE*)szModule,
            (DWORD)(wcslen(szModule) + 1) * sizeof(wchar_t));
        RegSetValueExW(hSubKey, c_szModelName, 0, REG_SZ, (const BYTE*)TEXTSERVICE_MODEL,
            (DWORD)(wcslen(TEXTSERVICE_MODEL) + 1) * sizeof(wchar_t));
        RegCloseKey(hSubKey);
    }
    RegCloseKey(hKey);
    CoTaskMemFree(pszCLSID);
    return TRUE;
}

BOOL UnregisterServer() {
    LPOLESTR pszCLSID = nullptr;
    if (FAILED(StringFromCLSID(CLSID_NomTextService, &pszCLSID))) return FALSE;

    std::wstring keyPath = c_szInfoKeyPrefix;
    keyPath += pszCLSID;
    keyPath += L"\\" + std::wstring(c_szInProcSvr32);

    RegDeleteKeyW(HKEY_CLASSES_ROOT, keyPath.c_str());

    keyPath = c_szInfoKeyPrefix;
    keyPath += pszCLSID;
    RegDeleteKeyW(HKEY_CLASSES_ROOT, keyPath.c_str());

    CoTaskMemFree(pszCLSID);
    return TRUE;
}

BOOL RegisterProfiles() {
    ITfInputProcessorProfileMgr* pProfileMgr = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_TF_InputProcessorProfiles, NULL, CLSCTX_INPROC_SERVER,
        IID_ITfInputProcessorProfileMgr, (void**)&pProfileMgr);

    if (FAILED(hr) || !pProfileMgr) return FALSE;

    wchar_t szModule[MAX_PATH] = {};
    GetModuleFileNameW(g_hInst, szModule, MAX_PATH);

    // Pick display name based on system UI language
    LANGID uiLang = GetUserDefaultUILanguage();
    WORD priLang = PRIMARYLANGID(uiLang);
    const wchar_t* desc = (priLang == LANG_VIETNAMESE) ? TEXTSERVICE_DESC_VI : TEXTSERVICE_DESC_EN;

    hr = pProfileMgr->RegisterProfile(
        CLSID_NomTextService,
        TEXTSERVICE_LANGID,
        GUID_NomProfile,
        desc,
        (ULONG)wcslen(desc),
        szModule,
        (ULONG)wcslen(szModule),
        0,      // icon index
        NULL,   // hklSubstitute
        0,      // dwPreferredLayout
        TRUE,   // fEnabledByDefault
        0       // dwFlags
    );

    pProfileMgr->Release();
    return SUCCEEDED(hr);
}

BOOL UnregisterProfiles() {
    ITfInputProcessorProfileMgr* pProfileMgr = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_TF_InputProcessorProfiles, NULL, CLSCTX_INPROC_SERVER,
        IID_ITfInputProcessorProfileMgr, (void**)&pProfileMgr);

    if (FAILED(hr) || !pProfileMgr) return FALSE;

    hr = pProfileMgr->UnregisterProfile(
        CLSID_NomTextService,
        TEXTSERVICE_LANGID,
        GUID_NomProfile,
        0
    );

    pProfileMgr->Release();
    return SUCCEEDED(hr);
}

BOOL RegisterCategories() {
    ITfCategoryMgr* pCategoryMgr = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_TF_CategoryMgr, NULL, CLSCTX_INPROC_SERVER,
        IID_ITfCategoryMgr, (void**)&pCategoryMgr);

    if (FAILED(hr) || !pCategoryMgr) return FALSE;

    // Register as TIP (Text Input Processor)
    hr = pCategoryMgr->RegisterCategory(
        CLSID_NomTextService,
        GUID_TFCAT_TIP_KEYBOARD,
        CLSID_NomTextService);

    pCategoryMgr->Release();
    return SUCCEEDED(hr);
}

BOOL UnregisterCategories() {
    ITfCategoryMgr* pCategoryMgr = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_TF_CategoryMgr, NULL, CLSCTX_INPROC_SERVER,
        IID_ITfCategoryMgr, (void**)&pCategoryMgr);

    if (FAILED(hr) || !pCategoryMgr) return FALSE;

    hr = pCategoryMgr->UnregisterCategory(
        CLSID_NomTextService,
        GUID_TFCAT_TIP_KEYBOARD,
        CLSID_NomTextService);

    pCategoryMgr->Release();
    return SUCCEEDED(hr);
}
