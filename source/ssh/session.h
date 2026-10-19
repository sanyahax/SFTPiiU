#pragma once
// One SSH-2 connection: transport (RFC 4253) with curve25519-sha256 and
// ssh-ed25519, user authentication (RFC 4252) by password or ed25519 key,
// and a single "session" channel (RFC 4254) running the sftp subsystem.

#include "../crypto/aes.h"
#include "../crypto/chacha.h"
#include "../crypto/sha2.h"
#include "../monitor.h"
#include "../platform.h"
#include "../sftp.h"
#include "buffer.h"

#include <memory>
#include <string>
#include <vector>

class SshServer;

class SshSession {
public:
    SshSession(SshServer &server, plat::Socket sock, std::string peer);
    ~SshSession();

    void run();

private:
    enum class Cipher { None, ChaChaPoly, AesCtr };

    struct Direction {
        Cipher cipher = Cipher::None;
        crypto::ChaChaPolySsh chacha;
        crypto::AesCtr aes;
        crypto::HmacSha256Key mac;
        uint32_t seq = 0;
    };

    struct Channel {
        bool open         = false;
        bool eofReceived  = false;
        bool closeSent    = false;
        uint32_t remoteId = 0;
        uint64_t remoteWindow  = 0;
        uint32_t remoteMaxPkt  = 0;
        uint64_t localWindow   = 0;
        uint64_t consumed      = 0;
    };

    // transport
    bool fill(size_t need);
    bool readPacket(std::vector<uint8_t> &payload, uint32_t &seq);
    bool sendPacket(const uint8_t *payload, size_t len);
    bool sendPacket(const ssh::Writer &w) { return sendPacket(w.data(), w.size()); }
    bool sendAll(const uint8_t *data, size_t len);
    bool disconnect(uint32_t reason, const char *why);
    bool exchangeVersions();
    bool sendKexInit();
    bool runKex(const std::vector<uint8_t> &clientKexInit);
    void deriveKeys(Direction &d, const std::string &cipher, const ssh::Writer &K, const uint8_t H[32],
                    char ivLetter, char keyLetter, char macLetter);

    // dispatch
    bool handlePacket(std::vector<uint8_t> &pkt, uint32_t seq);
    bool handleAuth(ssh::Reader &r);
    bool authFailure();
    bool authSuccess(const char *method);
    bool keyAuthorized(const uint8_t key[32]);
    bool handleChannelOpen(ssh::Reader &r);
    bool handleChannelRequest(ssh::Reader &r);
    bool closeChannel();
    bool sendChannelData(const uint8_t *data, size_t len);
    bool pumpSftp();
    bool interruptibleSleep(int ms);

    SshServer &server_;
    plat::Socket sock_;
    std::string peer_;
    Monitor *mon_   = nullptr;
    uint32_t monId_ = 0;

    std::vector<uint8_t> in_;
    size_t inPos_ = 0;
    size_t inLen_ = 0;
    std::vector<uint8_t> out_;
    uint64_t lastActivity_ = 0;
    uint64_t authDeadline_ = 0;

    Direction rx_, tx_;
    std::string clientVersion_;
    std::vector<uint8_t> serverKexInit_;
    std::vector<uint8_t> sessionId_;
    bool kexDone_     = false;
    bool kexInitSent_ = false;
    bool strictKex_   = false;

    bool userauthService_ = false;
    bool authed_          = false;
    int authAttempts_     = 0;
    int passwordFailures_ = 0;

    Channel chan_;
    std::unique_ptr<SftpSession> sftp_;
    std::vector<uint8_t> sftpIn_;
    size_t sftpPos_ = 0;
    ssh::Writer reply_;
    std::vector<uint8_t> dataPkt_;
};
