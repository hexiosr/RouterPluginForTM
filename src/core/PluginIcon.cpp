// PluginIcon.cpp - 插件图标加载
#include "pch.h"
#include "PluginIcon.h"
#include "resource.h"

HMODULE g_pluginModule = nullptr;

void SetPluginModule(HMODULE hModule) {
    g_pluginModule = hModule;
}

HICON LoadPluginIcon(int cx, int cy) {
    if (!g_pluginModule) return nullptr;
    return (HICON)LoadImageW(g_pluginModule, MAKEINTRESOURCEW(IDI_PLUGIN_ICON),
                             IMAGE_ICON, cx, cy, LR_DEFAULTCOLOR);
}
