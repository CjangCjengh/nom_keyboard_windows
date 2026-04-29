// ClassFactory.cpp — COM class factory implementation
#include "ClassFactory.h"
#include "TextService.h"

ClassFactory::ClassFactory() : refCount_(1) {}
ClassFactory::~ClassFactory() {}

STDMETHODIMP ClassFactory::QueryInterface(REFIID riid, void** ppvObj) {
    if (!ppvObj) return E_INVALIDARG;
    *ppvObj = nullptr;
    if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IClassFactory)) {
        *ppvObj = static_cast<IClassFactory*>(this);
        AddRef();
        return S_OK;
    }
    return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) ClassFactory::AddRef() {
    return InterlockedIncrement(&refCount_);
}

STDMETHODIMP_(ULONG) ClassFactory::Release() {
    LONG r = InterlockedDecrement(&refCount_);
    if (r == 0) delete this;
    return r;
}

STDMETHODIMP ClassFactory::CreateInstance(IUnknown* pUnkOuter, REFIID riid, void** ppvObj) {
    if (pUnkOuter) return CLASS_E_NOAGGREGATION;
    if (!ppvObj) return E_INVALIDARG;
    *ppvObj = nullptr;

    NomTextService* pService = new (std::nothrow) NomTextService();
    if (!pService) return E_OUTOFMEMORY;

    HRESULT hr = pService->QueryInterface(riid, ppvObj);
    pService->Release();
    return hr;
}

STDMETHODIMP ClassFactory::LockServer(BOOL fLock) {
    if (fLock) {
        InterlockedIncrement(&g_cRefDll);
    } else {
        InterlockedDecrement(&g_cRefDll);
    }
    return S_OK;
}
