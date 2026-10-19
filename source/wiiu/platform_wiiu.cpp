// Wii U implementation of the platform layer (wut + nsysnet via newlib).

#include "../platform.h"

#include <coreinit/debug.h>
#include <coreinit/thread.h>
#include <coreinit/time.h>

#include <arpa/inet.h>
#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

namespace plat {

bool net_init() {
    return true;
}

static bool set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    return flags != -1 && fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
}

Socket tcp_listen(uint16_t port) {
    int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd < 0) {
        return INVALID_SOCKET_;
    }
    int one = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(fd, (sockaddr *) &addr, sizeof(addr)) != 0 || listen(fd, 4) != 0 || !set_nonblocking(fd)) {
        LOG("listen setup failed: %d", errno);
        close(fd);
        return INVALID_SOCKET_;
    }
    return fd;
}

Socket tcp_accept(Socket listener, std::string &peerIp, int timeoutMs) {
    pollfd p{};
    p.fd     = (int) listener;
    p.events = POLLIN;
    int rc   = poll(&p, 1, timeoutMs);
    if (rc <= 0) {
        if (rc < 0) {
            sleep_ms(timeoutMs);
        }
        return INVALID_SOCKET_;
    }
    sockaddr_in addr{};
    socklen_t len = sizeof(addr);
    int fd        = accept((int) listener, (sockaddr *) &addr, &len);
    if (fd < 0) {
        return INVALID_SOCKET_;
    }
    if (!set_nonblocking(fd)) {
        close(fd);
        return INVALID_SOCKET_;
    }
    int one = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
    setsockopt(fd, SOL_SOCKET, SO_WINSCALE, &one, sizeof(one));
    int bufSize = 128 * 1024;
    setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &bufSize, sizeof(bufSize));
    setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &bufSize, sizeof(bufSize));

    uint32_t ip = ntohl(addr.sin_addr.s_addr);
    char buf[16];
    snprintf(buf, sizeof(buf), "%u.%u.%u.%u", (unsigned) (ip >> 24) & 0xff, (unsigned) (ip >> 16) & 0xff,
             (unsigned) (ip >> 8) & 0xff, (unsigned) ip & 0xff);
    peerIp = buf;
    return fd;
}

int tcp_recv(Socket s, void *buf, size_t len, int timeoutMs) {
    pollfd p{};
    p.fd     = (int) s;
    p.events = POLLIN;
    int rc   = poll(&p, 1, timeoutMs);
    if (rc < 0) {
        return errno == EINTR ? 0 : -1;
    }
    if (rc == 0) {
        return 0;
    }
    ssize_t n = recv((int) s, buf, len, 0);
    if (n == 0) {
        return -1;
    }
    if (n < 0) {
        return (errno == EWOULDBLOCK || errno == EAGAIN || errno == EINTR) ? 0 : -1;
    }
    return (int) n;
}

int tcp_send(Socket s, const void *buf, size_t len, int timeoutMs) {
    pollfd p{};
    p.fd     = (int) s;
    p.events = POLLOUT;
    int rc   = poll(&p, 1, timeoutMs);
    if (rc < 0) {
        return errno == EINTR ? 0 : -1;
    }
    if (rc == 0) {
        return 0;
    }
    ssize_t n = send((int) s, buf, len, 0);
    if (n < 0) {
        return (errno == EWOULDBLOCK || errno == EAGAIN || errno == EINTR) ? 0 : -1;
    }
    return (int) n;
}

void tcp_close(Socket s) {
    if (s != INVALID_SOCKET_) {
        close((int) s);
    }
}

uint64_t now_ms() {
    return OSTicksToMilliseconds(OSGetSystemTime());
}

void sleep_ms(int ms) {
    OSSleepTicks(OSMillisecondsToTicks(ms));
}

uint64_t fast_ticks() {
    return (uint64_t) OSGetSystemTime();
}

void tune_thread(std::thread &t, const char *name) {
    auto *native = (OSThread *) t.native_handle();
    OSSetThreadName(native, name);
    // Like ftpiiu: keep network work on core 2, away from most games' main
    // thread. Affinity changes can fail while the thread is busy; retry briefly.
    for (int i = 0; i < 50 && !OSSetThreadAffinity(native, OS_THREAD_ATTRIB_AFFINITY_CPU2); i++) {
        OSSleepTicks(OSMillisecondsToTicks(2));
    }
    for (int i = 0; i < 50 && !OSSetThreadPriority(native, 16); i++) {
        OSSleepTicks(OSMillisecondsToTicks(2));
    }
}

void log(const char *fmt, ...) {
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    OSReport("[SFTPiiU] %s\n", buf);
}

} // namespace plat
