#include "kernel.hpp"
#include "utils.hpp"

#include <filesystem>
#include <sstream>
#include <set>

Kernel::Kernel(Package pkg, Package headers) : m_pkg(pkg), m_headers(headers) {}

std::string Kernel::version() const {
    return m_pkg.version;
}

bool Kernel::is_installed() const {
    return m_pkg.is_installed;
}

bool Kernel::install() {
    return true;
}

bool Kernel::remove() {
    return true;
}

std::string Kernel::category() const {
    if (m_pkg.type == "manual" || m_pkg.name.find("kernel-neko") != std::string::npos) return "Custom";
    if (m_pkg.name.find("lts") != std::string::npos) return "Longterm";
    if (m_pkg.name.find("rt") != std::string::npos) return "Realtime";
    if (m_pkg.name.find("zen") != std::string::npos) return "Zen";
    if (m_pkg.name.find("hardened") != std::string::npos) return "Hardened";
    if (m_pkg.name.find("mainline") != std::string::npos) return "Mainline";
    return "Stable";
}

std::string Kernel::size() const {
    if (m_pkg.type == "manual") return "Unknown";
    std::string s = utils::exec("xbps-query -Rp installed_size " + m_pkg.name);
    if (s.empty() || s.find("not found") != std::string::npos) return "Unknown";
    return s;
}

std::string Kernel::installDate() const {
    if (m_pkg.type == "manual") return "N/A";
    std::string d = utils::exec("xbps-query -p install-date " + m_pkg.name);
    if (d.empty() || d.find("not found") != std::string::npos) return "N/A";
    return d;
}

std::vector<Kernel> Kernel::getKernels() {
    std::vector<Kernel> kernels;
    std::set<std::string> knownNames;
    std::set<std::string> knownVersions;

    // Detect XBPS Kernels
    if (utils::commandExists("xbps-query")) {
        std::string output = utils::exec("xbps-query -Rs linux | grep 'Linux kernel and modules'");
        output += "\n" + utils::exec("xbps-query -Rs kernel-neko");
        std::vector<std::string> lines = utils::split(output, '\n');

        for (const auto &line_raw : lines) {
            std::string line = line_raw;
            if (line.empty()) continue;

            std::istringstream iss(line);
            std::string status;
            std::string full_pkg;
            iss >> status >> full_pkg;
            if (full_pkg.empty()) continue;

            size_t hyphen_pos = full_pkg.find_last_of('-');
            if (hyphen_pos == std::string::npos || hyphen_pos == 0) continue;

            std::string pkg_name = full_pkg.substr(0, hyphen_pos);
            std::string version = full_pkg.substr(hyphen_pos + 1);

            if (pkg_name == "linux-api-headers" || pkg_name.find("-headers") != std::string::npos) continue;
            if (pkg_name == "linux-firmware" || pkg_name == "linux-libre") continue;
            if (knownNames.count(pkg_name)) continue;

            bool installed = (status == "[*]");
            Package pkg{pkg_name, version, "void", "xbps", installed};
            Package headers{pkg_name + "-headers", version, "void", "xbps", installed};
            kernels.emplace_back(pkg, headers);
            knownNames.insert(pkg_name);
            knownVersions.insert(version);
        }
    }

    // Detect manual kernels in /boot without invoking shell tools.
    std::filesystem::path bootPath("/boot");
    if (std::filesystem::exists(bootPath) && std::filesystem::is_directory(bootPath)) {
        for (const auto &entry : std::filesystem::directory_iterator(bootPath)) {
            if (!entry.is_regular_file() && !entry.is_symlink()) continue;
            std::string filename = entry.path().filename().string();
            if (filename.rfind("vmlinuz-", 0) != 0) continue;

            std::string fullPath = entry.path().string();
            bool owned = utils::fileOwnedByPackage(fullPath);
            if (owned) continue;

            std::string version = filename.substr(std::string("vmlinuz-").size());
            if (version.empty()) continue;
            if (knownVersions.count(version)) continue;

            Package pkg{"linux-manual-" + version, version, "local", "manual", true};
            Package headers{"none", "none", "local", "manual", false};
            kernels.emplace_back(pkg, headers);
            knownNames.insert(pkg.name);
            knownVersions.insert(version);
        }
    }

    return kernels;
}
