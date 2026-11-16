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

std::wstring FormatBytes(unsigned long long bytes, int decimals) {
    if (bytes >= (1ULL << 30)) return TextFmt::Fixed(bytes / 1073741824.0, decimals) + L"GB";
    if (bytes >= (1ULL << 20)) return TextFmt::Fixed(bytes / 1048576.0, decimals) + L"MB";
    if (bytes >= (1ULL << 10)) return TextFmt::Fixed(bytes / 1024.0, decimals) + L"KB";
    return std::to_wstring(bytes) + L"B";
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
    if (m_index == 2) return L"今日上传";
    if (m_index == 3) return L"今日下载";
    return m_index == 0 ? L"全网速率 ↑" : L"全网速率 ↓";
}

const wchar_t* CSpeedItem::GetItemId() const {
    // 沿用旧 ID，已添加到任务栏的项目不会失效
    if (m_index == 2) return L"today_up";
    if (m_index == 3) return L"today_down";
    return m_index == 0 ? L"device_up" : L"device_down";
}

const wchar_t* CSpeedItem::GetItemLableText() const {
    switch (m_index) {
    case 0:  return L"↑: ";
    case 1:  return L"↓: ";
    case 2:  return L"今日上传: ";
    default: return L"今日下载: ";
    }
}

const wchar_t* CSpeedItem::GetItemValueSampleText() const {
    // 非自绘项的数值列按样例宽度的最大值预留，避免数值变化时宽度抖动；
    // 样例要能覆盖实际最宽值（含 Mbps 比 MB/s 略宽的情况）
    return m_index >= 2 ? L"9999.99GB" : L"9999.99Mbps";
}

const wchar_t* CSpeedItem::GetItemValueText() const {
    if (!m_plugin) return L"";
    m_valueText = m_index >= 2 ? m_plugin->GetTodayText(m_index == 2)
                               : m_plugin->GetRateText(m_index == 0);
    return m_valueText.c_str();
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
    std::wstring dir = configDir;
    if (dir.empty()) {
        wchar_t buf[MAX_PATH] = {};
        GetCurrentDirectoryW(MAX_PATH, buf);
        dir = buf;
    }
    ConfigManager::Instance().SetConfigDir(dir);
    ConfigManager::Instance().Load();
    m_stats.Init(dir);
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
        auto cycleStart = std::chrono::steady_clock::now();
        double up = 0, down = 0;
        bool ok = wan.Sample(up, down);
        if (ok) {
            m_upKBps.store(up);
            m_downKBps.store(down);
            m_updatedAt.store((long long)time(nullptr));
            m_state.store(1);
            fails = 0;
            time_t now = time(nullptr);
            m_stats.Update(wan.LastRx(), wan.LastTx(), now);
            m_stats.FlushIfDue(now);
        } else if (++fails >= 2) {
            m_state.store(2);
        }

        // 采样间隔可配置；请求耗时从周期里扣除，保证显示按设定节奏更新
        int periodMs = wan.Ready() ? ConfigManager::Instance().Get().sampleMs : 3000;
        int elapsedMs = (int)std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - cycleStart).count();
        int waitMs = periodMs - elapsedMs;
        if (waitMs < 0) waitMs = 0;
        for (int t = 0; t < waitMs && !m_stop.load(); t += 100) Sleep(100);
    }
    m_stats.Flush();
}

std::wstring CRouterTotalSpeedPlugin::GetRateText(bool up) const {
    int st = m_state.load();
    // 状态文字要短于数值样例宽度，否则会被 TM 的数值列裁掉
    if (st == 0) return L"查找中";
    if (st == 2) return L"无路由器";

    DisplayConfig cfg = ConfigManager::Instance().Get();
    return FormatRate(up ? m_upKBps.load() : m_downKBps.load(),
                      cfg.showUnit, cfg.unitMode, cfg.decimals);
}

std::wstring CRouterTotalSpeedPlugin::GetTodayText(bool up) const {
    DisplayConfig cfg = ConfigManager::Instance().Get();
    return FormatBytes(up ? m_stats.TodayTx() : m_stats.TodayRx(), cfg.decimals);
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

    text += L"\n今日上传：";
    text += FormatBytes(m_stats.TodayTx(), cfg.decimals);
    text += L"\n今日下载：";
    text += FormatBytes(m_stats.TodayRx(), cfg.decimals);

    m_tooltipText = text;
    return m_tooltipText;
}

// ─── ITMPlugin 接口 ───
IPluginItem* CRouterTotalSpeedPlugin::GetItem(int index) {
    if (index >= 0 && index < 4) return &m_items[index];
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
