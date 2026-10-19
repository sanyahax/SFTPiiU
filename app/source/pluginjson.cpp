#include "pluginjson.h"

#include <cctype>

namespace pluginjson {

namespace {

size_t skip_space(const std::string &s, size_t p) {
    while (p < s.size() && (s[p] == ' ' || s[p] == '\t' || s[p] == '\r' || s[p] == '\n')) p++;
    return p;
}

// Position just after `"key":` (and any spaces), or npos.
size_t value_pos(const std::string &json, const char *key) {
    std::string pattern = std::string("\"") + key + "\"";
    for (size_t p = json.find(pattern); p != std::string::npos; p = json.find(pattern, p + 1)) {
        size_t q = skip_space(json, p + pattern.size());
        if (q < json.size() && json[q] == ':') {
            return skip_space(json, q + 1);
        }
    }
    return std::string::npos;
}

std::string quote(const std::string &s) {
    std::string out = "\"";
    for (char c : s) {
        if (c == '"' || c == '\\') {
            out += '\\';
            out += c;
        } else if ((unsigned char) c < 0x20) {
            continue; // never part of a password or hex key
        } else {
            out += c;
        }
    }
    return out + "\"";
}

} // namespace

bool find(const std::string &json, const char *key, std::string &out) {
    size_t p = value_pos(json, key);
    if (p == std::string::npos || p >= json.size()) {
        return false;
    }
    out.clear();
    if (json[p] == '"') {
        for (p++; p < json.size() && json[p] != '"'; p++) {
            if (json[p] == '\\' && p + 1 < json.size()) {
                p++;
            }
            out += json[p];
        }
        return p < json.size();
    }
    while (p < json.size() && json[p] != ',' && json[p] != '}' && !isspace((unsigned char) json[p])) {
        out += json[p++];
    }
    return !out.empty();
}

std::string set_bool(const std::string &json, const char *key, bool value) {
    const char *text = value ? "true" : "false";
    size_t p         = value_pos(json, key);
    if (p != std::string::npos && p < json.size() && json[p] != '"' && json[p] != '{' && json[p] != '[') {
        size_t end = p;
        while (end < json.size() && json[end] != ',' && json[end] != '}' && !isspace((unsigned char) json[end])) {
            end++;
        }
        return json.substr(0, p) + text + json.substr(end);
    }
    if (p != std::string::npos) {
        return {}; // the key holds something else; leave the file alone
    }
    size_t items = value_pos(json, "storageitems");
    if (items == std::string::npos || items >= json.size() || json[items] != '{') {
        return {};
    }
    bool empty       = skip_space(json, items + 1) < json.size() && json[skip_space(json, items + 1)] == '}';
    std::string item = std::string("\n        \"") + key + "\": " + text + (empty ? "\n    " : ",");
    return json.substr(0, items + 1) + item + json.substr(items + 1);
}

std::string create(const AppConfig &identity, bool enabled) {
    std::string j = "{\n    \"storageitems\": {\n";
    j += std::string("        \"enabled\": ") + (enabled ? "true" : "false") + ",\n";
    j += "        \"hostKeySeed\": " + quote(identity.hostKeyHex) + ",\n";
    j += "        \"password\": " + quote(identity.password) + ",\n";
    j += std::string("        \"passwordLogin\": ") + (identity.passwordOn ? "true" : "false") + ",\n";
    j += "        \"port\": " + std::to_string(identity.port) + ",\n";
    j += std::string("        \"slcWritable\": ") + (identity.slcWritable ? "true" : "false") + ",\n";
    j += std::string("        \"systemAccess\": ") + (identity.systemAccess ? "true" : "false") + "\n";
    j += "    }\n}";
    return j;
}

} // namespace pluginjson
