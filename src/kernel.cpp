#include "kernel.hpp"
#include "utils.hpp"

#include <filesystem>
#include <sstream>
#include <set>
#include <map>
#include <algorithm>

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

bool Kernel::install() {
    // La lógica real está en KernelBridge::installKernel
    return true;
}

bool Kernel::remove() {
    // La lógica real está en KernelBridge::removeKernel
    return true;
}

std::string Kernel::category() const {
    if (m_pkg.type == "manual" || 
        m_pkg.name.find("linux-neko") != std::string::npos ||
        m_pkg.name.find("linux-cachy-void") != std::string::npos) return "Custom";
    if (m_pkg.name.find("lts") != std::string::npos) return "Longterm";
    if (m_pkg.name.find("rt") != std::string::npos) return "Realtime";
    if (m_pkg.name.find("zen") != std::string::npos) return "Zen";
    if (m_pkg.name.find("hardened") != std::string::npos) return "Hardened";
    if (m_pkg.name.find("mainline") != std::string::npos) return "Mainline";
    return "Stable";
}

std::string Kernel::size() const {
    if (m_pkg.type == "manual") return "Unknown";
    std::string s = utils::exec("xbps-query -p installed_size " + m_pkg.name);
    if (s.empty() || s.find("not found") != std::string::npos) return "Unknown";
    return s;
}

std::string Kernel::installDate() const {
    if (m_pkg.type == "manual") return "N/A";
    std::string d = utils::exec("xbps-query -p install-date " + m_pkg.name);
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

    auto isKernelPackage = [](const std::string &name) -> bool {
        if (name.rfind("linux", 0) != 0) return false;

        static const std::vector<std::string> excludedNames = {
            "linux-api-headers", "linux-firmware", "linux-libre", "linux-base",
            "linux-user", "linux-atm", "linux-utils", "linux-gpib", "linux-pam",
            "linux-container", "linux-kmod", "linux-driver-management",
            "linux-driver-management-32bit", "linux-vt-setcolors", "linux-wifi-hotspot"
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

        if (name.rfind("linux-neko", 0) == 0 || name.rfind("linux-cachy", 0) == 0) return true;
        if (name.rfind("linux-manual-", 0) == 0) return true;

        if (name.size() > 5 && std::isdigit(name[5])) return true;
        if (name.rfind("linux-", 0) == 0 && name.size() > 6 && std::isdigit(name[6])) return true;

        return false;
    };

    // Devuelve el nombre del paquete propietario de un archivo. Primero intenta
    // casar "nombre-version" exacto contra los paquetes instalados (soporta pkgvers
    // con guiones); si no, cae al split en el último guión.
    auto getOwnerPackage = [&installedPkgs](const std::string &filePath) -> std::string {
        if (!utils::commandExists("xbps-query")) return "";
        std::string res = utils::exec("xbps-query -o " + filePath);
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

    // Fase 1: estado de paquetes instalados. La condición "instalado" depende del
    // NOMBRE del paquete listado por xbps-query -l, nunca de la existencia de archivos.
    if (utils::commandExists("xbps-query")) {
        std::string installedOutput = utils::exec("xbps-query -l");
        std::vector<std::string> installedLines = utils::split(installedOutput, '\n');
        for (const auto &line : installedLines) {
            if (line.size() < 4) continue;
            std::istringstream iss(line);
            std::string status, full_pkg;
            iss >> status >> full_pkg;
            if (full_pkg.empty()) continue;

            size_t hyphen_pos = full_pkg.find_last_of('-');
            if (hyphen_pos == std::string::npos || hyphen_pos == 0) continue;

            std::string pkg_name = full_pkg.substr(0, hyphen_pos);
            std::string version = full_pkg.substr(hyphen_pos + 1);

            if (isKernelPackage(pkg_name)) {
                installedPkgs[pkg_name] = version;
            }
        }
    }

    // Fase 2: escaneo de /boot. Para cada vmlinuz se resuelve su paquete propietario
    // y se guarda la release real del kernel en disco (p. ej. 7.2.0-rt-neko+), que
    // puede diferir del pkgver del paquete (p. ej. 7.2.0_2).
    std::filesystem::path bootPath("/boot");
    if (std::filesystem::exists(bootPath) && std::filesystem::is_directory(bootPath)) {
        for (const auto &entry : std::filesystem::directory_iterator(bootPath)) {
            if (!entry.is_regular_file() && !entry.is_symlink()) continue;
            std::string filename = entry.path().filename().string();
            if (filename.rfind("vmlinuz-", 0) != 0) continue;
            std::string version = filename.substr(std::string("vmlinuz-").size());
            if (version.empty() || version == "old" || version.rfind(".old") != std::string::npos || version.rfind(".bak") != std::string::npos) continue;
            if (!std::isdigit(version[0])) continue;

            std::string ownerPkg = getOwnerPackage(entry.path().string());
            if (!ownerPkg.empty() && isKernelPackage(ownerPkg)) {
                if (!diskVersions.count(ownerPkg)) diskVersions[ownerPkg] = version;
                manualCandidates.erase(version);
                continue;
            }
            if (!knownVersions.count(version)) manualCandidates[version] = true;
        }
    }

    // Fase 3: escaneo de /usr/lib/modules (o /lib/modules) para completar el mapa de
    // paquetes instalados que el escaneo de /boot no haya cubierto. xbps-query -o no
    // reporta directorios, así que se consulta un archivo habitual del directorio.
    // Un directorio de módulos sin vmlinuz en /boot es solo un resto huérfano y NO
    // se lista como kernel instalado.
    std::filesystem::path modulesPath("/usr/lib/modules");
    if (!std::filesystem::exists(modulesPath)) {
        modulesPath = "/lib/modules";
    }
    if (std::filesystem::exists(modulesPath) && std::filesystem::is_directory(modulesPath)) {
        for (const auto &entry : std::filesystem::directory_iterator(modulesPath)) {
            if (!entry.is_directory()) continue;
            std::string version = entry.path().filename().string();
            if (version.empty() || !std::isdigit(version[0])) continue;
            if (knownVersions.count(version)) continue;

            std::string ownerPkg;
            std::string probeFile = entry.path().string() + "/modules.dep";
            if (utils::fileExists(probeFile)) {
                ownerPkg = getOwnerPackage(probeFile);
            }
            if (!ownerPkg.empty() && isKernelPackage(ownerPkg)) {
                if (!diskVersions.count(ownerPkg)) diskVersions[ownerPkg] = version;
                manualCandidates.erase(version);
            }
        }
    }

    // Fase 4: emitir kernels instalados. "installed" proviene del estado del paquete;
    // la versión mostrada es la release real en disco cuando se encontró.
    for (const auto &pair : installedPkgs) {
        const std::string &pkg_name = pair.first;
        const std::string &pkgver = pair.second;
        std::string version = pkgver;
        auto it = diskVersions.find(pkg_name);
        if (it != diskVersions.end()) version = it->second;

        bool hasFiles = kernelFilesExist(version);
        Package pkg{pkg_name, version, "void", "xbps", true, hasFiles};
        Package headers{pkg_name + "-headers", pkgver, "void", "xbps", true, true};
        kernels.emplace_back(pkg, headers);
        knownNames.insert(pkg_name);
        knownVersions.insert(version);
    }

    // Fase 5: kernels presentes en disco (con dueño conocido) pero sin paquete instalado.
    for (const auto &pair : diskVersions) {
        if (installedPkgs.count(pair.first) || knownNames.count(pair.first)) continue;
        const std::string &pkg_name = pair.first;
        const std::string &version = pair.second;
        bool hasFiles = kernelFilesExist(version);
        Package pkg{pkg_name, version, "void", "xbps", true, hasFiles};
        Package headers{pkg_name + "-headers", version, "void", "xbps", true, true};
        kernels.emplace_back(pkg, headers);
        knownNames.insert(pkg_name);
        knownVersions.insert(version);
    }

    // Fase 6: kernels disponibles en los repositorios pero no instalados.
    if (utils::commandExists("xbps-query")) {
        std::string repoOutput;
        repoOutput += utils::exec("xbps-query -Rs linux");
        repoOutput += "\n" + utils::exec("xbps-query -Rs linux-neko");
        repoOutput += "\n" + utils::exec("xbps-query -Rs linux-cachy-void");
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

            bool installed = installedPkgs.count(pkg_name) > 0;
            bool hasFiles = false;
            if (installed) {
                std::string diskVer = version;
                auto it = diskVersions.find(pkg_name);
                if (it != diskVersions.end()) diskVer = it->second;
                hasFiles = kernelFilesExist(diskVer);
            }

            Package pkg{pkg_name, version, "void", "xbps", installed, hasFiles};
            Package headers{pkg_name + "-headers", version, "void", "xbps", installed, true};
            kernels.emplace_back(pkg, headers);
            knownNames.insert(pkg_name);
            knownVersions.insert(version);
        }
    }

    // Fase 7: kernels que no pertenecen a ningún paquete (instalados a mano).
    for (const auto &candidate : manualCandidates) {
        const std::string &version = candidate.first;
        if (knownVersions.count(version)) continue;
        std::string manualName = "linux-manual-" + version;
        if (knownNames.count(manualName)) continue;

        Package pkg{manualName, version, "local", "manual", true, candidate.second};
        Package headers{"none", "none", "local", "manual", false, false};
        kernels.emplace_back(pkg, headers);
        knownNames.insert(manualName);
        knownVersions.insert(version);
    }

    static const std::vector<std::string> priorityOrder = {
        "linux-neko-rt", "linux-neko-zen", "linux-cachy-void", "linux-mainline", "linux-lts"
    };
    auto priority = [](const std::string &name) -> int {
        for (size_t i = 0; i < priorityOrder.size(); ++i) {
            if (name == priorityOrder[i] || name.rfind(priorityOrder[i], 0) == 0) {
                return static_cast<int>(i);
            }
        }
        return static_cast<int>(priorityOrder.size());
    };

    std::stable_sort(kernels.begin(), kernels.end(),
        [&priority](const Kernel &a, const Kernel &b) {
            return priority(a.name()) < priority(b.name());
        });

    return kernels;
}