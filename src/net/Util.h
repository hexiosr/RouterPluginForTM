// Util.h - UTF-8 / 宽字符转换
#pragma once
#include "pch.h"

std::wstring Utf8ToWide(const std::string& s);
std::string WideToUtf8(const std::wstring& s);
