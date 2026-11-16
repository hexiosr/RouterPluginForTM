// RouterTotalSpeedPlugin.h - 插件主类与显示项（全网总速率 + 今日流量）
#pragma once
#include "pch.h"
#include "DisplayConfig.h"
#include "TrafficStats.h"

class CRouterTotalSpeedPlugin;

// 显示项：0 = 上行速率，1 = 下行速率，2 = 今日上传，3 = 今日下载。
// 全部交给 TrafficMonitor 绘制，标签文字可在「显示文本设置」里自定义
class CSpeedItem : public IPluginItem {
public:
    CSpeedItem(CRouterTotalSpeedPlugin* plugin, int index) : m_plugin(plugin), m_index(index) {}

    const wchar_t* GetItemName() const override;
    const wchar_t* GetItemId() const override;
    const wchar_t* GetItemLableText() const override;
    const wchar_t* GetItemValueText() const override;
    const wchar_t* GetItemValueSampleText() const override;

private:
    CRouterTotalSpeedPlugin* m_plugin;
    int m_index;
    mutable std::wstring m_valueText;
};

class CRouterTotalSpeedPlugin : public ITMPlugin {
public:
    CRouterTotalSpeedPlugin();
    ~CRouterTotalSpeedPlugin();

    IPluginItem* GetItem(int index) override;
    void DataRequired() override;
    OptionReturn ShowOptionsDialog(void* hParent) override;
    const wchar_t* GetInfo(PluginInfoIndex index) override;
    void OnExtenedInfo(ExtendedInfoIndex index, const wchar_t* data) override;
    void* GetPluginIcon() override;
    const wchar_t* GetTooltipInfo() override;

    // DllMain(DLL_PROCESS_DETACH) 调用：只置停止标志，不 join
    void Shutdown();

    std::wstring GetRateText(bool up) const;
    std::wstring GetTodayText(bool up) const;
    std::wstring GetTooltipSnapshot() const;

private:
    void StartOnce(const std::wstring& configDir);
    void StartWorker();
    void WanLoop();

    std::thread m_thread;
    std::atomic<bool> m_stop{false};
    std::atomic<bool> m_initialized{false};
    std::atomic<int> m_state{0};              // 0 查找中，1 正常，2 未发现
    std::atomic<double> m_upKBps{0};
    std::atomic<double> m_downKBps{0};
    std::atomic<long long> m_updatedAt{0};

    TrafficStats m_stats;
    CSpeedItem m_items[4] = { {this, 0}, {this, 1}, {this, 2}, {this, 3} };
    mutable std::wstring m_tooltipText;
};