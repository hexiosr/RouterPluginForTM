// OptionsDlg.h - 设置对话框
#pragma once
#include "pch.h"

class COptionsDlg {
public:
    // 返回 true 表示配置已修改并保存
    static bool Show(HWND hParent);
};
