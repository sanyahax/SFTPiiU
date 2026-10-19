// Platform layer for the WebAssembly test build. Sockets are provided by the
// Python runner (run_wasm.py) through imports from the "host" module.

#include "../../source/platform.h"

#include <cstdarg>
#include <cstdio>
#include <ctime>

#define HOST_IMPORT(name) __attribute__((import_module("host"), import_name(#name)))

extern "C" {
HOST_IMPORT(tcp_listen) int32_t host_tcp_listen(int32_t port);
HOST_IMPORT(tcp_accept) int32_t host_tcp_accept(int32_t listener, char *ip, int32_t ipLen, int32_t timeoutMs);
HOST_IMPORT(tcp_recv) int32_t host_tcp_recv(int32_t s, void *buf, int32_t len, int32_t timeoutMs);
HOST_IMPORT(tcp_send) int32_t host_tcp_send(int32_t s, const void *buf, int32_t len, int32_t timeoutMs);
HOST_IMPORT(tcp_close) void host_tcp_close(int32_t s);
HOST_IMPORT(sleep_ms) void host_sleep_ms(int32_t ms);
}

namespace plat {

bool net_init() {
    return true;
}

Socket tcp_listen(uint16_t port) {
    return host_tcp_listen(port);
}

Socket tcp_accept(Socket listener, std::string &peerIp, int timeoutMs) {
    char ip[64] = {0};
    int32_t s   = host_tcp_accept((int32_t) listener, ip, sizeof(ip) - 1, timeoutMs);
    if (s >= 0) {
        peerIp = ip;
    }
    return s;
}

int tcp_recv(Socket s, void *buf, size_t len, int timeoutMs) {
    return host_tcp_recv((int32_t) s, buf, (int32_t) (len > 0x100000 ? 0x100000 : len), timeoutMs);
}

int tcp_send(Socket s, const void *buf, size_t len, int timeoutMs) {
    return host_tcp_send((int32_t) s, buf, (int32_t) (len > 0x100000 ? 0x100000 : len), timeoutMs);
}

void tcp_close(Socket s) {
    if (s >= 0) {
        host_tcp_close((int32_t) s);
    }
}

uint64_t now_ms() {
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t) ts.tv_sec * 1000 + (uint64_t) ts.tv_nsec / 1000000;
}

void sleep_ms(int ms) {
    host_sleep_ms(ms);
}

uint64_t fast_ticks() {
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t) ts.tv_sec * 1000000000ull + (uint64_t) ts.tv_nsec;
}

void tune_thread(std::thread &, const char *) {
}

void log(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    fflush(stderr);
}

} // namespace plat
