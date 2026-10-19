// Windows implementation of the platform layer, used only for PC testing.

#include "../source/platform.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include <cstdarg>
#include <cstdio>
#include <mutex>

namespace plat {

bool net_init() {
    WSADATA wsa;
    return WSAStartup(MAKEWORD(2, 2), &wsa) == 0;
}

static void set_nonblocking(SOCKET s) {
    u_long one = 1;
    ioctlsocket(s, FIONBIO, &one);
}

Socket tcp_listen(uint16_t port) {
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) {
        return INVALID_SOCKET_;
    }
    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(s, (sockaddr *) &addr, sizeof(addr)) != 0 || listen(s, 4) != 0) {
        closesocket(s);
        return INVALID_SOCKET_;
    }
    set_nonblocking(s);
    return (Socket) s;
}

Socket tcp_accept(Socket listener, std::string &peerIp, int timeoutMs) {
    WSAPOLLFD p{};
    p.fd     = (SOCKET) listener;
    p.events = POLLRDNORM;
    int rc   = WSAPoll(&p, 1, timeoutMs);
    if (rc <= 0) {
        if (rc < 0) {
            Sleep(timeoutMs);
        }
        return INVALID_SOCKET_;
    }
    sockaddr_in addr{};
    int len  = sizeof(addr);
    SOCKET c = accept((SOCKET) listener, (sockaddr *) &addr, &len);
    if (c == INVALID_SOCKET) {
        return INVALID_SOCKET_;
    }
    set_nonblocking(c);
    BOOL nodelay = TRUE;
    setsockopt(c, IPPROTO_TCP, TCP_NODELAY, (const char *) &nodelay, sizeof(nodelay));
    char buf[64];
    inet_ntop(AF_INET, &addr.sin_addr, buf, sizeof(buf));
    peerIp = buf;
    return (Socket) c;
}

int tcp_recv(Socket s, void *buf, size_t len, int timeoutMs) {
    WSAPOLLFD p{};
    p.fd     = (SOCKET) s;
    p.events = POLLRDNORM;
    int rc   = WSAPoll(&p, 1, timeoutMs);
    if (rc < 0) {
        return -1;
    }
    if (rc == 0) {
        return 0;
    }
    int n = recv((SOCKET) s, (char *) buf, (int) (len > 0x40000000 ? 0x40000000 : len), 0);
    if (n == 0) {
        return -1;
    }
    if (n < 0) {
        return WSAGetLastError() == WSAEWOULDBLOCK ? 0 : -1;
    }
    return n;
}

int tcp_send(Socket s, const void *buf, size_t len, int timeoutMs) {
    WSAPOLLFD p{};
    p.fd     = (SOCKET) s;
    p.events = POLLWRNORM;
    int rc   = WSAPoll(&p, 1, timeoutMs);
    if (rc < 0) {
        return -1;
    }
    if (rc == 0) {
        return 0;
    }
    int n = send((SOCKET) s, (const char *) buf, (int) (len > 0x40000000 ? 0x40000000 : len), 0);
    if (n < 0) {
        return WSAGetLastError() == WSAEWOULDBLOCK ? 0 : -1;
    }
    return n;
}

void tcp_close(Socket s) {
    if (s != INVALID_SOCKET_) {
        closesocket((SOCKET) s);
    }
}

uint64_t now_ms() {
    return GetTickCount64();
}

void sleep_ms(int ms) {
    Sleep(ms);
}

uint64_t fast_ticks() {
    LARGE_INTEGER v;
    QueryPerformanceCounter(&v);
    return (uint64_t) v.QuadPart;
}

void tune_thread(std::thread &, const char *) {
}

void log(const char *fmt, ...) {
    static std::mutex m;
    std::lock_guard<std::mutex> lock(m);
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    fflush(stderr);
}

} // namespace plat
