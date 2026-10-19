#pragma once
// Force-included (-include) in the WebAssembly test build only. wasm32-wasi
// has no threads, so libc++ omits std::mutex and std::thread. These stand-ins
// make the server run single-threaded: a "thread" executes its function
// immediately, so connections are served one after another.

#include <functional>
#include <mutex>
#include <thread>
#include <utility>

namespace std {

struct mutex {
    void lock() {}
    void unlock() {}
    bool try_lock() { return true; }
};

class thread {
public:
    thread() = default;
    template <class F, class... A>
    explicit thread(F &&f, A &&...a) {
        std::invoke(std::forward<F>(f), std::forward<A>(a)...);
    }
    thread(thread &&)            = default;
    thread &operator=(thread &&) = default;
    bool joinable() const { return false; }
    void join() {}
};

} // namespace std
