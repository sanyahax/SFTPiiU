// SFTPiiU app: the SFTP server with a Wii U styled dashboard on TV and GamePad.

#include "background.h"
#include "config.h"
#include "crypto/bytes.h"
#include "crypto/curve25519.h"
#include "crypto/random.h"
#include "i18n.h"
#include "monitor.h"
#include "platform.h"
#include "selftest.h"
#include "server.h"
#include "ui.h"
#include "util/identity.h"
#include "wiiu/console.h"

#include <mocha/mocha.h>
#include <nn/ac.h>
#include <sysapp/launch.h>

#include <arpa/inet.h>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <memory>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

// The plugin, built from ../plugin and linked in by the Makefile (bin2s), for
// "keep running in the background".
extern "C" const uint8_t SFTPiiU_wps[];
extern "C" const uint8_t SFTPiiU_wps_end[];

namespace {

// If the SFTPiiU plugin is running, it already holds the port in this very
// process. Ask it to step aside (it only accepts this from the console
// itself), then wait until the port is free. True if the plugin answered.
bool ask_plugin_to_yield(uint16_t port) {
    std::string targets[2] = {"127.0.0.1", wiiu_own_ip()};
    for (const auto &ip : targets) {
        if (ip.empty()) {
            continue;
        }
        int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (fd < 0) {
            continue;
        }
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port   = htons(port);
        inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);
        // Non-blocking connect with a short timeout, so a dropped SYN can
        // never stall the app.
        fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
        bool connected = connect(fd, (sockaddr *) &addr, sizeof(addr)) == 0;
        if (!connected && errno == EINPROGRESS) {
            pollfd p{fd, POLLOUT, 0};
            int err       = -1;
            socklen_t len = sizeof(err);
            connected     = poll(&p, 1, 500) > 0 && getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &len) == 0 && err == 0;
        }
        if (connected) {
            std::string line = std::string(kYieldLine) + "\r\n";
            send(fd, line.data(), line.size(), 0);
            // Read the banner until the plugin hangs up.
            char buf[128];
            for (int i = 0; i < 20; i++) {
                pollfd p{fd, POLLIN, 0};
                if (poll(&p, 1, 100) > 0 && recv(fd, buf, sizeof(buf), 0) <= 0) {
                    break;
                }
            }
            close(fd);
            LOG("asked plugin on %s:%u to step aside", ip.c_str(), (unsigned) port);
            return true;
        }
        close(fd);
    }
    return false;
}

std::unique_ptr<SshServer> start_server(const ServerSettings &s, const Vfs &vfs) {
    for (int attempt = 0; attempt < 30; attempt++) {
        auto server = std::make_unique<SshServer>(s, vfs);
        if (server->start()) {
            return server;
        }
        plat::sleep_ms(100);
    }
    return nullptr;
}

// Number of ssh-ed25519 lines in authorized_keys, -1 if the file is missing.
int count_keys() {
    FILE *f = fopen(kAuthorizedKeys, "rb");
    if (!f) {
        return -1;
    }
    int n = 0;
    char line[4096];
    while (fgets(line, sizeof(line), f)) {
        const char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p != '#' && strstr(p, "ssh-ed25519 ")) {
            n++;
        }
    }
    fclose(f);
    return n;
}

#ifdef SFTPIIU_UI_TOUR
// Test builds only: lets the PC drive the UI by dropping command lines into
// SD:/sftpiiu_tour.txt (in Cemu that is a plain folder on the host).
void poll_tour(Ui &ui, std::vector<UiAction> &actions) {
    const char *path = "fs:/vol/external01/sftpiiu_tour.txt";
    FILE *f          = fopen(path, "rb");
    if (!f) {
        return;
    }
    char line[256];
    std::vector<std::string> cmds;
    while (fgets(line, sizeof(line), f)) {
        std::string s = line;
        while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
        if (!s.empty()) cmds.push_back(s);
    }
    fclose(f);
    remove(path);
    for (const auto &c : cmds) {
        LOG("tour: %s", c.c_str());
        ui.command(c, actions);
    }
}
#endif

} // namespace

int main(int, char **) {
    UiSettings uiSettings;
    Ui ui(uiSettings);
    if (!ui.init()) {
        ui.shutdown();
        return 0;
    }
    if (int missing = i18n_missing()) {
        LOG("i18n: %d missing translations", missing);
    }

    const int kSteps = 8;
    int stepNo       = 0;
    auto step        = [&](const char *what) {
        LOG("startup: %s", what);
        ui.splash((float) (++stepNo) / kSteps);
    };

    step("network");
    nn::ac::Initialize();
    nn::ac::ConnectAsync();
    step("mocha");
    bool mocha = Mocha_InitLibrary() == MOCHA_RESULT_SUCCESS;

    step("settings");
    AppConfig cfg          = load_config(mocha);
    uiSettings.fileActions = cfg.fileActions;
    uiSettings.showHidden  = cfg.showHidden;
    uiSettings.language    = cfg.language;
    ui.applyLanguage();
    step("rng");
    cfg.rngSeedHex = wiiu_seed_rng(mocha, cfg.rngSeedHex);
    step("selftest");
    const char *selftestFailure = crypto_selftest();

    uint8_t seed[32];
    if (!util::from_hex(cfg.hostKeyHex, seed, sizeof(seed))) {
        crypto::rng_bytes(seed, sizeof(seed));
        cfg.hostKeyHex = util::to_hex(seed, sizeof(seed));
    }
    if (cfg.password.empty()) {
        cfg.password = util::generate_password();
    }

    Monitor monitor;
    ServerSettings settings;
    settings.port               = cfg.port;
    settings.passwordAuth       = cfg.passwordOn;
    settings.password           = cfg.password;
    settings.authorizedKeysPath = kAuthorizedKeys;
    settings.monitor            = &monitor;
    crypto::ed25519_keypair_from_seed(seed, settings.hostPublic, settings.hostSecret);
    crypto::secure_zero(seed, sizeof(seed));

    step("drives");
    Vfs vfs;
    wiiu_mount_all(vfs, mocha, cfg.systemAccess, cfg.slcWritable);
    if (!save_config(cfg)) {
        LOG("could not write app.cfg (SD card missing?)");
    }

    std::unique_ptr<SshServer> server;
    enum { kNoError, kSelftestError, kPortError } error = kNoError;
    bool pluginAnswered                                  = false;
    if (selftestFailure) {
        error = kSelftestError;
    } else {
        step("plugin");
        pluginAnswered = ask_plugin_to_yield(cfg.port);
        step("server");
        server = start_server(settings, vfs);
        if (server) {
            monitor.event(EventKind::ServerStarted, std::to_string(cfg.port));
        } else {
            error = kPortError;
        }
    }

    AppState st;
    st.port           = cfg.port;
    st.password       = cfg.password;
    st.passwordLogin  = cfg.passwordOn;
    st.canNewPassword = !cfg.fromPlugin;
    st.fromPlugin     = cfg.fromPlugin;
    st.fingerprint    = util::host_fingerprint(settings.hostPublic);
    st.mocha          = mocha;
    st.vfs            = &vfs;
    st.serverStartMs  = plat::now_ms();
    st.keyCount       = count_keys();
    st.bg             = background::query(cfg, pluginAnswered);
    LOG("background plugin: installed=%d enabled=%d loaded=%d (boot %lld)", st.bg.installed, st.bg.enabled,
        st.bg.loaded, (long long) wiiu_boot_time());
    // A plugin file written in this session only runs after a restart.
    bool pluginWritten = false;

    bool quitting     = false;
    uint64_t lastSnap = 0, lastSlow = 0, lastKeys = plat::now_ms(), lastBg = plat::now_ms();
    std::vector<UiAction> actions;

    for (;;) {
        uint64_t now = plat::now_ms();
        if (now - lastSlow >= 1000) {
            st.ip    = wiiu_own_ip();
            lastSlow = now;
        }
        if (now - lastKeys >= 10000) {
            st.keyCount = count_keys();
            lastKeys    = now;
        }
        // The plugin can also be switched in the Aroma menu meanwhile.
        if (ui.onSettingsPage() && now - lastBg >= 3000) {
            st.bg        = background::query(cfg, pluginAnswered);
            st.bg.loaded = st.bg.loaded && !pluginWritten;
            lastBg       = now;
        }
        if (now - lastSnap >= 100) {
            st.snap    = monitor.snapshot();
            st.running = server != nullptr;
            lastSnap   = now;
        }
        // Rebuilt every frame so it follows the UI language.
        if (error == kSelftestError) {
            st.error = trf(S_ERR_SELFTEST, selftestFailure);
        } else if (error == kPortError) {
            st.error = trf(S_ERR_PORT, std::to_string(cfg.port));
        }

#ifdef SFTPIIU_UI_TOUR
        static uint64_t lastTour = 0;
        if (now - lastTour >= 300) {
            poll_tour(ui, actions);
            lastTour = now;
        }
#endif
        if (!ui.frame(st, actions)) {
            break;
        }
        for (UiAction a : actions) {
            if (a == UiAction::Quit && !quitting) {
                quitting = true;
                SYSLaunchMenu(); // ProcUI then ends the loop via SDL_QUIT
            } else if (a == UiAction::NewPassword && server && st.canNewPassword) {
                cfg.password = util::generate_password();
                save_config(cfg);
                settings.password = cfg.password;
                server->stop();
                server      = start_server(settings, vfs);
                st.password = cfg.password;
                monitor.event(EventKind::NewPassword);
            } else if (a == UiAction::SettingsChanged) {
                cfg.fileActions = uiSettings.fileActions;
                cfg.showHidden  = uiSettings.showHidden;
                cfg.language    = uiSettings.language;
                save_config(cfg);
            } else if (a == UiAction::BackgroundOn || a == UiAction::BackgroundOff) {
                bool on      = a == UiAction::BackgroundOn;
                bool written = false;
                int err      = on ? background::enable(cfg, SFTPiiU_wps, (size_t) (SFTPiiU_wps_end - SFTPiiU_wps), written)
                                  : background::disable(cfg);
                pluginWritten = pluginWritten || written;
                st.bg         = background::query(cfg, pluginAnswered);
                st.bg.loaded  = st.bg.loaded && !pluginWritten;
                lastBg        = now;
                if (err) {
                    ui.notify(trf(S_ERR_FAILED, strerror(err)), true);
                } else {
                    ui.notify(tr(written ? S_BG_INSTALLED : on ? (st.bg.loaded ? S_BG_ACTIVE : S_BG_AFTER_REBOOT)
                                                               : S_BG_OFF));
                    // The plugin's config now holds the identity both use.
                    cfg.fromPlugin    = true;
                    st.fromPlugin     = true;
                    st.canNewPassword = false;
                }
            }
        }
        actions.clear();
    }

    if (server) {
        server->stop();
        server.reset();
    }
    wiiu_unmount_all();
    if (mocha) {
        Mocha_DeInitLibrary();
    }
    crypto::secure_zero(settings.hostSecret, sizeof(settings.hostSecret));
    ui.shutdown();
    return 0;
}
