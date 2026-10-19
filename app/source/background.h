#pragma once
// "Keep running in the background". A Wii U app ends when you leave it, so
// the server can only keep running through the SFTPiiU Aroma plugin, which
// Aroma loads into every application. The app carries a copy of the plugin,
// installs it into the Aroma environment and switches it on or off through
// the plugin's config file; the plugin re-reads that file whenever an
// application starts.

#include "config.h"

#include <cstddef>
#include <cstdint>
#include <string>

namespace background {

struct Status {
    bool installed = false; // an SFTPiiU .wps is in the plugins folder
    bool enabled   = false; // the plugin's "SFTP server" setting
    bool loaded    = false; // Aroma loaded the plugin in this console session
    bool on() const { return installed && enabled; }
};

// pluginAnswered: the plugin answered the app's request to step aside, so it
// is loaded for sure (also for plugin versions that leave no boot marker).
Status query(const AppConfig &cfg, bool pluginAnswered);

// Installs the bundled plugin (or updates an older copy) and switches it on.
// If the plugin has no config yet, it gets the app's identity, so clients see
// the same host key and password either way. written tells whether the .wps
// file was (re)written, which only takes effect after a console restart.
// errno-style result.
int enable(const AppConfig &cfg, const uint8_t *plugin, size_t pluginSize, bool &written);
// Switches the plugin off; it stops when the next application starts.
int disable(const AppConfig &cfg);

} // namespace background
