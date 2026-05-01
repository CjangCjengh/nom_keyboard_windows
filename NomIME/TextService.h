// TextService.h — Main TSF Text Service (ITfTextInputProcessorEx)
#pragma once
#include "Globals.h"
#include "CandidateWindow.h"

class NomTextService : public ITfTextInputProcessorEx,
                       public ITfKeyEventSink,
                       public ITfCompositionSink,
                       public ITfThreadMgrEventSink,
                       public ITfTextEditSink {
public:
    NomTextService();
    virtual ~NomTextService();

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppvObj) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;

    // ITfTextInputProcessor
    STDMETHODIMP Activate(ITfThreadMgr* pThreadMgr, TfClientId tfClientId) override;
    STDMETHODIMP Deactivate() override;

    // ITfTextInputProcessorEx
    STDMETHODIMP ActivateEx(ITfThreadMgr* pThreadMgr, TfClientId tfClientId, DWORD dwFlags) override;

    // ITfKeyEventSink
    STDMETHODIMP OnSetFocus(BOOL fForeground) override;
    STDMETHODIMP OnTestKeyDown(ITfContext* pContext, WPARAM wParam, LPARAM lParam, BOOL* pfEaten) override;
    STDMETHODIMP OnTestKeyUp(ITfContext* pContext, WPARAM wParam, LPARAM lParam, BOOL* pfEaten) override;
    STDMETHODIMP OnKeyDown(ITfContext* pContext, WPARAM wParam, LPARAM lParam, BOOL* pfEaten) override;
    STDMETHODIMP OnKeyUp(ITfContext* pContext, WPARAM wParam, LPARAM lParam, BOOL* pfEaten) override;
    STDMETHODIMP OnPreservedKey(ITfContext* pContext, REFGUID rguid, BOOL* pfEaten) override;

    // ITfCompositionSink
    STDMETHODIMP OnCompositionTerminated(TfEditCookie ecWrite, ITfComposition* pComposition) override;

    // ITfThreadMgrEventSink
    STDMETHODIMP OnInitDocumentMgr(ITfDocumentMgr* pDocMgr) override;
    STDMETHODIMP OnUninitDocumentMgr(ITfDocumentMgr* pDocMgr) override;
    STDMETHODIMP OnSetFocus(ITfDocumentMgr* pDocMgrFocus, ITfDocumentMgr* pDocMgrPrevFocus) override;
    STDMETHODIMP OnPushContext(ITfContext* pContext) override;
    STDMETHODIMP OnPopContext(ITfContext* pContext) override;

    // ITfTextEditSink
    STDMETHODIMP OnEndEdit(ITfContext* pContext, TfEditCookie ecReadOnly,
                           ITfEditRecord* pEditRecord) override;

    // Internal methods called by key handling
    void OnChar(ITfContext* pContext, wchar_t ch);
    void OnBackspace(ITfContext* pContext);
    void OnSpace(ITfContext* pContext);
    void OnEnter(ITfContext* pContext);
    void OnEscape(ITfContext* pContext);
    void OnNumber(ITfContext* pContext, int num);

    // Candidate management
    void UpdateCandidates();
    void CommitCandidate(int index);
    void CommitComposing(ITfContext* pContext);

    TfClientId GetClientId() const { return clientId_; }
    ITfThreadMgr* GetThreadMgr() const { return threadMgr_; }

private:
    // Composition management
    HRESULT StartComposition(ITfContext* pContext);
    HRESULT EndComposition(ITfContext* pContext);
    HRESULT SetComposingText(ITfContext* pContext, const std::wstring& text);

    // Viết tắt (abbreviated input) support
    struct ShorthandSegments {
        bool active = false;
        std::vector<std::wstring> segments;
    };
    ShorthandSegments TryShorthandSplit(const std::wstring& raw) const;

    // Segment-mode pick history (for auto-learning)
    struct LockedStep {
        std::wstring rawConsumed;  // Vietnamese syllables consumed
        std::wstring nomText;      // Nom characters picked
        std::wstring learnKey;     // override reading for learning (e.g. from shorthand)
    };
    std::vector<LockedStep> lockedHistory_;

    // Auto-learning
    void LearnUserPhrases();
    void ObserveNgramForCommit(const std::wstring& commitString);

    LONG refCount_;
    TfClientId clientId_;
    ITfThreadMgr* threadMgr_;
    DWORD threadMgrSinkCookie_;
    DWORD textEditSinkCookie_;
    ITfComposition* composition_;
    ITfContext* compositionContext_;

    // Composing state
    std::wstring composing_;        // Current Vietnamese text being composed
    std::wstring lockedPrefix_;     // Already-confirmed Nom prefix (segment mode)

    // Shorthand state
    bool shorthandActive_;
    std::vector<std::wstring> shorthandSegments_;

    // True iff the PREVIOUS keystroke was a standalone-w shortcut (composing was empty
    // or the syllable had no prior letters, and the engine appended ư/Ư). Used on the
    // NEXT keystroke to distinguish `w`+`w` (undo the shortcut → literal `w`) from
    // `u`+`w`+`w` (undo the merge → `uw`). Cleared on any non-engine buffer mutation
    // (backspace, commit, focus change, shorthand, …).
    bool lastWasStandaloneW_ = false;

    // Candidates
    std::vector<std::wstring> currentCandidates_;
    std::vector<int> currentCandidateConsumed_;
    int candidatePage_;

    // Candidate window
    CandidateWindow* candidateWindow_;

    // Preferences
    bool toneStyleOld_;
    bool shorthandEnabled_;
    bool segmentMode_;

    // Recent counts for ranking
    std::unordered_map<std::wstring, int> recentCounts_;

    void LoadPreferences();
    void SaveRecentCounts();
    void LoadRecentCounts();
    void BumpRecent(const std::wstring& text);

    // Edit session helper class
    class EditSession;
};
