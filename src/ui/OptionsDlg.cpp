// OptionsDlg.cpp - 设置对话框（纯 Win32，无 RC 对话框模板）
#include "pch.h"
#include "OptionsDlg.h"
#include "DisplayConfig.h"

namespace {

enum {
    IDC_CHECK_UNIT = 1001,
    IDC_COMBO_UNIT,
    IDC_COMBO_DECIMALS,
    IDC_COMBO_WIDTH,
    IDC_EDIT_SAMPLE,
    IDC_BTN_OK,
    IDC_BTN_APPLY,
    IDC_BTN_CANCEL,
};

struct DlgState {
    bool result = false;
    bool closed = false;

    HWND hCheckUnit = nullptr;
    HWND hComboUnit = nullptr;
    HWND hComboDecimals = nullptr;
    HWND hComboWidth = nullptr;
    HWND hEditSample = nullptr;
    HWND hLblUnit = nullptr;
    HWND hLblDecimals = nullptr;
    HWND hLblWidth = nullptr;
    HWND hLblSample = nullptr;
    HWND hLblMs = nullptr;
};

void AlignVertically(HWND label, HWND ctl) {
    if (!label || !ctl) return;
    HWND parent = GetParent(ctl);
    RECT r{}, lr{};
    if (!GetWindowRect(ctl, &r) || !GetWindowRect(label, &lr)) return;
    MapWindowPoints(HWND_DESKTOP, parent, reinterpret_cast<LPPOINT>(&r), 2);
    MapWindowPoints(HWND_DESKTOP, parent, reinterpret_cast<LPPOINT>(&lr), 2);
    SetWindowPos(label, nullptr, lr.left, r.top, lr.right - lr.left, r.bottom - r.top,
                 SWP_NOZORDER | SWP_NOACTIVATE);
}

void AddCtrlImpl(HWND hWnd, HINSTANCE hInst, HFONT font, LPCWSTR cls, LPCWSTR text,
                 DWORD style, int x, int y, int w, int h, int id, HWND* out) {
    HWND hw = CreateWindowExW(0, cls, text, WS_CHILD | WS_VISIBLE | style,
                              x, y, w, h, hWnd, (HMENU)(intptr_t)id, hInst, NULL);
    if (hw && font) SendMessageW(hw, WM_SETFONT, (WPARAM)font, TRUE);
    if (out) *out = hw;
}

// 自绘按钮的悬停状态
bool g_btnHover[3] = {};
WNDPROC g_btnOldProc = nullptr;

LRESULT CALLBACK OwnerBtnProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    int idx = GetDlgCtrlID(h) - IDC_BTN_OK;
    if (idx >= 0 && idx < 3) {
        if (msg == WM_ERASEBKGND) return 1;
        if (msg == WM_MOUSEMOVE && !g_btnHover[idx]) {
            g_btnHover[idx] = true;
            TRACKMOUSEEVENT t = {};
            t.cbSize = sizeof(t);
            t.dwFlags = TME_LEAVE;
            t.hwndTrack = h;
            TrackMouseEvent(&t);
            InvalidateRect(h, nullptr, TRUE);
        } else if (msg == WM_MOUSELEAVE && g_btnHover[idx]) {
            g_btnHover[idx] = false;
            InvalidateRect(h, nullptr, TRUE);
        }
    }
    return g_btnOldProc ? CallWindowProcW(g_btnOldProc, h, msg, wp, lp)
                        : DefWindowProcW(h, msg, wp, lp);
}

void CreateControls(HWND hWnd, DlgState* st) {
    HINSTANCE hInst = (HINSTANCE)GetWindowLongPtrW(hWnd, GWLP_HINSTANCE);

    LOGFONTW lf = {};
    lf.lfHeight = -16;
    lf.lfWeight = FW_NORMAL;
    lstrcpynW(lf.lfFaceName, L"微软雅黑", LF_FACESIZE);
    HFONT hFont = CreateFontIndirectW(&lf);
    if (!hFont) hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

    auto add = [&](LPCWSTR cls, LPCWSTR text, DWORD style,
                   int x, int y, int w, int h, int id, HWND* out = nullptr) {
        AddCtrlImpl(hWnd, hInst, hFont, cls, text, style, x, y, w, h, id, out);
    };

    add(L"BUTTON", L"显示单位", BS_AUTOCHECKBOX, 146, 46, 140, 30,
        IDC_CHECK_UNIT, &st->hCheckUnit);
    add(L"STATIC", L"单位", SS_LEFT | SS_CENTERIMAGE, 48, 104, 86, 30, 0,
        &st->hLblUnit);
    add(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_BORDER,
        146, 104, 120, 200, IDC_COMBO_UNIT, &st->hComboUnit);
    add(L"STATIC", L"小数位", SS_LEFT | SS_CENTERIMAGE, 48, 162, 86, 30, 0,
        &st->hLblDecimals);
    add(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_BORDER,
        146, 162, 120, 200, IDC_COMBO_DECIMALS, &st->hComboDecimals);
    add(L"STATIC", L"宽度预留", SS_LEFT | SS_CENTERIMAGE, 48, 220, 86, 30, 0,
        &st->hLblWidth);
    add(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_BORDER,
        146, 220, 120, 200, IDC_COMBO_WIDTH, &st->hComboWidth);
    add(L"STATIC", L"采样间隔", SS_LEFT | SS_CENTERIMAGE, 48, 278, 86, 30, 0,
        &st->hLblSample);
    add(L"EDIT", L"", WS_BORDER | ES_NUMBER,
        146, 278, 120, 30, IDC_EDIT_SAMPLE, &st->hEditSample);
    add(L"STATIC", L"ms", SS_CENTERIMAGE, 272, 278, 26, 30, 0, &st->hLblMs);

    add(L"BUTTON", L"确定", BS_OWNERDRAW, 28, 346, 92, 38, IDC_BTN_OK);
    add(L"BUTTON", L"应用", BS_OWNERDRAW, 136, 346, 92, 38, IDC_BTN_APPLY);
    add(L"BUTTON", L"取消", BS_OWNERDRAW, 244, 346, 92, 38, IDC_BTN_CANCEL);
    g_btnHover[0] = g_btnHover[1] = g_btnHover[2] = false;
    g_btnOldProc = nullptr;
    for (HWND h = GetWindow(hWnd, GW_CHILD); h; h = GetWindow(h, GW_HWNDNEXT)) {
        if (GetWindowLongPtrW(h, GWLP_ID) >= IDC_BTN_OK &&
            GetWindowLongPtrW(h, GWLP_ID) <= IDC_BTN_CANCEL) {
            g_btnOldProc = (WNDPROC)SetWindowLongPtrW(h, GWLP_WNDPROC, (LONG_PTR)OwnerBtnProc);
        }
    }

    DisplayConfig cfg = ConfigManager::Instance().Get();
    SendMessageW(st->hCheckUnit, BM_SETCHECK, cfg.showUnit ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(st->hComboUnit, CB_ADDSTRING, 0, (LPARAM)L"MB/s");
    SendMessageW(st->hComboUnit, CB_ADDSTRING, 0, (LPARAM)L"Mbps");
    SendMessageW(st->hComboUnit, CB_SETCURSEL, cfg.unitMode == 1 ? 1 : 0, 0);
    SendMessageW(st->hComboDecimals, CB_ADDSTRING, 0, (LPARAM)L"不显示小数");
    SendMessageW(st->hComboDecimals, CB_ADDSTRING, 0, (LPARAM)L"1 位小数");
    SendMessageW(st->hComboDecimals, CB_ADDSTRING, 0, (LPARAM)L"2 位小数");
    SendMessageW(st->hComboDecimals, CB_SETCURSEL, cfg.decimals, 0);
    SendMessageW(st->hComboWidth, CB_ADDSTRING, 0, (LPARAM)L"自动");
    SendMessageW(st->hComboWidth, CB_ADDSTRING, 0, (LPARAM)L"预留 1 位");
    SendMessageW(st->hComboWidth, CB_ADDSTRING, 0, (LPARAM)L"预留 2 位");
    SendMessageW(st->hComboWidth, CB_ADDSTRING, 0, (LPARAM)L"预留 3 位");
    SendMessageW(st->hComboWidth, CB_ADDSTRING, 0, (LPARAM)L"预留 4 位");
    SendMessageW(st->hComboWidth, CB_SETCURSEL, cfg.widthReserve, 0);
    SendMessageW(st->hEditSample, EM_SETLIMITTEXT, 4, 0);
    wchar_t msBuf[16] = {};
    swprintf_s(msBuf, L"%d", cfg.sampleMs);
    SetWindowTextW(st->hEditSample, msBuf);

    // 静态文字按控件实际闭合高度对齐，避免标签比框内文字高/低几像素
    AlignVertically(st->hLblDecimals, st->hComboDecimals);
    AlignVertically(st->hLblUnit, st->hComboUnit);
    AlignVertically(st->hLblWidth, st->hComboWidth);
    AlignVertically(st->hLblSample, st->hEditSample);
    AlignVertically(st->hLblMs, st->hEditSample);

    SetFocus(hWnd);
}

void SaveFromDialog(DlgState* st) {
    DisplayConfig cfg;
    cfg.showUnit = (SendMessageW(st->hCheckUnit, BM_GETCHECK, 0, 0) == BST_CHECKED);
    LRESULT unitSel = SendMessageW(st->hComboUnit, CB_GETCURSEL, 0, 0);
    cfg.unitMode = (unitSel == 1) ? 1 : 0;
    LRESULT decSel = SendMessageW(st->hComboDecimals, CB_GETCURSEL, 0, 0);
    cfg.decimals = (decSel >= 0 && decSel <= 2) ? (int)decSel : 2;
    LRESULT widthSel = SendMessageW(st->hComboWidth, CB_GETCURSEL, 0, 0);
    cfg.widthReserve = (widthSel >= 0 && widthSel <= 4) ? (int)widthSel : 0;
    wchar_t msBuf[16] = {};
    GetWindowTextW(st->hEditSample, msBuf, 16);
    int ms = msBuf[0] ? (int)wcstol(msBuf, nullptr, 10) : 1000;
    if (ms < 200) ms = 200;
    if (ms > 5000) ms = 5000;
    cfg.sampleMs = ms;
    ConfigManager::Instance().Save(cfg);
    swprintf_s(msBuf, L"%d", ms);
    SetWindowTextW(st->hEditSample, msBuf);
}

void DrawStyledButton(const DRAWITEMSTRUCT* d) {
    int idx = d->CtlID - IDC_BTN_OK;
    if (idx < 0 || idx > 2) return;
    bool primary = (idx == 0);
    bool pressed = (d->itemState & ODS_SELECTED) != 0;
    bool hover = g_btnHover[idx];

    COLORREF bg;
    if (primary)   bg = pressed ? RGB(0, 84, 189) : hover ? RGB(0, 105, 227) : RGB(0, 122, 255);
    else           bg = pressed ? RGB(209, 209, 214) : hover ? RGB(222, 222, 226) : RGB(233, 233, 235);

    RECT rc = d->rcItem;
    if (rc.right - rc.left < 2) return;

    typedef UINT(WINAPI * PFN_GetDpiForWindow)(HWND);
    static PFN_GetDpiForWindow pGetDpi = (PFN_GetDpiForWindow)(void*)GetProcAddress(
        GetModuleHandleW(L"user32.dll"), "GetDpiForWindow");
    UINT dpi = pGetDpi ? pGetDpi(d->hwndItem) : 96;
    int radius = MulDiv(8, dpi, 96);

    HRGN rgn = CreateRoundRectRgn(rc.left, rc.top, rc.right + 1, rc.bottom + 1,
                                  radius, radius);
    HBRUSH br = CreateSolidBrush(bg);
    FillRgn(d->hDC, rgn, br);
    DeleteObject(br);
    DeleteObject(rgn);

    HFONT f = (HFONT)SendMessageW(d->hwndItem, WM_GETFONT, 0, 0);
    HFONT old = f ? (HFONT)SelectObject(d->hDC, f) : nullptr;
    SetBkMode(d->hDC, TRANSPARENT);
    SetTextColor(d->hDC, primary ? RGB(255, 255, 255) : RGB(29, 29, 31));
    wchar_t txt[32] = {};
    GetWindowTextW(d->hwndItem, txt, 32);
    RECT trc = rc;
    DrawTextW(d->hDC, txt, -1, &trc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    if (old) SelectObject(d->hDC, old);
}

LRESULT CALLBACK DlgWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    DlgState* st = reinterpret_cast<DlgState*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));

    switch (msg) {
    case WM_CREATE: {
        CREATESTRUCTW* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        st = reinterpret_cast<DlgState*>(cs->lpCreateParams);
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, (LONG_PTR)st);
        CreateControls(hWnd, st);
        return 0;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        // 按钮区上方一条发丝线，划分表单与操作区
        HPEN pen = CreatePen(PS_SOLID, 1, RGB(229, 229, 234));
        HPEN old = (HPEN)SelectObject(hdc, pen);
        MoveToEx(hdc, 48, 330, nullptr);
        LineTo(hdc, 298, 330);
        SelectObject(hdc, old);
        DeleteObject(pen);
        EndPaint(hWnd, &ps);
        return 0;
    }
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX: {
        HDC hdc = (HDC)wParam;
        SetBkColor(hdc, RGB(255, 255, 255));
        SetTextColor(hdc, RGB(29, 29, 31));
        return (LRESULT)GetStockObject(WHITE_BRUSH);
    }
    case WM_DRAWITEM: {
        DRAWITEMSTRUCT* d = (DRAWITEMSTRUCT*)lParam;
        if (d->CtlType == ODT_BUTTON) {
            DrawStyledButton(d);
            return TRUE;
        }
        break;
    }
    case DM_GETDEFID:
        // 自绘后没有 BS_DEFPUSHBUTTON，回车仍触发确定
        return MAKELRESULT(IDC_BTN_OK, DC_HASDEFID);
    case WM_COMMAND: {
        if (!st) break;
        int id = LOWORD(wParam);
        if (id == IDC_BTN_OK) {
            SaveFromDialog(st);
            st->result = true;
            st->closed = true;
        } else if (id == IDC_BTN_CANCEL) {
            st->result = false;
            st->closed = true;
        } else if (id == IDC_BTN_APPLY) {
            SaveFromDialog(st);
        }
        break;
    }
    case WM_CLOSE:
        if (st) { st->result = false; st->closed = true; }
        return 0;
    case WM_DESTROY:
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

ATOM RegisterDlgClass(HINSTANCE hInst) {
    static ATOM atom = 0;
    if (atom == 0) {
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = DlgWndProc;
        wc.hInstance = hInst;
        wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
        wc.lpszClassName = L"RouterTotalOptDlg";
        wc.hIcon = LoadPluginIcon(GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON));
        wc.hIconSm = LoadPluginIcon(GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON));
        atom = RegisterClassExW(&wc);
    }
    return atom;
}

} // namespace

bool COptionsDlg::Show(HWND hParent) {
    HINSTANCE hInst = hParent
        ? (HINSTANCE)GetWindowLongPtrW(hParent, GWLP_HINSTANCE)
        : GetModuleHandleW(NULL);

    RegisterDlgClass(hInst);

    DlgState state;

    // 客户区 346x408；按显示器工作区校正位置，避免弹到屏幕外
    RECT rcWnd = { 0, 0, 346, 408 };
    AdjustWindowRectEx(&rcWnd, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, FALSE,
                       WS_EX_DLGMODALFRAME);
    int W = rcWnd.right - rcWnd.left;
    int H = rcWnd.bottom - rcWnd.top;

    RECT wa{};
    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    HMONITOR mon = MonitorFromWindow(hParent ? hParent : GetForegroundWindow(),
                                     MONITOR_DEFAULTTONEAREST);
    if (mon && GetMonitorInfoW(mon, &mi))
        wa = mi.rcWork;
    else {
        wa.left = 0;
        wa.top = 0;
        wa.right = GetSystemMetrics(SM_CXSCREEN);
        wa.bottom = GetSystemMetrics(SM_CYSCREEN);
    }

    int workW = wa.right - wa.left;
    int workH = wa.bottom - wa.top;
    int px = wa.left + (workW - W) / 2;
    int py = wa.top + (workH - H) / 2;
    if (hParent) {
        RECT rc{};
        if (GetWindowRect(hParent, &rc)) {
            int cx = rc.left + (rc.right - rc.left - W) / 2;
            int cy = rc.top + (rc.bottom - rc.top - H) / 2;
            if (cx >= wa.left && cy >= wa.top && cx + W <= wa.right && cy + H <= wa.bottom) {
                px = cx;
                py = cy;
            }
        }
    }

    HWND hPrevForeground = GetForegroundWindow();

    HWND hDlg = CreateWindowExW(
        WS_EX_DLGMODALFRAME,
        L"RouterTotalOptDlg",
        L"路由器全网速率",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        px, py, W, H,
        hParent, NULL, hInst, &state);
    if (!hDlg) return false;

    if (hParent) EnableWindow(hParent, FALSE);
    ShowWindow(hDlg, SW_SHOW);
    UpdateWindow(hDlg);

    MSG msg{};
    while (!state.closed && GetMessageW(&msg, NULL, 0, 0) > 0) {
        if (!IsWindow(hDlg)) break;
        if (!IsDialogMessageW(hDlg, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    if (hParent) EnableWindow(hParent, TRUE);
    if (IsWindow(hDlg)) DestroyWindow(hDlg);

    HWND hFocus = nullptr;
    if (hParent && IsWindow(hParent) && IsWindowVisible(hParent))
        hFocus = hParent;
    else if (hPrevForeground && IsWindow(hPrevForeground) && IsWindowVisible(hPrevForeground))
        hFocus = hPrevForeground;
    if (hFocus) SetForegroundWindow(hFocus);

    return state.result;
}
