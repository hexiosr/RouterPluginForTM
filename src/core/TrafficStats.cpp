// TrafficStats.cpp - 今日流量累计
#include "pch.h"
#include "TrafficStats.h"

namespace {

constexpr unsigned long long kU32 = 4294967296ULL;

std::wstring StatsFile(const std::wstring& dir) {
    if (dir.empty()) return L"RouterTotalSpeed.ini";
    return dir + L"\\RouterTotalSpeed.ini";
}

// 旧版本把 [Stats] 单独放在 RouterTotalSpeedStats.ini，合并进主配置后删掉旧文件
void MigrateLegacy(const std::wstring& dir) {
    if (dir.empty()) return;
    std::wstring old = dir + L"\\RouterTotalSpeedStats.ini";
    if (GetFileAttributesW(old.c_str()) == INVALID_FILE_ATTRIBUTES) return;
    const wchar_t* keys[] = { L"Date", L"RxBytes", L"TxBytes", L"LastRx", L"LastTx" };
    for (const wchar_t* key : keys) {
        wchar_t buf[32] = {};
        GetPrivateProfileStringW(L"Stats", key, L"", buf, 32, old.c_str());
        if (buf[0] == L'\0') continue;
        WritePrivateProfileStringW(L"Stats", key, buf, StatsFile(dir).c_str());
    }
    DeleteFileW(old.c_str());
}

std::wstring ReadValue(const wchar_t* key, const std::wstring& file) {
    wchar_t buf[32] = {};
    GetPrivateProfileStringW(L"Stats", key, L"0", buf, 32, file.c_str());
    return buf;
}

// 0xFFFFFFFF 作为“无锚点”标记；其余按无符号十进制写
std::wstring AnchorText(unsigned int v) {
    if (v == (unsigned int)-1) return L"-1";
    return std::to_wstring((unsigned long long)v);
}

} // namespace

int TrafficStats::TodayDate(time_t now) {
    struct tm lt{};
    localtime_s(&lt, &now);
    return (lt.tm_year + 1900) * 10000 + (lt.tm_mon + 1) * 100 + lt.tm_mday;
}

void TrafficStats::Init(const std::wstring& dir) {
    m_path = StatsFile(dir);
    MigrateLegacy(dir);
    time_t now = time(nullptr);
    int today = TodayDate(now);
    if ((int)GetPrivateProfileIntW(L"Stats", L"Date", 0, m_path.c_str()) == today) {
        m_rx.store(wcstoull(ReadValue(L"RxBytes", m_path).c_str(), nullptr, 10));
        m_tx.store(wcstoull(ReadValue(L"TxBytes", m_path).c_str(), nullptr, 10));

        // 上次退出时的计数器；-1 表示没有可用锚点（文件来自旧版本或从未采样）
        wchar_t buf[32] = {};
        GetPrivateProfileStringW(L"Stats", L"LastRx", L"-1", buf, 32, m_path.c_str());
        long long lastRx = wcstoll(buf, nullptr, 10);
        GetPrivateProfileStringW(L"Stats", L"LastTx", L"-1", buf, 32, m_path.c_str());
        long long lastTx = wcstoll(buf, nullptr, 10);
        if (lastRx >= 0 && lastTx >= 0) {
            m_havePending = true;
            m_pendingRx = (unsigned int)lastRx;
            m_pendingTx = (unsigned int)lastTx;
        }
    }
    m_date = today;
    m_lastSave = now;
}

unsigned long long TrafficStats::Delta(unsigned int last, unsigned int cur) {
    if (cur >= last) return cur - last;
    if (last >= (unsigned int)(kU32 * 0.9375) && cur < (unsigned int)(kU32 * 0.0625))
        return cur + kU32 - last;   // 4 GiB 回绕
    return cur;                      // 路由器重启，计数从头开始
}

void TrafficStats::Update(double rawRx, double rawTx, time_t now) {
    int today = TodayDate(now);
    if (today != m_date) {
        m_date = today;
        m_rx.store(0);
        m_tx.store(0);
        m_hasLast = false;
        m_havePending = false;  // 跨天后无法把停机增量归到新的一天
    }

    unsigned int rx = (unsigned int)rawRx;
    unsigned int tx = (unsigned int)rawTx;
    if (!m_hasLast) {
        m_hasLast = true;
        if (m_havePending) {
            // 补上上次退出到本次启动之间路由器累计的流量
            m_rx.fetch_add(Delta(m_pendingRx, rx));
            m_tx.fetch_add(Delta(m_pendingTx, tx));
            m_havePending = false;
        }
        m_lastRx = rx;
        m_lastTx = tx;
        return;
    }

    m_rx.fetch_add(Delta(m_lastRx, rx));
    m_tx.fetch_add(Delta(m_lastTx, tx));
    m_lastRx = rx;
    m_lastTx = tx;
}

void TrafficStats::FlushIfDue(time_t now) {
    if (now - m_lastSave >= 60) Save();
}

void TrafficStats::Flush() {
    Save();
}

void TrafficStats::Save() {
    m_lastSave = time(nullptr);
    unsigned int lastRx = (unsigned int)-1;
    unsigned int lastTx = (unsigned int)-1;
    if (m_hasLast) {
        lastRx = m_lastRx;
        lastTx = m_lastTx;
    } else if (m_havePending) {
        lastRx = m_pendingRx;
        lastTx = m_pendingTx;
    }
    WritePrivateProfileStringW(L"Stats", L"Date", std::to_wstring(m_date).c_str(), m_path.c_str());
    WritePrivateProfileStringW(L"Stats", L"RxBytes", std::to_wstring(m_rx.load()).c_str(), m_path.c_str());
    WritePrivateProfileStringW(L"Stats", L"TxBytes", std::to_wstring(m_tx.load()).c_str(), m_path.c_str());
    // 按无符号写：计数器高位置 1 时不能变成负数，否则重启后被当作无锚点
    WritePrivateProfileStringW(L"Stats", L"LastRx", AnchorText(lastRx).c_str(), m_path.c_str());
    WritePrivateProfileStringW(L"Stats", L"LastTx", AnchorText(lastTx).c_str(), m_path.c_str());
}
