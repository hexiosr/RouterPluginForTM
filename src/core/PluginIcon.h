// PluginIcon.h - 插件图标加载
#pragma once
#include <Windows.h>

void SetPluginModule(HMODULE hModule);
HICON LoadPluginIcon(int cx, int cy);
