// Composition.cpp — Composition helper (TSF composition management)
// This file provides additional composition management utilities.
// The core composition logic is in TextService.cpp's EditSession class.

#include "TextService.h"
#include "StringUtil.h"

// Additional composition utilities can be added here as the IME grows.
// For now, the primary composition logic lives in TextService.cpp.

// Helper: get the caret position on screen for candidate window placement.
// This is called from the text service when the candidate window needs repositioning.
void CandidateWindow::MoveNearCaret(ITfContext* pContext) {
    if (!pContext || !hwnd_) return;

    // Try to get the composition range's screen coordinates
    ITfContextView* pView = nullptr;
    if (SUCCEEDED(pContext->GetActiveView(&pView))) {
        // Get the current selection/caret position
        // We need an edit session to read the selection, but for positioning
        // we can use a simpler approach: get the last known caret position
        // from the system caret or the IMM compatibility layer.

        POINT pt = { 0, 0 };
        HWND hFocusWnd = nullptr;

        // Method 1: Try GetGUIThreadInfo for the caret position
        GUITHREADINFO gti = {};
        gti.cbSize = sizeof(GUITHREADINFO);
        if (GetGUIThreadInfo(0, &gti)) {
            pt.x = gti.rcCaret.left;
            pt.y = gti.rcCaret.bottom;
            hFocusWnd = gti.hwndCaret;
            if (hFocusWnd) {
                ClientToScreen(hFocusWnd, &pt);
            }
        }

        // Fallback: try to use GetCaretPos
        if (pt.x == 0 && pt.y == 0) {
            GetCaretPos(&pt);
            HWND hFg = GetForegroundWindow();
            if (hFg) ClientToScreen(hFg, &pt);
        }

        MoveTo(pt.x, pt.y + 4);
        pView->Release();
    }
}
