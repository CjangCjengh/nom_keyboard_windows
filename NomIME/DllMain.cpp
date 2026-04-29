// DllMain.cpp — DLL entry point and COM exports
#include "Globals.h"
#include "ClassFactory.h"
#include "Register.h"

HINSTANCE g_hInst = nullptr;
LONG g_cRefDll = 0;

BOOL APIENTRY DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved) {
    switch (dwReason) {
    case DLL_PROCESS_ATTACH:
        g_hInst = hInstance;
        DisableThreadLibraryCalls(hInstance);
        break;
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv) {
    if (!ppv) return E_INVALIDARG;
    *ppv = nullptr;

    if (!IsEqualCLSID(rclsid, CLSID_NomTextService)) {
        return CLASS_E_CLASSNOTAVAILABLE;
    }

    ClassFactory* pFactory = new (std::nothrow) ClassFactory();
    if (!pFactory) return E_OUTOFMEMORY;

    HRESULT hr = pFactory->QueryInterface(riid, ppv);
    pFactory->Release();
    return hr;
}

STDAPI DllCanUnloadNow() {
    return (g_cRefDll <= 0) ? S_OK : S_FALSE;
}

STDAPI DllRegisterServer() {
    if (!RegisterServer()) return E_FAIL;
    if (!RegisterProfiles()) {
        UnregisterServer();
        return E_FAIL;
    }
    if (!RegisterCategories()) {
        UnregisterProfiles();
        UnregisterServer();
        return E_FAIL;
    }
    return S_OK;
}

STDAPI DllUnregisterServer() {
    UnregisterCategories();
    UnregisterProfiles();
    UnregisterServer();
    return S_OK;
}
