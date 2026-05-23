#include "kernel.hpp"
#include "utils.hpp"

#include <sstream>
#include <algorithm>

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
    if (m_pkg.type == "manual") return "Custom";
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
    
    // Detect XBPS Kernels
    if (utils::commandExists("xbps-query")) {
        // Detecting kernels and modules
        std::string output = utils::exec("xbps-query -Rs linux | grep 'Linux kernel and modules'");
        std::vector<std::string> lines = utils::split(output, '\n');

        for (const auto &line : lines) {
            if (line.empty()) continue;
            
            std::vector<std::string> parts = utils::split(line, ' ');
            if (parts.size() < 2) continue;
            
            std::string status = parts[0];
            std::string full_pkg = parts[1];
            size_t hyphen_pos = full_pkg.find_last_of('-');
            
            if (hyphen_pos == std::string::npos || hyphen_pos == 0) continue;
            
            std::string pkg_name = full_pkg.substr(0, hyphen_pos);
            std::string version = full_pkg.substr(hyphen_pos + 1);
            
            if (pkg_name == "linux-api-headers" || pkg_name.find("-headers") != std::string::npos) continue;
            
            bool already_added = false;
            for(const auto& k : kernels) if(k.name() == pkg_name) { already_added = true; break; }
            if(already_added) continue;

            bool installed = (status == "[*]");

            Package pkg{pkg_name, version, "void", "xbps", installed};
            Package headers{pkg_name + "-headers", version, "void", "xbps", installed};
            kernels.emplace_back(pkg, headers);
        }
    }

    // 2. Detect Manual Kernels in /boot
    std::string boot_files = utils::exec("ls /boot/vmlinuz-* 2>/dev/null");
    std::vector<std::string> vmlinuz_list = utils::split(boot_files, '\n');
    
    if (!vmlinuz_list.empty()) {
        for (const auto& path : vmlinuz_list) {
            if (path.empty()) continue;
            
            std::string ownership = utils::exec("xbps-query -o " + path + " 2>/dev/null");
            if (ownership.find("not owned by any package") != std::string::npos || ownership.empty()) {
                std::string version = path;
                size_t last_dash = version.find_last_of('-');
                if (last_dash != std::string::npos) {
                    version = version.substr(last_dash + 1);
                }
                
                bool exists = false;
                for (const auto& k : kernels) {
                    if (k.version() == version) { exists = true; break; }
                }
                if (exists) continue;

                Package pkg{"linux-manual-" + version, version, "local", "manual", true};
                Package headers{"none", "none", "local", "manual", false};
                kernels.emplace_back(pkg, headers);
            }
        }
    }

    return kernels;
}
