// RouterTotalSpeedPlugin.h - 插件主类与显示项（只显示全网总速率）
#pragma once
#include "pch.h"
#include "DisplayConfig.h"

class CRouterTotalSpeedPlugin;

// 显示单元：上下行速率文本
struct SlotCell {
    std::wstring up;
    std::wstring down;
};

// 自绘显示项：0 = 上行，1 = 下行（TM 双行排列会把它们叠成两行）
class CSpeedItem : public IPluginItem {
public:
    CSpeedItem(CRouterTotalSpeedPlugin* plugin, int index) : m_plugin(plugin), m_index(index) {}

    const wchar_t* GetItemName() const override;
    const wchar_t* GetItemId() const override;
    const wchar_t* GetItemLableText() const override { return L""; }
    const wchar_t* GetItemValueText() const override;
    const wchar_t* GetItemValueSampleText() const override { return L"↑: 999.99MB/s"; }
    bool IsCustomDraw() const override { return true; }
    int GetItemWidth() const override;
    int GetItemWidthEx(void* hDC) const override;
    void DrawItem(void* hDC, int x, int y, int w, int h, bool dark_mode) override;

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

    void GetDisplayCells(int itemIndex, std::vector<SlotCell>& cells, std::wstring& status) const;
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

    CSpeedItem m_items[2] = { {this, 0}, {this, 1} };
    mutable std::wstring m_tooltipText;
};
