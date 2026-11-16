// Http.cpp - WinHTTP 同步请求（直连，不走系统代理）
#include "pch.h"
#include <winhttp.h>
#include "Http.h"
#include "Util.h"

namespace Http {

bool Request(const std::wstring& method, const std::string& url,
             const std::string& extraHeaders, const std::string& body,
             Response& res, int timeoutMs) {
    res = Response{};
    if (url.empty()) { res.error = L"地址为空"; return false; }

    std::wstring urlW = Utf8ToWide(url);
    URL_COMPONENTS uc{};
    uc.dwStructSize = sizeof(uc);
    wchar_t host[256] = {};
    wchar_t path[4096] = {};
    wchar_t extra[2048] = {};
    uc.lpszHostName = host;      uc.dwHostNameLength = 255;
    uc.lpszUrlPath = path;       uc.dwUrlPathLength = 4095;
    uc.lpszExtraInfo = extra;    uc.dwExtraInfoLength = 2047;
    if (!WinHttpCrackUrl(urlW.c_str(), (DWORD)urlW.size(), 0, &uc) || uc.dwHostNameLength == 0) {
        res.error = L"URL 解析失败";
        return false;
    }
    std::wstring target = path;
    target += extra;

    // WINHTTP_ACCESS_TYPE_NO_PROXY：本机代理（如 7897）不能劫持内网请求
    HINTERNET session = WinHttpOpen(L"RouterTotalSpeed/0.1",
                                    WINHTTP_ACCESS_TYPE_NO_PROXY,
                                    WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) { res.error = L"WinHTTP 初始化失败"; return false; }
    WinHttpSetTimeouts(session, timeoutMs, timeoutMs, timeoutMs, timeoutMs);
    DWORD redirectPolicy = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
    WinHttpSetOption(session, WINHTTP_OPTION_REDIRECT_POLICY, &redirectPolicy, sizeof(redirectPolicy));

    bool cleanup = false;
    HINTERNET conn = WinHttpConnect(session, host, (INTERNET_PORT)uc.nPort, 0);
    HINTERNET req = nullptr;
    if (conn) {
        req = WinHttpOpenRequest(conn, method.c_str(), target.c_str(), NULL,
                                 WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                 uc.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0);
    }
    if (!req) {
        res.error = L"无法连接路由器";
        if (conn) WinHttpCloseHandle(conn);
        WinHttpCloseHandle(session);
        return false;
    }

    if (!extraHeaders.empty())
        WinHttpAddRequestHeaders(req, Utf8ToWide(extraHeaders).c_str(), (DWORD)-1L,
                                 WINHTTP_ADDREQ_FLAG_ADD);

    BOOL sent = WinHttpSendRequest(
        req, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
        body.empty() ? WINHTTP_NO_REQUEST_DATA : (LPVOID)body.data(),
        (DWORD)body.size(), (DWORD)body.size(), 0);
    if (!sent || !WinHttpReceiveResponse(req, NULL)) {
        res.error = L"网络连接失败";
        cleanup = true;
    }

    if (!cleanup) {
        DWORD status = 0, size = sizeof(status);
        WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX);
        res.status = (long)status;

        std::string data;
        DWORD avail = 0;
        while (WinHttpQueryDataAvailable(req, &avail) && avail > 0) {
            std::string chunk(avail, '\0');
            DWORD read = 0;
            if (!WinHttpReadData(req, &chunk[0], avail, &read) || read == 0) break;
            chunk.resize(read);
            data += chunk;
            if (data.size() > 2 * 1024 * 1024) break;
        }
        res.body = std::move(data);
    }

    WinHttpCloseHandle(req);
    WinHttpCloseHandle(conn);
    WinHttpCloseHandle(session);

    if (cleanup) return false;
    res.transportOk = true;
    return true;
}

} // namespace Http
