// RouterTotalSpeedPlugin.cpp - 插件主类实现（只显示全网总速率）
#include "pch.h"
#include "RouterTotalSpeedPlugin.h"
#include "OptionsDlg.h"
#include "UpnpWan.h"

// ═══════════════════════════════════════════════
// DLL 导出入口
// ═══════════════════════════════════════════════
static CRouterTotalSpeedPlugin* g_pluginInstance = nullptr;

namespace {
std::wstring FormatRate(double kbPerSec, bool showUnit, int unitMode, int decimals) {
    if (!(kbPerSec > 0)) kbPerSec = 0;
    bool useMbps = unitMode == 1;
    double v = useMbps ? kbPerSec * 8.0 / 1024.0 : kbPerSec / 1024.0;
    std::wstring s = TextFmt::Fixed(v, decimals);
    if (showUnit) s += useMbps ? L"Mbps" : L"MB/s";
    return s;
}

// widthReserve：0 = 自动（按当前值），N = 固定预留 N 位整数。
// 实际值超出预留宽度时按实际宽度放宽，不会被裁
std::wstring SampleRate(bool showUnit, int unitMode, int decimals, int widthReserve) {
    if (widthReserve <= 0) return {};
    std::wstring s((size_t)widthReserve, L'9');
    if (decimals > 0) s += L"." + std::wstring((size_t)decimals, L'9');
    if (showUnit) s += unitMode == 1 ? L"Mbps" : L"MB/s";
    return s;
}

std::wstring SampleValue(bool up, bool showUnit, int unitMode, int decimals, int widthReserve) {
    return (up ? L"↑: " : L"↓: ") + SampleRate(showUnit, unitMode, decimals, widthReserve);
}

int TextWidth(HDC dc, const std::wstring& s) {
    SIZE sz{};
    GetTextExtentPoint32W(dc, s.c_str(), (int)s.size(), &sz);
    return sz.cx;
}

int RawFontPixel(HDC dc) {
    HFONT cur = (HFONT)GetCurrentObject(dc, OBJ_FONT);
    LOGFONTW lf{};
    if (cur && GetObjectW(cur, sizeof(lf), &lf) == sizeof(lf) && lf.lfHeight != 0)
        return lf.lfHeight < 0 ? -lf.lfHeight : lf.lfHeight;
    TEXTMETRICW tm{};
    GetTextMetricsW(dc, &tm);
    return tm.tmHeight > 0 ? tm.tmHeight : 12;
}

int FontPixelFor(HDC dc, int itemHeight, int lines) {
    int px = RawFontPixel(dc);
    if (lines < 1) lines = 1;
    int cap = (itemHeight - 2) / lines;
    if (cap >= 8 && px > cap) px = cap;
    if (px < 8) px = 8;
    return px;
}

HFONT MakeFont(HDC dc, int pixel) {
    wchar_t face[64] = {};
    GetTextFaceW(dc, 64, face);
    if (face[0] == 0) lstrcpynW(face, L"微软雅黑", 64);
    return CreateFontW(-pixel, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
                       OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                       DEFAULT_PITCH | FF_DONTCARE, face);
}

int GapFor(HDC dc) {
    TEXTMETRICW tm{};
    GetTextMetricsW(dc, &tm);
    int gap = tm.tmHeight / 2;
    return gap < 6 ? 6 : gap;
}

int CellWidth(HDC dc, const SlotCell& c, bool showUnit, int unitMode, int decimals,
              int widthReserve) {
    int sampleW = 0;
    if (widthReserve > 0) {
        int upW = TextWidth(dc, SampleValue(true, showUnit, unitMode, decimals, widthReserve));
        int downW = TextWidth(dc, SampleValue(false, showUnit, unitMode, decimals, widthReserve));
        sampleW = upW > downW ? upW : downW;
    }

    int actualUp = TextWidth(dc, L"↑: " + c.up);
    int actualDown = TextWidth(dc, L"↓: " + c.down);
    int actualW = actualUp > actualDown ? actualUp : actualDown;

    return (sampleW > actualW ? sampleW : actualW) + 6;
}

int MeasureCells(HDC dc, const std::vector<SlotCell>& cells, const std::wstring& status,
                 bool showUnit, int unitMode, int decimals, int widthReserve) {
    if (!status.empty()) return TextWidth(dc, L"未发现路由器") + 8;
    int total = 0;
    int gap = GapFor(dc);
    for (size_t i = 0; i < cells.size(); ++i) {
        if (i > 0) total += gap;
        total += CellWidth(dc, cells[i], showUnit, unitMode, decimals, widthReserve);
    }
    return total;
}

enum class CellRows { Both, Up, Down };

void DrawCells(HDC dc, int x, int y, int w, int h, bool dark,
               const std::vector<SlotCell>& cells, const std::wstring& status, CellRows rows,
               bool showUnit, int unitMode, int decimals, int widthReserve) {
    if (w <= 0 || h <= 0) return;
    int oldBk = SetBkMode(dc, TRANSPARENT);
    COLORREF oldColor = SetTextColor(dc, dark ? RGB(245, 245, 245) : RGB(20, 20, 20));

    int px = FontPixelFor(dc, h, rows == CellRows::Both ? 2 : 1);
    HFONT font = MakeFont(dc, px);
    HGDIOBJ oldFont = font ? SelectObject(dc, font) : nullptr;

    if (!status.empty()) {
        RECT rc = { x, y, x + w, y + h };
        DrawTextW(dc, status.c_str(), -1, &rc,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    } else {
        int saved = SaveDC(dc);
        IntersectClipRect(dc, x, y, x + w, y + h);
        int gap = GapFor(dc);
        int rowH = (rows == CellRows::Both) ? h / 2 : h;
        int cx = x;
        for (size_t i = 0; i < cells.size(); ++i) {
            if (i > 0) cx += gap;
            const SlotCell& c = cells[i];
            int cw = CellWidth(dc, c, showUnit, unitMode, decimals, widthReserve);

            int vx = cx;
            if (rows != CellRows::Down) {
                std::wstring up = L"↑: " + c.up;
                RECT r1 = { vx, y, cx + cw, y + rowH };
                DrawTextW(dc, up.c_str(), -1, &r1,
                          DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            }
            if (rows != CellRows::Up) {
                std::wstring down = L"↓: " + c.down;
                RECT r2 = { vx, (rows == CellRows::Both) ? y + rowH : y, cx + cw, y + h };
                DrawTextW(dc, down.c_str(), -1, &r2,
                          DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            }
            cx += cw;
        }
        RestoreDC(dc, saved);
    }

    if (oldFont) SelectObject(dc, oldFont);
    if (font) DeleteObject(font);
    SetTextColor(dc, oldColor);
    SetBkMode(dc, oldBk);
}

} // namespace

extern "C" __declspec(dllexport)
ITMPlugin* TMPluginGetInstance() {
    if (!g_pluginInstance) {
        g_pluginInstance = new CRouterTotalSpeedPlugin();
    }
    return g_pluginInstance;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID) {
    switch (reason) {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        SetPluginModule(hModule);
        break;
    case DLL_PROCESS_DETACH:
        // 不要在这里 join 线程：loader lock 下会死锁
        if (g_pluginInstance) {
            g_pluginInstance->Shutdown();
            g_pluginInstance = nullptr;
        }
        break;
    }
    return TRUE;
}

// ═══════════════════════════════════════════════
// CSpeedItem
// ═══════════════════════════════════════════════
const wchar_t* CSpeedItem::GetItemName() const {
    return m_index == 0 ? L"全网速率 ↑" : L"全网速率 ↓";
}

const wchar_t* CSpeedItem::GetItemId() const {
    // 沿用旧 ID，已添加到任务栏的项目不会失效
    return m_index == 0 ? L"device_up" : L"device_down";
}

const wchar_t* CSpeedItem::GetItemValueText() const {
    std::vector<SlotCell> cells;
    std::wstring status;
    if (m_plugin) m_plugin->GetDisplayCells(m_index, cells, status);
    if (!status.empty()) {
        m_valueText = status;
    } else {
        m_valueText.clear();
        for (const auto& c : cells) {
            if (!m_valueText.empty()) m_valueText += L"  ";
            m_valueText += (m_index == 0 ? L"↑: " : L"↓: ") + (m_index == 0 ? c.up : c.down);
        }
    }
    return m_valueText.c_str();
}

int CSpeedItem::GetItemWidth() const {
    HDC dc = CreateCompatibleDC(nullptr);
    if (!dc) return 0;
    HGDIOBJ old = SelectObject(dc, GetStockObject(DEFAULT_GUI_FONT));
    std::vector<SlotCell> cells;
    std::wstring status;
    if (m_plugin) m_plugin->GetDisplayCells(m_index, cells, status);
    DisplayConfig cfg = ConfigManager::Instance().Get();
    int w = MeasureCells(dc, cells, status, cfg.showUnit, cfg.unitMode, cfg.decimals, cfg.widthReserve);
    SelectObject(dc, old);
    DeleteDC(dc);
    return w;
}

int CSpeedItem::GetItemWidthEx(void* hDC) const {
    HDC dc = (HDC)hDC;
    if (!dc) return GetItemWidth();
    std::vector<SlotCell> cells;
    std::wstring status;
    if (m_plugin) m_plugin->GetDisplayCells(m_index, cells, status);
    DisplayConfig cfg = ConfigManager::Instance().Get();
    return MeasureCells(dc, cells, status, cfg.showUnit, cfg.unitMode, cfg.decimals, cfg.widthReserve);
}

void CSpeedItem::DrawItem(void* hDC, int x, int y, int w, int h, bool dark_mode) {
    HDC dc = (HDC)hDC;
    if (!dc || !m_plugin) return;
    std::vector<SlotCell> cells;
    std::wstring status;
    m_plugin->GetDisplayCells(m_index, cells, status);
    DisplayConfig cfg = ConfigManager::Instance().Get();

    // 双行格子需要整窗高度；TM 双行排列中单项只有半行高时，只画本行内容
    bool full = h >= RawFontPixel(dc) * 2 + 4;
    CellRows rows = CellRows::Both;
    if (!full) rows = (y > 0) ? CellRows::Down : CellRows::Up;
    DrawCells(dc, x, y, w, h, dark_mode, cells, status, rows,
              cfg.showUnit, cfg.unitMode, cfg.decimals, cfg.widthReserve);
}

// ═══════════════════════════════════════════════
// CRouterTotalSpeedPlugin
// ═══════════════════════════════════════════════
CRouterTotalSpeedPlugin::CRouterTotalSpeedPlugin() {}

CRouterTotalSpeedPlugin::~CRouterTotalSpeedPlugin() {
    m_stop = true;
    if (m_thread.joinable()) m_thread.join();
}

void CRouterTotalSpeedPlugin::Shutdown() {
    m_stop = true;
}

void CRouterTotalSpeedPlugin::StartWorker() {
    if (!m_thread.joinable())
        m_thread = std::thread(&CRouterTotalSpeedPlugin::WanLoop, this);
}

void CRouterTotalSpeedPlugin::StartOnce(const std::wstring& configDir) {
    if (m_initialized.exchange(true)) return;
    if (!configDir.empty()) {
        ConfigManager::Instance().SetConfigDir(configDir);
    } else {
        wchar_t buf[MAX_PATH] = {};
        GetCurrentDirectoryW(MAX_PATH, buf);
        ConfigManager::Instance().SetConfigDir(buf);
    }
    ConfigManager::Instance().Load();
    StartWorker();
}

void CRouterTotalSpeedPlugin::OnExtenedInfo(ExtendedInfoIndex index, const wchar_t* data) {
    if (index == EI_CONFIG_DIR && data && data[0] != L'\0')
        StartOnce(data);
}

void CRouterTotalSpeedPlugin::DataRequired() {
    if (!m_initialized.load())
        StartOnce({});
}

void CRouterTotalSpeedPlugin::WanLoop() {
    UpnpWan wan;
    int fails = 0;

    while (!m_stop.load()) {
        double up = 0, down = 0;
        bool ok = wan.Sample(up, down);
        if (ok) {
            m_upKBps.store(up);
            m_downKBps.store(down);
            m_updatedAt.store((long long)time(nullptr));
            m_state.store(1);
            fails = 0;
        } else if (++fails >= 2) {
            m_state.store(2);
        }

        int waitMs = wan.Ready() ? 500 : 3000;
        for (int t = 0; t < waitMs && !m_stop.load(); t += 100) Sleep(100);
    }
}

void CRouterTotalSpeedPlugin::GetDisplayCells(int, std::vector<SlotCell>& cells,
                                              std::wstring& status) const {
    cells.clear();
    status.clear();

    int st = m_state.load();
    if (st == 0) {
        status = L"查找路由器";
        return;
    }
    if (st == 2) {
        status = L"未发现路由器";
        return;
    }

    DisplayConfig cfg = ConfigManager::Instance().Get();
    cells.push_back({ FormatRate(m_upKBps.load(), cfg.showUnit, cfg.unitMode, cfg.decimals),
                      FormatRate(m_downKBps.load(), cfg.showUnit, cfg.unitMode, cfg.decimals) });
}

std::wstring CRouterTotalSpeedPlugin::GetTooltipSnapshot() const {
    DisplayConfig cfg = ConfigManager::Instance().Get();
    int st = m_state.load();
    long long upd = m_updatedAt.load();

    std::wstring text = L"【路由器全网速率】";
    text += L"\n状态：";
    if (st == 1) text += L"已连接";
    else if (st == 0) text += L"查找路由器…";
    else text += L"未发现支持 UPnP 的路由器";

    if (upd > 0) {
        time_t t = (time_t)upd;
        std::tm tmv{};
        localtime_s(&tmv, &t);
        wchar_t timeBuf[32] = {};
        wcsftime(timeBuf, 32, L"%H:%M:%S", &tmv);
        text += L"\n更新：";
        text += timeBuf;
        long age = (long)(time(nullptr) - t);
        if (age >= 0) {
            text += L"（";
            text += std::to_wstring(age);
            text += L" 秒前）";
        }
    }

    if (st == 1) {
        text += L"\n上行：";
        text += FormatRate(m_upKBps.load(), true, cfg.unitMode, cfg.decimals);
        text += L"\n下行：";
        text += FormatRate(m_downKBps.load(), true, cfg.unitMode, cfg.decimals);
    } else if (st == 2) {
        text += L"\n请确认路由器已开启 UPnP";
    }

    m_tooltipText = text;
    return m_tooltipText;
}

// ─── ITMPlugin 接口 ───
IPluginItem* CRouterTotalSpeedPlugin::GetItem(int index) {
    if (index >= 0 && index < 2) return &m_items[index];
    return nullptr;
}

const wchar_t* CRouterTotalSpeedPlugin::GetInfo(PluginInfoIndex index) {
    switch (index) {
    case TMI_NAME:        return L"路由器全网速率";
    case TMI_DESCRIPTION: return L"通过路由器 UPnP 计数器显示全网上下行总速率";
    case TMI_AUTHOR:      return L"ziphooo";
    case TMI_COPYRIGHT:   return L"2026";
    case TMI_URL:         return L"";
    case TMI_VERSION:     return L"0.1.0";
    default:              return L"";
    }
}

void* CRouterTotalSpeedPlugin::GetPluginIcon() {
    static HICON icon = LoadPluginIcon(GetSystemMetrics(SM_CXSMICON),
                                       GetSystemMetrics(SM_CYSMICON));
    return icon;
}

ITMPlugin::OptionReturn CRouterTotalSpeedPlugin::ShowOptionsDialog(void* hParent) {
    return COptionsDlg::Show((HWND)hParent) ? OR_OPTION_CHANGED : OR_OPTION_UNCHANGED;
}

const wchar_t* CRouterTotalSpeedPlugin::GetTooltipInfo() {
    GetTooltipSnapshot();
    return m_tooltipText.c_str();
}
