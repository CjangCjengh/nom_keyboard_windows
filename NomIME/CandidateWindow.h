// CandidateWindow.h — Candidate selection window (Win10/11 style)
#pragma once
#include "Globals.h"

class NomTextService; // forward declaration

/**
 * Candidate selection window that mimics the Windows 10/11 Microsoft Pinyin IME style.
 * Features:
 *   - Rounded corners with shadow (DWM)
 *   - Fluent/Acrylic-like semi-transparent background
 *   - Number labels (1-9) for each candidate
 *   - Composing text display at the top
 *   - Page navigation indicators
 */
class CandidateWindow {
public:
    CandidateWindow(NomTextService* pService);
    ~CandidateWindow();

    // Create the window (call after service is activated)
    BOOL Create(HWND hParent = NULL);
    void Destroy();

    // Show/hide
    void Show();
    void Hide();
    BOOL IsVisible() const;

    // Position near the composition caret
    void MoveTo(int x, int y);
    void MoveNearCaret(ITfContext* pContext);

    // Update content
    void SetCandidates(const std::vector<std::wstring>& candidates, int page);
    void SetComposing(const std::wstring& composing);

    // Page navigation
    void NextPage();
    void PrevPage();
    int GetCurrentPage() const { return currentPage_; }

private:
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void OnPaint();
    void OnMouseDown(int x, int y);

    // Register window class (once)
    static BOOL RegisterWindowClass();
    static const wchar_t* WINDOW_CLASS_NAME;
    static bool classRegistered_;

    NomTextService* service_;
    HWND hwnd_;
    std::vector<std::wstring> candidates_;
    std::wstring composingText_;
    int currentPage_;
    int totalPages_;

    // Layout constants (Win10/11 style)
    static const int PADDING = 8;
    static const int ITEM_HEIGHT = 32;
    static const int NUMBER_WIDTH = 24;
    static const int MIN_WIDTH = 320;
    static const int CORNER_RADIUS = 8;
    static const int SHADOW_SIZE = 4;
    static const int COMPOSING_HEIGHT = 28;
    static const int FONT_SIZE = 16;
    static const int NUMBER_FONT_SIZE = 12;
};
