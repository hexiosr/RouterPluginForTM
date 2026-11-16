// UpnpWan.cpp - SSDP 发现 IGD，用 WANCommonInterfaceConfig 计数器算 WAN 总速率
#include "pch.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include "UpnpWan.h"
#include "Http.h"

namespace {

const char* kServiceNeedle = "WANCommonInterfaceConfig";

std::string TrimCopy(const std::string& s) {
    size_t b = 0, e = s.size();
    while (b < e && (s[b] == ' ' || s[b] == '\t' || s[b] == '\r' || s[b] == '\n')) ++b;
    while (e > b && (s[e - 1] == ' ' || s[e - 1] == '\t' || s[e - 1] == '\r' || s[e - 1] == '\n')) --e;
    return s.substr(b, e - b);
}

// 本机所有已启用网卡的 IPv4 地址（跳过回环）
std::vector<in_addr> LocalIPv4Addresses() {
    std::vector<in_addr> out;
    const ULONG flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER;
    ULONG size = 0;
    if (GetAdaptersAddresses(AF_INET, flags, nullptr, nullptr, &size) != ERROR_BUFFER_OVERFLOW)
        return out;
    std::vector<BYTE> buf(size);
    auto* head = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buf.data());
    if (GetAdaptersAddresses(AF_INET, flags, nullptr, head, &size) != NO_ERROR)
        return out;

    for (auto* a = head; a; a = a->Next) {
        if (a->OperStatus != IfOperStatusUp || a->IfType == IF_TYPE_SOFTWARE_LOOPBACK) continue;
        for (auto* ua = a->FirstUnicastAddress; ua; ua = ua->Next) {
            if (!ua->Address.lpSockaddr || ua->Address.lpSockaddr->sa_family != AF_INET) continue;
            auto* sin = reinterpret_cast<const sockaddr_in*>(ua->Address.lpSockaddr);
            if (sin->sin_addr.s_addr == htonl(INADDR_LOOPBACK)) continue;
            bool dup = std::any_of(out.begin(), out.end(), [&](const in_addr& x) {
                return x.s_addr == sin->sin_addr.s_addr;
            });
            if (!dup) out.push_back(sin->sin_addr);
        }
    }
    return out;
}

// 从非阻塞 socket 读完当前可读的响应，收集 LOCATION
void CollectLocations(SOCKET s, std::vector<std::string>& locations) {
    char buf[2048];
    for (;;) {
        int n = recv(s, buf, (int)sizeof(buf) - 1, 0);
        if (n <= 0) return;
        buf[n] = '\0';
        for (char* line = buf; line && *line; ) {
            char* eol = strstr(line, "\r\n");
            size_t len = eol ? (size_t)(eol - line) : strlen(line);
            if (len > 9 && _strnicmp(line, "location:", 9) == 0) {
                std::string loc = TrimCopy(std::string(line + 9, len - 9));
                if (!loc.empty() && std::find(locations.begin(), locations.end(), loc) == locations.end())
                    locations.push_back(loc);
                break;
            }
            if (!eol) break;
            line = eol + 2;
        }
        if (locations.size() >= 4) return;
    }
}

// 取 <tag ...>text</tag> 的 text
std::string TagText(const std::string& xml, const char* tag) {
    std::string open = std::string("<") + tag;
    size_t p = xml.find(open);
    if (p == std::string::npos) return {};
    size_t gt = xml.find('>', p);
    if (gt == std::string::npos) return {};
    size_t e = xml.find('<', gt + 1);
    if (e == std::string::npos) return {};
    return TrimCopy(xml.substr(gt + 1, e - gt - 1));
}

std::string AuthorityOf(const std::string& url) {
    size_t p = url.find("://");
    if (p == std::string::npos) return {};
    size_t e = url.find('/', p + 3);
    return url.substr(0, e == std::string::npos ? url.size() : e);
}

std::string ResolveUrl(const std::string& base, const std::string& rel) {
    if (rel.empty()) return {};
    if (rel.compare(0, 7, "http://") == 0 || rel.compare(0, 8, "https://") == 0) return rel;
    std::string auth = AuthorityOf(base);
    if (auth.empty()) return {};
    if (rel[0] == '/') return auth + rel;
    size_t slash = base.find_last_of('/');
    if (slash == std::string::npos) return {};
    return base.substr(0, slash + 1) + rel;
}

std::string SoapBody(const char* action, const std::string& args, const std::string& type) {
    return std::string("<?xml version=\"1.0\"?>"
        "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" "
        "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">"
        "<s:Body><u:") + action + " xmlns:u=\"" + type + "\">" + args +
        "</u:" + action + "></s:Body></s:Envelope>";
}

} // namespace

void UpnpWan::Reset() {
    m_controlUrl.clear();
    m_serviceType.clear();
    m_hist.clear();
}

bool UpnpWan::Sample(double& upKBps, double& downKBps) {
    upKBps = 0;
    downKBps = 0;
    if (m_controlUrl.empty() && !Discover()) return false;

    double rx = 0, tx = 0;
    if (!FetchCounters(rx, tx)) {
        Reset();
        return false;
    }

    auto now = std::chrono::steady_clock::now();
    m_hist.push_back({ now, rx, tx });
    while (m_hist.size() > 2) m_hist.pop_front();

    // 以上一次采样为基准：速率窗口等于采样间隔（默认 1 秒）
    const Point* anchor = m_hist.size() >= 2 ? &m_hist.front() : nullptr;
    if (anchor) {
        double dt = std::chrono::duration<double>(now - anchor->t).count();
        double dRx = rx - anchor->rx;
        double dTx = tx - anchor->tx;
        // 规范里 TotalBytes* 为 ui4（4 GiB 回绕）：基准接近上限且现值接近 0 时补一个周期，
        // 否则按路由器重启清零处理
        constexpr double kU32 = 4294967296.0;
        if (dRx < 0) dRx = (anchor->rx >= kU32 * 0.9375 && rx < kU32 * 0.0625) ? dRx + kU32 : 0;
        if (dTx < 0) dTx = (anchor->tx >= kU32 * 0.9375 && tx < kU32 * 0.0625) ? dTx + kU32 : 0;
        if (dt > 0.05) {
            downKBps = dRx / 1024.0 / dt;
            upKBps = dTx / 1024.0 / dt;
        }
    }
    return true;
}

bool UpnpWan::Discover() {
    static bool wsaReady = false;
    static std::once_flag wsaOnce;
    std::call_once(wsaOnce, [] {
        WSADATA d{};
        wsaReady = WSAStartup(MAKEWORD(2, 2), &d) == 0;
    });
    if (!wsaReady) return false;

    sockaddr_in dest{};
    dest.sin_family = AF_INET;
    dest.sin_port = htons(1900);
    dest.sin_addr.s_addr = inet_addr("239.255.255.250");

    const char* msg =
        "M-SEARCH * HTTP/1.1\r\n"
        "HOST: 239.255.255.250:1900\r\n"
        "MAN: \"ssdp:discover\"\r\n"
        "MX: 1\r\n"
        "ST: urn:schemas-upnp-org:device:InternetGatewayDevice:1\r\n"
        "\r\n";

    // VPN 占用默认路由、或路由器接在非默认网卡时，只从默认接口发会找不到：
    // 每个本机 IPv4 地址各开一个 socket 发送，再统一收响应
    std::vector<in_addr> localAddrs = LocalIPv4Addresses();
    if (localAddrs.empty()) localAddrs.emplace_back();  // 枚举失败时退回 0.0.0.0

    std::vector<SOCKET> socks;
    for (const auto& addr : localAddrs) {
        SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (s == INVALID_SOCKET) continue;
        sockaddr_in src{};
        src.sin_family = AF_INET;
        src.sin_addr = addr;
        if (bind(s, (sockaddr*)&src, sizeof(src)) != 0) { closesocket(s); continue; }
        if (addr.s_addr != 0)
            setsockopt(s, IPPROTO_IP, IP_MULTICAST_IF, (const char*)&addr, sizeof(addr));
        u_long nonBlocking = 1;
        ioctlsocket(s, FIONBIO, &nonBlocking);
        sendto(s, msg, (int)strlen(msg), 0, (sockaddr*)&dest, sizeof(dest));
        socks.push_back(s);
    }

    std::vector<std::string> locations;
    const auto start = std::chrono::steady_clock::now();
    const auto deadline = start + std::chrono::milliseconds(1600);
    bool resent = false;
    while (!socks.empty() && locations.size() < 4) {
        auto now = std::chrono::steady_clock::now();
        if (now >= deadline) break;
        if (!resent && now - start >= std::chrono::milliseconds(800)) {
            for (SOCKET s : socks)
                sendto(s, msg, (int)strlen(msg), 0, (sockaddr*)&dest, sizeof(dest));
            resent = true;
        }

        fd_set rfds;
        FD_ZERO(&rfds);
        for (SOCKET s : socks) FD_SET(s, &rfds);
        auto left = std::chrono::duration_cast<std::chrono::microseconds>(deadline - now).count();
        timeval tv{};
        tv.tv_sec = (long)(left / 1000000);
        tv.tv_usec = (long)(left % 1000000);
        int ready = select(0, &rfds, nullptr, nullptr, &tv);
        if (ready <= 0) break;
        for (SOCKET s : socks)
            if (FD_ISSET(s, &rfds)) CollectLocations(s, locations);
    }
    for (SOCKET s : socks) closesocket(s);

    for (const auto& loc : locations)
        if (ResolveService(loc)) return true;
    return false;
}

bool UpnpWan::ResolveService(const std::string& location) {
    Http::Response res;
    if (!Http::Request(L"GET", location, "", "", res, 2500) || !res.transportOk || res.status != 200)
        return false;

    const std::string& xml = res.body;
    size_t pos = 0;
    while ((pos = xml.find("<service>", pos)) != std::string::npos) {
        size_t end = xml.find("</service>", pos);
        if (end == std::string::npos) break;
        std::string block = xml.substr(pos, end - pos);
        if (block.find(kServiceNeedle) != std::string::npos) {
            std::string type = TagText(block, "serviceType");
            std::string ctrl = TagText(block, "controlURL");
            if (!type.empty() && !ctrl.empty()) {
                std::string url = ResolveUrl(location, ctrl);
                if (!url.empty()) {
                    m_serviceType = type;
                    m_controlUrl = url;
                    return true;
                }
            }
        }
        pos = end + 10;
    }
    return false;
}

bool UpnpWan::FetchCounters(double& rx, double& tx) {
    return FetchOne("GetTotalBytesReceived", "NewTotalBytesReceived", rx) &&
           FetchOne("GetTotalBytesSent", "NewTotalBytesSent", tx);
}

bool UpnpWan::FetchOne(const char* action, const char* resultTag, double& out) {
    std::string args = m_noArg ? std::string() : "<NewInterfaceOnly>0</NewInterfaceOnly>";
    std::string headers = "Content-Type: text/xml; charset=\"utf-8\"\r\nSOAPAction: \"" +
                          m_serviceType + "#" + action + "\"\r\n";
    Http::Response res;
    if (!Http::Request(L"POST", m_controlUrl, headers, SoapBody(action, args, m_serviceType), res, 2000) ||
        !res.transportOk || res.status != 200)
        return false;

    std::string text = TagText(res.body, resultTag);
    if (text.empty() && !m_noArg) {
        m_noArg = true;
        return FetchOne(action, resultTag, out);
    }
    if (text.empty()) return false;
    out = atof(text.c_str());
    return true;
}
