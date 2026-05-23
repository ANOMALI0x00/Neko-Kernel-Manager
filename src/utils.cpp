#include "utils.hpp"

#include <array>
#include <memory>
#include <cstdio>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>

namespace utils {

std::string exec(const std::string &cmd) {
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
    if (!pipe) return "";
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    // Remove trailing newline
    if (!result.empty() && result.back() == '\n') result.pop_back();
    return result;
}

void runCommand(const std::string &cmd) {
    system(cmd.c_str());
}

void runInTerminal(const std::string &cmd) {
    // Try to find a terminal emulator
    const char* term = getenv("TERMINAL");
    std::string terminal;
    if (term) {
        terminal = term;
    } else {
        // Fallback list
        for (const char* t : {"kitty", "alacritty", "foot", "xfce4-terminal", "konsole", "xterm"}) {
            if (system((std::string("which ") + t + " >/dev/null 2>&1").c_str()) == 0) {
                terminal = t;
                break;
            }
        }
    }

    if (!terminal.empty()) {
        std::string finalCmd = terminal + " -e sh -c \"" + cmd + "; echo 'Press enter to close...'; read\" &";
        system(finalCmd.c_str());
    } else {
        system(cmd.c_str());
    }
}

std::vector<std::string> split(const std::string &s, char delim) {
    std::vector<std::string> result;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim)) {
        result.push_back(item);
    }
    return result;
}

std::string join(const std::vector<std::string> &v, const std::string &delim) {
    std::string result;
    for (size_t i = 0; i < v.size(); ++i) {
        if (i > 0) result += delim;
        result += v[i];
    }
    return result;
}

std::vector<std::string> fetchKernelOrgVersions(const std::string &variant) {
    std::vector<std::string> versions;
    const std::string url = "https://www.kernel.org/releases.json";
    
    std::string cmd = "curl -s " + url + " | python3 -c 'import sys, json; data = json.load(sys.stdin); ";
    if (variant == "stable") {
        cmd += "print(\"\\n\".join([r[\"version\"] for r in data[\"releases\"] if r.get(\"moniker\") == \"stable\"]))'";
    } else if (variant == "lts") {
        cmd += "print(\"\\n\".join([r[\"version\"] for r in data[\"releases\"] if r.get(\"moniker\") == \"longterm\"]))'";
    } else if (variant == "mainline") {
        cmd += "print(\"\\n\".join([r[\"version\"] for r in data[\"releases\"] if r.get(\"moniker\") == \"mainline\"]))'";
    } else {
        cmd += "print(\"\\n\".join([r[\"version\"] for r in data[\"releases\"]]))'";
    }
    
    const std::string output = exec(cmd);
    for (const auto &line : split(output, '\n')) {
        if (!line.empty()) {
            versions.push_back(line);
        }
    }
    return versions;
}

bool downloadFile(const std::string &url, const std::string &dest) {
    const std::string cmd = "mkdir -p $(dirname '" + dest + "') && curl -L -o '" + dest + "' '" + url + "'";
    return system(cmd.c_str()) == 0;
}

std::string kernelOrgDownloadUrl(const std::string &variant, const std::string &version) {
    const std::string base = "https://cdn.kernel.org/pub/linux/kernel";
    const std::string major = version.substr(0, version.find('.'));

    if (variant == "lts" || variant == "stable" || variant == "mainline" || variant == "all") {
        return base + "/v" + major + "/linux-" + version + ".tar.xz";
    }
    return base + "/v" + major + "/linux-" + version + ".tar.xz";
}

bool gitClone(const std::string &repoUrl, const std::string &dest) {
    const std::string cmd = "rm -rf '" + dest + "' && git clone " + repoUrl + " '" + dest + "'";
    return system(cmd.c_str()) == 0;
}

bool commandExists(const std::string &cmd) {
    return system(("command -v " + cmd + " >/dev/null 2>&1").c_str()) == 0;
}

bool dirExists(const std::string &path) {
    struct ::stat info;
    if (::stat(path.c_str(), &info) != 0) return false;
    return (info.st_mode & S_IFDIR);
}

} // namespace utils