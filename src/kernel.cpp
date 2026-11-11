#include "kernel.hpp"
#include "utils.hpp"

#include <filesystem>
#include <sstream>
#include <set>
#include <map>
#include <algorithm>
#include <cstdint>
#include <cctype>

Kernel::Kernel(Package pkg, Package headers) : m_pkg(pkg), m_headers(headers) {}

std::string Kernel::version() const {
    return m_pkg.version;
}

bool Kernel::is_installed() const {
    return m_pkg.is_installed;
}

bool Kernel::has_files() const {
    return m_pkg.has_files;
}

std::string Kernel::category() const {
    if (m_pkg.type == "vkpurge") return "Orphan / Obsolete";
    if (m_pkg.name.find("bore") != std::string::npos) return "BORE";
    if (m_pkg.name.find("lts") != std::string::npos) return "Longterm";
    if (m_pkg.name.find("rt") != std::string::npos) return "Realtime";
    if (m_pkg.name.find("zen") != std::string::npos) return "Zen";
    if (m_pkg.name.find("hardened") != std::string::npos) return "Hardened";
    if (m_pkg.name.find("mainline") != std::string::npos) return "Mainline";
    if (m_pkg.type == "manual" || 
        m_pkg.name.find("linux-neko") != std::string::npos ||
        m_pkg.name.find("kernel-neko") != std::string::npos ||
        m_pkg.name.find("linux-cachy-void") != std::string::npos) return "Custom";
    return "Stable";
}

std::string Kernel::size() const {
    if (m_pkg.type == "manual" || m_pkg.type == "vkpurge") {
        std::uintmax_t totalBytes = 0;
        const std::vector<std::string> files = {
            "/boot/vmlinuz-" + m_pkg.version,
            "/boot/initramfs-" + m_pkg.version + ".img",
            "/boot/initrd.img-" + m_pkg.version,
            "/boot/System.map-" + m_pkg.version,
            "/boot/config-" + m_pkg.version
        };
        std::error_code ec;
        for (const auto &path : files) {
            if (std::filesystem::exists(path, ec) && std::filesystem::is_regular_file(path, ec)) {
                totalBytes += std::filesystem::file_size(path, ec);
            }
        }
        if (totalBytes == 0) return "Unknown";
        const std::uintmax_t mb = totalBytes / (1024 * 1024);
        if (mb > 0) return std::to_string(mb) + "MB";
        return std::to_string(totalBytes / 1024) + "KB";
    }
    if (!m_pkg.is_installed) return "Unknown";
    std::string s = utils::execProgram("xbps-query", {"-p", "installed_size", m_pkg.name});
    if (s.empty() || s.find("not found") != std::string::npos) return "Unknown";
    return s;
}

std::string Kernel::installDate() const {
    if (m_pkg.type == "manual" || m_pkg.type == "vkpurge") return "N/A";
    if (!m_pkg.is_installed) return "N/A";
    std::string d = utils::execProgram("xbps-query", {"-p", "install-date", m_pkg.name});
    if (d.empty() || d.find("not found") != std::string::npos) return "N/A";
    return d;
}

// Verifica si los archivos de un kernel existen en /boot y /lib/modules
static bool kernelFilesExist(const std::string &version) {
    // Verificar vmlinuz en /boot (OBLIGATORIO)
    std::string vmlinuzPath = "/boot/vmlinuz-" + version;
    bool hasVmlinuz = utils::fileExists(vmlinuzPath);
    
    // Si no hay vmlinuz, el kernel no es funcional, devolvemos false
    if (!hasVmlinuz) {
        return false;
    }
    
    // Si existe vmlinuz, consideramos que el kernel tiene archivos
    // (no es necesario comprobar módulos porque el kernel ya es funcional con vmlinuz)
    return true;
}

std::vector<Kernel> Kernel::getKernels() {
    std::vector<Kernel> kernels;
    std::set<std::string> knownNames;
    std::set<std::string> knownVersions;
    std::map<std::string, std::string> installedPkgs; // nombre -> pkgver (estado del paquete)
    std::map<std::string, std::string> diskVersions;  // paquete -> release real encontrada en disco
    std::map<std::string, bool> manualCandidates;     // version -> tiene vmlinuz en /boot
    std::map<std::string, bool> vkpurgeCandidates;    // version -> huérfano detectado por vkpurge

    // Consultar kernels antiguos rastreados por vkpurge
    std::set<std::string> vkpurgeSet;
    if (utils::commandExists("vkpurge")) {
        std::string vkOut = utils::execProgram("vkpurge", {"list"});
        std::vector<std::string> vkLines = utils::split(vkOut, '\n');
        for (const auto &vkLine : vkLines) {
            std::string t = utils::trim(vkLine);
            if (!t.empty() && std::isdigit(static_cast<unsigned char>(t[0]))) {
                vkpurgeSet.insert(t);
            }
        }
    }

    auto detectKernelRepo = [](const std::string &pkgName, bool installed) -> std::string {
        if (pkgName.rfind("kernel-neko", 0) == 0 || pkgName.rfind("linux-neko", 0) == 0) {
            return "Neko Void";
        }
        if (pkgName.rfind("linux-cachy", 0) == 0) {
            return "CachyOS / Void";
        }
        if (!utils::commandExists("xbps-query")) {
            return "System";
        }
        std::string rawRepo = installed
            ? utils::execProgram("xbps-query", {"-p", "repository", pkgName})
            : utils::execProgram("xbps-query", {"-R", "-p", "repository", pkgName});
        std::string repoUrl = utils::trim(rawRepo);
        if (repoUrl.find("neko-void") != std::string::npos || repoUrl.find("sourceforge.net") != std::string::npos) {
            return "Neko Void";
        }
        if (repoUrl.find("voidlinux.org") != std::string::npos) {
            return "Void Official";
        }
        if (repoUrl.rfind("/", 0) == 0 || repoUrl.rfind("file:", 0) == 0) {
            return "Local Repo";
        }
        if (!repoUrl.empty()) {
            return repoUrl;
        }
        return "Void Repository";
    };

    auto isKernelPackage = [](const std::string &name) -> bool {
        bool isLinux = (name.rfind("linux", 0) == 0);
        bool isKernel = (name.rfind("kernel-", 0) == 0);
        if (!isLinux && !isKernel) return false;

        static const std::vector<std::string> excludedNames = {
            "linux-api-headers", "linux-firmware", "linux-libre", "linux-base",
            "linux-user", "linux-atm", "linux-utils", "linux-gpib", "linux-pam",
            "linux-container", "linux-kmod", "linux-driver-management",
            "linux-driver-management-32bit", "linux-vt-setcolors", "linux-wifi-hotspot",
            "kernel-libc-headers"
        };
        for (const auto &excluded : excludedNames) {
            if (name == excluded) return false;
        }

        if (name.rfind("linux-firmware-", 0) == 0) return false;
        if (name.find("-headers") != std::string::npos ||
            name.find("-devel") != std::string::npos ||
            name.find("-dbg") != std::string::npos ||
            name.find("-docs") != std::string::npos ||
            name.find("-tools") != std::string::npos ||
            name.find("-common") != std::string::npos ||
            name.find("-progs") != std::string::npos) return false;

        static const std::vector<std::string> allowedNames = {
            "linux", "linux-lts", "linux-mainline", "linux-zen", "linux-rt", "linux-hardened"
        };
        for (const auto &allowed : allowedNames) {
            if (name == allowed) return true;
        }

        if (name.rfind("linux-neko", 0) == 0 || name.rfind("kernel-neko", 0) == 0 || name.rfind("linux-cachy", 0) == 0) return true;
        if (name.rfind("kernel-", 0) == 0) return true;
        if (name.rfind("linux-manual-", 0) == 0 || name.rfind("linux-orphan-", 0) == 0) return true;

        if (name.size() > 5 && std::isdigit(static_cast<unsigned char>(name[5]))) return true;
        if (name.rfind("linux-", 0) == 0 && name.size() > 6 && std::isdigit(static_cast<unsigned char>(name[6]))) return true;
        if (name.rfind("linux-", 0) == 0) return true;

        return false;
    };

    // Devuelve el nombre del paquete propietario de un archivo
    auto getOwnerPackage = [&installedPkgs](const std::string &filePath) -> std::string {
        if (!utils::commandExists("xbps-query")) return "";
        std::string res = utils::execProgram("xbps-query", {"-o", filePath});
        if (res.empty() || res.find("not owned") != std::string::npos || res.find("No such file") != std::string::npos) return "";
        size_t colon_pos = res.find(':');
        if (colon_pos == std::string::npos) return "";
        std::string full_pkg = res.substr(0, colon_pos);
        for (const auto &pair : installedPkgs) {
            if (full_pkg == pair.first + "-" + pair.second) return pair.first;
        }
        size_t hyphen_pos = full_pkg.find_last_of('-');
        if (hyphen_pos == std::string::npos || hyphen_pos == 0) return "";
        return full_pkg.substr(0, hyphen_pos);
    };

    // Fase 1: estado de paquetes instalados. Solo se aceptan paquetes con estado 'ii'
    if (utils::commandExists("xbps-query")) {
        std::string installedOutput = utils::execProgram("xbps-query", {"-l"});
        std::vector<std::string> installedLines = utils::split(installedOutput, '\n');
        for (const auto &line : installedLines) {
            if (line.size() < 4) continue;
            std::istringstream iss(line);
            std::string status, full_pkg;
            iss >> status >> full_pkg;
            if (full_pkg.empty() || status != "ii") continue;

            size_t hyphen_pos = full_pkg.find_last_of('-');
            if (hyphen_pos == std::string::npos || hyphen_pos == 0) continue;

            std::string pkg_name = full_pkg.substr(0, hyphen_pos);
            std::string version = full_pkg.substr(hyphen_pos + 1);

            if (isKernelPackage(pkg_name)) {
                installedPkgs[pkg_name] = version;
            }
        }

        // En Void Linux, 'linux', 'linux-lts' y 'linux-mainline' son metapaquetes vacíos de conveniencia.
        // Si el paquete real concreto (ej. linux6.18) ya está instalado, evitar listar el metapaquete duplicado.
        if (installedPkgs.count("linux")) {
            for (const auto &p : installedPkgs) {
                if (p.first != "linux" && p.first.rfind("linux", 0) == 0 && p.first.size() > 5 && std::isdigit(static_cast<unsigned char>(p.first[5]))) {
                    installedPkgs.erase("linux");
                    break;
                }
            }
        }
        if (installedPkgs.count("linux-lts") && installedPkgs.count("linux6.12")) {
            installedPkgs.erase("linux-lts");
        }
        if (installedPkgs.count("linux-mainline") && installedPkgs.count("linux7.3")) {
            installedPkgs.erase("linux-mainline");
        }
    }

    // Fase 2: escaneo de /boot para resolución de releases reales y detección de huérfanos
    std::error_code fsEc;
    std::filesystem::path bootPath("/boot");
    if (std::filesystem::exists(bootPath, fsEc) && std::filesystem::is_directory(bootPath, fsEc)) {
        for (const auto &entry : std::filesystem::directory_iterator(bootPath, fsEc)) {
            if (fsEc) break;
            if (!entry.is_regular_file(fsEc) && !entry.is_symlink(fsEc)) continue;
            std::string filename = entry.path().filename().string();
            if (filename.rfind("vmlinuz-", 0) != 0) continue;
            std::string version = filename.substr(std::string("vmlinuz-").size());
            if (version.empty() || version == "old" || version.rfind(".old") != std::string::npos || version.rfind(".bak") != std::string::npos) continue;
            if (!std::isdigit(static_cast<unsigned char>(version[0]))) continue;

            std::string ownerPkg = getOwnerPackage(entry.path().string());
            if (!ownerPkg.empty() && isKernelPackage(ownerPkg)) {
                if (!diskVersions.count(ownerPkg)) diskVersions[ownerPkg] = version;
                manualCandidates.erase(version);
                vkpurgeCandidates.erase(version);
                continue;
            }

            if (!knownVersions.count(version)) {
                if (vkpurgeSet.count(version)) {
                    vkpurgeCandidates[version] = true;
                } else {
                    manualCandidates[version] = true;
                }
            }
        }
    }

    // Fase 3: escaneo de /usr/lib/modules (o /lib/modules)
    std::filesystem::path modulesPath("/usr/lib/modules");
    if (!std::filesystem::exists(modulesPath, fsEc)) {
        modulesPath = "/lib/modules";
    }
    if (std::filesystem::exists(modulesPath, fsEc) && std::filesystem::is_directory(modulesPath, fsEc)) {
        for (const auto &entry : std::filesystem::directory_iterator(modulesPath, fsEc)) {
            if (fsEc) break;
            if (!entry.is_directory(fsEc)) continue;
            std::string version = entry.path().filename().string();
            if (version.empty() || !std::isdigit(static_cast<unsigned char>(version[0]))) continue;
            if (knownVersions.count(version)) continue;

            std::string ownerPkg;
            std::string probeFile = entry.path().string() + "/modules.dep";
            if (utils::fileExists(probeFile)) {
                ownerPkg = getOwnerPackage(probeFile);
            }
            if (!ownerPkg.empty() && isKernelPackage(ownerPkg)) {
                if (!diskVersions.count(ownerPkg)) diskVersions[ownerPkg] = version;
                manualCandidates.erase(version);
                vkpurgeCandidates.erase(version);
            }
        }
    }

    // Fase 4: emitir kernels instalados
    for (const auto &pair : installedPkgs) {
        const std::string &pkg_name = pair.first;
        const std::string &pkgver = pair.second;
        std::string version = pkgver;
        auto it = diskVersions.find(pkg_name);
        if (it != diskVersions.end()) version = it->second;

        bool hasFiles = kernelFilesExist(version);
        std::string repoName = detectKernelRepo(pkg_name, true);
        Package pkg{pkg_name, version, repoName, "xbps", true, hasFiles};
        Package headers{pkg_name + "-headers", pkgver, repoName, "xbps", true, true};
        kernels.emplace_back(pkg, headers);
        knownNames.insert(pkg_name);
        knownVersions.insert(version);
    }

    // Fase 5: kernels presentes en disco con dueño conocido pero sin paquete en installedPkgs
    for (const auto &pair : diskVersions) {
        if (installedPkgs.count(pair.first) || knownNames.count(pair.first)) continue;
        const std::string &pkg_name = pair.first;
        const std::string &version = pair.second;
        bool hasFiles = kernelFilesExist(version);
        std::string repoName = detectKernelRepo(pkg_name, false);
        Package pkg{pkg_name, version, repoName, "xbps", true, hasFiles};
        Package headers{pkg_name + "-headers", version, repoName, "xbps", true, true};
        kernels.emplace_back(pkg, headers);
        knownNames.insert(pkg_name);
        knownVersions.insert(version);
    }

    // Fase 6: kernels disponibles en los repositorios configurados del sistema
    if (utils::commandExists("xbps-query")) {
        std::string repoOutput;
        repoOutput += utils::execProgram("xbps-query", {"-Rs", "linux"});
        repoOutput += "\n" + utils::execProgram("xbps-query", {"-Rs", "kernel"});
        repoOutput += "\n" + utils::execProgram("xbps-query", {"-Rs", "neko"});
        std::vector<std::string> repoLines = utils::split(repoOutput, '\n');

        for (const auto &line_raw : repoLines) {
            std::string line = line_raw;
            if (line.empty()) continue;

            std::istringstream iss(line);
            std::string status, full_pkg;
            iss >> status >> full_pkg;
            if (full_pkg.empty()) continue;

            size_t hyphen_pos = full_pkg.find_last_of('-');
            if (hyphen_pos == std::string::npos || hyphen_pos == 0) continue;

            std::string pkg_name = full_pkg.substr(0, hyphen_pos);
            std::string version = full_pkg.substr(hyphen_pos + 1);

            if (!isKernelPackage(pkg_name)) continue;
            if (knownNames.count(pkg_name)) continue;

            if (pkg_name == "linux") {
                bool hasSeries = false;
                for (const auto &kn : knownNames) {
                    if (kn != "linux" && kn.rfind("linux", 0) == 0 && kn.size() > 5 && std::isdigit(static_cast<unsigned char>(kn[5]))) {
                        hasSeries = true;
                        break;
                    }
                }
                if (hasSeries) continue;
            }

            bool installed = (installedPkgs.count(pkg_name) > 0) || (status == "[*]");
            bool hasFiles = false;
            if (installed) {
                std::string diskVer = version;
                auto it = diskVersions.find(pkg_name);
                if (it != diskVersions.end()) diskVer = it->second;
                hasFiles = kernelFilesExist(diskVer);
            }

            std::string repoName = detectKernelRepo(pkg_name, false);
            Package pkg{pkg_name, version, repoName, "xbps", installed, hasFiles};
            Package headers{pkg_name + "-headers", version, repoName, "xbps", installed, true};
            kernels.emplace_back(pkg, headers);
            knownNames.insert(pkg_name);
            knownVersions.insert(version);
        }
    }

    // Fase 7: kernels obsoletos rastreados por vkpurge
    for (const auto &candidate : vkpurgeCandidates) {
        const std::string &version = candidate.first;
        if (knownVersions.count(version)) continue;
        std::string orphanName = "linux-orphan-" + version;
        if (knownNames.count(orphanName)) continue;

        Package pkg{orphanName, version, "System (vkpurge)", "vkpurge", true, candidate.second};
        Package headers{"none", "none", "System (vkpurge)", "vkpurge", false, false};
        kernels.emplace_back(pkg, headers);
        knownNames.insert(orphanName);
        knownVersions.insert(version);
    }

    // Fase 8: kernels instalados manualmente a mano en /boot
    for (const auto &candidate : manualCandidates) {
        const std::string &version = candidate.first;
        if (knownVersions.count(version)) continue;
        std::string manualName = "linux-manual-" + version;
        if (knownNames.count(manualName)) continue;

        Package pkg{manualName, version, "Local /boot", "manual", true, candidate.second};
        Package headers{"none", "none", "Local /boot", "manual", false, false};
        kernels.emplace_back(pkg, headers);
        knownNames.insert(manualName);
        knownVersions.insert(version);
    }

    static const std::vector<std::string> priorityOrder = {
        "kernel-neko-bore", "kernel-neko", "linux-neko-rt", "linux-neko-zen", "linux-neko-lts",
        "linux-neko-rolling", "linux-neko", "linux-cachy-void", "linux-mainline", "linux-lts", "linux"
    };
    auto priority = [](const std::string &name) -> int {
        for (size_t i = 0; i < priorityOrder.size(); ++i) {
            if (name == priorityOrder[i] || name.rfind(priorityOrder[i], 0) == 0) {
                return static_cast<int>(i);
            }
        }
        return static_cast<int>(priorityOrder.size());
    };

    // Primero los instalados, y luego por prioridad de distro
    std::stable_sort(kernels.begin(), kernels.end(),
        [&priority](const Kernel &a, const Kernel &b) {
            if (a.is_installed() != b.is_installed()) {
                return a.is_installed() > b.is_installed();
            }
            return priority(a.name()) < priority(b.name());
        });

    return kernels;
}