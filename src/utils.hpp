#ifndef UTILS_HPP
#define UTILS_HPP

#include <string>
#include <vector>

namespace utils {
    // Estructura para devolver salida separada de un comando
    struct CommandOutput {
        std::string stdout;
        std::string stderr;
        int exitCode;
    };

    std::string exec(const std::string& cmd);
    CommandOutput execWithOutput(const std::string& cmd);
    bool runCommand(const std::string& cmd);
    bool runPrivilegedCommand(const std::string& cmd);
    void runInTerminal(const std::string& cmd);
    std::vector<std::string> split(const std::string& s, char delim);
    std::string join(const std::vector<std::string>& v, const std::string& delim);
    bool commandExists(const std::string &cmd);
    bool fileExists(const std::string &path);   // NUEVA: comprueba si un archivo existe
    bool dirExists(const std::string &path);
    bool packageExists(const std::string &pkgName);
    bool packageInstalled(const std::string &pkgName);
    bool fileOwnedByPackage(const std::string &filePath);
    std::string getRealHome();
    std::string getRealUser();
    std::string detectCpuLevel();
} // namespace utils

#endif // UTILS_HPP