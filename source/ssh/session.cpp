#include "session.h"
#include "../crypto/bytes.h"
#include "../crypto/curve25519.h"
#include "../crypto/random.h"
#include "../server.h"
#include "../util/base64.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>

using crypto::load_be32;
using crypto::store_be32;

namespace {

enum : uint8_t {
    MSG_DISCONNECT                = 1,
    MSG_IGNORE                    = 2,
    MSG_UNIMPLEMENTED             = 3,
    MSG_DEBUG                     = 4,
    MSG_SERVICE_REQUEST           = 5,
    MSG_SERVICE_ACCEPT            = 6,
    MSG_KEXINIT                   = 20,
    MSG_NEWKEYS                   = 21,
    MSG_KEX_ECDH_INIT             = 30,
    MSG_KEX_ECDH_REPLY            = 31,
    MSG_USERAUTH_REQUEST          = 50,
    MSG_USERAUTH_FAILURE          = 51,
    MSG_USERAUTH_SUCCESS          = 52,
    MSG_USERAUTH_PK_OK            = 60,
    MSG_GLOBAL_REQUEST            = 80,
    MSG_REQUEST_SUCCESS           = 81,
    MSG_REQUEST_FAILURE           = 82,
    MSG_CHANNEL_OPEN              = 90,
    MSG_CHANNEL_OPEN_CONFIRMATION = 91,
    MSG_CHANNEL_OPEN_FAILURE      = 92,
    MSG_CHANNEL_WINDOW_ADJUST     = 93,
    MSG_CHANNEL_DATA              = 94,
    MSG_CHANNEL_EXTENDED_DATA     = 95,
    MSG_CHANNEL_EOF               = 96,
    MSG_CHANNEL_CLOSE             = 97,
    MSG_CHANNEL_REQUEST           = 98,
    MSG_CHANNEL_SUCCESS           = 99,
    MSG_CHANNEL_FAILURE           = 100,
};

enum : uint32_t {
    DISC_PROTOCOL_ERROR        = 2,
    DISC_KEY_EXCHANGE_FAILED   = 3,
    DISC_MAC_ERROR             = 5,
    DISC_SERVICE_NOT_AVAILABLE = 7,
    DISC_BY_APPLICATION        = 11,
    DISC_NO_MORE_AUTH_METHODS  = 14,
};

enum : uint32_t {
    OPEN_ADMINISTRATIVELY_PROHIBITED = 1,
    OPEN_UNKNOWN_CHANNEL_TYPE        = 3,
    OPEN_RESOURCE_SHORTAGE           = 4,
};

const char kServerVersion[] = "SSH-2.0-SFTPiiU_1.0";
const char kKexAlgs[]       = "curve25519-sha256,curve25519-sha256@libssh.org";
const char kStrictKexS[]    = "kex-strict-s-v00@openssh.com";
const char kStrictKexC[]    = "kex-strict-c-v00@openssh.com";
const char kHostKeyAlg[]    = "ssh-ed25519";
const char kCiphers[]       = "chacha20-poly1305@openssh.com,aes128-ctr,aes192-ctr,aes256-ctr";
const char kMacs[]          = "hmac-sha2-256";
const char kChaCha[]        = "chacha20-poly1305@openssh.com";

constexpr uint32_t kMaxPacket       = 256 * 1024;
constexpr uint32_t kLocalWindow     = 256 * 1024;
constexpr uint32_t kLocalMaxPacket  = 64 * 1024;
constexpr uint64_t kPreAuthTimeout  = 60 * 1000;
constexpr uint64_t kIdleTimeout     = 30 * 60 * 1000;
constexpr int kMaxAuthAttempts      = 20;
constexpr int kMaxPasswordFailures  = 5;
constexpr int kPasswordFailDelayMs  = 1500;

std::string_view first_name(std::string_view list) {
    size_t c = list.find(',');
    return c == std::string_view::npos ? list : list.substr(0, c);
}

} // namespace

SshSession::SshSession(SshServer &server, plat::Socket sock, std::string peer)
    : server_(server), sock_(sock), peer_(std::move(peer)), mon_(server.settings().monitor) {
    in_.resize(64 * 1024);
}

SshSession::~SshSession() {
    sftp_.reset();
    if (sock_ != plat::INVALID_SOCKET_) {
        plat::tcp_close(sock_);
    }
}

// ---------------------------------------------------------------- transport

bool SshSession::fill(size_t need) {
    while (inLen_ - inPos_ < need) {
        if (server_.stopping()) {
            return false;
        }
        uint64_t now = plat::now_ms();
        if (!authed_ && now > authDeadline_) {
            LOG("[%s] login timeout", peer_.c_str());
            return false;
        }
        if (now - lastActivity_ > kIdleTimeout) {
            LOG("[%s] idle timeout", peer_.c_str());
            return false;
        }
        if (inPos_ == inLen_) {
            inPos_ = inLen_ = 0;
        }
        if (in_.size() - inPos_ < need || inLen_ == in_.size()) {
            if (inPos_) {
                memmove(in_.data(), in_.data() + inPos_, inLen_ - inPos_);
                inLen_ -= inPos_;
                inPos_ = 0;
            }
            if (in_.size() < need || inLen_ == in_.size()) {
                in_.resize(std::max(need, in_.size()) + 16 * 1024);
            }
        }
        int n = plat::tcp_recv(sock_, in_.data() + inLen_, in_.size() - inLen_, 250);
        if (n < 0) {
            return false;
        }
        if (n > 0) {
            inLen_ += (size_t) n;
            lastActivity_ = now;
        }
    }
    return true;
}

bool SshSession::readPacket(std::vector<uint8_t> &payload, uint32_t &seq) {
    seq          = rx_.seq;
    size_t first = rx_.cipher == Cipher::AesCtr ? 16 : 4;
    if (!fill(first)) {
        return false;
    }
    uint8_t *p = in_.data() + inPos_;
    uint32_t plen;
    size_t macLen = 0;
    switch (rx_.cipher) {
        case Cipher::None:
            plen = load_be32(p);
            break;
        case Cipher::ChaChaPoly:
            plen   = rx_.chacha.decrypt_length(rx_.seq, p);
            macLen = 16;
            break;
        case Cipher::AesCtr:
        default:
            rx_.aes.crypt(p, 16);
            plen   = load_be32(p);
            macLen = 32;
            break;
    }
    if (plen < 5 || plen > kMaxPacket || (rx_.cipher == Cipher::AesCtr && (4 + plen) % 16 != 0)) {
        return disconnect(DISC_PROTOCOL_ERROR, "bad packet length");
    }
    size_t total = 4 + (size_t) plen + macLen;
    if (!fill(total)) {
        return false;
    }
    p = in_.data() + inPos_;
    if (rx_.cipher == Cipher::ChaChaPoly) {
        if (!rx_.chacha.open(rx_.seq, p, 4 + plen, p + 4 + plen)) {
            return disconnect(DISC_MAC_ERROR, "message authentication failed");
        }
    } else if (rx_.cipher == Cipher::AesCtr) {
        rx_.aes.crypt(p + 16, 4 + plen - 16);
        uint8_t seqbuf[4], mac[32];
        store_be32(seqbuf, rx_.seq);
        rx_.mac.mac(seqbuf, 4, p, 4 + plen, mac);
        if (!crypto::ct_equal(mac, p + 4 + plen, 32)) {
            return disconnect(DISC_MAC_ERROR, "message authentication failed");
        }
    }
    uint32_t pad = p[4];
    if (pad < 4 || pad + 1 >= plen) {
        return disconnect(DISC_PROTOCOL_ERROR, "bad padding");
    }
    size_t n = plen - pad - 1;
    payload.assign(p + 5, p + 5 + n);
    inPos_ += total;
    rx_.seq++;
    return true;
}

bool SshSession::sendPacket(const uint8_t *payload, size_t n) {
    size_t block = tx_.cipher == Cipher::AesCtr ? 16 : 8;
    size_t padlen;
    if (tx_.cipher == Cipher::ChaChaPoly) {
        // Length is authenticated, not encrypted as part of the blocks.
        padlen = block - ((1 + n) % block);
    } else {
        padlen = block - ((5 + n) % block);
    }
    if (padlen < 4) {
        padlen += block;
    }
    size_t plen   = 1 + n + padlen;
    size_t macLen = tx_.cipher == Cipher::ChaChaPoly ? 16 : (tx_.cipher == Cipher::AesCtr ? 32 : 0);
    out_.resize(4 + plen + macLen);
    uint8_t *p = out_.data();
    store_be32(p, (uint32_t) plen);
    p[4] = (uint8_t) padlen;
    memcpy(p + 5, payload, n);
    crypto::rng_bytes(p + 5 + n, padlen);
    if (tx_.cipher == Cipher::ChaChaPoly) {
        tx_.chacha.seal(tx_.seq, p, 4 + plen, p + 4 + plen);
    } else if (tx_.cipher == Cipher::AesCtr) {
        uint8_t seqbuf[4];
        store_be32(seqbuf, tx_.seq);
        tx_.mac.mac(seqbuf, 4, p, 4 + plen, p + 4 + plen);
        tx_.aes.crypt(p, 4 + plen);
    }
    tx_.seq++;
    return sendAll(p, out_.size());
}

bool SshSession::sendAll(const uint8_t *data, size_t len) {
    uint64_t lastProgress = plat::now_ms();
    while (len) {
        int n = plat::tcp_send(sock_, data, len, 250);
        if (n < 0) {
            return false;
        }
        uint64_t now = plat::now_ms();
        if (n == 0) {
            // Only give up on a stalled socket, so a final DISCONNECT still
            // goes out while the server is shutting down.
            if (server_.stopping() || now - lastProgress > 60 * 1000) {
                LOG("[%s] send timeout", peer_.c_str());
                return false;
            }
            continue;
        }
        data += n;
        len -= (size_t) n;
        lastProgress = now;
    }
    return true;
}

bool SshSession::disconnect(uint32_t reason, const char *why) {
    LOG("[%s] disconnect: %s", peer_.c_str(), why);
    ssh::Writer w;
    w.u8(MSG_DISCONNECT);
    w.u32(reason);
    w.str(why);
    w.str("");
    sendPacket(w);
    return false;
}

bool SshSession::exchangeVersions() {
    std::string ours = std::string(kServerVersion) + "\r\n";
    if (!sendAll((const uint8_t *) ours.data(), ours.size())) {
        return false;
    }
    // RFC 4253 4.2: the client may not send other lines first, but be lenient.
    for (int lines = 0; lines < 20; lines++) {
        std::string line;
        for (;;) {
            if (!fill(1)) {
                return false;
            }
            char c = (char) in_[inPos_++];
            if (c == '\n') {
                break;
            }
            if (line.size() > 255) {
                return false;
            }
            line += c;
        }
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line == kYieldLine) {
            const ServerSettings &cfg = server_.settings();
            if (cfg.allowYield && cfg.allowYield(peer_.c_str())) {
                LOG("[%s] stepping aside for the SFTPiiU app", peer_.c_str());
                server_.requestStop();
                if (cfg.onYield) {
                    cfg.onYield();
                }
            }
            return false;
        }
        if (line.rfind("SSH-", 0) == 0) {
            if (line.rfind("SSH-2.0-", 0) != 0 && line.rfind("SSH-1.99-", 0) != 0) {
                LOG("[%s] unsupported protocol: %s", peer_.c_str(), line.c_str());
                return false;
            }
            clientVersion_ = line;
            LOG("[%s] client %s", peer_.c_str(), line.c_str());
            if (mon_) {
                // "SSH-2.0-OpenSSH_9.5 extra" -> "OpenSSH_9.5"
                size_t dash = line.find('-', 4);
                std::string name = dash == std::string::npos ? line : line.substr(dash + 1);
                mon_->sessionClient(monId_, name.substr(0, name.find(' ')));
            }
            return true;
        }
    }
    return false;
}

bool SshSession::sendKexInit() {
    ssh::Writer w;
    w.u8(MSG_KEXINIT);
    uint8_t cookie[16];
    crypto::rng_bytes(cookie, sizeof(cookie));
    w.raw(cookie, sizeof(cookie));
    std::string kex = kKexAlgs;
    if (!kexDone_) {
        kex += ",";
        kex += kStrictKexS;
    }
    w.str(kex);
    w.str(kHostKeyAlg);
    w.str(kCiphers);
    w.str(kCiphers);
    w.str(kMacs);
    w.str(kMacs);
    w.str("none");
    w.str("none");
    w.str("");
    w.str("");
    w.boolean(false);
    w.u32(0);
    serverKexInit_ = w.buf;
    kexInitSent_   = true;
    return sendPacket(w);
}

void SshSession::deriveKeys(Direction &d, const std::string &cipher, const ssh::Writer &K, const uint8_t H[32],
                            char ivLetter, char keyLetter, char macLetter) {
    auto derive = [&](char letter, size_t need) {
        std::vector<uint8_t> out;
        uint8_t blk[32];
        crypto::Sha256 h;
        h.update(K.data(), K.size());
        h.update(H, 32);
        h.update(&letter, 1);
        h.update(sessionId_.data(), sessionId_.size());
        h.final(blk);
        out.insert(out.end(), blk, blk + 32);
        while (out.size() < need) {
            crypto::Sha256 h2;
            h2.update(K.data(), K.size());
            h2.update(H, 32);
            h2.update(out.data(), out.size());
            h2.final(blk);
            out.insert(out.end(), blk, blk + 32);
        }
        crypto::secure_zero(blk, sizeof(blk));
        out.resize(need);
        return out;
    };

    if (cipher == kChaCha) {
        auto key = derive(keyLetter, 64);
        d.chacha.set_key(key.data());
        d.cipher = Cipher::ChaChaPoly;
        crypto::secure_zero(key.data(), key.size());
    } else {
        size_t keyLen = cipher == "aes128-ctr" ? 16 : (cipher == "aes192-ctr" ? 24 : 32);
        auto key      = derive(keyLetter, keyLen);
        auto iv       = derive(ivLetter, 16);
        auto mac      = derive(macLetter, 32);
        d.aes.set_key(key.data(), keyLen, iv.data());
        d.mac.set_key(mac.data(), mac.size());
        d.cipher = Cipher::AesCtr;
        crypto::secure_zero(key.data(), key.size());
        crypto::secure_zero(mac.data(), mac.size());
    }
}

bool SshSession::runKex(const std::vector<uint8_t> &ic) {
    ssh::Reader r(ic.data(), ic.size());
    r.u8();
    r.bytes(16);
    std::string_view kexList  = r.str();
    std::string_view hostList = r.str();
    std::string_view encC2S   = r.str();
    std::string_view encS2C   = r.str();
    std::string_view macC2S   = r.str();
    std::string_view macS2C   = r.str();
    std::string_view compC2S  = r.str();
    std::string_view compS2C  = r.str();
    r.str();
    r.str();
    bool guessFollows = r.boolean();
    r.u32();
    if (!r.ok) {
        return disconnect(DISC_PROTOCOL_ERROR, "malformed KEXINIT");
    }

    bool initial = !kexDone_;
    if (initial) {
        strictKex_ = ssh::namelist_has(kexList, kStrictKexC);
    }

    std::string kexAlg  = ssh::namelist_pick(kexList, kKexAlgs);
    std::string hostAlg = ssh::namelist_pick(hostList, kHostKeyAlg);
    std::string encIn   = ssh::namelist_pick(encC2S, kCiphers);
    std::string encOut  = ssh::namelist_pick(encS2C, kCiphers);
    std::string macIn   = ssh::namelist_pick(macC2S, kMacs);
    std::string macOut  = ssh::namelist_pick(macS2C, kMacs);
    bool compOk         = ssh::namelist_has(compC2S, "none") && ssh::namelist_has(compS2C, "none");
    if (kexAlg.empty() || hostAlg.empty() || encIn.empty() || encOut.empty() || !compOk ||
        (encIn != kChaCha && macIn.empty()) || (encOut != kChaCha && macOut.empty())) {
        LOG("[%s] no common algorithms (kex=%s host=%s enc=%s/%s)", peer_.c_str(), kexAlg.c_str(),
            hostAlg.c_str(), encIn.c_str(), encOut.c_str());
        return disconnect(DISC_KEY_EXCHANGE_FAILED, "no matching algorithms");
    }

    if (!kexInitSent_ && !sendKexInit()) {
        return false;
    }

    bool skipGuess = guessFollows && (first_name(kexList) != kexAlg || first_name(hostList) != hostAlg);
    bool lenient   = !(strictKex_ && initial);

    std::vector<uint8_t> pkt;
    uint32_t seq;
    for (;;) {
        if (!readPacket(pkt, seq)) {
            return false;
        }
        uint8_t t = pkt[0];
        if (t == MSG_KEX_ECDH_INIT) {
            if (skipGuess) {
                skipGuess = false;
                continue;
            }
            break;
        }
        if (t == MSG_DISCONNECT) {
            return false;
        }
        if (lenient && (t == MSG_IGNORE || t == MSG_DEBUG || t == MSG_UNIMPLEMENTED)) {
            continue;
        }
        return disconnect(DISC_PROTOCOL_ERROR, "unexpected message during key exchange");
    }

    ssh::Reader er(pkt.data(), pkt.size());
    er.u8();
    std::string_view qc = er.str();
    if (!er.ok || qc.size() != 32) {
        return disconnect(DISC_KEY_EXCHANGE_FAILED, "bad ECDH public key");
    }

    uint64_t ticks = plat::fast_ticks();
    crypto::rng_add_entropy(&ticks, sizeof(ticks));
    uint8_t eph[32], qs[32], shared[32];
    crypto::rng_bytes(eph, sizeof(eph));
    crypto::x25519_base(qs, eph);
    bool okShared = crypto::x25519(shared, eph, ssh::u8p(qc));
    crypto::secure_zero(eph, sizeof(eph));
    if (!okShared) {
        return disconnect(DISC_KEY_EXCHANGE_FAILED, "invalid ECDH public key");
    }

    const ServerSettings &cfg = server_.settings();
    ssh::Writer hostKey;
    hostKey.str(kHostKeyAlg);
    hostKey.str(cfg.hostPublic, 32);

    ssh::Writer K;
    K.mpint(shared, 32);
    crypto::secure_zero(shared, sizeof(shared));

    ssh::Writer hw;
    hw.str(clientVersion_);
    hw.str(kServerVersion);
    hw.str(ic.data(), ic.size());
    hw.str(serverKexInit_.data(), serverKexInit_.size());
    hw.str(hostKey.data(), hostKey.size());
    hw.str(qc.data(), qc.size());
    hw.str(qs, 32);
    hw.raw(K.data(), K.size());
    uint8_t H[32];
    crypto::sha256(hw.data(), hw.size(), H);
    if (initial) {
        sessionId_.assign(H, H + 32);
    }

    uint8_t sig[64];
    crypto::ed25519_sign(sig, H, 32, cfg.hostSecret);
    ssh::Writer sigBlob;
    sigBlob.str(kHostKeyAlg);
    sigBlob.str(sig, 64);

    ssh::Writer reply;
    reply.u8(MSG_KEX_ECDH_REPLY);
    reply.str(hostKey.data(), hostKey.size());
    reply.str(qs, 32);
    reply.str(sigBlob.data(), sigBlob.size());
    if (!sendPacket(reply)) {
        return false;
    }

    Direction newTx, newRx;
    deriveKeys(newTx, encOut, K, H, 'B', 'D', 'F');
    deriveKeys(newRx, encIn, K, H, 'A', 'C', 'E');
    crypto::secure_zero(K.buf.data(), K.buf.size());

    ssh::Writer nk;
    nk.u8(MSG_NEWKEYS);
    if (!sendPacket(nk)) {
        return false;
    }
    uint32_t txSeq = tx_.seq;
    tx_            = newTx;
    tx_.seq        = strictKex_ ? 0 : txSeq;

    for (;;) {
        if (!readPacket(pkt, seq)) {
            return false;
        }
        uint8_t t = pkt[0];
        if (t == MSG_NEWKEYS) {
            break;
        }
        if (t == MSG_DISCONNECT) {
            return false;
        }
        if (lenient && (t == MSG_IGNORE || t == MSG_DEBUG || t == MSG_UNIMPLEMENTED)) {
            continue;
        }
        return disconnect(DISC_PROTOCOL_ERROR, "expected NEWKEYS");
    }
    uint32_t rxSeq = rx_.seq;
    rx_            = newRx;
    rx_.seq        = strictKex_ ? 0 : rxSeq;

    kexDone_     = true;
    kexInitSent_ = false;
    if (initial && mon_) {
        mon_->sessionCipher(monId_, encOut);
    }
    LOG("[%s] %s %s, cipher %s%s", peer_.c_str(), initial ? "keys established" : "rekeyed", kexAlg.c_str(),
        encOut.c_str(), strictKex_ ? ", strict kex" : "");
    return true;
}

// ---------------------------------------------------------------- dispatch

bool SshSession::interruptibleSleep(int ms) {
    while (ms > 0) {
        if (server_.stopping()) {
            return false;
        }
        int step = ms < 100 ? ms : 100;
        plat::sleep_ms(step);
        ms -= step;
    }
    return true;
}

bool SshSession::handlePacket(std::vector<uint8_t> &pkt, uint32_t seq) {
    ssh::Reader r(pkt.data(), pkt.size());
    uint8_t type = r.u8();

    switch (type) {
        case MSG_DISCONNECT:
            LOG("[%s] client disconnected", peer_.c_str());
            return false;
        case MSG_IGNORE:
        case MSG_DEBUG:
        case MSG_UNIMPLEMENTED:
            return true;
        case MSG_KEXINIT:
            return runKex(pkt);
        case MSG_SERVICE_REQUEST: {
            std::string_view name = r.str();
            if (!r.ok || name != "ssh-userauth") {
                return disconnect(DISC_SERVICE_NOT_AVAILABLE, "service not available");
            }
            userauthService_ = true;
            ssh::Writer w;
            w.u8(MSG_SERVICE_ACCEPT);
            w.str(name);
            return sendPacket(w);
        }
        case MSG_USERAUTH_REQUEST:
            if (!userauthService_) {
                return disconnect(DISC_PROTOCOL_ERROR, "ssh-userauth not requested");
            }
            if (authed_) {
                return true; // RFC 4252 5.1: ignore further requests
            }
            return handleAuth(r);
        default:
            break;
    }

    if (!authed_) {
        return disconnect(DISC_PROTOCOL_ERROR, "not authenticated");
    }

    switch (type) {
        case MSG_GLOBAL_REQUEST: {
            r.str();
            bool want = r.boolean();
            if (want) {
                ssh::Writer w;
                w.u8(MSG_REQUEST_FAILURE);
                return sendPacket(w);
            }
            return true;
        }
        case MSG_REQUEST_SUCCESS:
        case MSG_REQUEST_FAILURE:
        case MSG_CHANNEL_SUCCESS:
        case MSG_CHANNEL_FAILURE:
            return true;
        case MSG_CHANNEL_OPEN:
            return handleChannelOpen(r);
        case MSG_CHANNEL_REQUEST:
            return handleChannelRequest(r);
        case MSG_CHANNEL_WINDOW_ADJUST: {
            r.u32();
            uint32_t add = r.u32();
            if (!r.ok) {
                return disconnect(DISC_PROTOCOL_ERROR, "malformed window adjust");
            }
            if (chan_.open) {
                chan_.remoteWindow = std::min<uint64_t>(chan_.remoteWindow + add, 0xffffffffu);
            }
            return true;
        }
        case MSG_CHANNEL_DATA:
        case MSG_CHANNEL_EXTENDED_DATA: {
            r.u32();
            if (type == MSG_CHANNEL_EXTENDED_DATA) {
                r.u32();
            }
            std::string_view data = r.str();
            if (!r.ok) {
                return disconnect(DISC_PROTOCOL_ERROR, "malformed channel data");
            }
            if (!chan_.open || chan_.closeSent) {
                return true;
            }
            chan_.localWindow -= std::min<uint64_t>(chan_.localWindow, data.size());
            if (sftp_ && type == MSG_CHANNEL_DATA) {
                if (sftpIn_.size() - sftpPos_ + data.size() > kLocalWindow + SftpSession::kMaxRequest) {
                    return disconnect(DISC_PROTOCOL_ERROR, "channel window exceeded");
                }
                sftpIn_.insert(sftpIn_.end(), data.begin(), data.end());
            } else {
                chan_.consumed += data.size();
            }
            return true;
        }
        case MSG_CHANNEL_EOF:
            if (chan_.open) {
                chan_.eofReceived = true;
            }
            return true;
        case MSG_CHANNEL_CLOSE:
            if (chan_.open) {
                if (!chan_.closeSent) {
                    ssh::Writer w;
                    w.u8(MSG_CHANNEL_CLOSE);
                    w.u32(chan_.remoteId);
                    if (!sendPacket(w)) {
                        return false;
                    }
                }
                chan_ = Channel();
                sftp_.reset();
                sftpIn_.clear();
                sftpPos_ = 0;
            }
            return true;
        default: {
            ssh::Writer w;
            w.u8(MSG_UNIMPLEMENTED);
            w.u32(seq);
            return sendPacket(w);
        }
    }
}

bool SshSession::authFailure() {
    if (++authAttempts_ > kMaxAuthAttempts) {
        return disconnect(DISC_NO_MORE_AUTH_METHODS, "too many authentication attempts");
    }
    ssh::Writer w;
    w.u8(MSG_USERAUTH_FAILURE);
    w.str(server_.settings().passwordAuth ? "publickey,password" : "publickey");
    w.boolean(false);
    return sendPacket(w);
}

bool SshSession::authSuccess(const char *method) {
    ssh::Writer w;
    w.u8(MSG_USERAUTH_SUCCESS);
    if (!sendPacket(w)) {
        return false;
    }
    authed_ = true;
    LOG("[%s] logged in (%s)", peer_.c_str(), method);
    if (mon_) {
        mon_->sessionAuthed(monId_, method);
    }
    if (server_.settings().onLogin) {
        server_.settings().onLogin(peer_.c_str(), method);
    }
    return true;
}

bool SshSession::keyAuthorized(const uint8_t key[32]) {
    const std::string &path = server_.settings().authorizedKeysPath;
    if (path.empty()) {
        return false;
    }
    FILE *f = fopen(path.c_str(), "rb");
    if (!f) {
        return false;
    }
    bool found = false;
    char line[4096];
    std::vector<uint8_t> blob;
    while (!found && fgets(line, sizeof(line), f)) {
        std::string_view l(line);
        size_t first = l.find_first_not_of(" \t");
        if (first == std::string_view::npos || l[first] == '#') {
            continue;
        }
        size_t at = l.find("ssh-ed25519 ");
        if (at == std::string_view::npos) {
            continue;
        }
        size_t b = at + 12;
        while (b < l.size() && (l[b] == ' ' || l[b] == '\t')) {
            b++;
        }
        size_t e = b;
        while (e < l.size() && !isspace((unsigned char) l[e])) {
            e++;
        }
        if (!util::base64_decode(l.substr(b, e - b), blob)) {
            continue;
        }
        ssh::Reader kr(blob.data(), blob.size());
        std::string_view type = kr.str();
        std::string_view pk   = kr.str();
        if (kr.ok && type == kHostKeyAlg && pk.size() == 32 && memcmp(pk.data(), key, 32) == 0) {
            found = true;
        }
    }
    fclose(f);
    return found;
}

bool SshSession::handleAuth(ssh::Reader &r) {
    std::string_view user    = r.str();
    std::string_view service = r.str();
    std::string_view method  = r.str();
    if (!r.ok) {
        return disconnect(DISC_PROTOCOL_ERROR, "malformed auth request");
    }
    if (service != "ssh-connection") {
        return authFailure();
    }
    const ServerSettings &cfg = server_.settings();

    if (method == "password") {
        bool change         = r.boolean();
        std::string_view pw = r.str();
        if (!r.ok || change || !cfg.passwordAuth || cfg.password.empty()) {
            return authFailure();
        }
        if (server_.passwordLoginBlocked()) {
            LOG("[%s] password login blocked (too many failures)", peer_.c_str());
            if (!interruptibleSleep(kPasswordFailDelayMs)) {
                return false;
            }
            return authFailure();
        }
        bool ok = pw.size() == cfg.password.size() &&
                  crypto::ct_equal(ssh::u8p(pw), (const uint8_t *) cfg.password.data(), pw.size());
        if (ok) {
            server_.notePasswordSuccess();
            return authSuccess("password");
        }
        bool locked = server_.notePasswordFailure();
        LOG("[%s] wrong password for '%.*s'", peer_.c_str(), (int) user.size(), user.data());
        if (cfg.onAuthFailure) {
            cfg.onAuthFailure(peer_.c_str(), locked);
        }
        if (mon_) {
            mon_->event(locked ? EventKind::LockedOut : EventKind::WrongPassword, peer_);
        }
        if (!interruptibleSleep(kPasswordFailDelayMs)) {
            return false;
        }
        if (++passwordFailures_ >= kMaxPasswordFailures) {
            return disconnect(DISC_NO_MORE_AUTH_METHODS, "too many wrong passwords");
        }
        return authFailure();
    }

    if (method == "publickey") {
        bool hasSig           = r.boolean();
        std::string_view alg  = r.str();
        std::string_view blob = r.str();
        if (!r.ok || alg != kHostKeyAlg) {
            return authFailure();
        }
        ssh::Reader br(ssh::u8p(blob), blob.size());
        std::string_view type = br.str();
        std::string_view pk   = br.str();
        if (!br.ok || type != kHostKeyAlg || pk.size() != 32 || !keyAuthorized(ssh::u8p(pk))) {
            return authFailure();
        }
        if (!hasSig) {
            ssh::Writer w;
            w.u8(MSG_USERAUTH_PK_OK);
            w.str(alg);
            w.str(blob);
            return sendPacket(w);
        }
        std::string_view sigBlob = r.str();
        ssh::Reader sr(ssh::u8p(sigBlob), sigBlob.size());
        std::string_view sigType = sr.str();
        std::string_view sig     = sr.str();
        if (!r.ok || !sr.ok || sigType != kHostKeyAlg || sig.size() != 64) {
            return authFailure();
        }
        ssh::Writer signedData;
        signedData.str(sessionId_.data(), sessionId_.size());
        signedData.u8(MSG_USERAUTH_REQUEST);
        signedData.str(user);
        signedData.str(service);
        signedData.str("publickey");
        signedData.boolean(true);
        signedData.str(alg);
        signedData.str(blob);
        if (!crypto::ed25519_verify(ssh::u8p(sig), signedData.data(), signedData.size(), ssh::u8p(pk))) {
            LOG("[%s] bad key signature", peer_.c_str());
            return authFailure();
        }
        return authSuccess("publickey");
    }

    // "none", "keyboard-interactive", ...: tell the client what we accept.
    return authFailure();
}

bool SshSession::handleChannelOpen(ssh::Reader &r) {
    std::string_view type = r.str();
    uint32_t sender       = r.u32();
    uint32_t window       = r.u32();
    uint32_t maxPkt       = r.u32();
    if (!r.ok) {
        return disconnect(DISC_PROTOCOL_ERROR, "malformed channel open");
    }
    if (type != "session" || chan_.open) {
        ssh::Writer w;
        w.u8(MSG_CHANNEL_OPEN_FAILURE);
        w.u32(sender);
        w.u32(type != "session" ? OPEN_UNKNOWN_CHANNEL_TYPE : OPEN_RESOURCE_SHORTAGE);
        w.str(type != "session" ? "unsupported channel type" : "only one channel per connection");
        w.str("");
        return sendPacket(w);
    }
    chan_              = Channel();
    chan_.open         = true;
    chan_.remoteId     = sender;
    chan_.remoteWindow = window;
    chan_.remoteMaxPkt = std::max<uint32_t>(std::min<uint32_t>(maxPkt, kLocalMaxPacket), 512);
    chan_.localWindow  = kLocalWindow;

    ssh::Writer w;
    w.u8(MSG_CHANNEL_OPEN_CONFIRMATION);
    w.u32(sender);
    w.u32(0);
    w.u32(kLocalWindow);
    w.u32(kLocalMaxPacket);
    return sendPacket(w);
}

bool SshSession::handleChannelRequest(ssh::Reader &r) {
    r.u32();
    std::string_view type = r.str();
    bool want             = r.boolean();
    if (!r.ok) {
        return disconnect(DISC_PROTOCOL_ERROR, "malformed channel request");
    }
    if (!chan_.open || chan_.closeSent) {
        return true;
    }
    bool ok = false;
    if (type == "subsystem") {
        std::string_view name = r.str();
        ok                    = r.ok && name == "sftp" && !sftp_;
    } else if (type == "exec") {
        // Some clients fall back to running the server binary directly.
        std::string_view cmd = r.str();
        ok                   = r.ok && cmd.find("sftp-server") != std::string_view::npos && !sftp_;
    }
    if (ok) {
        sftp_ = std::make_unique<SftpSession>(server_.vfs(), mon_, monId_);
        sftpIn_.clear();
        sftpPos_ = 0;
        LOG("[%s] sftp started", peer_.c_str());
    }
    if (want) {
        ssh::Writer w;
        w.u8(ok ? MSG_CHANNEL_SUCCESS : MSG_CHANNEL_FAILURE);
        w.u32(chan_.remoteId);
        return sendPacket(w);
    }
    return true;
}

bool SshSession::closeChannel() {
    if (chan_.open && !chan_.closeSent) {
        ssh::Writer w;
        w.u8(MSG_CHANNEL_CLOSE);
        w.u32(chan_.remoteId);
        chan_.closeSent = true;
        return sendPacket(w);
    }
    return true;
}

bool SshSession::sendChannelData(const uint8_t *data, size_t len) {
    while (len) {
        if (!chan_.open || chan_.closeSent) {
            return true;
        }
        if (chan_.remoteWindow == 0) {
            // Wait for WINDOW_ADJUST; meanwhile queue whatever else arrives.
            std::vector<uint8_t> pkt;
            uint32_t seq;
            if (!readPacket(pkt, seq) || !handlePacket(pkt, seq)) {
                return false;
            }
            continue;
        }
        size_t chunk = std::min<uint64_t>({(uint64_t) len, chan_.remoteWindow, (uint64_t) chan_.remoteMaxPkt});
        dataPkt_.resize(9 + chunk);
        dataPkt_[0] = MSG_CHANNEL_DATA;
        store_be32(&dataPkt_[1], chan_.remoteId);
        store_be32(&dataPkt_[5], (uint32_t) chunk);
        memcpy(&dataPkt_[9], data, chunk);
        if (!sendPacket(dataPkt_.data(), dataPkt_.size())) {
            return false;
        }
        chan_.remoteWindow -= chunk;
        data += chunk;
        len -= chunk;
    }
    return true;
}

bool SshSession::pumpSftp() {
    while (sftp_ && chan_.open && !chan_.closeSent) {
        size_t avail = sftpIn_.size() - sftpPos_;
        if (avail < 4) {
            break;
        }
        uint32_t len = load_be32(sftpIn_.data() + sftpPos_);
        if (len == 0 || len > SftpSession::kMaxRequest) {
            LOG("[%s] bad sftp packet length %u", peer_.c_str(), (unsigned) len);
            return closeChannel();
        }
        if (avail < 4 + (size_t) len) {
            break;
        }
        reply_.clear();
        bool ok = sftp_->handle(sftpIn_.data() + sftpPos_ + 4, len, reply_);
        sftpPos_ += 4 + (size_t) len;
        chan_.consumed += 4 + (size_t) len;
        if (!ok) {
            LOG("[%s] malformed sftp request", peer_.c_str());
            return closeChannel();
        }
        if (!sendChannelData(reply_.data(), reply_.size())) {
            return false;
        }
    }

    if (sftpPos_ && sftpPos_ == sftpIn_.size()) {
        sftpIn_.clear();
        sftpPos_ = 0;
    } else if (sftpPos_ > 64 * 1024) {
        sftpIn_.erase(sftpIn_.begin(), sftpIn_.begin() + (ptrdiff_t) sftpPos_);
        sftpPos_ = 0;
    }

    if (chan_.open && !chan_.closeSent && chan_.consumed >= kLocalWindow / 2) {
        ssh::Writer w;
        w.u8(MSG_CHANNEL_WINDOW_ADJUST);
        w.u32(chan_.remoteId);
        w.u32((uint32_t) chan_.consumed);
        chan_.localWindow += chan_.consumed;
        chan_.consumed = 0;
        if (!sendPacket(w)) {
            return false;
        }
    }

    if (chan_.open && chan_.eofReceived && !chan_.closeSent) {
        // Like sshd after sftp-server exits: report success, then EOF and
        // CLOSE. Without exit-status, scp treats the transfer as failed.
        ssh::Writer status;
        status.u8(MSG_CHANNEL_REQUEST);
        status.u32(chan_.remoteId);
        status.str("exit-status");
        status.boolean(false);
        status.u32(0);
        ssh::Writer eof;
        eof.u8(MSG_CHANNEL_EOF);
        eof.u32(chan_.remoteId);
        if (!sendPacket(status) || !sendPacket(eof)) {
            return false;
        }
        return closeChannel();
    }
    return true;
}

void SshSession::run() {
    lastActivity_ = plat::now_ms();
    authDeadline_ = lastActivity_ + kPreAuthTimeout;
    LOG("[%s] connected", peer_.c_str());
    if (mon_) {
        monId_ = mon_->sessionOpened(peer_);
    }

    uint64_t ticks = plat::fast_ticks();
    crypto::rng_add_entropy(&ticks, sizeof(ticks));
    crypto::rng_add_entropy(peer_.data(), peer_.size());

    if (exchangeVersions() && sendKexInit()) {
        std::vector<uint8_t> pkt;
        uint32_t seq;
        while (readPacket(pkt, seq)) {
            if (!kexDone_ && pkt[0] != MSG_KEXINIT) {
                disconnect(DISC_PROTOCOL_ERROR, "expected KEXINIT");
                break;
            }
            if (!handlePacket(pkt, seq) || !pumpSftp()) {
                break;
            }
        }
    }
    if (server_.stopping() && kexDone_) {
        disconnect(DISC_BY_APPLICATION, "server shutting down");
    }
    sftp_.reset();
    plat::tcp_close(sock_);
    sock_ = plat::INVALID_SOCKET_;
    LOG("[%s] closed", peer_.c_str());
    if (mon_) {
        mon_->sessionClosed(monId_);
    }
}
