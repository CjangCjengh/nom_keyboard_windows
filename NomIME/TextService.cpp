// TextService.cpp — Main TSF Text Service implementation// TextService.cpp — Main TSF Text Service implementation
#include "TextService.h"
#include "TelexEngine.h"
#include "NomDictionary.h"
#include "UserDictionary.h"
#include "StringUtil.h"

// ---- EditSession: helper to perform edits inside a TSF edit cookie ----
class NomTextService::EditSession : public ITfEditSession {
public:
    enum Action { SET_COMPOSING, COMMIT_TEXT, END_COMPOSITION };

    EditSession(NomTextService* pService, ITfContext* pContext, Action action,
        const std::wstring& text = L"")
        : refCount_(1), service_(pService), context_(pContext),
        action_(action), text_(text)
    {
        context_->AddRef();
    }

    ~EditSession() { SafeRelease(context_); }

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
        case SET_COMPOSING: {
            // Start or update composition
            if (!service_->composition_) {
                ITfInsertAtSelection* pInsert = nullptr;
                if (SUCCEEDED(context_->QueryInterface(IID_ITfInsertAtSelection, (void**)&pInsert))) {
                    ITfRange* pRange = nullptr;
                    if (SUCCEEDED(pInsert->InsertTextAtSelection(ec, TF_IAS_QUERYONLY,
                        NULL, 0, &pRange))) {
                        ITfContextComposition* pContextComp = nullptr;
                        if (SUCCEEDED(context_->QueryInterface(IID_ITfContextComposition,
                            (void**)&pContextComp))) {
                            pContextComp->StartComposition(ec, pRange,
                                service_, &service_->composition_);
                            pContextComp->Release();
                        }
                        pRange->Release();
                    }
                    pInsert->Release();
                }
            }
            if (service_->composition_) {
                ITfRange* pRange = nullptr;
                if (SUCCEEDED(service_->composition_->GetRange(&pRange))) {
                    pRange->SetText(ec, 0, text_.c_str(), (LONG)text_.size());
                    // Move cursor to end of composition
                    pRange->Collapse(ec, TF_ANCHOR_END);
                    ITfRange* pSelRange = nullptr;
                    // Set selection at end
                    TF_SELECTION sel = {};
                    sel.range = pRange;
                    sel.style.ase = TF_AE_END;
                    sel.style.fInterimChar = FALSE;
                    context_->SetSelection(ec, 1, &sel);
                    pRange->Release();
                }
            }
            break;
        }
        case COMMIT_TEXT: {
            if (service_->composition_) {
                ITfRange* pRange = nullptr;
                if (SUCCEEDED(service_->composition_->GetRange(&pRange))) {
                    pRange->SetText(ec, 0, text_.c_str(), (LONG)text_.size());
                    pRange->Collapse(ec, TF_ANCHOR_END);
                    service_->composition_->EndComposition(ec);
                    SafeRelease(service_->composition_);
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
            if (service_->composition_) {
                service_->composition_->EndComposition(ec);
                SafeRelease(service_->composition_);
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
};

// ---- Constructor / Destructor ----

NomTextService::NomTextService()
    : refCount_(1), clientId_(TF_CLIENTID_NULL), threadMgr_(nullptr),
    threadMgrSinkCookie_(TF_INVALID_COOKIE), textEditSinkCookie_(TF_INVALID_COOKIE),
    composition_(nullptr), compositionContext_(nullptr),
    shorthandActive_(false), candidatePage_(0), candidateWindow_(nullptr),
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
    else if (IsEqualIID(riid, IID_ITfKeyEventSink)) {
        *ppvObj = static_cast<ITfKeyEventSink*>(this);
    }
    else if (IsEqualIID(riid, IID_ITfCompositionSink)) {
        *ppvObj = static_cast<ITfCompositionSink*>(this);
    }
    else if (IsEqualIID(riid, IID_ITfThreadMgrEventSink)) {
        *ppvObj = static_cast<ITfThreadMgrEventSink*>(this);
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

    // Create candidate window
    candidateWindow_ = new CandidateWindow(this);
    candidateWindow_->Create(NULL);

    return S_OK;
}

STDMETHODIMP NomTextService::Deactivate() {
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

    // Destroy candidate window
    if (candidateWindow_) {
        candidateWindow_->Destroy();
        delete candidateWindow_;
        candidateWindow_ = nullptr;
    }

    SafeRelease(composition_);
    SafeRelease(compositionContext_);
    SafeRelease(threadMgr_);
    clientId_ = TF_CLIENTID_NULL;

    SaveRecentCounts();
    return S_OK;
}

// ---- ITfKeyEventSink ----

STDMETHODIMP NomTextService::OnSetFocus(BOOL fForeground) { return S_OK; }

STDMETHODIMP NomTextService::OnTestKeyDown(ITfContext* pContext, WPARAM wParam, LPARAM lParam, BOOL* pfEaten) {
    *pfEaten = FALSE;

    // Never eat keys when Ctrl or Alt is held — let shortcuts (Ctrl+A, Ctrl+C, Alt+F4 etc.) pass through
    BYTE keyState[256];
    GetKeyboardState(keyState);
    bool ctrl = (keyState[VK_CONTROL] & 0x80) != 0;
    bool alt = (keyState[VK_MENU] & 0x80) != 0;
    if (ctrl || alt) {
        return S_OK;
    }

    // While composing, eat most keys
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

    // Never eat keys when Ctrl or Alt is held — let shortcuts pass through
    BYTE keyState[256];
    GetKeyboardState(keyState);
    bool ctrl = (keyState[VK_CONTROL] & 0x80) != 0;
    bool alt = (keyState[VK_MENU] & 0x80) != 0;
    if (ctrl || alt) {
        // If composing, commit first so Ctrl+A selects committed text
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

    // Non-letter characters while composing: commit first
    if (hasComposition && wParam > 0x20 && wParam < 0x80 && !iswalpha((wchar_t)wParam)) {
        CommitComposing(pContext);
        // Don't eat the key, let it pass through
        *pfEaten = FALSE;
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
    SafeRelease(composition_);
    composing_.clear();
    lockedPrefix_.clear();
    shorthandActive_ = false;
    shorthandSegments_.clear();
    currentCandidates_.clear();
    currentCandidateConsumed_.clear();
    candidatePage_ = 0;
    if (candidateWindow_) candidateWindow_->Hide();
    return S_OK;
}

// ---- ITfThreadMgrEventSink ----

STDMETHODIMP NomTextService::OnInitDocumentMgr(ITfDocumentMgr* pDocMgr) { return S_OK; }
STDMETHODIMP NomTextService::OnUninitDocumentMgr(ITfDocumentMgr* pDocMgr) { return S_OK; }
STDMETHODIMP NomTextService::OnSetFocus(ITfDocumentMgr* pDocMgrFocus, ITfDocumentMgr* pDocMgrPrevFocus) {
    return S_OK;
}
STDMETHODIMP NomTextService::OnPushContext(ITfContext* pContext) { return S_OK; }
STDMETHODIMP NomTextService::OnPopContext(ITfContext* pContext) { return S_OK; }

// ---- ITfTextEditSink ----

STDMETHODIMP NomTextService::OnEndEdit(ITfContext* pContext, TfEditCookie ecReadOnly,
    ITfEditRecord* pEditRecord) {
    return S_OK;
}

// ---- Input handling ----

void NomTextService::OnChar(ITfContext* pContext, wchar_t ch) {
    if (shorthandActive_) {
        composing_ += ch;
    }
    else {
        size_t lastSpace = composing_.rfind(L' ');
        std::wstring head = (lastSpace != std::wstring::npos) ? composing_.substr(0, lastSpace + 1) : L"";
        std::wstring tail = (lastSpace != std::wstring::npos) ? composing_.substr(lastSpace + 1) : composing_;
        std::wstring newTail = TelexEngine::Apply(tail, ch, toneStyleOld_);
        composing_ = head + newTail;
    }
    UpdateCandidates();

    // Build display text
    std::wstring display;
    if (lockedPrefix_.empty()) display = composing_;
    else if (composing_.empty()) display = lockedPrefix_;
    else display = lockedPrefix_ + L" " + composing_;

    // Update composing text in the editor
    EditSession* pSession = new EditSession(this, pContext,
        EditSession::SET_COMPOSING, display);
    HRESULT hr;
    pContext->RequestEditSession(clientId_, pSession, TF_ES_ASYNCDONTCARE | TF_ES_READWRITE, &hr);
    pSession->Release();
}

void NomTextService::OnBackspace(ITfContext* pContext) {
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
        HRESULT hr;
        pContext->RequestEditSession(clientId_, pSession, TF_ES_ASYNCDONTCARE | TF_ES_READWRITE, &hr);
        pSession->Release();
        currentCandidates_.clear();
        currentCandidateConsumed_.clear();
        candidatePage_ = 0;
        shorthandActive_ = false;
        shorthandSegments_.clear();
        if (candidateWindow_) candidateWindow_->Hide();
        return;
    }

    UpdateCandidates();

    std::wstring display;
    if (lockedPrefix_.empty()) display = composing_;
    else if (composing_.empty()) display = lockedPrefix_;
    else display = lockedPrefix_ + L" " + composing_;

    EditSession* pSession = new EditSession(this, pContext,
        EditSession::SET_COMPOSING, display);
    HRESULT hr;
    pContext->RequestEditSession(clientId_, pSession, TF_ES_ASYNCDONTCARE | TF_ES_READWRITE, &hr);
    pSession->Release();
}

void NomTextService::OnSpace(ITfContext* pContext) {
    if (composing_.empty() && lockedPrefix_.empty()) {
        // Nothing composing, let space pass through
        return;
    }
    if (shorthandActive_) {
        CommitComposing(pContext);
        return;
    }
    if (composing_.empty()) {
        CommitComposing(pContext);
        return;
    }
    if (!composing_.empty() && composing_.back() == L' ') {
        CommitComposing(pContext);
        return;
    }
    if (currentCandidates_.empty()) {
        CommitComposing(pContext);
        return;
    }

    // Add space as syllable separator
    composing_ += L' ';
    UpdateCandidates();

    std::wstring display;
    if (lockedPrefix_.empty()) display = composing_;
    else display = lockedPrefix_ + L" " + composing_;

    EditSession* pSession = new EditSession(this, pContext,
        EditSession::SET_COMPOSING, display);
    HRESULT hr;
    pContext->RequestEditSession(clientId_, pSession, TF_ES_ASYNCDONTCARE | TF_ES_READWRITE, &hr);
    pSession->Release();
}

void NomTextService::OnEnter(ITfContext* pContext) {
    CommitComposing(pContext);
}

void NomTextService::OnEscape(ITfContext* pContext) {
    composing_.clear();
    lockedPrefix_.clear();
    currentCandidates_.clear();
    currentCandidateConsumed_.clear();
    candidatePage_ = 0;
    shorthandActive_ = false;
    shorthandSegments_.clear();
    if (candidateWindow_) candidateWindow_->Hide();

    EditSession* pSession = new EditSession(this, pContext, EditSession::END_COMPOSITION);
    HRESULT hr;
    pContext->RequestEditSession(clientId_, pSession, TF_ES_ASYNCDONTCARE | TF_ES_READWRITE, &hr);
    pSession->Release();
}

void NomTextService::OnNumber(ITfContext* pContext, int num) {
    int idx = candidatePage_ * MAX_CANDIDATES_DISPLAY + num - 1;
    if (idx < 0 || idx >= (int)currentCandidates_.size()) return;
    CommitCandidate(idx);

    std::wstring text = lockedPrefix_ + currentCandidates_[idx];

    // Clear state and commit
    std::wstring commitText = text;
    BumpRecent(currentCandidates_[idx]);

    composing_.clear();
    lockedPrefix_.clear();
    currentCandidates_.clear();
    currentCandidateConsumed_.clear();
    candidatePage_ = 0;
    shorthandActive_ = false;
    shorthandSegments_.clear();
    if (candidateWindow_) candidateWindow_->Hide();

    EditSession* pSession = new EditSession(this, pContext,
        EditSession::COMMIT_TEXT, commitText);
    HRESULT hr;
    pContext->RequestEditSession(clientId_, pSession, TF_ES_ASYNCDONTCARE | TF_ES_READWRITE, &hr);
    pSession->Release();
}

void NomTextService::CommitCandidate(int index) {
    if (index < 0 || index >= (int)currentCandidates_.size()) return;
    // For now simple commit - advanced segment picking can be added later
}

void NomTextService::CommitComposing(ITfContext* pContext) {
    std::wstring display;
    if (lockedPrefix_.empty()) display = composing_;
    else if (composing_.empty()) display = lockedPrefix_;
    else display = lockedPrefix_ + L" " + composing_;

    std::wstring text = StringUtil::Trim(display);
    if (text.empty()) {
        EditSession* pSession = new EditSession(this, pContext, EditSession::END_COMPOSITION);
        HRESULT hr;
        pContext->RequestEditSession(clientId_, pSession, TF_ES_ASYNCDONTCARE | TF_ES_READWRITE, &hr);
        pSession->Release();
    }
    else {
        EditSession* pSession = new EditSession(this, pContext,
            EditSession::COMMIT_TEXT, text);
        HRESULT hr;
        pContext->RequestEditSession(clientId_, pSession, TF_ES_ASYNCDONTCARE | TF_ES_READWRITE, &hr);
        pSession->Release();
    }

    composing_.clear();
    lockedPrefix_.clear();
    currentCandidates_.clear();
    currentCandidateConsumed_.clear();
    candidatePage_ = 0;
    shorthandActive_ = false;
    shorthandSegments_.clear();
    if (candidateWindow_) candidateWindow_->Hide();
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

void NomTextService::UpdateCandidates() {
    if (composing_.empty() && lockedPrefix_.empty()) {
        currentCandidates_.clear();
        currentCandidateConsumed_.clear();
        candidatePage_ = 0;
        if (candidateWindow_) candidateWindow_->Hide();
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
            auto results = (k > 1) ? dict.LookupWord(prefix) : dict.LookupSingle(prefix);
            for (auto& v : results) {
                bool found = false;
                for (auto& c : currentCandidates_) if (c == v) { found = true; break; }
                if (!found) {
                    currentCandidates_.push_back(v);
                    currentCandidateConsumed_.push_back(k);
                }
            }
            if (k == 1) {
                auto singles = dict.LookupSingle(prefix);
                for (auto& v : singles) {
                    bool found = false;
                    for (auto& c : currentCandidates_) if (c == v) { found = true; break; }
                    if (!found) {
                        currentCandidates_.push_back(v);
                        currentCandidateConsumed_.push_back(1);
                    }
                }
            }
        }
    }
    else {
        // Simple lookup
        std::wstring trimmed = StringUtil::Trim(composing_);
        currentCandidates_ = dict.Lookup(trimmed);
        currentCandidateConsumed_.resize(currentCandidates_.size());
        for (int i = 0; i < (int)currentCandidateConsumed_.size(); i++) {
            currentCandidateConsumed_[i] = 1;
        }
    }

    // Sort by recency
    if (!currentCandidates_.empty()) {
        // Create index array and sort
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
            return ra > rb;
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

    candidatePage_ = 0;

    // Update candidate window
    if (!currentCandidates_.empty()) {
        // Ensure window exists
        if (!candidateWindow_) {
            candidateWindow_ = new CandidateWindow(this);
            candidateWindow_->Create(NULL);
        }

        // Build composing display for the candidate bar
        std::wstring composingDisplay;
        if (lockedPrefix_.empty()) composingDisplay = composing_;
        else if (composing_.empty()) composingDisplay = lockedPrefix_;
        else composingDisplay = lockedPrefix_ + L" " + composing_;
        candidateWindow_->SetComposing(composingDisplay);

        candidateWindow_->SetCandidates(currentCandidates_, candidatePage_);

        // Position near the system caret
        POINT pt = { 0, 0 };
        GUITHREADINFO gti = {};
        gti.cbSize = sizeof(GUITHREADINFO);
        if (GetGUIThreadInfo(0, &gti) && gti.hwndCaret) {
            pt.x = gti.rcCaret.left;
            pt.y = gti.rcCaret.bottom + 4;
            ClientToScreen(gti.hwndCaret, &pt);
        }
        else {
            // Fallback: use GetCaretPos
            GetCaretPos(&pt);
            HWND hFg = GetForegroundWindow();
            if (hFg) ClientToScreen(hFg, &pt);
            pt.y += 24;
        }
        candidateWindow_->MoveTo(pt.x, pt.y);
        candidateWindow_->Show();
    }
    else if (candidateWindow_) {
        candidateWindow_->Hide();
    }
}

// ---- Preferences ----

void NomTextService::LoadPreferences() {
    // Read from registry: HKCU\Software\NomKeyboard
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\NomKeyboard", 0,
        KEY_READ, &hKey) == ERROR_SUCCESS)
    {
        DWORD val = 0, sz = sizeof(DWORD);
        if (RegQueryValueExW(hKey, L"ToneStyleOld", NULL, NULL, (BYTE*)&val, &sz) == ERROR_SUCCESS)
            toneStyleOld_ = (val != 0);
        val = 1; sz = sizeof(DWORD);
        if (RegQueryValueExW(hKey, L"ShorthandEnabled", NULL, NULL, (BYTE*)&val, &sz) == ERROR_SUCCESS)
            shorthandEnabled_ = (val != 0);
        val = 1; sz = sizeof(DWORD);
        if (RegQueryValueExW(hKey, L"SegmentMode", NULL, NULL, (BYTE*)&val, &sz) == ERROR_SUCCESS)
            segmentMode_ = (val != 0);
        RegCloseKey(hKey);
    }
}

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
