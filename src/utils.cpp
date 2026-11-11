#include "utils.hpp"

#include <QProcess>
#include <QStandardPaths>
#include <cctype>
#include <fstream>
#include <sstream>
#include <sys/stat.h>

namespace utils {

std::string execProgram(const std::string &program,
                        const std::vector<std::string> &args,
                        bool mergeStderr) {
    QStringList qargs;
    qargs.reserve(static_cast<int>(args.size()));
    for (const auto &a : args) {
        qargs.append(QString::fromStdString(a));
    }

    QProcess process;
    process.setProgram(QString::fromStdString(program));
    process.setArguments(qargs);
    process.setProcessChannelMode(mergeStderr ? QProcess::MergedChannels : QProcess::SeparateChannels);
    process.start();
    if (!process.waitForFinished(-1)) {
        return "";
    }

    QString output = QString::fromUtf8(process.readAllStandardOutput());
    if (!output.isEmpty() && output.back() == '\n') {
        output.chop(1);
    }
    return output.toStdString();
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

std::string trim(const std::string &s) {
    if (s.empty()) return "";
    size_t start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) {
        start++;
    }
    if (start >= s.size()) return "";
    size_t end = s.size() - 1;
    while (end > start && std::isspace(static_cast<unsigned char>(s[end]))) {
        end--;
    }
    return s.substr(start, end - start + 1);
}

bool commandExists(const std::string &cmd) {
    const QString name = QString::fromStdString(cmd);
    if (!QStandardPaths::findExecutable(name).isEmpty()) {
        return true;
    }
    static const QStringList sbinPaths = {"/usr/local/sbin", "/usr/sbin", "/sbin"};
    return !QStandardPaths::findExecutable(name, sbinPaths).isEmpty();
}

bool fileExists(const std::string &path) {
    struct ::stat info;
    return (::stat(path.c_str(), &info) == 0) && S_ISREG(info.st_mode);
}

bool dirExists(const std::string &path) {
    struct ::stat info;
    if (::stat(path.c_str(), &info) != 0) return false;
    return S_ISDIR(info.st_mode);
}

bool packageInstalled(const std::string &pkgName) {
    if (!commandExists("xbps-query")) return false;
    return QProcess::execute("xbps-query", {"-p", "pkgver", QString::fromStdString(pkgName)}) == 0;
}

std::string detectCpuLevel() {
    std::ifstream cpuinfo("/proc/cpuinfo");
    std::string line;
    std::string flags;
    while (std::getline(cpuinfo, line)) {
        if (line.rfind("flags", 0) == 0) {
            flags = line;
            break;
        }
    }
    const bool has_v2 = flags.find("popcnt") != std::string::npos &&
                        flags.find("sse4_1") != std::string::npos &&
                        flags.find("sse4_2") != std::string::npos &&
                        flags.find("ssse3") != std::string::npos;
    if (!has_v2) return "x86-64";
    const bool has_v3 = flags.find("avx") != std::string::npos &&
                        flags.find("avx2") != std::string::npos &&
                        flags.find("bmi1") != std::string::npos &&
                        flags.find("bmi2") != std::string::npos &&
                        flags.find("f16c") != std::string::npos &&
                        flags.find("fma") != std::string::npos &&
                        flags.find("movbe") != std::string::npos &&
                        flags.find("xsave") != std::string::npos;
    if (!has_v3) return "x86-64-v2";
    const bool has_v4 = flags.find("avx512f") != std::string::npos &&
                        flags.find("avx512bw") != std::string::npos &&
                        flags.find("avx512cd") != std::string::npos &&
                        flags.find("avx512dq") != std::string::npos &&
                        flags.find("avx512vl") != std::string::npos;
    if (!has_v4) return "x86-64-v3";
    return "x86-64-v4";
}

} // namespace utils
