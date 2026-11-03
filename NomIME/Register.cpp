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

    CoTaskMemFree(pszCLSID);
    auto write = [](const std::wstring& path, const wchar_t* name, const wchar_t* value) {
        return RegSetKeyValueW(HKEY_CLASSES_ROOT, path.c_str(), name, REG_SZ, value,
            static_cast<DWORD>((wcslen(value) + 1) * sizeof(wchar_t))) == ERROR_SUCCESS;
    };
    const std::wstring server = keyPath + L"\\" + c_szInProcSvr32;
    return write(keyPath, nullptr, TEXTSERVICE_DESC_EN) && write(server, nullptr, szModule) &&
        write(server, c_szModelName, TEXTSERVICE_MODEL);
}

BOOL UnregisterServer() {
    LPOLESTR clsid = nullptr;
    if (FAILED(StringFromCLSID(CLSID_NomTextService, &clsid))) return FALSE;
    const std::wstring path = std::wstring(c_szInfoKeyPrefix) + clsid;
    CoTaskMemFree(clsid);
    LSTATUS result = RegDeleteTreeW(HKEY_CLASSES_ROOT, path.c_str());
    return result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND || result == ERROR_PATH_NOT_FOUND;
}

BOOL RegisterProfiles() {
    ITfInputProcessorProfileMgr* pProfileMgr = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_TF_InputProcessorProfiles, NULL, CLSCTX_INPROC_SERVER,
        IID_ITfInputProcessorProfileMgr, (void**)&pProfileMgr);

    if (FAILED(hr) || !pProfileMgr) return FALSE;

    // Always unregister first so display name gets updated on reinstall/upgrade
    pProfileMgr->UnregisterProfile(
        CLSID_NomTextService,
        TEXTSERVICE_LANGID,
        GUID_NomProfile,
        0);

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

    // Register every capability category a modern TIP needs. TIP_KEYBOARD alone
    // is enough on Win10 classic Win32 apps, but Win11 runs Notepad, Settings,
    // the search box and the shell as "immersive" (packaged) apps -- a TIP that
    // omits IMMERSIVESUPPORT is not loaded there, so the IME appears to never
    // switch on. SYSTRAYSUPPORT/UIELEMENTENABLED/etc. round out tray + UI
    // hosting. DISPLAYATTRIBUTEPROVIDER is needed because we set a display
    // attribute (GUID_NomDisplayAttribute_Input) on the composition.
    //
    // Deliberately NOT registered: GUID_TFCAT_TIPCAP_INPUTMODECOMPARTMENT.
    // That category tells Win11 the TIP has an on/off "input mode" tracked via
    // GUID_COMPARTMENT_KEYBOARD_OPENCLOSE. Nom is always composing (no Latin/off
    // mode), so advertising it caused the IME to come up "closed" after switching
    // away and back -- selected but passing keystrokes straight through as Latin.
    // As a simple always-on keyboard TIP we omit it; ActivateEx also asserts the
    // open compartment defensively (see NomTextService::SetKeyboardOpen).
    static const GUID* const categories[] = {
        &GUID_TFCAT_TIP_KEYBOARD,
        &GUID_TFCAT_DISPLAYATTRIBUTEPROVIDER,
        &GUID_TFCAT_TIPCAP_SECUREMODE,
        &GUID_TFCAT_TIPCAP_UIELEMENTENABLED,
        &GUID_TFCAT_TIPCAP_COMLESS,
        &GUID_TFCAT_TIPCAP_WOW16,
        &GUID_TFCAT_TIPCAP_IMMERSIVESUPPORT,
        &GUID_TFCAT_TIPCAP_SYSTRAYSUPPORT,
    };

    BOOL ok = TRUE;
    for (const GUID* cat : categories) {
        if (FAILED(pCategoryMgr->RegisterCategory(
                CLSID_NomTextService, *cat, CLSID_NomTextService))) {
            ok = FALSE;
        }
    }

    pCategoryMgr->Release();
    return ok;
}

BOOL UnregisterCategories() {
    ITfCategoryMgr* pCategoryMgr = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_TF_CategoryMgr, NULL, CLSCTX_INPROC_SERVER,
        IID_ITfCategoryMgr, (void**)&pCategoryMgr);

    if (FAILED(hr) || !pCategoryMgr) return FALSE;

    // Mirror of the set registered in RegisterCategories().
    static const GUID* const categories[] = {
        &GUID_TFCAT_TIP_KEYBOARD,
        &GUID_TFCAT_DISPLAYATTRIBUTEPROVIDER,
        &GUID_TFCAT_TIPCAP_SECUREMODE,
        &GUID_TFCAT_TIPCAP_UIELEMENTENABLED,
        &GUID_TFCAT_TIPCAP_INPUTMODECOMPARTMENT,
        &GUID_TFCAT_TIPCAP_COMLESS,
        &GUID_TFCAT_TIPCAP_WOW16,
        &GUID_TFCAT_TIPCAP_IMMERSIVESUPPORT,
        &GUID_TFCAT_TIPCAP_SYSTRAYSUPPORT,
    };

    for (const GUID* cat : categories) {
        pCategoryMgr->UnregisterCategory(CLSID_NomTextService, *cat, CLSID_NomTextService);
    }

    pCategoryMgr->Release();
    return TRUE;
}
