@echo off
rem Builds the PC test version of the SSH/SFTP core with zig (pip install ziglang).
setlocal
cd /d "%~dp0"
python -m ziglang c++ -std=gnu++20 -O2 -Wall -Wextra -Wno-unused-parameter -D_FILE_OFFSET_BITS=64 ^
  -target x86_64-windows-gnu ^
  ..\source\crypto\sha2.cpp ..\source\crypto\curve25519.cpp ..\source\crypto\chacha.cpp ^
  ..\source\crypto\aes.cpp ..\source\crypto\random.cpp ..\source\util\base64.cpp ^
  ..\source\util\identity.cpp ..\source\vfs.cpp ..\source\sftp.cpp ..\source\server.cpp ^
  ..\source\monitor.cpp ..\source\ssh\session.cpp ^
  ..\source\selftest.cpp platform_host.cpp main_host.cpp ^
  -lws2_32 -lbcrypt -o sftpiiu_host.exe
