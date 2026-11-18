// TextService.cpp — Main TSF Text Service implementation
#include "TextService.h"
#include "CandidateWindow.h"
#include "TelexEngine.h"
#include "NomDictionary.h"
#include "UserDictionary.h"
#include "NgramModel.h"
#include "StringUtil.h"
#include <unordered_set>

// ---- EditSession: helper to perform edits inside a TSF edit cookie ----
class NomTextService::EditSession : public ITfEditSession {
public:
    enum Action { SET_COMPOSING, COMMIT_TEXT, END_COMPOSITION, READ_LAYOUT };

    EditSession(NomTextService* pService, ITfContext* pContext, Action action,
        const std::wstring& text = L"", ITfComposition* compositionToEnd = nullptr)
        : refCount_(1), service_(pService), context_(pContext),
        action_(action), text_(text), generation_(service_->compositionGeneration_),
        compositionToEnd_(compositionToEnd)
    {
        service_->AddRef();
        context_->AddRef();
        if (compositionToEnd_) {
            compositionToEnd_->AddRef();
            ++service_->pendingCompositionEnds_;
        }
    }

    ~EditSession() {
        if (compositionToEnd_) {
            compositionToEnd_->Release();
            --service_->pendingCompositionEnds_;
        }
        SafeRelease(context_);
        service_->Release();
    }

    HRESULT RequestWrite() {
        // Writes must run after any queued end so that a new composition can
        // start even if the caret has already moved back into the old range.
        DWORD access = service_->pendingCompositionEnds_ ? TF_ES_ASYNC : TF_ES_ASYNCDONTCARE;
        HRESULT result = E_FAIL;
        HRESULT hr = context_->RequestEditSession(service_->clientId_, this, access | TF_ES_READWRITE, &result);
        return FAILED(hr) ? hr : result;
    }

    STDMETHODIMP QueryInterface(REFIID riid, void** ppvObj) override {
        if (!ppvObj) return E_INVALIDARG;
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfEditSession)) {
            *ppvObj = static_cast<ITfEditSession*>(this);
            AddRef();
            return S_OK;
        }
        *ppvObj = nullptr;
        return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return InterlockedIncrement(&refCount_); }
    STDMETHODIMP_(ULONG) Release() override {
        LONG r = InterlockedDecrement(&refCount_);
        if (r == 0) delete this;
        return r;
    }

    STDMETHODIMP DoEditSession(TfEditCookie ec) override {
        if (!service_ || !context_) return E_FAIL;

        switch (action_) {
        case READ_LAYOUT:
            if (context_ == service_->candidateContext_) {
                service_->layoutReadPending_ = false;
                service_->PresentCandidates(context_, ec);
            }
            break;
        case SET_COMPOSING: {
            if (generation_ != service_->compositionGeneration_) break;
            // Start or update composition
            if (!service_->composition_) {
                ITfInsertAtSelection* pInsert = nullptr;
                if (SUCCEEDED(context_->QueryInterface(IID_ITfInsertAtSelection, (void**)&pInsert))) {
                    ITfRange* pRange = nullptr;
                    // Insert the composing text directly (replaces any selected text)
                    if (SUCCEEDED(pInsert->InsertTextAtSelection(ec, 0,
                        text_.c_str(), (LONG)text_.size(), &pRange))) {
                        ITfContextComposition* pContextComp = nullptr;
                        if (SUCCEEDED(context_->QueryInterface(IID_ITfContextComposition,
                            (void**)&pContextComp))) {
                            pContextComp->StartComposition(ec, pRange,
                                service_, &service_->composition_);
                            pContextComp->Release();
                            if (service_->composition_) service_->WatchComposition(context_);
                        }
                        // Set cursor at end
                        pRange->Collapse(ec, TF_ANCHOR_END);
                        TF_SELECTION sel = {};
                        sel.range = pRange;
                        sel.style.ase = TF_AE_END;
                        sel.style.fInterimChar = FALSE;
                        context_->SetSelection(ec, 1, &sel);
                        pRange->Release();
                    }
                    pInsert->Release();
                }
            }
            else {
                // Composition already active: update the text in place
                ITfRange* pRange = nullptr;
                if (SUCCEEDED(service_->composition_->GetRange(&pRange))) {
                    pRange->SetText(ec, 0, text_.c_str(), (LONG)text_.size());
                    pRange->Collapse(ec, TF_ANCHOR_END);
                    TF_SELECTION sel = {};
                    sel.range = pRange;
                    sel.style.ase = TF_AE_END;
                    sel.style.fInterimChar = FALSE;
                    context_->SetSelection(ec, 1, &sel);
                    pRange->Release();
                }
            }
            service_->PresentCandidates(context_, ec);
            break;
        }
        case COMMIT_TEXT: {
            if (service_->composition_) {
                ITfRange* pRange = nullptr;
                if (SUCCEEDED(service_->composition_->GetRange(&pRange))) {
                    pRange->SetText(ec, 0, text_.c_str(), (LONG)text_.size());
                    pRange->Collapse(ec, TF_ANCHOR_END);
                    service_->composition_->EndComposition(ec);
                    service_->ReleaseComposition();
                    pRange->Release();
                }
            }
            else {
                // No composition, insert directly
                ITfInsertAtSelection* pInsert = nullptr;
                if (SUCCEEDED(context_->QueryInterface(IID_ITfInsertAtSelection, (void**)&pInsert))) {
                    pInsert->InsertTextAtSelection(ec, 0,
                        text_.c_str(), (LONG)text_.size(), NULL);
                    pInsert->Release();
                }
            }
            break;
        }
        case END_COMPOSITION: {
            if (compositionToEnd_) return compositionToEnd_->EndComposition(ec);
            if (service_->composition_) {
                service_->composition_->EndComposition(ec);
                service_->ReleaseComposition();
            }
            break;
        }
        }
        return S_OK;
    }

private:
    LONG refCount_;
    NomTextService* service_;
    ITfContext* context_;
    Action action_;
    std::wstring text_;
    unsigned generation_;
    ITfComposition* compositionToEnd_;
};

// ---- Constructor / Destructor ----

NomTextService::NomTextService()
    : refCount_(1), clientId_(TF_CLIENTID_NULL), threadMgr_(nullptr),
    threadMgrSinkCookie_(TF_INVALID_COOKIE), textEditSinkCookie_(TF_INVALID_COOKIE),
    composition_(nullptr),
    shorthandActive_(false), candidateWindow_(nullptr),
    toneStyleOld_(false), shorthandEnabled_(true), segmentMode_(true)
{
    InterlockedIncrement(&g_cRefDll);
}

NomTextService::~NomTextService() {
    InterlockedDecrement(&g_cRefDll);
}

// ---- IUnknown ----

STDMETHODIMP NomTextService::QueryInterface(REFIID riid, void** ppvObj) {
    if (!ppvObj) return E_INVALIDARG;
    *ppvObj = nullptr;

    if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfTextInputProcessor) ||
        IsEqualIID(riid, IID_ITfTextInputProcessorEx)) {
        *ppvObj = static_cast<ITfTextInputProcessorEx*>(this);
    }
    else if (IsEqualIID(riid, IID_ITfFnConfigure) || IsEqualIID(riid, IID_ITfFunction)) {
        *ppvObj = static_cast<ITfFnConfigure*>(this);
    }
    else if (IsEqualIID(riid, IID_ITfKeyEventSink)) {
        *ppvObj = static_cast<ITfKeyEventSink*>(this);
    }
    else if (IsEqualIID(riid, IID_ITfCompositionSink)) {
        *ppvObj = static_cast<ITfCompositionSink*>(this);
    }
    else if (IsEqualIID(riid, IID_ITfThreadMgrEventSink)) {
        *ppvObj = static_cast<ITfThreadMgrEventSink*>(this);
    }
    else if (IsEqualIID(riid, IID_ITfTextLayoutSink)) {
        *ppvObj = static_cast<ITfTextLayoutSink*>(this);
    }
    else if (IsEqualIID(riid, IID_ITfTextEditSink)) {
        *ppvObj = static_cast<ITfTextEditSink*>(this);
    }
    else {
        return E_NOINTERFACE;
    }
    AddRef();
    return S_OK;
}

STDMETHODIMP_(ULONG) NomTextService::AddRef() {
    return InterlockedIncrement(&refCount_);
}

STDMETHODIMP_(ULONG) NomTextService::Release() {
    LONG r = InterlockedDecrement(&refCount_);
    if (r == 0) delete this;
    return r;
}

// ---- ITfTextInputProcessorEx ----

STDMETHODIMP NomTextService::Activate(ITfThreadMgr* pThreadMgr, TfClientId tfClientId) {
    return ActivateEx(pThreadMgr, tfClientId, 0);
}

STDMETHODIMP NomTextService::ActivateEx(ITfThreadMgr* pThreadMgr, TfClientId tfClientId, DWORD dwFlags) {
    threadMgr_ = pThreadMgr;
    threadMgr_->AddRef();
    clientId_ = tfClientId;

    // Load dictionaries
    std::wstring dataDir = StringUtil::GetInstallDir();
    NomDictionary::Instance().EnsureLoaded(dataDir);
    UserDictionary::Instance().EnsureLoaded();
    NgramModel::Instance().EnsureLoaded();
    NgramModel::Instance().SetMaxN(3);

    // Load preferences & recent counts
    LoadPreferences();
    LoadRecentCounts();

    // Install key event sink
    ITfKeystrokeMgr* pKeystrokeMgr = nullptr;
    if (SUCCEEDED(threadMgr_->QueryInterface(IID_ITfKeystrokeMgr, (void**)&pKeystrokeMgr))) {
        pKeystrokeMgr->AdviseKeyEventSink(clientId_, static_cast<ITfKeyEventSink*>(this), TRUE);
        pKeystrokeMgr->Release();
    }

    // Install thread manager event sink
    ITfSource* pSource = nullptr;
    if (SUCCEEDED(threadMgr_->QueryInterface(IID_ITfSource, (void**)&pSource))) {
        pSource->AdviseSink(IID_ITfThreadMgrEventSink,
            static_cast<ITfThreadMgrEventSink*>(this), &threadMgrSinkCookie_);
        pSource->Release();
    }

    // Defensively assert the keyboard is "open" (on) via
    // GUID_COMPARTMENT_KEYBOARD_OPENCLOSE. We deliberately do NOT register the
    // INPUTMODECOMPARTMENT capability (see RegisterCategories) because Nom has no
    // off mode, but some Win11 hosts still consult this per-session compartment
    // when an IME is (re)selected. If it carries a stale "closed" value from a
    // previously selected mode-aware IME, keystrokes pass straight through as
    // Latin -- Nom looks selected but behaves like the previous IME. Forcing it
    // open on every activation guarantees re-selection actually turns Nom on.
    SetKeyboardOpen(TRUE);

    // Create candidate window
    candidateWindow_ = new CandidateWindow(this);
    candidateWindow_->Create(NULL);

    return S_OK;
}

// Set GUID_COMPARTMENT_KEYBOARD_OPENCLOSE on the thread manager's compartment.
void NomTextService::SetKeyboardOpen(BOOL open) {
    if (!threadMgr_) return;
    ITfCompartmentMgr* pCompMgr = nullptr;
    if (FAILED(threadMgr_->QueryInterface(IID_ITfCompartmentMgr, (void**)&pCompMgr)) || !pCompMgr)
        return;
    ITfCompartment* pComp = nullptr;
    if (SUCCEEDED(pCompMgr->GetCompartment(GUID_COMPARTMENT_KEYBOARD_OPENCLOSE, &pComp)) && pComp) {
        VARIANT var;
        VariantInit(&var);
        var.vt = VT_I4;
        var.lVal = open ? 1 : 0;
        pComp->SetValue(clientId_, &var);
        pComp->Release();
    }
    pCompMgr->Release();
}

STDMETHODIMP NomTextService::Deactivate() {
    ++compositionGeneration_;
    // Remove key event sink
    ITfKeystrokeMgr* pKeystrokeMgr = nullptr;
    if (SUCCEEDED(threadMgr_->QueryInterface(IID_ITfKeystrokeMgr, (void**)&pKeystrokeMgr))) {
        pKeystrokeMgr->UnadviseKeyEventSink(clientId_);
        pKeystrokeMgr->Release();
    }

    // Remove thread manager event sink
    if (threadMgrSinkCookie_ != TF_INVALID_COOKIE) {
        ITfSource* pSource = nullptr;
        if (SUCCEEDED(threadMgr_->QueryInterface(IID_ITfSource, (void**)&pSource))) {
            pSource->UnadviseSink(threadMgrSinkCookie_);
            pSource->Release();
        }
        threadMgrSinkCookie_ = TF_INVALID_COOKIE;
    }

    // Persist n-gram model
    NgramModel::Instance().PersistNow();

    HideCandidates();
    // Destroy candidate window
    if (candidateWindow_) {
        candidateWindow_->Destroy();
        delete candidateWindow_;
        candidateWindow_ = nullptr;
    }

    ReleaseComposition();
    SafeRelease(threadMgr_);
    clientId_ = TF_CLIENTID_NULL;

    SaveRecentCounts();
    return S_OK;
}

// ---- ITfKeyEventSink ----

STDMETHODIMP NomTextService::OnSetFocus(BOOL foreground) {
    if (foreground) LoadPreferences();
    else HideCandidates();
    return S_OK;
}

STDMETHODIMP NomTextService::OnTestKeyDown(ITfContext* pContext, WPARAM wParam, LPARAM lParam, BOOL* pfEaten) {
    *pfEaten = FALSE;

    // Check modifier keys
    BYTE keyState[256];
    GetKeyboardState(keyState);
    bool ctrl = (keyState[VK_CONTROL] & 0x80) != 0;
    bool alt = (keyState[VK_MENU] & 0x80) != 0;
    bool win = (keyState[VK_LWIN] & 0x80) != 0 || (keyState[VK_RWIN] & 0x80) != 0;

    // Ctrl/Alt/Win combos: pass through without committing (e.g. Win+Shift+S screenshot)
    if (ctrl || alt || win) return S_OK;

    // Page Up/Down: eat when candidates are showing (for page navigation)
    if ((wParam == VK_PRIOR || wParam == VK_NEXT) && !currentCandidates_.empty()) {
        *pfEaten = TRUE;
        return S_OK;
    }

    // Function keys, navigation, modifier-only keys: always pass through
    if ((wParam >= VK_F1 && wParam <= VK_F24) ||
        wParam == VK_PRINT || wParam == VK_SNAPSHOT ||
        wParam == VK_INSERT || wParam == VK_DELETE ||
        wParam == VK_HOME || wParam == VK_END ||
        wParam == VK_PRIOR || wParam == VK_NEXT ||
        wParam == VK_LEFT || wParam == VK_RIGHT ||
        wParam == VK_UP || wParam == VK_DOWN ||
        wParam == VK_TAB || wParam == VK_CAPITAL ||
        wParam == VK_NUMLOCK || wParam == VK_SCROLL ||
        wParam == VK_LWIN || wParam == VK_RWIN || wParam == VK_APPS ||
        wParam == VK_PAUSE || wParam == VK_CANCEL ||
        wParam == VK_SHIFT || wParam == VK_CONTROL || wParam == VK_MENU) {
        return S_OK;
    }

    // While composing, eat most remaining keys
    bool hasComposition = !composing_.empty() || !lockedPrefix_.empty();

    if (hasComposition) {
        if (wParam == VK_BACK || wParam == VK_RETURN || wParam == VK_ESCAPE ||
            wParam == VK_SPACE) {
            *pfEaten = TRUE;
            return S_OK;
        }
        // Number keys 1-9 to select candidates
        if (!currentCandidates_.empty() && wParam >= '1' && wParam <= '9') {
            *pfEaten = TRUE;
            return S_OK;
        }
        // Punctuation / non-letter printable keys: eat them so we can commit + insert in OnKeyDown
        if (wParam > 0x20 && wParam < 0x80 && !iswalpha((wchar_t)wParam)) {
            *pfEaten = TRUE;
            return S_OK;
        }
        // OEM keys (punctuation like ;',./ etc on different keyboard layouts)
        if (wParam >= VK_OEM_1 && wParam <= VK_OEM_102) {
            *pfEaten = TRUE;
            return S_OK;
        }
    }

    // Letter keys are always eaten when we want Telex processing
    if ((wParam >= 'A' && wParam <= 'Z') || (wParam >= 'a' && wParam <= 'z')) {
        *pfEaten = TRUE;
        return S_OK;
    }

    return S_OK;
}

STDMETHODIMP NomTextService::OnTestKeyUp(ITfContext* pContext, WPARAM wParam, LPARAM lParam, BOOL* pfEaten) {
    *pfEaten = FALSE;
    return S_OK;
}

STDMETHODIMP NomTextService::OnKeyDown(ITfContext* pContext, WPARAM wParam, LPARAM lParam, BOOL* pfEaten) {
    *pfEaten = FALSE;

    BYTE keyState[256];
    GetKeyboardState(keyState);
    bool ctrl = (keyState[VK_CONTROL] & 0x80) != 0;
    bool alt = (keyState[VK_MENU] & 0x80) != 0;
    bool win = (keyState[VK_LWIN] & 0x80) != 0 || (keyState[VK_RWIN] & 0x80) != 0;

    // Win key combos (Win+Shift+S, Win+V, etc.): pass through WITHOUT committing
    if (win) return S_OK;

    // Page Up/Down for candidate page navigation
    if ((wParam == VK_PRIOR || wParam == VK_NEXT) && !currentCandidates_.empty() && candidateWindow_) {
        if (wParam == VK_PRIOR) candidateWindow_->PrevPage();
        else candidateWindow_->NextPage();
        candidateWindow_->Show();
        *pfEaten = TRUE;
        return S_OK;
    }

    // Function keys, navigation, modifier-only: pass through without committing
    if ((wParam >= VK_F1 && wParam <= VK_F24) ||
        wParam == VK_PRINT || wParam == VK_SNAPSHOT ||
        wParam == VK_INSERT || wParam == VK_DELETE ||
        wParam == VK_HOME || wParam == VK_END ||
        wParam == VK_PRIOR || wParam == VK_NEXT ||
        wParam == VK_LEFT || wParam == VK_RIGHT ||
        wParam == VK_UP || wParam == VK_DOWN ||
        wParam == VK_TAB || wParam == VK_CAPITAL ||
        wParam == VK_NUMLOCK || wParam == VK_SCROLL ||
        wParam == VK_LWIN || wParam == VK_RWIN || wParam == VK_APPS ||
        wParam == VK_PAUSE || wParam == VK_CANCEL ||
        wParam == VK_SHIFT || wParam == VK_CONTROL || wParam == VK_MENU) {
        return S_OK;
    }

    // Ctrl/Alt combos (Ctrl+A, Ctrl+C, etc.): commit composing text first, then pass through
    if (ctrl || alt) {
        bool hasComp = !composing_.empty() || !lockedPrefix_.empty();
        if (hasComp) {
            CommitComposing(pContext);
        }
        return S_OK;
    }

    bool hasComposition = !composing_.empty() || !lockedPrefix_.empty();

    // Handle backspace
    if (wParam == VK_BACK && hasComposition) {
        OnBackspace(pContext);
        *pfEaten = TRUE;
        return S_OK;
    }

    // Handle Enter
    if (wParam == VK_RETURN && hasComposition) {
        OnEnter(pContext);
        *pfEaten = TRUE;
        return S_OK;
    }

    // Handle Escape
    if (wParam == VK_ESCAPE && hasComposition) {
        OnEscape(pContext);
        *pfEaten = TRUE;
        return S_OK;
    }

    // Handle Space
    if (wParam == VK_SPACE && hasComposition) {
        OnSpace(pContext);
        *pfEaten = TRUE;
        return S_OK;
    }

    // Handle number keys for candidate selection
    if (!currentCandidates_.empty() && wParam >= '1' && wParam <= '9') {
        int num = (int)(wParam - '0');
        OnNumber(pContext, num);
        *pfEaten = TRUE;
        return S_OK;
    }

    // Handle letter keys
    if (wParam >= 'A' && wParam <= 'Z') {
        BYTE keyState[256];
        GetKeyboardState(keyState);
        bool shift = (keyState[VK_SHIFT] & 0x80) != 0;
        bool capsLock = (keyState[VK_CAPITAL] & 1) != 0;
        bool upper = shift ^ capsLock;
        wchar_t ch = upper ? (wchar_t)wParam : (wchar_t)(wParam + 32);
        OnChar(pContext, ch);
        *pfEaten = TRUE;
        return S_OK;
    }

    // Punctuation / non-letter keys while composing: commit composing + punctuation together
    if (hasComposition) {
        // Translate the virtual key to the actual character
        BYTE ks2[256];
        GetKeyboardState(ks2);
        wchar_t chars[4] = {};
        int result = ToUnicode((UINT)wParam, (UINT)((lParam >> 16) & 0xFF), ks2, chars, 4, 0);

        // Build the full commit string: composing text + punctuation
        std::wstring display;
        if (lockedPrefix_.empty()) display = composing_;
        else if (composing_.empty()) display = lockedPrefix_;
        else display = lockedPrefix_ + L" " + composing_;
        // Preserve a trailing space the user explicitly typed in composing_
        bool hadTrailingSpace = !composing_.empty() && composing_.back() == L' ';
        std::wstring commitText = StringUtil::Trim(display);
        if (hadTrailingSpace) commitText += L' ';

        if (result >= 1) {
            commitText += std::wstring(chars, result);
        }

        ClearComposingState();
        NgramModel::Instance().ResetContext();

        // Commit everything in one shot
        if (!commitText.empty()) {
            EditSession* pSession = new EditSession(this, pContext,
                EditSession::COMMIT_TEXT, commitText);
            pSession->RequestWrite();
            pSession->Release();
        }
        *pfEaten = TRUE;
        return S_OK;
    }

    return S_OK;
}

STDMETHODIMP NomTextService::OnKeyUp(ITfContext* pContext, WPARAM wParam, LPARAM lParam, BOOL* pfEaten) {
    *pfEaten = FALSE;
    return S_OK;
}

STDMETHODIMP NomTextService::OnPreservedKey(ITfContext* pContext, REFGUID rguid, BOOL* pfEaten) {
    *pfEaten = FALSE;
    return S_OK;
}

// ---- ITfCompositionSink ----

STDMETHODIMP NomTextService::OnCompositionTerminated(TfEditCookie ecWrite, ITfComposition* pComposition) {
    if (pComposition == composition_) {
        ++compositionGeneration_;
        ReleaseComposition();
        ClearComposingState();
    }
    return S_OK;
}

// ---- ITfThreadMgrEventSink ----

STDMETHODIMP NomTextService::OnInitDocumentMgr(ITfDocumentMgr* pDocMgr) { return S_OK; }
STDMETHODIMP NomTextService::OnUninitDocumentMgr(ITfDocumentMgr* pDocMgr) { return S_OK; }
STDMETHODIMP NomTextService::OnSetFocus(ITfDocumentMgr* pDocMgrFocus, ITfDocumentMgr* pDocMgrPrevFocus) {
    ++compositionGeneration_;
    ReleaseComposition();
    ClearComposingState();
    return S_OK;
}
STDMETHODIMP NomTextService::OnPushContext(ITfContext* pContext) { return S_OK; }
STDMETHODIMP NomTextService::OnPopContext(ITfContext* pContext) {
    if (pContext == compositionContext_) {
        ++compositionGeneration_;
        ReleaseComposition();
        ClearComposingState();
    } else if (pContext == candidateContext_) {
        HideCandidates();
    }
    return S_OK;
}

// ---- ITfTextEditSink ----

STDMETHODIMP NomTextService::OnEndEdit(ITfContext* pContext, TfEditCookie ecReadOnly,
    ITfEditRecord* pEditRecord) {
    if (!pEditRecord || pContext != compositionContext_ || !composition_ ||
        (composing_.empty() && lockedPrefix_.empty())) return S_OK;

    BOOL selectionChanged = FALSE;
    if (FAILED(pEditRecord->GetSelectionStatus(&selectionChanged)) || !selectionChanged) return S_OK;

    TF_SELECTION selection{};
    ULONG fetched = 0;
    if (FAILED(pContext->GetSelection(ecReadOnly, TF_DEFAULT_SELECTION, 1, &selection, &fetched)) ||
        fetched != 1) return S_OK;

    ITfRange* range = nullptr;
    LONG start = 0, end = 0;
    bool outside = SUCCEEDED(composition_->GetRange(&range)) &&
        SUCCEEDED(selection.range->CompareStart(ecReadOnly, range, TF_ANCHOR_START, &start)) &&
        SUCCEEDED(selection.range->CompareEnd(ecReadOnly, range, TF_ANCHOR_END, &end)) &&
        (start < 0 || end > 0);
    SafeRelease(range);
    selection.range->Release();
    if (!outside) return S_OK;

    // Finish the text in place while leaving the caret where the editor moved it.
    // OnEndEdit only has a read cookie. Queue the end for this exact composition
    // and detach it so the next key starts with a fresh buffer.
    auto* session = new (std::nothrow) EditSession(this, pContext, EditSession::END_COMPOSITION, L"", composition_);
    if (!session) return E_OUTOFMEMORY;
    HRESULT hr = session->RequestWrite();
    session->Release();
    if (SUCCEEDED(hr)) {
        ++compositionGeneration_;
        ReleaseComposition();
        ClearComposingState();
        NgramModel::Instance().ResetContext();
    }
    return S_OK;
}

void NomTextService::WatchComposition(ITfContext* context) {
    compositionContext_ = context;
    compositionContext_->AddRef();
    ITfSource* source = nullptr;
    if (SUCCEEDED(context->QueryInterface(IID_PPV_ARGS(&source)))) {
        source->AdviseSink(IID_ITfTextEditSink, static_cast<ITfTextEditSink*>(this), &textEditSinkCookie_);
        source->Release();
    }
}

void NomTextService::ReleaseComposition() {
    if (textEditSinkCookie_ != TF_INVALID_COOKIE) {
        ITfSource* source = nullptr;
        if (SUCCEEDED(compositionContext_->QueryInterface(IID_PPV_ARGS(&source)))) {
            source->UnadviseSink(textEditSinkCookie_);
            source->Release();
        }
        textEditSinkCookie_ = TF_INVALID_COOKIE;
    }
    SafeRelease(compositionContext_);
    SafeRelease(composition_);
}

void NomTextService::ClearComposingState() {
    composing_.clear();
    lockedPrefix_.clear();
    lockedHistory_.clear();
    currentCandidates_.clear();
    currentCandidateConsumed_.clear();
    shorthandActive_ = false;
    shorthandSegments_.clear();
    lastWasStandaloneW_ = false;
    HideCandidates();
}

// Keep a layout subscription only for the editor which owns the candidate list.
void NomTextService::SetCandidateContext(ITfContext* context) {
    if (context == candidateContext_) return;
    if (candidateWindow_) candidateWindow_->Hide();
    if (layoutCookie_ != TF_INVALID_COOKIE) {
        ITfSource* source = nullptr;
        if (SUCCEEDED(candidateContext_->QueryInterface(IID_PPV_ARGS(&source)))) {
            source->UnadviseSink(layoutCookie_);
            source->Release();
        }
        layoutCookie_ = TF_INVALID_COOKIE;
    }
    SafeRelease(candidateContext_);
    layoutReadPending_ = false;
    candidateContext_ = context;
    if (context) {
        context->AddRef();
        ITfSource* source = nullptr;
        if (SUCCEEDED(context->QueryInterface(IID_PPV_ARGS(&source)))) {
            source->AdviseSink(IID_ITfTextLayoutSink, static_cast<ITfTextLayoutSink*>(this), &layoutCookie_);
            source->Release();
        }
    }
}

void NomTextService::HideCandidates() {
    SetCandidateContext(nullptr);
}

void NomTextService::PresentCandidates(ITfContext* context, TfEditCookie ec) {
    if (context != candidateContext_ || !candidateWindow_) return;
    ITfContextView* view = nullptr;
    RECT caret{}, document{};
    HWND hwnd = nullptr;
    bool haveCaret = false, haveDocument = false;
    if (SUCCEEDED(context->GetActiveView(&view)) && view) {
        view->GetWnd(&hwnd);
        haveDocument = SUCCEEDED(view->GetScreenExt(&document)) && !IsRectEmpty(&document);
        if (!haveDocument && hwnd && GetClientRect(hwnd, &document) && !IsRectEmpty(&document)) {
            POINT origin{};
            if (ClientToScreen(hwnd, &origin)) {
                OffsetRect(&document, origin.x, origin.y);
                haveDocument = true;
            }
        }
        ITfRange* range = nullptr;
        if (composition_ && SUCCEEDED(composition_->GetRange(&range))) {
            BOOL clipped = FALSE;
            if (SUCCEEDED(range->Collapse(ec, TF_ANCHOR_END))) {
                haveCaret = SUCCEEDED(view->GetTextExt(ec, range, &caret, &clipped)) &&
                    !clipped && caret.bottom > caret.top;
                // Some hosts can measure text but have no extent for an insertion point.
                if (!haveCaret) {
                    LONG moved = 0;
                    haveCaret = SUCCEEDED(range->ShiftStart(ec, -1, &moved, nullptr)) && moved == -1 &&
                        SUCCEEDED(view->GetTextExt(ec, range, &caret, &clipped)) && !clipped &&
                        caret.bottom > caret.top;
                    caret.left = caret.right;
                }
            }
            range->Release();
        }
        view->Release();
    }
    candidateWindow_->PresentAtCaret(haveCaret ? &caret : nullptr, haveDocument ? &document : nullptr, hwnd);
}

STDMETHODIMP NomTextService::OnLayoutChange(ITfContext* context, TfLayoutCode code, ITfContextView*) {
    if (!context || context != candidateContext_) return S_OK;
    if (code == TF_LC_DESTROY) {
        if (candidateWindow_) candidateWindow_->Hide();
    } else if (!layoutReadPending_) {
        // Editors may return TS_E_NOLAYOUT during the write. Retry under a
        // read cookie after the host lays out the text, even for the first key.
        auto* session = new (std::nothrow) EditSession(this, context, EditSession::READ_LAYOUT);
        if (!session) return E_OUTOFMEMORY;
        layoutReadPending_ = true;
        HRESULT result = E_FAIL;
        HRESULT hr = context->RequestEditSession(clientId_, session, TF_ES_ASYNC | TF_ES_READ, &result);
        session->Release();
        if (FAILED(hr) || FAILED(result)) layoutReadPending_ = false;
    }
    return S_OK;
}

// ---- Input handling ----

void NomTextService::OnChar(ITfContext* pContext, wchar_t ch) {
    if (shorthandActive_) {
        composing_ += ch;
        lastWasStandaloneW_ = false;
    }
    else {
        size_t lastSpace = composing_.rfind(L' ');
        std::wstring head = (lastSpace != std::wstring::npos) ? composing_.substr(0, lastSpace + 1) : L"";
        std::wstring tail = (lastSpace != std::wstring::npos) ? composing_.substr(lastSpace + 1) : composing_;
        // Capture the previous standalone-w flag and compute the new one via the engine's
        // authoritative predicate. This covers both the empty/non-letter tail case and the
        // pure-consonant-onset case (e.g. "ng" + w → "ngư"), while correctly excluding
        // tails that already contain a vowel (e.g. "u" + w → ư merge, "ban" + w → băn).
        bool prevStandaloneW = lastWasStandaloneW_;
        lastWasStandaloneW_ = TelexEngine::WouldBeStandaloneW(tail, ch);
        std::wstring newTail = TelexEngine::Apply(tail, ch, toneStyleOld_, prevStandaloneW);
        composing_ = head + newTail;
    }
    UpdateCandidates(pContext);

    // Build display text
    std::wstring display;
    if (lockedPrefix_.empty()) display = composing_;
    else if (composing_.empty()) display = lockedPrefix_;
    else display = lockedPrefix_ + L" " + composing_;

    // Update composing text in the editor
    EditSession* pSession = new EditSession(this, pContext,
        EditSession::SET_COMPOSING, display);
    pSession->RequestWrite();
    pSession->Release();
}

void NomTextService::OnBackspace(ITfContext* pContext) {
    // Any backspace breaks the "previous keystroke was standalone-w" chain, so a
    // subsequent 'w' must NOT be treated as the second half of a w+w undo.
    lastWasStandaloneW_ = false;
    if (!composing_.empty()) {
        composing_.pop_back();
    }
    else if (!lockedPrefix_.empty()) {
        // Undo last locked character
        lockedPrefix_.pop_back();
    }

    if (composing_.empty() && lockedPrefix_.empty()) {
        // Nothing left, end composition
        EditSession* pSession = new EditSession(this, pContext,
            EditSession::END_COMPOSITION);
        pSession->RequestWrite();
        pSession->Release();
        ClearComposingState();
        return;
    }

    UpdateCandidates(pContext);

    std::wstring display;
    if (lockedPrefix_.empty()) display = composing_;
    else if (composing_.empty()) display = lockedPrefix_;
    else display = lockedPrefix_ + L" " + composing_;

    EditSession* pSession = new EditSession(this, pContext,
        EditSession::SET_COMPOSING, display);
    pSession->RequestWrite();
    pSession->Release();
}

void NomTextService::OnSpace(ITfContext* pContext) {
    if (composing_.empty() && lockedPrefix_.empty()) {
        // Nothing composing, let space pass through
        return;
    }
    if (shorthandActive_) {
        CommitComposing(pContext, /*appendTrailingSpace=*/true);
        return;
    }
    if (composing_.empty()) {
        CommitComposing(pContext, /*appendTrailingSpace=*/true);
        return;
    }
    if (!composing_.empty() && composing_.back() == L' ') {
        CommitComposing(pContext);
        return;
    }
    if (currentCandidates_.empty()) {
        CommitComposing(pContext, /*appendTrailingSpace=*/true);
        return;
    }

    // Add space as syllable separator
    composing_ += L' ';
    UpdateCandidates(pContext);

    std::wstring display;
    if (lockedPrefix_.empty()) display = composing_;
    else display = lockedPrefix_ + L" " + composing_;

    EditSession* pSession = new EditSession(this, pContext,
        EditSession::SET_COMPOSING, display);
    pSession->RequestWrite();
    pSession->Release();
}

void NomTextService::OnEnter(ITfContext* pContext) {
    CommitComposing(pContext);
}

void NomTextService::OnEscape(ITfContext* pContext) {
    ClearComposingState();

    EditSession* pSession = new EditSession(this, pContext, EditSession::END_COMPOSITION);
    pSession->RequestWrite();
    pSession->Release();
}

void NomTextService::OnNumber(ITfContext* pContext, int num) {
    int page = candidateWindow_ ? candidateWindow_->GetCurrentPage() : 0;
    int idx = page * MAX_CANDIDATES_DISPLAY + num - 1;
    PickCandidate(pContext, idx);
}

void NomTextService::PickCandidate(ITfContext* pContext, int idx) {
    if (idx < 0 || idx >= (int)currentCandidates_.size()) return;

    std::wstring picked = currentCandidates_[idx];
    int consumed = (idx < (int)currentCandidateConsumed_.size()) ? currentCandidateConsumed_[idx] : 1;
    BumpRecent(picked);

    // Split composing into syllables
    auto syllables = StringUtil::SplitWhitespace(composing_);
    int totalSyl = (int)syllables.size();
    if (totalSyl == 0) totalSyl = 1;
    int k = (std::min)(consumed, totalSyl);

    // Record this pick step
    std::wstring consumedRaw;
    for (int i = 0; i < k && i < (int)syllables.size(); i++) {
        if (!consumedRaw.empty()) consumedRaw += L' ';
        consumedRaw += syllables[i];
    }
    if (consumedRaw.empty()) consumedRaw = composing_;
    lockedHistory_.push_back({ consumedRaw, picked, L"" });

    if (k >= totalSyl) {
        // Final pick: candidate covers all remaining syllables
        std::wstring commitText = lockedPrefix_ + picked;

        // Auto-learn from the pick history
        LearnUserPhrases();
        // Feed the committed Nom into the n-gram model
        ObserveNgramForCommit(commitText);

        ClearComposingState();

        EditSession* pSession = new EditSession(this, pContext,
            EditSession::COMMIT_TEXT, commitText);
        pSession->RequestWrite();
        pSession->Release();
    }
    else {
        // Partial pick: consume first k syllables, keep the rest
        lockedPrefix_ += picked;

        // Rebuild composing from remaining syllables
        std::wstring remaining;
        for (int i = k; i < (int)syllables.size(); i++) {
            if (!remaining.empty()) remaining += L' ';
            remaining += syllables[i];
        }
        composing_ = remaining;

        // Refresh candidates for the remaining syllables
        UpdateCandidates(pContext);

        // Update the composing text shown in the editor
        std::wstring display;
        if (lockedPrefix_.empty()) display = composing_;
        else if (composing_.empty()) display = lockedPrefix_;
        else display = lockedPrefix_ + L" " + composing_;

        EditSession* pSession = new EditSession(this, pContext,
            EditSession::SET_COMPOSING, display);
        pSession->RequestWrite();
        pSession->Release();
    }
}

void NomTextService::CommitCandidate(int index) {
    if (!threadMgr_ || !candidateWindow_ || !candidateWindow_->IsVisible()) return;
    ITfDocumentMgr* document = nullptr;
    ITfContext* context = nullptr;
    if (SUCCEEDED(threadMgr_->GetFocus(&document)) && document &&
        SUCCEEDED(document->GetTop(&context)) && context) {
        PickCandidate(context, index);
    }
    SafeRelease(context);
    SafeRelease(document);
}

void NomTextService::CommitComposing(ITfContext* pContext, bool appendTrailingSpace) {
    std::wstring display;
    if (lockedPrefix_.empty()) display = composing_;
    else if (composing_.empty()) display = lockedPrefix_;
    else display = lockedPrefix_ + L" " + composing_;

    // Preserve a trailing space the user explicitly typed in composing_
    bool hadTrailingSpace = !composing_.empty() && composing_.back() == L' ';
    std::wstring text = StringUtil::Trim(display);
    if (hadTrailingSpace && !text.empty()) text += L' ';
    // OnSpace's commit branches eat the space keypress (*pfEaten = TRUE), so when the
    // user pressed space to finish a syllable that has no Nom candidates (e.g. "mỹ"),
    // to terminate a shorthand run, or to flush a fully-locked prefix, we must emit a
    // real trailing space ourselves – otherwise the space is silently swallowed. Mirrors
    // the Android onSpaceSegment behaviour (commitComposing() + commitText(" ")).
    // Guarded so we never produce a double space when one is already present.
    if (appendTrailingSpace) {
        if (text.empty()) text = L" ";
        else if (text.back() != L' ') text += L' ';
    }
    auto action = text.empty() ? EditSession::END_COMPOSITION : EditSession::COMMIT_TEXT;
    EditSession* pSession = new EditSession(this, pContext, action, text);
    pSession->RequestWrite();
    pSession->Release();

    ClearComposingState();
    NgramModel::Instance().ResetContext();
}

// ---- Candidate gathering ----

NomTextService::ShorthandSegments NomTextService::TryShorthandSplit(const std::wstring& raw) const {
    ShorthandSegments result;
    if (raw.size() < 2 || raw.find(L' ') != std::wstring::npos) return result;

    std::wstring asciiLower = StringUtil::StripDiacritics(StringUtil::ToLower(raw));
    for (wchar_t c : asciiLower) {
        if (c < L'a' || c > L'z') return result;
    }

    if (NomDictionary::Instance().HasAsciiSinglePrefix(asciiLower)) return result;
    // The dictionary only knows readings that have a Chữ Nôm character, so legal
    // Vietnamese syllables like "my" / "sy" are missing from it. Without this guard
    // the gate would hijack them into shorthand mode and a subsequent tone trigger
    // ('x' for "mỹ") would be appended literally instead of applied by Telex.
    // See TelexEngine::IsValidVietnameseSyllableShape for the rationale.
    if (TelexEngine::IsValidVietnameseSyllableShape(asciiLower)) return result;

    static const std::wstring multiOnsets[] = {
        L"ngh", L"ng", L"nh", L"ph", L"th", L"tr", L"ch", L"gh", L"kh"
    };

    std::vector<std::wstring> segments;
    size_t i = 0;
    while (i < asciiLower.size()) {
        // Try longest syllable key
        std::wstring syllableMatch;
        int maxLen = (int)(std::min)((size_t)8, asciiLower.size() - i);
        for (int len = maxLen; len >= 2; len--) {
            std::wstring sub = asciiLower.substr(i, len);
            if (NomDictionary::Instance().IsAsciiSingleKey(sub)) {
                syllableMatch = sub; break;
            }
        }
        if (!syllableMatch.empty()) {
            segments.push_back(syllableMatch);
            i += syllableMatch.size();
            continue;
        }

        // Try multi-letter onset
        std::wstring onsetMatch;
        for (auto& o : multiOnsets) {
            if (asciiLower.substr(i, o.size()) == o) {
                onsetMatch = o; break;
            }
        }
        if (!onsetMatch.empty()) {
            segments.push_back(onsetMatch);
            i += onsetMatch.size();
            continue;
        }

        // Single letter
        segments.push_back(asciiLower.substr(i, 1));
        i += 1;
    }

    if (segments.size() < 2) return result;
    result.active = true;
    result.segments = segments;
    return result;
}

void NomTextService::UpdateCandidates(ITfContext* pContext) {
    if (composing_.empty() && lockedPrefix_.empty()) {
        currentCandidates_.clear();
        currentCandidateConsumed_.clear();
        HideCandidates();
        return;
    }

    // Try shorthand split
    if (shorthandEnabled_) {
        auto sh = TryShorthandSplit(composing_);
        shorthandActive_ = sh.active;
        shorthandSegments_ = sh.segments;
    }

    auto& dict = NomDictionary::Instance();

    if (shorthandActive_ && shorthandSegments_.size() >= 2) {
        // Shorthand mode: lookup by viết tắt
        currentCandidates_.clear();
        currentCandidateConsumed_.clear();

        auto results = dict.LookupWordByVietTat(shorthandSegments_);
        for (auto& [key, values] : results) {
            for (auto& v : values) {
                bool found = false;
                for (auto& c : currentCandidates_) if (c == v) { found = true; break; }
                if (!found) {
                    currentCandidates_.push_back(v);
                    currentCandidateConsumed_.push_back((int)shorthandSegments_.size());
                }
            }
        }
        // Also add single-char candidates for first segment
        auto singles = dict.LookupSinglePrefix(shorthandSegments_[0]);
        for (auto& s : singles) {
            bool found = false;
            for (auto& c : currentCandidates_) if (c == s) { found = true; break; }
            if (!found) {
                currentCandidates_.push_back(s);
                currentCandidateConsumed_.push_back(1);
            }
        }
    }
    else if (segmentMode_ && composing_.find(L' ') != std::wstring::npos) {
        // Segment mode with multiple syllables
        auto syllables = StringUtil::SplitWhitespace(composing_);
        currentCandidates_.clear();
        currentCandidateConsumed_.clear();

        for (int k = (int)syllables.size(); k >= 1; k--) {
            std::wstring prefix;
            for (int i = 0; i < k; i++) {
                if (i > 0) prefix += L' ';
                prefix += syllables[i];
            }
            auto results = (k > 1) ? dict.LookupWord(prefix, true) : dict.LookupSingle(prefix, true);
            for (auto& v : results) {
                bool found = false;
                for (auto& c : currentCandidates_) if (c == v) { found = true; break; }
                if (!found) {
                    currentCandidates_.push_back(v);
                    currentCandidateConsumed_.push_back(k);
                }
            }

        }
    }
    else {
        // Simple lookup — single syllable: only suggest single characters (not compounds)
        // This matches Android's pref_single_syl_single_char_only = true default.
        std::wstring trimmed = StringUtil::Trim(composing_);
        bool isSingleSyllable = (trimmed.find(L' ') == std::wstring::npos);
        if (isSingleSyllable) {
            // Single-char only: exact match + prefix match
            auto exact = dict.LookupSingle(trimmed, true);
            auto prefix = dict.LookupSinglePrefix(trimmed, 500, true);
            currentCandidates_ = std::move(exact);
            std::unordered_set<std::wstring> seen(currentCandidates_.begin(), currentCandidates_.end());
            for (auto& candidate : prefix) {
                if (seen.insert(candidate).second) currentCandidates_.push_back(std::move(candidate));
            }
        }
        else {
            currentCandidates_ = dict.Lookup(trimmed, true);
        }
        currentCandidateConsumed_.resize(currentCandidates_.size());
        for (int i = 0; i < (int)currentCandidateConsumed_.size(); i++) {
            currentCandidateConsumed_[i] = 1;
        }
    }

    // Sort by (consumed desc, recency desc, n-gram score desc)
    if (!currentCandidates_.empty()) {
        auto& ngram = NgramModel::Instance();
        std::vector<int> indices(currentCandidates_.size());
        for (int i = 0; i < (int)indices.size(); i++) indices[i] = i;
        std::stable_sort(indices.begin(), indices.end(), [&](int a, int b) {
            int ca = currentCandidateConsumed_[a], cb = currentCandidateConsumed_[b];
            if (ca != cb) return ca > cb;
            int ra = 0, rb = 0;
            auto it = recentCounts_.find(currentCandidates_[a]);
            if (it != recentCounts_.end()) ra = it->second;
            it = recentCounts_.find(currentCandidates_[b]);
            if (it != recentCounts_.end()) rb = it->second;
            if (ra != rb) return ra > rb;
            int na = ngram.Score(currentCandidates_[a]);
            int nb = ngram.Score(currentCandidates_[b]);
            return na > nb;
            });

        std::vector<std::wstring> sorted;
        std::vector<int> sortedConsumed;
        for (int i : indices) {
            sorted.push_back(currentCandidates_[i]);
            sortedConsumed.push_back(currentCandidateConsumed_[i]);
        }
        currentCandidates_ = sorted;
        currentCandidateConsumed_ = sortedConsumed;
    }


    // Update candidate window
    if (!currentCandidates_.empty()) {
        // Ensure window exists
        if (!candidateWindow_) {
            candidateWindow_ = new CandidateWindow(this);
            candidateWindow_->Create(NULL);
        }

        SetCandidateContext(pContext);
        candidateWindow_->SetCandidates(currentCandidates_);
        candidateWindow_->Show();
    }
    else {
        HideCandidates();
    }
}

// ---- Preferences ----

void NomTextService::BumpRecent(const std::wstring& text) {
    recentCounts_[text]++;
    if (recentCounts_.size() > 500) {
        // Prune to top 400
        std::vector<std::pair<std::wstring, int>> entries(recentCounts_.begin(), recentCounts_.end());
        std::sort(entries.begin(), entries.end(), [](auto& a, auto& b) { return a.second > b.second; });
        recentCounts_.clear();
        for (int i = 0; i < (std::min)(400, (int)entries.size()); i++) {
            recentCounts_[entries[i].first] = entries[i].second;
        }
    }
    SaveRecentCounts();
}

void NomTextService::SaveRecentCounts() {
    std::wstring path = StringUtil::GetUserDataDir() + L"\\recent.tsv";
    std::string utf8;
    for (auto& [k, v] : recentCounts_) {
        utf8 += StringUtil::WideToUTF8(k) + "\t" + std::to_string(v) + "\n";
    }
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (file.is_open()) {
        file.write(utf8.c_str(), utf8.size());
    }
}

void NomTextService::LoadRecentCounts() {
    std::wstring path = StringUtil::GetUserDataDir() + L"\\recent.tsv";
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return;

    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();

    std::wstring wContent = StringUtil::UTF8ToWide(content);
    std::wistringstream stream(wContent);
    std::wstring line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == L'\r') line.pop_back();
        auto parts = StringUtil::Split(line, L'\t');
        if (parts.size() >= 2) {
            try {
                int v = std::stoi(parts[1]);
                if (!parts[0].empty()) recentCounts_[parts[0]] = v;
            }
            catch (...) {}
        }
    }
}

// ---- Auto-learning ----

void NomTextService::LearnUserPhrases() {
    if (lockedHistory_.empty()) return;

    auto& userDict = UserDictionary::Instance();
    auto& dict = NomDictionary::Instance();

    // Build reading->nom from steps
    auto readingOf = [](const LockedStep& step) -> std::wstring {
        return step.learnKey.empty() ? step.rawConsumed : step.learnKey;
    };

    // Full phrase (always learn)
    std::wstring fullKey, fullNom;
    for (auto& step : lockedHistory_) {
        std::wstring r = StringUtil::Trim(StringUtil::ToLower(readingOf(step)));
        if (r.empty()) continue;
        if (!fullKey.empty()) fullKey += L' ';
        fullKey += r;
        fullNom += step.nomText;
    }

    if (fullKey.empty() || fullNom.empty()) return;

    // Skip if already in bundled dictionary
    if (!dict.FindBundledKeyForNomByVietTat(fullNom, StringUtil::SplitWhitespace(
        NomDictionary::StripDiacritics(fullKey))).empty()) {
        return;
    }

    // Learn the full phrase
    auto existing = userDict.LookupWord(fullKey);
    std::vector<std::wstring> merged;
    merged.push_back(fullNom);
    for (auto& v : existing) {
        if (v != fullNom) merged.push_back(v);
    }
    userDict.PutEntry(fullKey, merged);

    // Also learn contiguous sub-phrases of length 2..N-1
    int maxSubLen = (std::min)((int)lockedHistory_.size() - 1, 3);
    for (int len = maxSubLen; len >= 2; len--) {
        for (int start = 0; start + len <= (int)lockedHistory_.size(); start++) {
            if (start == 0 && len == (int)lockedHistory_.size()) continue; // already done above

            std::wstring subKey, subNom;
            for (int i = start; i < start + len; i++) {
                std::wstring r = StringUtil::Trim(StringUtil::ToLower(readingOf(lockedHistory_[i])));
                if (r.empty()) { subKey.clear(); break; }
                if (!subKey.empty()) subKey += L' ';
                subKey += r;
                subNom += lockedHistory_[i].nomText;
            }
            if (subKey.empty() || subNom.empty()) continue;

            // Skip bundled entries
            auto subSegs = StringUtil::SplitWhitespace(NomDictionary::StripDiacritics(subKey));
            if (!dict.FindBundledKeyForNomByVietTat(subNom, subSegs).empty()) continue;

            auto subExisting = userDict.LookupWord(subKey);
            std::vector<std::wstring> subMerged;
            subMerged.push_back(subNom);
            for (auto& v : subExisting) {
                if (v != subNom) subMerged.push_back(v);
            }
            userDict.PutEntry(subKey, subMerged);
        }
    }
}

void NomTextService::ObserveNgramForCommit(const std::wstring& commitString) {
    auto& ngram = NgramModel::Instance();
    for (size_t i = 0; i < commitString.size(); ) {
        // Handle surrogate pairs for CJK characters outside BMP
        wchar_t ch = commitString[i];
        std::wstring token;
        if (ch >= 0xD800 && ch <= 0xDBFF && i + 1 < commitString.size()) {
            token = commitString.substr(i, 2);
            i += 2;
        }
        else {
            token = std::wstring(1, ch);
            i += 1;
        }
        // Skip whitespace
        if (token == L" " || token == L"\t") continue;
        ngram.Observe(token);
    }
}
