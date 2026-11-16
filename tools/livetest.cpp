// livetest.cpp - 用真实插件配置目录取一次显示值（只读配置，不写文件）
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
    if (argc < 3) { printf("usage: livetest <dll> <configDir>\n"); return 2; }
    HMODULE h = LoadLibraryW(argv[1]);
    if (!h) { printf("LoadLibrary failed: %lu\n", GetLastError()); return 1; }
    auto getInst = (ITMPlugin * (*)())GetProcAddress(h, "TMPluginGetInstance");
    if (!getInst) { printf("missing export\n"); return 1; }

    ITMPlugin* p = getInst();
    p->OnExtenedInfo(ITMPlugin::EI_CONFIG_DIR, argv[2]);
    p->DataRequired();

    IPluginItem* a = p->GetItem(0);
    IPluginItem* b = p->GetItem(1);
    printf("double line exclusive: devices=%d total=%d\n",
           a ? a->IsDoubleLineExclusive() : -1,
           b ? b->IsDoubleLineExclusive() : -1);

    std::string last;
    for (int i = 0; i < 30; ++i) {
        Sleep(500);
        p->DataRequired();
        std::string v0 = a ? U8(a->GetItemValueText()) : "?";
        std::string v1 = b ? U8(b->GetItemValueText()) : "?";
        std::string line = v0 + " || " + v1;
        if (line != last) {
            printf("[%2d] devices='%s' total='%s'\n", i, v0.c_str(), v1.c_str());
            last = line;
        }
        if (v0.find("KB/s") != std::string::npos || v0.find("MB/s") != std::string::npos) break;
    }
    printf("--- tooltip ---\n%s\n", U8(p->GetTooltipInfo()).c_str());
    printf("OK\n");
    ExitProcess(0);
}
