// TextFmt.h - 轻量数字格式化
#pragma once
#include <string>
#include <cstdio>
#include <cwchar>

namespace TextFmt {

inline std::wstring Fixed(double value, int digits) {
    wchar_t buf[64] = {};
    int n = swprintf(buf, 64, L"%.*f", digits, value);
    if (n <= 0) return {};
    return std::wstring(buf, (size_t)(n < 63 ? n : 63));
}

}
