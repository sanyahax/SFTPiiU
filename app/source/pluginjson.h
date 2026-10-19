#pragma once
// Reading and editing the SFTPiiU plugin's config file, the JSON that the
// Aroma plugin backend writes for WUPS storage:
//   {"storageitems": {"enabled": true, "password": "...", "port": 22, ...}}
// Only flat values are needed, so this is a small text scanner rather than a
// full JSON parser. Edits keep everything else in the file untouched.

#include "config.h"

#include <string>

namespace pluginjson {

// Value of a top-level key inside the file: strings unquoted, other values
// as written ("true", "22"). False if the key is missing.
bool find(const std::string &json, const char *key, std::string &out);

// Sets a boolean key, replacing its value or adding it to "storageitems".
// Returns an empty string if the text is not a storage file.
std::string set_bool(const std::string &json, const char *key, bool value);

// A complete config carrying the app's identity, for a plugin that has not
// run yet. The plugin adds its own keys (rngSeed, ...) when it starts.
std::string create(const AppConfig &identity, bool enabled);

} // namespace pluginjson
