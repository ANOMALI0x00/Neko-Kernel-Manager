#ifndef UTILS_HPP
#define UTILS_HPP

#include <string>
#include <vector>

namespace utils {
    struct CommandOutput {
        std::string stdout;
        std::string stderr;
        int exitCode;
    };

    std::string exec(const std::string& cmd);
    CommandOutput execWithOutput(const std::string& cmd);
    std::vector<std::string> split(const std::string& s, char delim);
    std::string trim(const std::string &s);
    bool commandExists(const std::string &cmd);
    bool fileExists(const std::string &path);
    bool dirExists(const std::string &path);
    bool packageInstalled(const std::string &pkgName);
    std::string getRealHome();
    std::string detectCpuLevel();
} // namespace utils

#endif // UTILS_HPP
