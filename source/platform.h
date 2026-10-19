#pragma once
// Thin OS layer. Implemented in wiiu/platform_wiiu.cpp for the console and in
// host/platform_host.cpp for the PC test build.

#include <cstddef>
#include <cstdint>
#include <string>
#include <thread>

namespace plat {

using Socket                    = intptr_t;
constexpr Socket INVALID_SOCKET_ = -1;

bool net_init();
Socket tcp_listen(uint16_t port);
// Waits up to timeoutMs for a client. Returns INVALID_SOCKET_ on timeout/error.
Socket tcp_accept(Socket listener, std::string &peerIp, int timeoutMs);
// > 0: bytes received, 0: timeout, < 0: closed or failed.
int tcp_recv(Socket s, void *buf, size_t len, int timeoutMs);
// > 0: bytes sent, 0: timeout, < 0: failed.
int tcp_send(Socket s, const void *buf, size_t len, int timeoutMs);
void tcp_close(Socket s);

uint64_t now_ms();
void sleep_ms(int ms);
// Fast high-resolution counter, only used as an entropy source.
uint64_t fast_ticks();

// Applies platform-specific scheduling (core affinity, priority, name).
void tune_thread(std::thread &t, const char *name);

void log(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

} // namespace plat

#define LOG(...) plat::log(__VA_ARGS__)
