// CandidateWindow.cpp — Horizontal candidate bar (Win10/11 Pinyin style)
#include "CandidateWindow.h"
#include "TextService.h"
#include <dwmapi.h>

const wchar_t* CandidateWindow::WINDOW_CLASS_NAME = L"NomIME_CandidateWindow";
bool CandidateWindow::classRegistered_ = false;

CandidateWindow::CandidateWindow(NomTextService* pService)
    : service_(pService), hwnd_(nullptr), currentPage_(0), totalPages_(0),
      totalWidth_(0), totalHeight_(0)
{
    rcPrev_ = rcNext_ = { 0,0,0,0 };
}

CandidateWindow::~CandidateWindow() { Destroy(); }

BOOL CandidateWindow::RegisterWindowClass() {
    if (classRegistered_) return TRUE;
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DROPSHADOW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = g_hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.lpszClassName = WINDOW_CLASS_NAME;
    if (RegisterClassExW(&wc)) { classRegistered_ = true; return TRUE; }
    return FALSE;
}

BOOL CandidateWindow::Create(HWND hParent) {
    if (!RegisterWindowClass()) return FALSE;
    hwnd_ = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        WINDOW_CLASS_NAME, L"", WS_POPUP,
        0, 0, 100, 40, hParent, NULL, g_hInst, this);
    if (!hwnd_) return FALSE;
    DWORD corner = 2;
    DwmSetWindowAttribute(hwnd_, 33, &corner, sizeof(corner));
    return TRUE;
}

void CandidateWindow::Destroy() {
    if (hwnd_) { DestroyWindow(hwnd_); hwnd_ = nullptr; }
}

void CandidateWindow::Show() {
    if (!hwnd_) Create(NULL);
    if (hwnd_ && !candidates_.empty()) {
        RecalcLayout();
        SetWindowPos(hwnd_, HWND_TOPMOST, 0, 0, totalWidth_, totalHeight_,
            SWP_NOMOVE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
        ShowWindow(hwnd_, SW_SHOWNA);
        InvalidateRect(hwnd_, NULL, TRUE);
    }
}

void CandidateWindow::Hide() {
    if (hwnd_) ShowWindow(hwnd_, SW_HIDE);
}

BOOL CandidateWindow::IsVisible() const {
    return hwnd_ && IsWindowVisible(hwnd_);
}

void CandidateWindow::MoveTo(int x, int y) {
    if (!hwnd_) return;
    RECT rcWork;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &rcWork, 0);
    if (x + totalWidth_ > rcWork.right) x = rcWork.right - totalWidth_;
    if (y + totalHeight_ > rcWork.bottom) y = rcWork.bottom - totalHeight_ - 30;
    if (x < rcWork.left) x = rcWork.left;
    if (y < rcWork.top) y = rcWork.top;
    SetWindowPos(hwnd_, HWND_TOPMOST, x, y, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE);
}

void CandidateWindow::SetCandidates(const std::vector<std::wstring>& candidates, int page) {
    candidates_ = candidates;
    currentPage_ = page;
    totalPages_ = ((int)candidates.size() + MAX_CANDIDATES_DISPLAY - 1) / MAX_CANDIDATES_DISPLAY;
    if (hwnd_) { RecalcLayout(); InvalidateRect(hwnd_, NULL, TRUE); }
}

void CandidateWindow::SetComposing(const std::wstring& composing) {
    composingText_ = composing;
    // No visual update — composing text is shown in the editor, not the candidate bar
}

void CandidateWindow::NextPage() {
    if (currentPage_ < totalPages_ - 1) { currentPage_++; if (hwnd_) { RecalcLayout(); InvalidateRect(hwnd_, NULL, TRUE); } }
}
void CandidateWindow::PrevPage() {
    if (currentPage_ > 0) { currentPage_--; if (hwnd_) { RecalcLayout(); InvalidateRect(hwnd_, NULL, TRUE); } }
}

void CandidateWindow::RecalcLayout() {
    if (!hwnd_) return;
    HDC hdc = GetDC(hwnd_);

    HFONT hCandFont = CreateFontW(CAND_FONT_SIZE, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei UI");
    HFONT hNumFont = CreateFontW(NUM_FONT_SIZE, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei UI");
    HFONT hOld = (HFONT)SelectObject(hdc, hCandFont);

    itemRects_.clear();
    int startIdx = currentPage_ * MAX_CANDIDATES_DISPLAY;
    int endIdx = (std::min)(startIdx + MAX_CANDIDATES_DISPLAY, (int)candidates_.size());

    int x = EDGE_PAD;
    int y = EDGE_PAD;

    for (int i = startIdx; i < endIdx; i++) {
        int num = i - startIdx + 1;

        // Measure number width
        wchar_t numStr[4];
        swprintf_s(numStr, L"%d", num);
        SelectObject(hdc, hNumFont);
        SIZE szNum;
        GetTextExtentPoint32W(hdc, numStr, (int)wcslen(numStr), &szNum);

        // Measure candidate text width
        SelectObject(hdc, hCandFont);
        SIZE szText;
        GetTextExtentPoint32W(hdc, candidates_[i].c_str(), (int)candidates_[i].size(), &szText);

        int itemW = szNum.cx + ITEM_INNER_PAD + szText.cx + ITEM_GAP;
        ItemRect ir;
        ir.rc = { x, y, x + itemW, y + ROW_HEIGHT };
        ir.index = i;
        itemRects_.push_back(ir);
        x += itemW;
    }

    // Page nav
    if (totalPages_ > 1) {
        rcPrev_ = { x + 4, y, x + 4 + NAV_BTN_W, y + ROW_HEIGHT };
        x += 4 + NAV_BTN_W;
        rcNext_ = { x + 2, y, x + 2 + NAV_BTN_W, y + ROW_HEIGHT };
        x += 2 + NAV_BTN_W;
    } else {
        rcPrev_ = rcNext_ = { 0,0,0,0 };
    }

    x += EDGE_PAD;
    totalWidth_ = x;
    totalHeight_ = y + ROW_HEIGHT + EDGE_PAD;

    SelectObject(hdc, hOld);
    DeleteObject(hCandFont);
    DeleteObject(hNumFont);
    ReleaseDC(hwnd_, hdc);
}

LRESULT CALLBACK CandidateWindow::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    CandidateWindow* pWnd = nullptr;
    if (msg == WM_NCCREATE) {
        auto lpcs = reinterpret_cast<LPCREATESTRUCT>(lParam);
        pWnd = reinterpret_cast<CandidateWindow*>(lpcs->lpCreateParams);
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pWnd));
    } else {
        pWnd = reinterpret_cast<CandidateWindow*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
    }
    switch (msg) {
    case WM_PAINT:       if (pWnd) pWnd->OnPaint(); return 0;
    case WM_LBUTTONDOWN: if (pWnd) pWnd->OnMouseDown(LOWORD(lParam), HIWORD(lParam)); return 0;
    case WM_ERASEBKGND:  return 1;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

void CandidateWindow::OnPaint() {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd_, &ps);
    RECT rc;
    GetClientRect(hwnd_, &rc);

    HDC hdcMem = CreateCompatibleDC(hdc);
    HBITMAP hbm = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
    HBITMAP hbmOld = (HBITMAP)SelectObject(hdcMem, hbm);

    // White background
    HBRUSH hBg = CreateSolidBrush(RGB(255, 255, 255));
    FillRect(hdcMem, &rc, hBg);
    DeleteObject(hBg);

    // Thin border
    HPEN hBorder = CreatePen(PS_SOLID, 1, RGB(200, 200, 200));
    HPEN hOldPen = (HPEN)SelectObject(hdcMem, hBorder);
    SelectObject(hdcMem, (HBRUSH)GetStockObject(NULL_BRUSH));
    Rectangle(hdcMem, 0, 0, rc.right, rc.bottom);
    SelectObject(hdcMem, hOldPen);
    DeleteObject(hBorder);

    HFONT hCandFont = CreateFontW(CAND_FONT_SIZE, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei UI");
    HFONT hNumFont = CreateFontW(NUM_FONT_SIZE, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei UI");

    SetBkMode(hdcMem, TRANSPARENT);

    for (size_t i = 0; i < itemRects_.size(); i++) {
        auto& ir = itemRects_[i];
        int num = (int)(i + 1);
        int idx = ir.index;

        // Highlight first item
        if (i == 0) {
            HBRUSH hHL = CreateSolidBrush(RGB(204, 232, 255));
            FillRect(hdcMem, &ir.rc, hHL);
            DeleteObject(hHL);
        }

        // Number label
        wchar_t numStr[4];
        swprintf_s(numStr, L"%d", num);
        HFONT hPrev = (HFONT)SelectObject(hdcMem, hNumFont);
        SetTextColor(hdcMem, RGB(140, 140, 140));
        SIZE szNum;
        GetTextExtentPoint32W(hdcMem, numStr, (int)wcslen(numStr), &szNum);
        RECT rcNum = ir.rc;
        rcNum.right = rcNum.left + szNum.cx + 2;
        DrawTextW(hdcMem, numStr, -1, &rcNum, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        // Candidate character
        SelectObject(hdcMem, hCandFont);
        SetTextColor(hdcMem, RGB(0, 0, 0));
        RECT rcText = ir.rc;
        rcText.left = rcNum.right + ITEM_INNER_PAD;
        DrawTextW(hdcMem, candidates_[idx].c_str(), (int)candidates_[idx].size(),
            &rcText, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        SelectObject(hdcMem, hPrev);
    }

    // Page nav arrows
    if (totalPages_ > 1) {
        HFONT hPrev = (HFONT)SelectObject(hdcMem, hNumFont);
        SetTextColor(hdcMem, RGB(120, 120, 120));
        DrawTextW(hdcMem, L"\x25C0", 1, &rcPrev_, DT_CENTER | DT_VCENTER | DT_SINGLELINE); // ◀
        DrawTextW(hdcMem, L"\x25B6", 1, &rcNext_, DT_CENTER | DT_VCENTER | DT_SINGLELINE); // ▶
        SelectObject(hdcMem, hPrev);
    }

    DeleteObject(hCandFont);
    DeleteObject(hNumFont);

    BitBlt(hdc, 0, 0, rc.right, rc.bottom, hdcMem, 0, 0, SRCCOPY);
    SelectObject(hdcMem, hbmOld);
    DeleteObject(hbm);
    DeleteDC(hdcMem);
    EndPaint(hwnd_, &ps);
}

void CandidateWindow::OnMouseDown(int x, int y) {
    for (auto& ir : itemRects_) {
        POINT pt = { x, y };
        if (PtInRect(&ir.rc, pt)) {
            int num = ir.index - currentPage_ * MAX_CANDIDATES_DISPLAY + 1;
            if (service_ && num >= 1 && num <= 9) {
                HWND hFocus = GetFocus();
                if (hFocus) PostMessageW(hFocus, WM_KEYDOWN, '0' + num, 0);
            }
            return;
        }
    }
    if (totalPages_ > 1) {
        POINT pt = { x, y };
        if (PtInRect(&rcPrev_, pt)) { PrevPage(); Show(); return; }
        if (PtInRect(&rcNext_, pt)) { NextPage(); Show(); return; }
    }
}

void CandidateWindow::MoveNearCaret(ITfContext* pContext) {
    if (!pContext || !hwnd_) return;
    POINT pt = { 0, 0 };
    GUITHREADINFO gti = {};
    gti.cbSize = sizeof(GUITHREADINFO);
    if (GetGUIThreadInfo(0, &gti) && gti.hwndCaret) {
        pt.x = gti.rcCaret.left;
        pt.y = gti.rcCaret.bottom + 4;
        ClientToScreen(gti.hwndCaret, &pt);
    } else {
        GetCaretPos(&pt);
        HWND hFg = GetForegroundWindow();
        if (hFg) ClientToScreen(hFg, &pt);
        pt.y += 24;
    }
    MoveTo(pt.x, pt.y);
}
