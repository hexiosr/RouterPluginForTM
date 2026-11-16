// DisplayConfig.cpp - 显示设置（INI）
#include "pch.h"
#include "DisplayConfig.h"

namespace {

std::wstring ConfigFile(const std::wstring& dir) {
    if (dir.empty()) return L"RouterTotalSpeed.ini";
    return dir + L"\\RouterTotalSpeed.ini";
}

} // namespace

ConfigManager& ConfigManager::Instance() {
    static ConfigManager inst;
    return inst;
}

void ConfigManager::SetConfigDir(const std::wstring& dir) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_dir = dir;
}

std::wstring ConfigManager::GetFilePath() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return ConfigFile(m_dir);
}

void ConfigManager::Load() {
    std::wstring file = GetFilePath();

    DisplayConfig cfg;
    cfg.showUnit = GetPrivateProfileIntW(L"Display", L"ShowUnit", 1, file.c_str()) != 0;
    cfg.unitMode = (int)GetPrivateProfileIntW(L"Display", L"UnitMode", 0, file.c_str());
    if (cfg.unitMode != 1) cfg.unitMode = 0;
    cfg.decimals = (int)GetPrivateProfileIntW(L"Display", L"Decimals", 2, file.c_str());
    if (cfg.decimals < 0 || cfg.decimals > 2) cfg.decimals = 2;
    cfg.sampleMs = (int)GetPrivateProfileIntW(L"Display", L"SampleMs", 1000, file.c_str());
    if (cfg.sampleMs < 200 || cfg.sampleMs > 5000) cfg.sampleMs = 1000;

    std::lock_guard<std::mutex> lock(m_mutex);
    m_cfg = cfg;
}

void ConfigManager::Save(const DisplayConfig& cfg) {
    std::wstring file = GetFilePath();
    WritePrivateProfileStringW(L"Display", L"ShowUnit", cfg.showUnit ? L"1" : L"0", file.c_str());
    WritePrivateProfileStringW(L"Display", L"UnitMode", std::to_wstring(cfg.unitMode).c_str(), file.c_str());
    WritePrivateProfileStringW(L"Display", L"Decimals", std::to_wstring(cfg.decimals).c_str(), file.c_str());
    WritePrivateProfileStringW(L"Display", L"SampleMs", std::to_wstring(cfg.sampleMs).c_str(), file.c_str());

    std::lock_guard<std::mutex> lock(m_mutex);
    m_cfg = cfg;
}

DisplayConfig ConfigManager::Get() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_cfg;
}
