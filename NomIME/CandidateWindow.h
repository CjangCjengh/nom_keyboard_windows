// CandidateWindow.h — Candidate selection window (Win10/11 horizontal style)
#pragma once
#include "Globals.h"

class NomTextService;

/**
 * Horizontal candidate bar like Windows 10/11 Microsoft Pinyin IME.
 * Single row: "1 人  2 忍  3 认  4 壬  5 任  < >"
 * No composing text display (that's already shown in the editor's underline).
 */
class CandidateWindow {
public:
    CandidateWindow(NomTextService* pService);
    ~CandidateWindow();

    BOOL Create(HWND hParent = NULL);
    void Destroy();

    void Show();
    void Hide();
    BOOL IsVisible() const;

    void MoveTo(int x, int y);
    void MoveNearCaret(ITfContext* pContext);

    void SetCandidates(const std::vector<std::wstring>& candidates, int page);
    void SetComposing(const std::wstring& composing); // kept for API compat, does nothing visually

    void NextPage();
    void PrevPage();
    int GetCurrentPage() const { return currentPage_; }

private:
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void OnPaint();
    void OnMouseDown(int x, int y);
    void RecalcLayout();

    static BOOL RegisterWindowClass();
    static const wchar_t* WINDOW_CLASS_NAME;
    static bool classRegistered_;

    NomTextService* service_;
    HWND hwnd_;
    std::vector<std::wstring> candidates_;
    std::wstring composingText_;
    int currentPage_;
    int totalPages_;

    struct ItemRect { RECT rc; int index; };
    std::vector<ItemRect> itemRects_;
    RECT rcPrev_, rcNext_;
    int totalWidth_, totalHeight_;

    // Candidate text size: 20px for CJK characters
    static const int CAND_FONT_SIZE = 20;
    // Number label size
    static const int NUM_FONT_SIZE  = 13;
    // Row height
    static const int ROW_HEIGHT     = 36;
    // Horizontal padding between items
    static const int ITEM_GAP       = 6;
    // Padding inside each item (number + text)
    static const int ITEM_INNER_PAD = 4;
    // Window edge padding
    static const int EDGE_PAD       = 8;
    // Nav button width
    static const int NAV_BTN_W      = 24;
};
