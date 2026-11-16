// TrafficStats.h - 今日流量累计（基于 WAN 计数器增量，账本独立于显示配置）
#pragma once
#include "pch.h"

class TrafficStats {
public:
    void Init(const std::wstring& dir);
    // 每次成功采样调用一次原始计数，内部处理 4 GiB 回绕与路由器重启清零
    void Update(double rawRx, double rawTx, time_t now);
    void FlushIfDue(time_t now);
    void Flush();

    unsigned long long TodayRx() const { return m_rx.load(); }
    unsigned long long TodayTx() const { return m_tx.load(); }

private:
    static int TodayDate(time_t now);
    static unsigned long long Delta(unsigned int last, unsigned int cur);
    void Save();

    std::wstring m_path;
    std::atomic<unsigned long long> m_rx{0};
    std::atomic<unsigned long long> m_tx{0};
    bool m_hasLast = false;
    unsigned int m_lastRx = 0;
    unsigned int m_lastTx = 0;
    // 上次退出时保存的计数器：启动后第一次采样时用它补上停机期间的增量
    bool m_havePending = false;
    unsigned int m_pendingRx = 0;
    unsigned int m_pendingTx = 0;
    int m_date = 0;
    time_t m_lastSave = 0;
};
