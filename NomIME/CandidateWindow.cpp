// CandidateWindow.cpp — Horizontal candidate bar (Win10/11 Pinyin style)
#include "CandidateWindow.h"
#include "TextService.h"
#include "StringUtil.h"
#include <dwmapi.h>

const wchar_t* CandidateWindow::WINDOW_CLASS_NAME = L"NomIME_CandidateWindow";


CandidateWindow::CandidateWindow(NomTextService* pService)
    : service_(pService), hwnd_(nullptr), currentPage_(0), totalPages_(0),
      totalWidth_(0), totalHeight_(0)
{
    rcPrev_ = rcNext_ = { 0,0,0,0 };
}

CandidateWindow::~CandidateWindow() { Destroy(); }

BOOL CandidateWindow::RegisterWindowClass() {
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DROPSHADOW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = g_hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.lpszClassName = WINDOW_CLASS_NAME;
    return RegisterClassExW(&wc) || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
}

BOOL CandidateWindow::Create(HWND hParent) {
    if (!RegisterWindowClass()) return FALSE;
    hwnd_ = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        WINDOW_CLASS_NAME, L"", WS_POPUP,
        0, 0, 100, 40, hParent, NULL, g_hInst, this);
    if (!hwnd_) return FALSE;
    dpi_ = GetDpiForWindow(hwnd_);
    DWORD corner = 2;
    DwmSetWindowAttribute(hwnd_, 33, &corner, sizeof(corner));
    return TRUE;
}

void CandidateWindow::Destroy() {
    if (hwnd_) { DestroyWindow(hwnd_); hwnd_ = nullptr; }
    ReleaseFonts();
    if (privateFont_) RemoveFontMemResourceEx(privateFont_);
    privateFont_ = nullptr;
    triedPrivateFont_ = false;
    hasAnchor_ = false;
}

void CandidateWindow::Show() {
    if (!hwnd_) Create(NULL);
    if (!hwnd_ || !hasAnchor_ || candidates_.empty()) return;
    MONITORINFO info{sizeof(info)};
    if (!GetMonitorInfoW(MonitorFromPoint({anchor_.x, anchor_.y - 1}, MONITOR_DEFAULTTONEAREST), &info)) return;
    const RECT& work = info.rcWork;
    int x = (std::max)(work.left, (std::min)(anchor_.x, work.right - totalWidth_));
    int y = anchor_.y + Px(2);
    if (y + totalHeight_ > work.bottom) y = (std::max)(work.top, caretTop_ - Px(2) - totalHeight_);
    SetWindowPos(hwnd_, HWND_TOPMOST, x, y, totalWidth_, totalHeight_, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void CandidateWindow::Hide() {
    hasAnchor_ = false;
    if (hwnd_) ShowWindow(hwnd_, SW_HIDE);
}

BOOL CandidateWindow::IsVisible() const {
    return hwnd_ && IsWindowVisible(hwnd_);
}

void CandidateWindow::SetCandidates(const std::vector<std::wstring>& candidates) {
    candidates_ = candidates;
    currentPage_ = 0;
    totalPages_ = ((int)candidates.size() + MAX_CANDIDATES_DISPLAY - 1) / MAX_CANDIDATES_DISPLAY;
    if (hwnd_) { RecalcLayout(); InvalidateRect(hwnd_, NULL, TRUE); }
}

void CandidateWindow::NextPage() {
    if (currentPage_ < totalPages_ - 1) { currentPage_++; if (hwnd_) { RecalcLayout(); InvalidateRect(hwnd_, NULL, TRUE); } }
}
void CandidateWindow::PrevPage() {
    if (currentPage_ > 0) { currentPage_--; if (hwnd_) { RecalcLayout(); InvalidateRect(hwnd_, NULL, TRUE); } }
}

void CandidateWindow::EnsureFonts() {
    if (!triedPrivateFont_) {
        triedPrivateFont_ = true;
        // Load only when displaying candidates. GDI copies the bytes, so the
        // installed font file is not locked by every editor using the IME.
        std::ifstream file(StringUtil::GetInstallDir() + L"\\NomKeyboardSerif.ttf",
            std::ios::binary | std::ios::ate);
        const std::streamoff length = file.tellg();
        if (length > 0 && length <= 32 * 1024 * 1024) {
            std::vector<char> bytes(static_cast<size_t>(length));
            file.seekg(0);
            if (file.read(bytes.data(), length)) {
                DWORD count = 0;
                privateFont_ = AddFontMemResourceEx(bytes.data(), static_cast<DWORD>(length), nullptr, &count);
            }
        }
    }
    auto makeFont = [this](int size, const wchar_t* family) {
        return CreateFontW(-Px(size), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, family);
    };
    if (!candidateFont_) candidateFont_ = makeFont(CAND_FONT_SIZE, privateFont_ ? L"Nom Keyboard Serif" : L"Segoe UI");
    if (!numberFont_) numberFont_ = makeFont(NUM_FONT_SIZE, L"Segoe UI");
}

void CandidateWindow::ReleaseFonts() {
    DeleteObject(candidateFont_);
    DeleteObject(numberFont_);
    candidateFont_ = numberFont_ = nullptr;
}

void CandidateWindow::RecalcLayout() {
    if (!hwnd_) return;
    HDC hdc = GetDC(hwnd_);
    if (!hdc) return;

    EnsureFonts();
    HFONT hOld = (HFONT)SelectObject(hdc, candidateFont_);

    itemRects_.clear();
    int startIdx = currentPage_ * MAX_CANDIDATES_DISPLAY;
    int endIdx = (std::min)(startIdx + MAX_CANDIDATES_DISPLAY, (int)candidates_.size());

    int x = Px(EDGE_PAD);
    int y = Px(EDGE_PAD);

    for (int i = startIdx; i < endIdx; i++) {
        int num = i - startIdx + 1;

        // Measure number width
        wchar_t numStr[4];
        swprintf_s(numStr, L"%d", num);
        SelectObject(hdc, numberFont_);
        SIZE szNum;
        GetTextExtentPoint32W(hdc, numStr, (int)wcslen(numStr), &szNum);

        // Measure candidate text width
        SelectObject(hdc, candidateFont_);
        RECT textBounds{};
        DrawTextW(hdc, candidates_[i].c_str(), (int)candidates_[i].size(), &textBounds,
            DT_CALCRECT | DT_SINGLELINE | DT_NOPREFIX);

        int itemW = Px(6) + szNum.cx + Px(ITEM_INNER_PAD) + textBounds.right + Px(ITEM_GAP);
        ItemRect ir;
        ir.rc = { x, y, x + itemW, y + Px(ROW_HEIGHT) };
        ir.index = i;
        itemRects_.push_back(ir);
        x += itemW;
    }

    // Page nav
    if (totalPages_ > 1) {
        rcPrev_ = { x + 4, y, x + 4 + Px(NAV_BTN_W), y + Px(ROW_HEIGHT) };
        x += 4 + Px(NAV_BTN_W);
        rcNext_ = { x + 2, y, x + 2 + Px(NAV_BTN_W), y + Px(ROW_HEIGHT) };
        x += 2 + Px(NAV_BTN_W);
    } else {
        rcPrev_ = rcNext_ = { 0,0,0,0 };
    }

    x += Px(EDGE_PAD);
    totalWidth_ = x;
    totalHeight_ = y + Px(ROW_HEIGHT) + Px(EDGE_PAD);

    SelectObject(hdc, hOld);
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
    case WM_MOUSEACTIVATE: return MA_NOACTIVATE;
    case WM_DPICHANGED:
        if (pWnd) {
            pWnd->ReleaseFonts();
            pWnd->dpi_ = HIWORD(wParam);
            pWnd->RecalcLayout();
            if (pWnd->IsVisible()) pWnd->Show();
        }
        return 0;
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

    DWORD light = 1, size = sizeof(light);
    RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        L"AppsUseLightTheme", RRF_RT_REG_DWORD, nullptr, &light, &size);
    HIGHCONTRASTW hc{sizeof(hc)};
    bool contrast = SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(hc), &hc, 0) &&
        (hc.dwFlags & HCF_HIGHCONTRASTON);
    COLORREF surface = contrast ? GetSysColor(COLOR_WINDOW) : light ? RGB(249,249,249) : RGB(44,44,44);
    COLORREF ink = contrast ? GetSysColor(COLOR_WINDOWTEXT) : light ? RGB(28,28,28) : RGB(248,248,248);
    COLORREF muted = contrast ? ink : light ? RGB(91,91,91) : RGB(205,205,205);
    COLORREF selected = contrast ? GetSysColor(COLOR_HIGHLIGHT) : light ? RGB(233,233,233) : RGB(62,62,62);
    HBRUSH hBg = CreateSolidBrush(surface);
    FillRect(hdcMem, &rc, hBg);
    DeleteObject(hBg);
    HBRUSH edge = CreateSolidBrush(contrast ? ink : light ? RGB(210,210,210) : RGB(74,74,74));
    FrameRect(hdcMem, &rc, edge);
    DeleteObject(edge);

    EnsureFonts();

    SetBkMode(hdcMem, TRANSPARENT);

    for (size_t i = 0; i < itemRects_.size(); i++) {
        auto& ir = itemRects_[i];
        int num = (int)(i + 1);
        int idx = ir.index;

        // Highlight first item
        if (i == 0) {
            HBRUSH hHL = CreateSolidBrush(selected);
            HGDIOBJ prevBrush = SelectObject(hdcMem, hHL);
            HGDIOBJ prevPen = SelectObject(hdcMem, GetStockObject(NULL_PEN));
            RoundRect(hdcMem, ir.rc.left, ir.rc.top, ir.rc.right, ir.rc.bottom, Px(6), Px(6));
            SelectObject(hdcMem, prevPen);
            SelectObject(hdcMem, prevBrush);
            DeleteObject(hHL);
            RECT marker{ir.rc.left, ir.rc.top + Px(6), ir.rc.left + Px(3), ir.rc.bottom - Px(6)};
            FillRect(hdcMem, &marker, GetSysColorBrush(contrast ? COLOR_HIGHLIGHTTEXT : COLOR_HIGHLIGHT));
        }

        // Number label
        wchar_t numStr[4];
        swprintf_s(numStr, L"%d", num);
        HFONT hPrev = (HFONT)SelectObject(hdcMem, numberFont_);
        SetTextColor(hdcMem, i == 0 && contrast ? GetSysColor(COLOR_HIGHLIGHTTEXT) : muted);
        SIZE szNum;
        GetTextExtentPoint32W(hdcMem, numStr, (int)wcslen(numStr), &szNum);
        RECT rcNum = ir.rc;
        rcNum.left += Px(6);
        rcNum.right = rcNum.left + szNum.cx + 2;
        DrawTextW(hdcMem, numStr, -1, &rcNum, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        // Candidate character
        SelectObject(hdcMem, candidateFont_);
        SetTextColor(hdcMem, i == 0 && contrast ? GetSysColor(COLOR_HIGHLIGHTTEXT) : ink);
        RECT rcText = ir.rc;
        rcText.left = rcNum.right + Px(ITEM_INNER_PAD);
        DrawTextW(hdcMem, candidates_[idx].c_str(), (int)candidates_[idx].size(),
            &rcText, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        SelectObject(hdcMem, hPrev);
    }

    // Page nav arrows
    if (totalPages_ > 1) {
        HFONT hPrev = (HFONT)SelectObject(hdcMem, numberFont_);
        SetTextColor(hdcMem, currentPage_ == 0 ? GetSysColor(COLOR_GRAYTEXT) : ink);
        DrawTextW(hdcMem, L"\x2039", 1, &rcPrev_, DT_CENTER | DT_VCENTER | DT_SINGLELINE); // ◀
        SetTextColor(hdcMem, currentPage_ + 1 == totalPages_ ? GetSysColor(COLOR_GRAYTEXT) : ink);
        DrawTextW(hdcMem, L"\x203A", 1, &rcNext_, DT_CENTER | DT_VCENTER | DT_SINGLELINE); // ▶
        SelectObject(hdcMem, hPrev);
    }


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
            if (service_) service_->CommitCandidate(ir.index);
            return;
        }
    }
    if (totalPages_ > 1) {
        POINT pt = { x, y };
        if (PtInRect(&rcPrev_, pt)) { PrevPage(); Show(); return; }
        if (PtInRect(&rcNext_, pt)) { NextPage(); Show(); return; }
    }
}

// A collapsed TSF range may legitimately have zero width. A zero-height
// rectangle, including the documented S_OK/{0,0,0,0} result, has no layout.
static bool IsUsableCaret(const RECT& caret, const RECT* document) {
    if (caret.bottom <= caret.top || caret.right < caret.left) return false;
    if (document && (caret.left < document->left || caret.left > document->right ||
        caret.bottom <= document->top || caret.top >= document->bottom)) return false;
    return MonitorFromPoint({caret.left, caret.bottom - 1}, MONITOR_DEFAULTTONULL) != nullptr;
}

void CandidateWindow::PresentAtCaret(const RECT* textCaret, const RECT* document, HWND viewWindow) {
    RECT caret{};
    if (textCaret && IsUsableCaret(*textCaret, document)) {
        caret = *textCaret;
    } else if (!hasAnchor_) {
        // Custom editors need not create a Win32 caret. Never substitute
        // GetCaretPos (thread-relative), the mouse, or the popup's initial (0,0).
        GUITHREADINFO gui{sizeof(gui)};
        if (!GetGUIThreadInfo(GetCurrentThreadId(), &gui) || !gui.hwndCaret ||
            !gui.hwndFocus || GetAncestor(gui.hwndFocus, GA_ROOT) != GetAncestor(gui.hwndCaret, GA_ROOT) ||
            (viewWindow && GetAncestor(viewWindow, GA_ROOT) != GetAncestor(gui.hwndCaret, GA_ROOT))) return;
        POINT origin{};
        if (!ClientToScreen(gui.hwndCaret, &origin)) return;
        caret = gui.rcCaret;
        OffsetRect(&caret, origin.x, origin.y);
        if (!IsUsableCaret(caret, document)) return;
    }
    if (caret.bottom > caret.top) {
        caretTop_ = caret.top;
        anchor_ = {caret.left, caret.bottom};
        hasAnchor_ = true;
    }
    Show();
}
