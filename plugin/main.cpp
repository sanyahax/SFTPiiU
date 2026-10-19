// SFTPiiU: SSH/SFTP server as an Aroma plugin.

#include "crypto/bytes.h"
#include "crypto/curve25519.h"
#include "crypto/random.h"
#include "platform.h"
#include "selftest.h"
#include "server.h"
#include "util/identity.h"
#include "wiiu/console.h"

#include <mocha/mocha.h>
#include <nn/ac.h>
#include <notifications/notifications.h>
#include <wups.h>
#include <wups/config/WUPSConfigCategory.h>
#include <wups/config/WUPSConfigItemBoolean.h>
#include <wups/config/WUPSConfigItemMultipleValues.h>
#include <wups/config/WUPSConfigItemStub.h>
#include <wups/config_api.h>

#include <atomic>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>

#define VERSION "v1.3.0"

WUPS_PLUGIN_NAME("SFTPiiU");
WUPS_PLUGIN_DESCRIPTION("SFTP-Server (SSH) mit Passwort- und Schluessel-Login");
WUPS_PLUGIN_VERSION(VERSION);
WUPS_PLUGIN_AUTHOR("sanyahax - ajk.dev");
WUPS_PLUGIN_LICENSE("MIT");

WUPS_USE_WUT_DEVOPTAB();
WUPS_USE_STORAGE("sftpiiu");

namespace {

// Storage keys. The SFTPiiU app reads the same keys from the plugin's
// config file and switches "enabled" there, so keep them stable.
const char kKeyEnabled[]     = "enabled";
const char kKeySystem[]      = "systemAccess";
const char kKeySlcWritable[] = "slcWritable";
const char kKeyPasswordOn[]  = "passwordLogin";
const char kKeyPort[]        = "port";
const char kKeyPassword[]    = "password";
const char kKeyHostKey[]     = "hostKeySeed";
const char kKeyRngSeed[]     = "rngSeed";
const char kKeyLoadedBoot[]  = "loadedBoot"; // tells the app the plugin runs this session

struct Settings {
    bool enabled      = true;
    bool systemAccess = true;
    bool slcWritable  = false;
    bool passwordOn   = true;
    int32_t port      = 22;
    std::string password;
};

Settings gSettings;
Settings gAppliedSettings;
uint8_t gHostSecret[64];
uint8_t gHostPublic[32];
bool gHostKeyReady           = false;
const char *gSelftestFailure = nullptr;
bool gNotificationsReady     = false;

std::unique_ptr<SshServer> gServer;
bool gMochaReady = false;
// Set when the SFTPiiU app took over the port for the current application.
std::atomic<bool> gYielded{false};

void notify(const char *text, bool error) {
    if (!gNotificationsReady) {
        return;
    }
    if (error) {
        NotificationModule_AddErrorNotification(text);
    } else {
        NotificationModule_AddInfoNotification(text);
    }
}

void on_login(const char *ip, const char *method) {
    char buf[96];
    snprintf(buf, sizeof(buf), "SFTP: Login von %s (%s)", ip, strcmp(method, "password") == 0 ? "Passwort" : "Schluessel");
    notify(buf, false);
}

void on_auth_failure(const char *ip, bool lockedOut) {
    char buf[96];
    if (lockedOut) {
        snprintf(buf, sizeof(buf), "SFTP: Zu viele Fehlversuche, Passwort-Login 5 Min. gesperrt");
    } else {
        snprintf(buf, sizeof(buf), "SFTP: Falsches Passwort von %s", ip);
    }
    notify(buf, true);
}

// Only the console itself may ask the plugin to step aside.
bool allow_yield(const char *ip) {
    if (strcmp(ip, "127.0.0.1") == 0) {
        return true;
    }
    std::string own = wiiu_own_ip();
    return !own.empty() && own == ip;
}

void on_yield() {
    gYielded = true;
}

template <typename T>
void load_or_default(const char *key, T &value, const T &def) {
    if (WUPSStorageAPI::GetOrStoreDefault(key, value, def) != WUPS_STORAGE_ERROR_SUCCESS) {
        value = def;
    }
}

template <typename T>
void reload_value(const char *key, T &value) {
    T tmp{};
    if (WUPSStorageAPI::Get(key, tmp) == WUPS_STORAGE_ERROR_SUCCESS) {
        value = tmp;
    }
}

// The SFTPiiU app edits the config file directly (its "keep running in the
// background" switch), so pick up changes before using the settings.
void reload_settings() {
    if (WUPSStorageAPI::ForceReloadStorage() != WUPS_STORAGE_ERROR_SUCCESS) {
        LOG("could not reload storage");
        return;
    }
    reload_value(kKeyEnabled, gSettings.enabled);
    reload_value(kKeySystem, gSettings.systemAccess);
    reload_value(kKeySlcWritable, gSettings.slcWritable);
    reload_value(kKeyPasswordOn, gSettings.passwordOn);
    reload_value(kKeyPort, gSettings.port);
    std::string password;
    if (WUPSStorageAPI::Get(kKeyPassword, password) == WUPS_STORAGE_ERROR_SUCCESS && !password.empty()) {
        gSettings.password = password;
    }
}

// ---------------------------------------------------------------- server

void stop_server() {
    if (gServer) {
        gServer->stop();
        gServer.reset();
        LOG("server stopped");
    }
    wiiu_unmount_all();
}

void start_server() {
    if (gServer || !gSettings.enabled || gYielded) {
        return;
    }
    if (gSelftestFailure) {
        LOG("not starting: crypto selftest failed (%s)", gSelftestFailure);
        return;
    }
    if (!gHostKeyReady) {
        LOG("not starting: no host key");
        return;
    }

    Vfs vfs;
    wiiu_mount_all(vfs, gMochaReady, gSettings.systemAccess, gSettings.slcWritable);

    ServerSettings s;
    s.port               = (uint16_t) gSettings.port;
    s.passwordAuth       = gSettings.passwordOn;
    s.password           = gSettings.password;
    s.authorizedKeysPath = kAuthorizedKeys;
    memcpy(s.hostPublic, gHostPublic, sizeof(gHostPublic));
    memcpy(s.hostSecret, gHostSecret, sizeof(gHostSecret));
    s.onLogin       = on_login;
    s.onAuthFailure = on_auth_failure;
    s.allowYield    = allow_yield;
    s.onYield       = on_yield;

    gServer = std::make_unique<SshServer>(s, std::move(vfs));
    crypto::secure_zero(s.hostSecret, sizeof(s.hostSecret));
    if (!gServer->start()) {
        gServer.reset();
        wiiu_unmount_all();
        notify("SFTP: Server konnte nicht starten (Port belegt?)", true);
        return;
    }
    gAppliedSettings = gSettings;
}

bool settings_need_restart() {
    const Settings &a = gSettings, &b = gAppliedSettings;
    return a.systemAccess != b.systemAccess || a.slcWritable != b.slcWritable || a.passwordOn != b.passwordOn ||
           a.port != b.port || a.password != b.password;
}

// ---------------------------------------------------------------- config menu

void on_enabled(ConfigItemBoolean *, bool v) {
    gSettings.enabled = v;
    WUPSStorageAPI::Store(kKeyEnabled, v);
}

void on_system(ConfigItemBoolean *, bool v) {
    gSettings.systemAccess = v;
    WUPSStorageAPI::Store(kKeySystem, v);
}

void on_slc(ConfigItemBoolean *, bool v) {
    gSettings.slcWritable = v;
    WUPSStorageAPI::Store(kKeySlcWritable, v);
}

void on_password_on(ConfigItemBoolean *, bool v) {
    gSettings.passwordOn = v;
    WUPSStorageAPI::Store(kKeyPasswordOn, v);
}

void on_port(ConfigItemMultipleValues *, uint32_t v) {
    gSettings.port = (int32_t) v;
    WUPSStorageAPI::Store(kKeyPort, gSettings.port);
}

// Custom item: shows the password, A generates a new one.
int32_t pw_display(void *, char *out, int32_t size) {
    snprintf(out, size, "  %s", gSettings.password.c_str());
    return 0;
}

int32_t pw_display_selected(void *, char *out, int32_t size) {
    snprintf(out, size, "(A: neu)  %s", gSettings.password.c_str());
    return 0;
}

void pw_input(void *, WUPSConfigSimplePadData input) {
    if (input.buttons_d & WUPS_CONFIG_BUTTON_A) {
        gSettings.password = util::generate_password();
        WUPSStorageAPI::Store(kKeyPassword, gSettings.password);
    }
}

void add_password_item(WUPSConfigCategoryHandle root) {
    WUPSConfigAPIItemCallbacksV2 cb = {
            .getCurrentValueDisplay         = &pw_display,
            .getCurrentValueSelectedDisplay = &pw_display_selected,
            .onSelected                     = nullptr,
            .restoreDefault                 = nullptr,
            .isMovementAllowed              = nullptr,
            .onCloseCallback                = nullptr,
            .onInput                        = &pw_input,
            .onInputEx                      = nullptr,
            .onDelete                       = nullptr,
    };
    WUPSConfigAPIItemOptionsV2 opts = {
            .displayName = "Passwort",
            .context     = nullptr,
            .callbacks   = cb,
    };
    WUPSConfigItemHandle handle;
    if (WUPSConfigAPI_Item_Create(opts, &handle) == WUPSCONFIG_API_RESULT_SUCCESS) {
        if (WUPSConfigAPI_Category_AddItem(root, handle) != WUPSCONFIG_API_RESULT_SUCCESS) {
            WUPSConfigAPI_Item_Destroy(handle);
        }
    }
}

WUPSConfigAPICallbackStatus menu_opened(WUPSConfigCategoryHandle rootHandle) {
    // Show (and later save) what the app may have changed meanwhile.
    reload_settings();
    std::string ip = wiiu_own_ip();
    try {
        WUPSConfigCategory root(rootHandle);
        root.add(WUPSConfigItemBoolean::CreateEx(kKeyEnabled, "SFTP-Server", true, gSettings.enabled, &on_enabled, "an", "aus"));
        root.add(WUPSConfigItemBoolean::CreateEx(kKeySystem, "Zugriff auf MLC / SLC / USB", true, gSettings.systemAccess, &on_system, "ja", "nein"));
        root.add(WUPSConfigItemBoolean::CreateEx(kKeySlcWritable, "SLC beschreibbar (Brick-Gefahr!)", false, gSettings.slcWritable, &on_slc, "ja", "nein"));
        root.add(WUPSConfigItemBoolean::CreateEx(kKeyPasswordOn, "Passwort-Login", true, gSettings.passwordOn, &on_password_on, "erlaubt", "aus"));

        static ConfigItemMultipleValuesPair ports[] = {{22, "22"}, {2222, "2222"}, {8022, "8022"}};
        int current = 0;
        for (int i = 0; i < 3; i++) {
            if ((int32_t) ports[i].value == gSettings.port) {
                current = i;
            }
        }
        WUPSConfigItemMultipleValues_AddToCategory(rootHandle, kKeyPort, "Port", 0, current, ports, 3, &on_port);

        add_password_item(rootHandle);

        root.add(WUPSConfigItemStub::Create("---"));
        char line[96];
        if (!ip.empty()) {
            snprintf(line, sizeof(line), "Adresse: sftp://%s:%d  (Benutzer beliebig)", ip.c_str(), (int) gSettings.port);
        } else {
            snprintf(line, sizeof(line), "Konsole ist nicht mit einem Netzwerk verbunden");
        }
        root.add(WUPSConfigItemStub::Create(line));
        if (gSelftestFailure) {
            snprintf(line, sizeof(line), "FEHLER: Krypto-Selbsttest fehlgeschlagen (%s)", gSelftestFailure);
            root.add(WUPSConfigItemStub::Create(line));
        } else if (gHostKeyReady) {
            root.add(WUPSConfigItemStub::Create("Host-Key " + util::host_fingerprint(gHostPublic)));
        }
        root.add(WUPSConfigItemStub::Create("Schluessel: SD:/wiiu/sftpiiu/authorized_keys (ssh-ed25519)"));
        if (gYielded) {
            root.add(WUPSConfigItemStub::Create("Pausiert: die SFTPiiU-App betreibt gerade den Server"));
        }
        if (gSettings.systemAccess && !gMochaReady) {
            root.add(WUPSConfigItemStub::Create("Hinweis: MochaPayload fehlt, nur SD verfuegbar"));
        }
        root.add(WUPSConfigItemStub::Create("SFTPiiU " VERSION " - made by sanyahax (github @sanyahax) - ajk.dev"));
    } catch (std::exception &e) {
        LOG("config menu: %s", e.what());
        return WUPSCONFIG_API_CALLBACK_RESULT_ERROR;
    }
    return WUPSCONFIG_API_CALLBACK_RESULT_SUCCESS;
}

void menu_closed() {
    WUPSStorageAPI::SaveStorage();
    if (gYielded) {
        return; // the app owns the port until it exits
    }
    if (!gSettings.enabled) {
        stop_server();
    } else if (!gServer) {
        start_server();
    } else if (settings_need_restart()) {
        stop_server();
        start_server();
    }
}

} // namespace

INITIALIZE_PLUGIN() {
    WUPSConfigAPIOptionsV1 configOptions = {.name = "SFTPiiU"};
    if (WUPSConfigAPI_Init(configOptions, menu_opened, menu_closed) != WUPSCONFIG_API_RESULT_SUCCESS) {
        LOG("failed to init config api");
    }

    gNotificationsReady = NotificationModule_InitLibrary() == NOTIFICATION_MODULE_RESULT_SUCCESS;

    load_or_default(kKeyEnabled, gSettings.enabled, true);
    load_or_default(kKeySystem, gSettings.systemAccess, true);
    load_or_default(kKeySlcWritable, gSettings.slcWritable, false);
    load_or_default(kKeyPasswordOn, gSettings.passwordOn, true);
    load_or_default(kKeyPort, gSettings.port, (int32_t) 22);

    gSelftestFailure = crypto_selftest();
    if (gSelftestFailure) {
        LOG("crypto selftest FAILED: %s", gSelftestFailure);
        notify("SFTPiiU: Krypto-Selbsttest fehlgeschlagen, Server bleibt aus", true);
    }

    bool mocha = Mocha_InitLibrary() == MOCHA_RESULT_SUCCESS;
    std::string storedSeed;
    WUPSStorageAPI::Get(kKeyRngSeed, storedSeed);
    WUPSStorageAPI::Store(kKeyRngSeed, wiiu_seed_rng(mocha, storedSeed));
    if (mocha) {
        Mocha_DeInitLibrary();
    }

    std::string seedHex;
    uint8_t seed[32];
    if (WUPSStorageAPI::Get(kKeyHostKey, seedHex) != WUPS_STORAGE_ERROR_SUCCESS || !util::from_hex(seedHex, seed, sizeof(seed))) {
        crypto::rng_bytes(seed, sizeof(seed));
        WUPSStorageAPI::Store(kKeyHostKey, util::to_hex(seed, sizeof(seed)));
        LOG("generated new host key");
    }
    crypto::ed25519_keypair_from_seed(seed, gHostPublic, gHostSecret);
    crypto::secure_zero(seed, sizeof(seed));
    gHostKeyReady = true;

    if (WUPSStorageAPI::Get(kKeyPassword, gSettings.password) != WUPS_STORAGE_ERROR_SUCCESS ||
        gSettings.password.empty()) {
        gSettings.password = util::generate_password();
        WUPSStorageAPI::Store(kKeyPassword, gSettings.password);
    }
    WUPSStorageAPI::Store(kKeyLoadedBoot, std::to_string(wiiu_boot_time()));

    WUPSStorageAPI::SaveStorage();
    LOG("initialized, host key %s", util::host_fingerprint(gHostPublic).c_str());
}

DEINITIALIZE_PLUGIN() {
    stop_server();
    crypto::secure_zero(gHostSecret, sizeof(gHostSecret));
    if (gNotificationsReady) {
        NotificationModule_DeInitLibrary();
    }
}

ON_APPLICATION_START() {
    nn::ac::Initialize();
    nn::ac::ConnectAsync();
    gYielded    = false;
    gMochaReady = Mocha_InitLibrary() == MOCHA_RESULT_SUCCESS;
    reload_settings();
    start_server();
}

ON_APPLICATION_ENDS() {
    stop_server();
    gYielded = false;
    if (gMochaReady) {
        Mocha_DeInitLibrary();
        gMochaReady = false;
    }
}
