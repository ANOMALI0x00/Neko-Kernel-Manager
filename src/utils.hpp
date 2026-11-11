#ifndef UTILS_HPP
#define UTILS_HPP

#include <string>
#include <vector>

namespace utils {
    std::string execProgram(const std::string &program,
                            const std::vector<std::string> &args = {},
                            bool mergeStderr = false);
    std::vector<std::string> split(const std::string &s, char delim);
    std::string trim(const std::string &s);
    bool commandExists(const std::string &cmd);
    bool fileExists(const std::string &path);
    bool dirExists(const std::string &path);
    bool packageInstalled(const std::string &pkgName);
    std::string detectCpuLevel();
} // namespace utils

#endif // UTILS_HPP
