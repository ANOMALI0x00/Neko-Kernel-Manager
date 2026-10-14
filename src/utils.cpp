#include "utils.hpp"

#include <QProcess>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>

namespace utils {

std::string exec(const std::string &cmd) {
    QProcess process;
    process.setProgram("sh");
    process.setArguments({"-c", QString::fromStdString(cmd)});
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start();
    process.waitForFinished(-1);

    QString output = process.readAllStandardOutput();
    if (!output.isEmpty() && output.back() == '\n')
        output.chop(1);
    return output.toStdString();
}

utils::CommandOutput execWithOutput(const std::string &cmd) {
    QProcess process;
    process.setProgram("sh");
    process.setArguments({"-c", QString::fromStdString(cmd)});
    process.setProcessChannelMode(QProcess::SeparateChannels);
    process.start();
    process.waitForFinished(-1);

    utils::CommandOutput result;
    result.stdout = process.readAllStandardOutput().toStdString();
    result.stderr = process.readAllStandardError().toStdString();
    result.exitCode = process.exitCode();
    return result;
}

bool runCommand(const std::string& cmd) {
    return system(cmd.c_str()) == 0;
}

static bool canAuthenticate(const std::string &privilegeCmd) {
    std::string testCmd = privilegeCmd + " true 2>/dev/null";
    int ret = system(testCmd.c_str());
    return (ret == 0);
}

static std::string detectPrivilegeSystem() {
    if (system("command -v pkexec >/dev/null 2>&1") == 0) {
        if (canAuthenticate("pkexec")) {
            return "pkexec";
        }
    }
    if (system("command -v doas >/dev/null 2>&1") == 0) {
        if (canAuthenticate("doas")) {
            return "doas";
        }
    }
    if (system("command -v sudo >/dev/null 2>&1") == 0) {
        if (canAuthenticate("sudo")) {
            return "sudo";
        }
        return "sudo";
    }
    return "";
}

bool runPrivilegedCommand(const std::string& cmd) {
    if (getuid() == 0) {
        return system(cmd.c_str()) == 0;
    }
    if (cmd.find("pkexec") != std::string::npos ||
        cmd.find("doas") != std::string::npos ||
        cmd.find("sudo") != std::string::npos) {
        return system(cmd.c_str()) == 0;
    }
    std::string privilege = detectPrivilegeSystem();
    if (privilege == "pkexec") {
        std::string pkcmd = "pkexec sh -c \"" + cmd + "\"";
        return system(pkcmd.c_str()) == 0;
    } else if (privilege == "doas") {
        std::string doascmd = "doas sh -c \"" + cmd + "\"";
        return system(doascmd.c_str()) == 0;
    } else if (privilege == "sudo") {
        std::string sudocmd;
        if (getenv("SUDO_ASKPASS") != nullptr) {
            sudocmd = "sudo -A sh -c \"" + cmd + "\"";
        } else {
            sudocmd = "sudo sh -c \"" + cmd + "\"";
        }
        return system(sudocmd.c_str()) == 0;
    }
    return false;
}

void runInTerminal(const std::string& cmd) {
    const char* term = getenv("TERMINAL");
    std::string terminal;
    if (term) {
        terminal = term;
    } else {
        for (const char* t : {"kitty", "alacritty", "foot", "xfce4-terminal", "konsole", "xterm"}) {
            if (system((std::string("command -v ") + t + " >/dev/null 2>&1").c_str()) == 0) {
                terminal = t;
                break;
            }
        }
    }
    std::string finalCmd = cmd;
    if (cmd.find("doas") == std::string::npos &&
        cmd.find("sudo") == std::string::npos &&
        cmd.find("pkexec") == std::string::npos) {
        std::string privilege = detectPrivilegeSystem();
        if (!privilege.empty()) {
            finalCmd = privilege + " sh -c \"" + cmd + "\"";
        }
    }
    if (!terminal.empty()) {
        std::string termCmd = terminal + " -e sh -c \"" + finalCmd + "; echo 'Press enter to close...'; read\"";
        int res = system(termCmd.c_str());
        (void)res;
    } else {
        int res = system(finalCmd.c_str());
        (void)res;
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

bool commandExists(const std::string &cmd) {
    QString escaped = QString::fromStdString(cmd).replace("'", "'\\''");
    return QProcess::execute("sh", {"-c", "command -v '" + escaped + "' >/dev/null 2>&1"}) == 0;
}

bool fileExists(const std::string &path) {
    struct ::stat info;
    return (::stat(path.c_str(), &info) == 0) && (info.st_mode & S_IFREG);
}

bool dirExists(const std::string &path) {
    struct ::stat info;
    if (::stat(path.c_str(), &info) != 0) return false;
    return (info.st_mode & S_IFDIR);
}

bool packageExists(const std::string &pkgName) {
    if (!commandExists("xbps-query")) return false;
    return QProcess::execute("xbps-query", {"-Rs", QString::fromStdString(pkgName)}) == 0;
}

bool packageInstalled(const std::string &pkgName) {
    if (!commandExists("xbps-query")) return false;
    QProcess process;
    process.setProgram("xbps-query");
    process.setArguments({"-l"});
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start();
    process.waitForFinished(-1);
    if (process.exitCode() != 0) return false;
    QString output = process.readAllStandardOutput();
    return output.contains(QString::fromStdString(pkgName));
}

std::string getRealHome() {
    const char* sudo_user = getenv("SUDO_USER");
    if (sudo_user && std::string(sudo_user) != "root") {
        struct passwd* pw = getpwnam(sudo_user);
        if (pw) return pw->pw_dir;
    }
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
    std::string flags = exec("grep -m1 flags /proc/cpuinfo");
    bool has_v2 = flags.find("popcnt") != std::string::npos &&
                  flags.find("sse4_1") != std::string::npos &&
                  flags.find("sse4_2") != std::string::npos &&
                  flags.find("ssse3") != std::string::npos;
    if (!has_v2) return "x86-64";
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