// DisplayConfig.h - 显示设置（INI）
#pragma once
#include "pch.h"

struct DisplayConfig {
    bool showUnit = true;
    int unitMode = 0;        // 0 = MB/s，1 = Mbps
    int decimals = 2;        // 0..2
    int widthReserve = 0;    // 0 = 自动（按当前值），1..4 = 固定整数位
};

class ConfigManager {
public:
    static ConfigManager& Instance();

    void SetConfigDir(const std::wstring& dir);
    std::wstring GetFilePath() const;

    void Load();
    void Save(const DisplayConfig& cfg);
    DisplayConfig Get() const;

private:
    ConfigManager() = default;
    mutable std::mutex m_mutex;
    DisplayConfig m_cfg;
    std::wstring m_dir;
};
