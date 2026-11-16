// UpnpWan.h - UPnP IGD 的 WAN 字节计数器（比 HostInfo 的 ~2 秒量化刷新快得多）
#pragma once
#include "pch.h"

class UpnpWan {
public:
    // 采样一次；返回 false 表示拿不到（未发现 IGD / 请求失败 / 不支持）
    bool Sample(double& upKBps, double& downKBps);
    void Reset();
    bool Ready() const { return !m_controlUrl.empty(); }

private:
    bool Discover();
    bool ResolveService(const std::string& location);
    bool FetchCounters(double& rx, double& tx);
    bool FetchOne(const char* action, const char* resultTag, double& out);

    std::string m_controlUrl;
    std::string m_serviceType;
    bool m_noArg = false;   // 部分实现不接受 NewInterfaceOnly 参数

    struct Point {
        std::chrono::steady_clock::time_point t;
        double rx = 0;
        double tx = 0;
    };
    std::deque<Point> m_hist;   // 上一次与本次计数，按采样间隔算速率
};
