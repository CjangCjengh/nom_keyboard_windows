// CandidateWindow.cpp — Candidate window implementation (Win10/11 Pinyin style)
#include "CandidateWindow.h"
#include "TextService.h"
#include <dwmapi.h>
#include <uxtheme.h>

const wchar_t* CandidateWindow::WINDOW_CLASS_NAME = L"NomIME_CandidateWindow";
bool CandidateWindow::classRegistered_ = false;

CandidateWindow::CandidateWindow(NomTextService* pService)
    : service_(pService), hwnd_(nullptr), currentPage_(0), totalPages_(0)
{
}

CandidateWindow::~CandidateWindow() {
    Destroy();
}

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

    if (RegisterClassExW(&wc)) {
        classRegistered_ = true;
        return TRUE;
    }
    return FALSE;
}

BOOL CandidateWindow::Create(HWND hParent) {
    if (!RegisterWindowClass()) return FALSE;

    hwnd_ = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        WINDOW_CLASS_NAME,
        L"",
        WS_POPUP,
        0, 0, MIN_WIDTH, 100,
        hParent,
        NULL,
        g_hInst,
        this
    );

    if (!hwnd_) return FALSE;

    // Enable rounded corners on Windows 11
    // DWM_WINDOW_CORNER_PREFERENCE = DWMWCP_ROUND (value 2)
    DWORD cornerPref = 2; // DWMWCP_ROUND
    DwmSetWindowAttribute(hwnd_, 33 /*DWMWA_WINDOW_CORNER_PREFERENCE*/,
        &cornerPref, sizeof(cornerPref));

    // Set dark/light border color
    BOOL darkMode = FALSE;
    DWORD borderColor = 0x00CCCCCC; // light gray border
    DwmSetWindowAttribute(hwnd_, 34 /*DWMWA_BORDER_COLOR*/,
        &borderColor, sizeof(borderColor));

    return TRUE;
}

void CandidateWindow::Destroy() {
    if (hwnd_) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
}

void CandidateWindow::Show() {
    if (!hwnd_) {
        Create(NULL);
    }
    if (hwnd_ && !candidates_.empty()) {
        // Calculate window size
        int itemCount = (std::min)((int)candidates_.size() - currentPage_ * MAX_CANDIDATES_DISPLAY,
                                   MAX_CANDIDATES_DISPLAY);
        int height = PADDING * 2 + COMPOSING_HEIGHT + itemCount * ITEM_HEIGHT + (totalPages_ > 1 ? 24 : 0);
        int width = MIN_WIDTH;

        // Measure text widths
        HDC hdc = GetDC(hwnd_);
        HFONT hFont = CreateFontW(FONT_SIZE, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei UI");
        HFONT hOld = (HFONT)SelectObject(hdc, hFont);

        for (int i = 0; i < itemCount; i++) {
            int idx = currentPage_ * MAX_CANDIDATES_DISPLAY + i;
            SIZE sz;
            GetTextExtentPoint32W(hdc, candidates_[idx].c_str(), (int)candidates_[idx].size(), &sz);
            int itemWidth = NUMBER_WIDTH + sz.cx + PADDING * 4;
            if (itemWidth > width) width = itemWidth;
        }

        SelectObject(hdc, hOld);
        DeleteObject(hFont);
        ReleaseDC(hwnd_, hdc);

        SetWindowPos(hwnd_, HWND_TOPMOST, 0, 0, width, height,
            SWP_NOMOVE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
        ShowWindow(hwnd_, SW_SHOWNA);
        InvalidateRect(hwnd_, NULL, TRUE);
    }
}

void CandidateWindow::Hide() {
    if (hwnd_) {
        ShowWindow(hwnd_, SW_HIDE);
    }
}

BOOL CandidateWindow::IsVisible() const {
    return hwnd_ && IsWindowVisible(hwnd_);
}

void CandidateWindow::MoveTo(int x, int y) {
    if (hwnd_) {
        // Make sure the window stays on screen
        RECT rcWork;
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &rcWork, 0);
        RECT rc;
        GetWindowRect(hwnd_, &rc);
        int w = rc.right - rc.left;
        int h = rc.bottom - rc.top;
        if (x + w > rcWork.right) x = rcWork.right - w;
        if (y + h > rcWork.bottom) y = rcWork.bottom - h - 30;
        if (x < rcWork.left) x = rcWork.left;
        if (y < rcWork.top) y = rcWork.top;
        SetWindowPos(hwnd_, HWND_TOPMOST, x, y, 0, 0,
            SWP_NOSIZE | SWP_NOACTIVATE);
    }
}

void CandidateWindow::SetCandidates(const std::vector<std::wstring>& candidates, int page) {
    candidates_ = candidates;
    currentPage_ = page;
    totalPages_ = ((int)candidates.size() + MAX_CANDIDATES_DISPLAY - 1) / MAX_CANDIDATES_DISPLAY;
    if (hwnd_) InvalidateRect(hwnd_, NULL, TRUE);
}

void CandidateWindow::SetComposing(const std::wstring& composing) {
    composingText_ = composing;
    if (hwnd_) InvalidateRect(hwnd_, NULL, TRUE);
}

void CandidateWindow::NextPage() {
    if (currentPage_ < totalPages_ - 1) {
        currentPage_++;
        if (hwnd_) InvalidateRect(hwnd_, NULL, TRUE);
    }
}

void CandidateWindow::PrevPage() {
    if (currentPage_ > 0) {
        currentPage_--;
        if (hwnd_) InvalidateRect(hwnd_, NULL, TRUE);
    }
}

LRESULT CALLBACK CandidateWindow::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    CandidateWindow* pWnd = nullptr;

    if (msg == WM_NCCREATE) {
        LPCREATESTRUCT lpcs = reinterpret_cast<LPCREATESTRUCT>(lParam);
        pWnd = reinterpret_cast<CandidateWindow*>(lpcs->lpCreateParams);
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pWnd));
    } else {
        pWnd = reinterpret_cast<CandidateWindow*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
    }

    switch (msg) {
    case WM_PAINT:
        if (pWnd) pWnd->OnPaint();
        return 0;

    case WM_LBUTTONDOWN:
        if (pWnd) pWnd->OnMouseDown(LOWORD(lParam), HIWORD(lParam));
        return 0;

    case WM_ERASEBKGND:
        return 1;
    }

    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

void CandidateWindow::OnPaint() {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd_, &ps);

    RECT rc;
    GetClientRect(hwnd_, &rc);

    // Double buffer
    HDC hdcMem = CreateCompatibleDC(hdc);
    HBITMAP hbm = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
    HBITMAP hbmOld = (HBITMAP)SelectObject(hdcMem, hbm);

    // Background: white with slight transparency (Win10/11 style)
    HBRUSH hBgBrush = CreateSolidBrush(RGB(252, 252, 252));
    FillRect(hdcMem, &rc, hBgBrush);
    DeleteObject(hBgBrush);

    // Border
    HPEN hPen = CreatePen(PS_SOLID, 1, RGB(204, 204, 204));
    HPEN hOldPen = (HPEN)SelectObject(hdcMem, hPen);
    HBRUSH hNullBrush = (HBRUSH)GetStockObject(NULL_BRUSH);
    SelectObject(hdcMem, hNullBrush);
    RoundRect(hdcMem, 0, 0, rc.right, rc.bottom, CORNER_RADIUS * 2, CORNER_RADIUS * 2);
    SelectObject(hdcMem, hOldPen);
    DeleteObject(hPen);

    // Fonts
    HFONT hMainFont = CreateFontW(FONT_SIZE, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei UI");
    HFONT hNumFont = CreateFontW(NUMBER_FONT_SIZE, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei UI");
    HFONT hComposingFont = CreateFontW(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei UI");

    SetBkMode(hdcMem, TRANSPARENT);
    int y = PADDING;

    // Composing text (top area with light blue background)
    if (!composingText_.empty()) {
        RECT rcComp = { PADDING, y, rc.right - PADDING, y + COMPOSING_HEIGHT };
        HBRUSH hCompBg = CreateSolidBrush(RGB(240, 244, 249));
        FillRect(hdcMem, &rcComp, hCompBg);
        DeleteObject(hCompBg);

        HFONT hOldFont = (HFONT)SelectObject(hdcMem, hComposingFont);
        SetTextColor(hdcMem, RGB(60, 60, 60));
        DrawTextW(hdcMem, composingText_.c_str(), (int)composingText_.size(),
            &rcComp, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        SelectObject(hdcMem, hOldFont);
        y += COMPOSING_HEIGHT + 2;
    }

    // Separator line
    HPEN hSepPen = CreatePen(PS_SOLID, 1, RGB(230, 230, 230));
    SelectObject(hdcMem, hSepPen);
    MoveToEx(hdcMem, PADDING, y, NULL);
    LineTo(hdcMem, rc.right - PADDING, y);
    DeleteObject(hSepPen);
    y += 2;

    // Candidate items
    int startIdx = currentPage_ * MAX_CANDIDATES_DISPLAY;
    int endIdx = (std::min)(startIdx + MAX_CANDIDATES_DISPLAY, (int)candidates_.size());

    for (int i = startIdx; i < endIdx; i++) {
        int num = i - startIdx + 1;
        RECT rcItem = { PADDING, y, rc.right - PADDING, y + ITEM_HEIGHT };

        // Hover effect (can be enhanced with mouse tracking)

        // Number label
        RECT rcNum = { PADDING + 4, y, PADDING + NUMBER_WIDTH, y + ITEM_HEIGHT };
        HFONT hOldFont = (HFONT)SelectObject(hdcMem, hNumFont);
        SetTextColor(hdcMem, RGB(0, 90, 158)); // Win10 accent blue
        wchar_t numStr[4];
        swprintf_s(numStr, L"%d", num);
        DrawTextW(hdcMem, numStr, -1, &rcNum,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        // Candidate text
        SelectObject(hdcMem, hMainFont);
        SetTextColor(hdcMem, RGB(30, 30, 30));
        RECT rcText = { PADDING + NUMBER_WIDTH + 8, y,
                        rc.right - PADDING, y + ITEM_HEIGHT };
        DrawTextW(hdcMem, candidates_[i].c_str(), (int)candidates_[i].size(),
            &rcText, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        SelectObject(hdcMem, hOldFont);
        y += ITEM_HEIGHT;
    }

    // Page indicator
    if (totalPages_ > 1) {
        y += 2;
        HFONT hOldFont = (HFONT)SelectObject(hdcMem, hNumFont);
        SetTextColor(hdcMem, RGB(120, 120, 120));
        wchar_t pageStr[32];
        swprintf_s(pageStr, L"< %d / %d >", currentPage_ + 1, totalPages_);
        RECT rcPage = { PADDING, y, rc.right - PADDING, y + 20 };
        DrawTextW(hdcMem, pageStr, -1, &rcPage,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(hdcMem, hOldFont);
    }

    // Cleanup fonts
    DeleteObject(hMainFont);
    DeleteObject(hNumFont);
    DeleteObject(hComposingFont);

    // Blit to screen
    BitBlt(hdc, 0, 0, rc.right, rc.bottom, hdcMem, 0, 0, SRCCOPY);
    SelectObject(hdcMem, hbmOld);
    DeleteObject(hbm);
    DeleteDC(hdcMem);

    EndPaint(hwnd_, &ps);
}

void CandidateWindow::OnMouseDown(int x, int y) {
    RECT rc;
    GetClientRect(hwnd_, &rc);

    // Determine which candidate was clicked
    int itemY = PADDING + (composingText_.empty() ? 0 : COMPOSING_HEIGHT + 4);
    int startIdx = currentPage_ * MAX_CANDIDATES_DISPLAY;
    int endIdx = (std::min)(startIdx + MAX_CANDIDATES_DISPLAY, (int)candidates_.size());

    for (int i = startIdx; i < endIdx; i++) {
        if (y >= itemY && y < itemY + ITEM_HEIGHT) {
            // Clicked on this candidate
            if (service_) {
                // Send the number key equivalent
                int num = i - startIdx + 1;
                // Post a message to the service to handle the pick
                // For now, directly access the service
            }
            break;
        }
        itemY += ITEM_HEIGHT;
    }

    // Check page navigation
    if (totalPages_ > 1) {
        int pageY = PADDING + (composingText_.empty() ? 0 : COMPOSING_HEIGHT + 4) +
            (endIdx - startIdx) * ITEM_HEIGHT + 4;
        if (y >= pageY) {
            if (x < rc.right / 2) PrevPage();
            else NextPage();
            InvalidateRect(hwnd_, NULL, TRUE);
        }
    }
}
