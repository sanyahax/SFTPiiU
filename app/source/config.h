#pragma once
// Settings and identity for the app. If the SFTPiiU plugin has been set up,
// the app reuses its host key, password and options (read from the plugin's
// config file), so clients see the same server either way. Otherwise the app
// keeps its own identity in SD:/wiiu/sftpiiu/app.cfg. The app's own UI
// settings live in that file in both cases.

#include <cstdint>
#include <string>

struct AppConfig {
    std::string environment; // Aroma environment, e.g. "fs:/vol/external01/wiiu/environments/aroma"
    bool fromPlugin   = false;
    uint16_t port     = 22;
    bool passwordOn   = true;
    bool systemAccess = true;
    bool slcWritable  = false;
    std::string password;
    std::string hostKeyHex; // 32-byte ed25519 seed
    std::string rngSeedHex; // refreshed every start

    // App UI settings, always stored in app.cfg.
    bool fileActions = false; // delete/rename/copy/move/save in the file browser
    bool showHidden  = false; // show dot files
    int language     = -1;    // -1: follow the console, else a Lang value
};

// Loads the plugin's settings if present, then the app's own file.
AppConfig load_config(bool mochaAvailable);
// Writes the app's own file (identity only when not taken from the plugin).
bool save_config(const AppConfig &cfg);

// The plugin's config file in the Aroma environment.
std::string plugin_config_path(const AppConfig &cfg);
// Whole file as text (up to 256 KB), empty if it cannot be read.
std::string read_text_file(const char *path);
