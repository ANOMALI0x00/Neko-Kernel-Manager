#include "utils.hpp"

#include <QProcess>
#include <array>
#include <memory>
#include <cstdio>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>

namespace utils {

std::string exec(const std::string &cmd) {
    std::array<char, 128> buffer;
    std::string result;
    FILE* raw_pipe = popen(cmd.c_str(), "r");
    if (!raw_pipe) return "";
    auto closer = [](FILE *pipe) {
        if (pipe) pclose(pipe);
    };
    std::unique_ptr<FILE, decltype(closer)> pipe(raw_pipe, closer);
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    if (!result.empty() && result.back() == '\n') result.pop_back();
    return result;
}

bool runCommand(const std::string& cmd) {
    return system(cmd.c_str()) == 0;
}

bool runPrivilegedCommand(const std::string& cmd) {
    if (getuid() == 0) {
        return system(cmd.c_str()) == 0;
    }

    // If the command already includes pkexec or sudo, run it as-is to avoid double-wrapping
    if (cmd.find("pkexec") != std::string::npos || cmd.find("sudo") != std::string::npos) {
        return system(cmd.c_str()) == 0;
    }

    // Prefer pkexec when available
    if (system("command -v pkexec >/dev/null 2>&1") == 0) {
        std::string pkcmd = "pkexec sh -c \"" + cmd + "\"";
        return system(pkcmd.c_str()) == 0;
    }

    // Fallback to sudo if pkexec is not available (will fail in GUI without terminal, but it's a last resort)
    std::string sudocmd = "sudo sh -c \"" + cmd + "\"";
    return system(sudocmd.c_str()) == 0;
}

void runInTerminal(const std::string& cmd) {
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
        std::string finalCmd = terminal + " -e sh -c \"" + cmd + "; echo 'Press enter to close...'; read\"";
        system(finalCmd.c_str());
    } else {
        system(cmd.c_str());
    }
}

bool fileOwnedByPackage(const std::string &filePath) {
    if (!commandExists("xbps-query")) return false;
    QProcess process;
    process.setProgram("xbps-query");
    process.setArguments({"-o", QString::fromStdString(filePath)});
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start();
    process.waitForFinished(-1);
    return process.exitCode() == 0;
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

    std::string cmd = "curl -sSf " + url + " | python3 -c 'import sys, json; d=json.load(sys.stdin); ";
    if (variant == "stable") {
        cmd += "print(*(r[\"version\"] for r in d[\"releases\"] if r.get(\"moniker\") == \"stable\"), sep=\"\\n\")'";
    } else if (variant == "lts") {
        cmd += "print(*(r[\"version\"] for r in d[\"releases\"] if r.get(\"moniker\") == \"longterm\"), sep=\"\\n\")'";
    } else if (variant == "mainline") {
        cmd += "print(*(r[\"version\"] for r in d[\"releases\"] if r.get(\"moniker\") == \"mainline\"), sep=\"\\n\")'";
    } else {
        cmd += "print(*(r[\"version\"] for r in d[\"releases\"]), sep=\"\\n\")'";
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
    (void)variant; // The path layout on kernel.org is based on the major version, so variant does not change the URL for current kernels.
    const std::string base = "https://cdn.kernel.org/pub/linux/kernel";
    size_t firstDot = version.find('.');
    if (firstDot == std::string::npos) return "";

    const std::string major = version.substr(0, firstDot);
    std::string path = "/v" + major + ".x";
    if (major == "2") path = "/v2.6";

    return base + path + "/linux-" + version + ".tar.xz";
}

bool gitClone(const std::string &repoUrl, const std::string &dest) {
    // Ensure parent directory exists
    std::string parentDir = dest.substr(0, dest.find_last_of('/'));
    system(("mkdir -p " + parentDir).c_str());
    
    const std::string cmd = "rm -rf '" + dest + "' && git clone --depth 1 " + repoUrl + " '" + dest + "' 2>&1";
    
    // We want to capture errors to debug why it fails
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return false;
    
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe)) {
        // Output is not easily returned here, but we can print to stderr or just rely on the exit code
        // For now, let's just ensure we wait for it.
    }
    
    return pclose(pipe) == 0;
}

bool commandExists(const std::string &cmd) {
    QString escaped = QString::fromStdString(cmd).replace("'", "'\\''");
    return QProcess::execute("sh", {"-c", "command -v '" + escaped + "' >/dev/null 2>&1"}) == 0;
}

bool dirExists(const std::string &path) {
    struct ::stat info;
    if (::stat(path.c_str(), &info) != 0) return false;
    return (info.st_mode & S_IFDIR);
}

bool packageExists(const std::string &pkgName) {
    if (!commandExists("xbps-query")) return false;
    return QProcess::execute("xbps-query", {"-S", QString::fromStdString(pkgName)}) == 0;
}

bool packageInstalled(const std::string &pkgName) {
    if (!commandExists("xbps-query")) return false;
    return QProcess::execute("xbps-query", {"-W", QString::fromStdString(pkgName)}) == 0;
}

std::string getRealHome() {
    const char* sudo_user = getenv("SUDO_USER");
    if (sudo_user && std::string(sudo_user) != "root") {
        struct passwd* pw = getpwnam(sudo_user);
        if (pw) return pw->pw_dir;
    }
    
    // If we are root but no SUDO_USER, try to find the user who owns /home/*
    if (getuid() == 0) {
        struct passwd* pw = getpwuid(1000);
        if (pw) return pw->pw_dir;
    }

    const char* home = getenv("HOME");
    if (home) return home;
    
    struct passwd* pw = getpwuid(getuid());
    return pw ? pw->pw_dir : "/tmp";
}

std::string getRealUser() {
    const char* sudo_user = getenv("SUDO_USER");
    if (sudo_user && std::string(sudo_user) != "root") return sudo_user;
    
    if (getuid() == 0) {
        struct passwd* pw = getpwuid(1000);
        if (pw) return pw->pw_name;
    }

    struct passwd* pw = getpwuid(getuid());
    return pw ? pw->pw_name : "unknown";
}

std::string detectCpuLevel() {
    // Check CPU features using /proc/cpuinfo or by executing a check script
    // A simple way is to use a helper that checks flags
    // v1: basic x86-64
    // v2: +popcnt, +sse4_1, +sse4_2, +ssse3
    // v3: +avx, +avx2, +bmi1, +bmi2, +f16c, +fma, +movbe, +xsave
    // v4: +avx512f, +avx512bw, +avx512cd, +avx512dq, +avx512vl
    
    std::string flags = exec("grep -m1 flags /proc/cpuinfo");
    
    bool has_v2 = flags.find("popcnt") != std::string::npos && 
                  flags.find("sse4_1") != std::string::npos && 
                  flags.find("sse4_2") != std::string::npos && 
                  flags.find("ssse3") != std::string::npos;
                  
    if (!has_v2) return "x86-64"; // v1
    
    bool has_v3 = flags.find("avx") != std::string::npos && 
                  flags.find("avx2") != std::string::npos && 
                  flags.find("bmi1") != std::string::npos && 
                  flags.find("bmi2") != std::string::npos && 
                  flags.find("f16c") != std::string::npos && 
                  flags.find("fma") != std::string::npos && 
                  flags.find("movbe") != std::string::npos && 
                  flags.find("xsave") != std::string::npos;
                  
    if (!has_v3) return "x86-64-v2";
    
    bool has_v4 = flags.find("avx512f") != std::string::npos && 
                  flags.find("avx512bw") != std::string::npos && 
                  flags.find("avx512cd") != std::string::npos && 
                  flags.find("avx512dq") != std::string::npos && 
                  flags.find("avx512vl") != std::string::npos;
                  
    if (!has_v4) return "x86-64-v3";
    
    return "x86-64-v4";
}

} // namespace utils