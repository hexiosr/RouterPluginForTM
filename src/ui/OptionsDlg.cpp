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
    IDC_BTN_OK,
    IDC_BTN_CANCEL,
    IDC_BTN_APPLY,
};

struct DlgState {
    bool result = false;
    bool closed = false;

    HWND hCheckUnit = nullptr;
    HWND hComboUnit = nullptr;
    HWND hComboDecimals = nullptr;
    HWND hComboWidth = nullptr;
};

void AddCtrlImpl(HWND hWnd, HINSTANCE hInst, HFONT font, LPCWSTR cls, LPCWSTR text,
                 DWORD style, int x, int y, int w, int h, int id, HWND* out) {
    HWND hw = CreateWindowExW(0, cls, text, WS_CHILD | WS_VISIBLE | style,
                              x, y, w, h, hWnd, (HMENU)(intptr_t)id, hInst, NULL);
    if (hw && font) SendMessageW(hw, WM_SETFONT, (WPARAM)font, TRUE);
    if (out) *out = hw;
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

    add(L"BUTTON", L"显示", BS_GROUPBOX, 15, 15, 490, 104, 0);
    add(L"BUTTON", L"显示单位", BS_AUTOCHECKBOX, 35, 35, 110, 26,
        IDC_CHECK_UNIT, &st->hCheckUnit);
    add(L"STATIC", L"单位:", 0, 165, 36, 40, 24, 0);
    add(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_BORDER,
        210, 31, 100, 200, IDC_COMBO_UNIT, &st->hComboUnit);
    add(L"STATIC", L"小数位:", 0, 335, 36, 64, 24, 0);
    add(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_BORDER,
        405, 31, 100, 200, IDC_COMBO_DECIMALS, &st->hComboDecimals);
    add(L"STATIC", L"宽度预留:", 0, 35, 70, 80, 24, 0);
    add(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_BORDER,
        120, 65, 180, 200, IDC_COMBO_WIDTH, &st->hComboWidth);

    add(L"BUTTON", L"确定", BS_DEFPUSHBUTTON, 100, 135, 100, 34, IDC_BTN_OK);
    add(L"BUTTON", L"应用", BS_PUSHBUTTON, 210, 135, 100, 34, IDC_BTN_APPLY);
    add(L"BUTTON", L"取消", BS_PUSHBUTTON, 320, 135, 100, 34, IDC_BTN_CANCEL);

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
    ConfigManager::Instance().Save(cfg);
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
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
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

    // 客户区 520x180；按显示器工作区校正位置，避免弹到屏幕外
    RECT rcWnd = { 0, 0, 520, 180 };
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
