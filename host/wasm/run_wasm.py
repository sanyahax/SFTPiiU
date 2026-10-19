"""Runs sftpiiu_test.wasm under wasmtime, providing TCP sockets to the guest.

Usage: python run_wasm.py [--workdir DIR] -- <args for the server>
The work directory is mounted as "/" inside the guest; pass guest paths such
as --root /testroot --keys /authorized_keys.
"""

import argparse
import os
import select
import socket
import sys
import time

import wasmtime as wt

HERE = os.path.dirname(os.path.abspath(__file__))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--workdir", default=os.getcwd())
    ap.add_argument("--wasm", default=os.path.join(HERE, "sftpiiu_test.wasm"))
    ap.add_argument("rest", nargs=argparse.REMAINDER)
    a = ap.parse_args()
    guest_args = [x for x in a.rest if x != "--"]

    engine = wt.Engine()
    store = wt.Store(engine)
    wasi = wt.WasiConfig()
    wasi.argv = ["sftpiiu"] + guest_args
    wasi.preopen_dir(a.workdir, "/")
    wasi.inherit_stdout()
    wasi.inherit_stderr()
    store.set_wasi(wasi)

    linker = wt.Linker(engine)
    linker.define_wasi()

    socks = {}
    next_id = [1]

    def add(s):
        sid = next_id[0]
        next_id[0] += 1
        socks[sid] = s
        return sid

    i32 = wt.ValType.i32()

    def mem(caller):
        return caller["memory"]

    def tcp_listen(caller, port):
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        try:
            s.bind(("0.0.0.0", port))
            s.listen(4)
        except OSError as e:
            print("listen failed:", e, file=sys.stderr)
            s.close()
            return -1
        s.setblocking(False)
        return add(s)

    def tcp_accept(caller, ls, ip_ptr, ip_len, timeout):
        s = socks.get(ls)
        if s is None:
            return -1
        r, _, _ = select.select([s], [], [], timeout / 1000.0)
        if not r:
            return -1
        try:
            c, addr = s.accept()
        except BlockingIOError:
            return -1
        c.setblocking(False)
        c.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        ip = addr[0].encode()[: ip_len]
        mem(caller).write(caller, ip + b"\0", ip_ptr)
        return add(c)

    def tcp_recv(caller, sid, ptr, length, timeout):
        s = socks.get(sid)
        if s is None:
            return -1
        r, _, _ = select.select([s], [], [], timeout / 1000.0)
        if not r:
            return 0
        try:
            data = s.recv(length)
        except BlockingIOError:
            return 0
        except OSError:
            return -1
        if not data:
            return -1
        mem(caller).write(caller, data, ptr)
        return len(data)

    def tcp_send(caller, sid, ptr, length, timeout):
        s = socks.get(sid)
        if s is None:
            return -1
        _, w, _ = select.select([], [s], [], timeout / 1000.0)
        if not w:
            return 0
        data = bytes(mem(caller).read(caller, ptr, ptr + length))
        try:
            return s.send(data)
        except BlockingIOError:
            return 0
        except OSError:
            return -1

    def tcp_close(caller, sid):
        s = socks.pop(sid, None)
        if s is not None:
            s.close()

    def sleep_ms(caller, ms):
        time.sleep(ms / 1000.0)

    defs = [
        ("tcp_listen", [i32], [i32], tcp_listen),
        ("tcp_accept", [i32, i32, i32, i32], [i32], tcp_accept),
        ("tcp_recv", [i32, i32, i32, i32], [i32], tcp_recv),
        ("tcp_send", [i32, i32, i32, i32], [i32], tcp_send),
        ("tcp_close", [i32], [], tcp_close),
        ("sleep_ms", [i32], [], sleep_ms),
    ]
    for name, params, results, fn in defs:
        linker.define_func("host", name, wt.FuncType(params, results), fn, access_caller=True)

    module = wt.Module.from_file(engine, a.wasm)
    instance = linker.instantiate(store, module)
    try:
        instance.exports(store)["_start"](store)
    except wt.ExitTrap as e:
        sys.exit(e.code)


if __name__ == "__main__":
    main()
