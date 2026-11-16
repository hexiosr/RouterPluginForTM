// dlgtest.cpp - 打开设置对话框做视觉检查（配置存到临时目录，不碰真实配置）
#include "pch.h"
#include "OptionsDlg.h"
#include "DisplayConfig.h"
#include <shlobj.h>

int wmain() {
    wchar_t tmp[MAX_PATH] = {};
    GetTempPathW(MAX_PATH, tmp);
    std::wstring dir = std::wstring(tmp) + L"RouterTotalSpeedDlgTest";
    CreateDirectoryW(dir.c_str(), NULL);

    ConfigManager::Instance().SetConfigDir(dir);
    ConfigManager::Instance().Load();

    COptionsDlg::Show(nullptr);
    return 0;
}
