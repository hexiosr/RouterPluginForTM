// loadtest.cpp - 冒烟测试：加载 DLL，枚举显示项（不初始化后台线程）
#include <windows.h>
#include "PluginInterface.h"
#include <cstdio>
#include <string>

static std::string U8(const wchar_t* s) {
    if (!s) return "(null)";
    int n = WideCharToMultiByte(CP_UTF8, 0, s, -1, NULL, 0, NULL, NULL);
    if (n <= 1) return "";
    std::string out((size_t)(n - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, s, -1, out.data(), n, NULL, NULL);
    return out;
}

int wmain(int argc, wchar_t** argv) {
    SetConsoleOutputCP(CP_UTF8);
    if (argc < 2) { printf("usage: loadtest <dll>\n"); return 2; }
    HMODULE h = LoadLibraryW(argv[1]);
    if (!h) { printf("LoadLibrary failed: %lu\n", GetLastError()); return 1; }

    auto getInst = (ITMPlugin * (*)())GetProcAddress(h, "TMPluginGetInstance");
    if (!getInst) { printf("missing TMPluginGetInstance\n"); FreeLibrary(h); return 1; }

    ITMPlugin* p = getInst();
    printf("api version: %d\n", p->GetAPIVersion());
    printf("name: %s\n", U8(p->GetInfo(ITMPlugin::TMI_NAME)).c_str());
    printf("desc: %s\n", U8(p->GetInfo(ITMPlugin::TMI_DESCRIPTION)).c_str());
    printf("version: %s\n", U8(p->GetInfo(ITMPlugin::TMI_VERSION)).c_str());

    for (int i = 0;; ++i) {
        IPluginItem* item = p->GetItem(i);
        if (!item) { printf("items: %d\n", i); break; }
        printf("  [%d] id=%s name=%s label='%s' value='%s'\n", i,
               U8(item->GetItemId()).c_str(), U8(item->GetItemName()).c_str(),
               U8(item->GetItemLableText()).c_str(), U8(item->GetItemValueText()).c_str());
    }

    HICON icon = (HICON)p->GetPluginIcon();
    printf("icon: %p\n", (void*)icon);
    printf("tooltip:\n%s\n", U8(p->GetTooltipInfo()).c_str());

    FreeLibrary(h);
    printf("OK\n");
    return 0;
}
