@echo off
rem Builds the SSH/SFTP core as wasm32-wasi for testing with run_wasm.py.
setlocal
cd /d "%~dp0"
set S=..\..\source
python -m ziglang c++ -std=gnu++20 -O2 -Wall -Wextra -Wno-unused-parameter ^
  -target wasm32-wasi -include shim.h -fno-exceptions ^
  %S%\crypto\sha2.cpp %S%\crypto\curve25519.cpp %S%\crypto\chacha.cpp %S%\crypto\aes.cpp ^
  %S%\crypto\random.cpp %S%\util\base64.cpp %S%\util\identity.cpp %S%\vfs.cpp %S%\sftp.cpp ^
  %S%\server.cpp %S%\monitor.cpp ^
  %S%\ssh\session.cpp %S%\selftest.cpp platform_wasm.cpp ..\main_host.cpp ^
  -o sftpiiu_test.wasm
