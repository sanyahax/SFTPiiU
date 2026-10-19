#include "background.h"
#include "platform.h"
#include "pluginjson.h"
#include "wiiu/console.h"

#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace background {

namespace {

std::string plugins_dir(const AppConfig &cfg) {
    return cfg.environment + "/plugins";
}

// An installed SFTPiiU plugin, whatever the user named it ("SFTPiiU.wps",
// "sftpiiu_v1.1.wps", ...). Empty if there is none.
std::string find_plugin(const AppConfig &cfg) {
    std::string dir = plugins_dir(cfg);
    DIR *d          = opendir(dir.c_str());
    if (!d) {
        return {};
    }
    std::string found;
    while (struct dirent *e = readdir(d)) {
        std::string n = e->d_name;
        for (auto &c : n) c = (char) std::tolower((unsigned char) c);
        if (n.size() > 4 && n.compare(n.size() - 4, 4, ".wps") == 0 && n.find("sftpiiu") != std::string::npos) {
            found = dir + "/" + e->d_name;
            break;
        }
    }
    closedir(d);
    return found;
}

bool same_content(const std::string &path, const uint8_t *data, size_t size) {
    struct stat st;
    if (stat(path.c_str(), &st) != 0 || (size_t) st.st_size != size) {
        return false;
    }
    int fd = open(path.c_str(), O_RDONLY);
    if (fd < 0) {
        return false;
    }
    std::vector<uint8_t> buf(64 * 1024);
    size_t off = 0;
    bool same  = true;
    while (same && off < size) {
        ssize_t n = read(fd, buf.data(), buf.size());
        if (n <= 0) {
            same = false;
            break;
        }
        same = (size_t) n <= size - off && memcmp(buf.data(), data + off, (size_t) n) == 0;
        off += (size_t) n;
    }
    close(fd);
    return same && off == size;
}

// Writes next to the target first, so a failed write never leaves a broken
// plugin or config behind.
int write_file(const std::string &path, const void *data, size_t size) {
    std::string tmp = path + ".sftpiiu-tmp";
    int fd          = open(tmp.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0) {
        return errno ? errno : EIO;
    }
    size_t off = 0;
    while (off < size) {
        ssize_t n = write(fd, (const uint8_t *) data + off, size - off);
        if (n <= 0) {
            int err = n < 0 && errno ? errno : ENOSPC;
            close(fd);
            unlink(tmp.c_str());
            return err;
        }
        off += (size_t) n;
    }
    if (close(fd) != 0) {
        int err = errno ? errno : EIO;
        unlink(tmp.c_str());
        return err;
    }
    // The Wii U file systems do not replace on rename.
    unlink(path.c_str());
    return rename(tmp.c_str(), path.c_str()) == 0 ? 0 : (errno ? errno : EIO);
}

int make_dir(const std::string &path) {
    return mkdir(path.c_str(), 0777) == 0 || errno == EEXIST ? 0 : errno;
}

int set_enabled(const AppConfig &cfg, bool on) {
    std::string path = plugin_config_path(cfg);
    std::string js   = read_text_file(path.c_str());
    std::string out, v;
    // Edit an existing config in place (it holds the plugin's identity and
    // the boot marker); write a fresh one only if it is missing or unusable.
    if (!js.empty() && pluginjson::find(js, "password", v) && pluginjson::find(js, "hostKeySeed", v)) {
        out = pluginjson::set_bool(js, "enabled", on);
    }
    if (out.empty()) {
        out = pluginjson::create(cfg, on);
    }
    if (int err = make_dir(plugins_dir(cfg) + "/config")) {
        return err;
    }
    return write_file(path, out.data(), out.size());
}

} // namespace

Status query(const AppConfig &cfg, bool pluginAnswered) {
    Status s;
    s.installed    = !find_plugin(cfg).empty();
    std::string js = read_text_file(plugin_config_path(cfg).c_str());
    std::string v;
    // The plugin's default is on.
    s.enabled = !(pluginjson::find(js, "enabled", v) && v == "false");
    if (pluginjson::find(js, "loadedBoot", v)) {
        // Both sides compute the boot time from two clocks; allow for drift
        // and clock changes.
        s.loaded = llabs(atoll(v.c_str()) - (long long) wiiu_boot_time()) <= 120;
    }
    s.loaded = s.loaded || pluginAnswered;
    return s;
}

int enable(const AppConfig &cfg, const uint8_t *plugin, size_t pluginSize, bool &written) {
    written          = false;
    std::string path = find_plugin(cfg);
    if (path.empty()) {
        if (int err = make_dir(plugins_dir(cfg))) {
            return err;
        }
        path = plugins_dir(cfg) + "/SFTPiiU.wps";
    }
    if (!same_content(path, plugin, pluginSize)) {
        if (int err = write_file(path, plugin, pluginSize)) {
            return err;
        }
        written = true;
        LOG("installed plugin to %s", path.c_str());
    }
    return set_enabled(cfg, true);
}

int disable(const AppConfig &cfg) {
    return set_enabled(cfg, false);
}

} // namespace background
