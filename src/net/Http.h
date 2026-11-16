// Http.h - WinHTTP 同步请求（直连，不走系统代理）
#pragma once
#include "pch.h"

namespace Http {

struct Response {
    long   status = 0;
    std::string body;
    bool   transportOk = false;
    std::wstring error;
};

bool Request(const std::wstring& method, const std::string& url,
             const std::string& extraHeaders, const std::string& body,
             Response& res, int timeoutMs = 5000);

} // namespace Http
