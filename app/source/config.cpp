#include "config.h"
#include "platform.h"
#include "pluginjson.h"

#include <mocha/mocha.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

const char kAppConfig[]         = "fs:/vol/external01/wiiu/sftpiiu/app.cfg";
const char kDefaultEnvironment[] = "fs:/vol/external01/wiiu/environments/aroma";

// Aroma's environment folder, e.g. "fs:/vol/external01/wiiu/environments/aroma".
std::string environment_path(bool mochaAvailable) {
    if (mochaAvailable) {
        char buf[0x100] = {0};
        if (Mocha_GetEnvironmentPath(buf, sizeof(buf)) == MOCHA_RESULT_SUCCESS) {
            std::string p = buf;
            size_t at     = p.find("/vol/external01");
            if (at != std::string::npos) {
                return "fs:" + p.substr(at);
            }
        }
    }
    return kDefaultEnvironment;
}

bool load_plugin_config(const std::string &path, AppConfig &c) {
    std::string js = read_text_file(path.c_str());
    if (js.empty()) {
        return false;
    }
    std::string password, hostKey, v;
    if (!pluginjson::find(js, "password", password) || !pluginjson::find(js, "hostKeySeed", hostKey) ||
        hostKey.size() != 64) {
        return false;
    }
    c.password   = password;
    c.hostKeyHex = hostKey;
    if (pluginjson::find(js, "port", v)) c.port = (uint16_t) atoi(v.c_str());
    if (pluginjson::find(js, "passwordLogin", v)) c.passwordOn = v == "true";
    if (pluginjson::find(js, "systemAccess", v)) c.systemAccess = v == "true";
    if (pluginjson::find(js, "slcWritable", v)) c.slcWritable = v == "true";
    c.fromPlugin = true;
    return true;
}

void load_app_file(AppConfig &c) {
    std::string data = read_text_file(kAppConfig);
    size_t start     = 0;
    while (start < data.size()) {
        size_t end = data.find('\n', start);
        if (end == std::string::npos) end = data.size();
        std::string line = data.substr(start, end - start);
        start            = end + 1;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        size_t eq = line.find('=');
        if (line.empty() || line[0] == '#' || eq == std::string::npos) continue;
        std::string k = line.substr(0, eq), v = line.substr(eq + 1);
        if (k == "rngSeed") {
            c.rngSeedHex = v;
        } else if (k == "fileActions") {
            c.fileActions = v == "1";
        } else if (k == "showHidden") {
            c.showHidden = v == "1";
        } else if (k == "language") {
            c.language = v == "auto" ? -1 : atoi(v.c_str());
            if (c.language < -1 || c.language > 11) c.language = -1;
        }
        if (c.fromPlugin) {
            continue; // the plugin's identity wins
        }
        if (k == "password") c.password = v;
        else if (k == "hostKeySeed") c.hostKeyHex = v;
        else if (k == "port") c.port = (uint16_t) atoi(v.c_str());
        else if (k == "passwordLogin") c.passwordOn = v == "1";
        else if (k == "systemAccess") c.systemAccess = v == "1";
        else if (k == "slcWritable") c.slcWritable = v == "1";
    }
}

} // namespace

std::string read_text_file(const char *path) {
    std::string data;
    FILE *f = fopen(path, "rb");
    if (!f) {
        return data;
    }
    char buf[1024];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0 && data.size() < 256 * 1024) {
        data.append(buf, n);
    }
    fclose(f);
    return data;
}

std::string plugin_config_path(const AppConfig &c) {
    return c.environment + "/plugins/config/sftpiiu.json";
}

AppConfig load_config(bool mochaAvailable) {
    AppConfig c;
    c.environment            = environment_path(mochaAvailable);
    std::string pluginConfig = plugin_config_path(c);
    if (load_plugin_config(pluginConfig, c)) {
        LOG("using identity from %s", pluginConfig.c_str());
    }
    load_app_file(c);
    if (c.port == 0) {
        c.port = 22;
    }
    return c;
}

bool save_config(const AppConfig &c) {
    FILE *f = fopen(kAppConfig, "wb");
    if (!f) {
        return false;
    }
    fprintf(f, "# SFTPiiU app settings. fileActions, showHidden and language always apply.\n");
    fprintf(f, "# Port, password, host key and access options are only used while the\n");
    fprintf(f, "# SFTPiiU plugin is not set up; otherwise the plugin's settings win.\n");
    fprintf(f, "rngSeed=%s\n", c.rngSeedHex.c_str());
    fprintf(f, "fileActions=%d\n", c.fileActions ? 1 : 0);
    fprintf(f, "showHidden=%d\n", c.showHidden ? 1 : 0);
    if (c.language < 0) {
        fprintf(f, "language=auto\n");
    } else {
        fprintf(f, "language=%d\n", c.language);
    }
    if (!c.fromPlugin) {
        fprintf(f, "port=%u\n", (unsigned) c.port);
        fprintf(f, "password=%s\n", c.password.c_str());
        fprintf(f, "passwordLogin=%d\n", c.passwordOn ? 1 : 0);
        fprintf(f, "systemAccess=%d\n", c.systemAccess ? 1 : 0);
        fprintf(f, "slcWritable=%d\n", c.slcWritable ? 1 : 0);
        fprintf(f, "hostKeySeed=%s\n", c.hostKeyHex.c_str());
    }
    fclose(f);
    return true;
}
