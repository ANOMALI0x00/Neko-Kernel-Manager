#include "kernel-bridge.hpp"
#include "kernel.hpp"
#include <QtConcurrent/QtConcurrent>
#include <QDateTime>
#include "utils.hpp"
#include <QDir>
#include <QStandardPaths>
#include <QFile>
#include <QTimer>
#include <QTextStream>
#include <filesystem>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>

KernelBridge::KernelBridge(QObject *parent) : QObject(parent), m_verbose(false) {
}

KernelBridge::~KernelBridge() {
    int res = system("rm -rf /tmp/neko-kernel-*");
    (void)res;
}

void KernelBridge::setBusy(bool b) {
    if (b) {
        ++m_busyCount;
        if (!m_busy) {
            m_busy = true;
            emit busyChanged();
        }
    } else {
        if (m_busyCount > 0) {
            --m_busyCount;
        }
        if (m_busyCount == 0 && m_busy) {
            m_busy = false;
            emit busyChanged();
            setProgress(0);
        }
    }
}

void KernelBridge::setStatusMessage(const QString &message, bool isError) {
    m_statusMessage = message;
    m_statusIsError = isError;
    emit statusMessageChanged();
}

QString KernelBridge::activeKernelVersion() const {
    std::string out = utils::exec("uname -r");
    return QString::fromStdString(out).trimmed();
}

QString KernelBridge::detectedCpuLevel() const {
    return QString::fromStdString(utils::detectCpuLevel());
}

// Void Linux usa GRUB como bootloader por defecto
QString KernelBridge::detectedBootloader() const {
    return "GRUB";
}

QVariantList KernelBridge::getKernels() {
    QVariantList list;
    auto kernels = Kernel::getKernels();
    for (const auto &k : kernels) {
        QVariantMap map;
        map["name"] = QString::fromStdString(k.name());
        map["version"] = QString::fromStdString(k.version());
        map["category"] = QString::fromStdString(k.category());
        map["size"] = QString::fromStdString(k.size());
        map["installDate"] = QString::fromStdString(k.installDate());
        map["installed"] = k.is_installed();
        map["hasFiles"] = k.has_files();
        map["type"] = QString::fromStdString(k.type());
        list.append(map);
    }
    return list;
}

void KernelBridge::updateKernels() {
    setBusy(true);
    setProgress(15);
    setStatusMessage("Scanning installed & available kernels...", false);
    appendLog("Scanning system for installed and available kernels...");

    auto future = QtConcurrent::run([this]() {
        QMetaObject::invokeMethod(this, [this]() { setProgress(40); });

        auto newList = getKernels();

        QMetaObject::invokeMethod(this, [this, newList]() {
            m_kernelsCache = newList;
            setProgress(75);
            emit kernelsChanged();
            appendLog(QString("Kernel scan completed. Found %1 kernel entries:").arg(newList.size()));
            for (const auto &var : newList) {
                QVariantMap m = var.toMap();
                appendLog(QString("  • %1 (%2) [Type: %3, Installed: %4]")
                            .arg(m["name"].toString())
                            .arg(m["version"].toString())
                            .arg(m["type"].toString())
                            .arg(m["installed"].toBool() ? "Yes" : "No"));
            }
        });

        // --- REPOSITORY CREATION REMOVED ---
        // The custom Neko repository is already part of the system (Neko Void).
        // The application must not create or modify any repository files.

        QMetaObject::invokeMethod(this, [this, newList]() {
            m_kernelsCache = newList;
            setProgress(100);
            setStatusMessage("Kernels loaded successfully", false);
            setBusy(false);
            emit kernelsChanged();
            QTimer::singleShot(1000, this, [this]() { setProgress(0); });
        });
    });
    (void)future;
}

void KernelBridge::appendLog(const QString &line) {
    if (QThread::currentThread() != this->thread()) {
        QMetaObject::invokeMethod(this, [this, line]() { appendLog(line); });
        return;
    }

    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    QString formattedLine = "[" + timestamp + "] ";

    if (m_verbose) {
        formattedLine += "[VERBOSE] ";
    }

    if (line.toLower().contains("error") || line.toLower().contains("failed")) {
        formattedLine += "<font color='#ff5555'>" + line + "</font>";
    } else if (line.toLower().contains("success") || line.toLower().contains("complete") || line.toLower().contains("finished")) {
        formattedLine += "<font color='#50fa7b'>" + line + "</font>";
    } else if (line.toLower().contains("warning")) {
        formattedLine += "<font color='#f1fa8c'>" + line + "</font>";
    } else if (line.startsWith("Executing:") || line.startsWith("Starting") || line.startsWith("Initiating") || line.startsWith("Scanning") || line.startsWith("Requesting")) {
        formattedLine += "<font color='#bd93f9'>" + line + "</font>";
    } else {
        formattedLine += line;
    }

    m_logs += formattedLine + "<br>";
    if (m_logs.length() > 60000) m_logs = m_logs.right(45000);
    emit logsChanged();
}

void KernelBridge::exportLogs(const QString &filePath) {
    if (m_logs.isEmpty()) {
        appendLog("WARNING: No logs to export.");
        setStatusMessage("No logs to export", true);
        return;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        appendLog("ERROR: Could not open file for writing: " + filePath);
        setStatusMessage("Failed to export logs", true);
        return;
    }

    QTextStream out(&file);
    QString plainLogs = m_logs;
    plainLogs.replace(QRegularExpression("<font[^>]*>"), "");
    plainLogs.replace("</font>", "");
    plainLogs.replace("<br>", "\n");
    out << plainLogs;
    file.close();

    appendLog("Logs exported successfully to: " + filePath);
    setStatusMessage("Logs exported to " + filePath, false);
}

void KernelBridge::installKernel(const QString &name) {
    appendLog("Starting installation of kernel package: " + name);
    setBusy(true);
    setProgress(10);

    auto future = QtConcurrent::run([this, name]() {
        std::string pkgName = name.toStdString();
        std::string headersPkg = pkgName + "-headers";

        std::string cmd = "pkexec sh -c \"xbps-install -y " + pkgName + " && (xbps-install -y " + headersPkg + " || true)\"";
        appendLog("Executing: " + QString::fromStdString(cmd));

        auto result = utils::execWithOutput(cmd);
        if (!result.stdout.empty()) {
            appendLog(QString::fromStdString(result.stdout));
        }
        if (!result.stderr.empty()) {
            appendLog(QString::fromStdString(result.stderr));
        }
        bool success = (result.exitCode == 0);

        QMetaObject::invokeMethod(this, [this, success, name]() {
            if (success) {
                appendLog("Kernel installation finished successfully: " + name);
                setStatusMessage("Kernel installed successfully", false);
            } else {
                appendLog("ERROR: Main kernel installation failed for " + name);
                setStatusMessage("Installation failed", true);
            }
            setBusy(false);
            updateKernels();
            updateDkmsModules();
            updateDefaultKernel();
        });
    });
    (void)future;
}

// ----------------------------------------------------------------------------
// removeKernel – CORREGIDA con detección de error por dependencias
// ----------------------------------------------------------------------------
void KernelBridge::removeKernel(const QString &name, const QString &type, const QString &version) {
    bool isManual = (type.toLower() == "manual");
    QString runningVer = activeKernelVersion().trimmed();
    QString targetVersion = version;
    if (targetVersion.isEmpty() && isManual && name.startsWith("linux-manual-")) {
        targetVersion = name.mid(QString("linux-manual-").size());
    }
    if (targetVersion.isEmpty()) {
        targetVersion = name;
    }

    if (!runningVer.isEmpty()) {
        QString verToCheck = isManual ? targetVersion : name;
        if (verToCheck.contains(runningVer) || runningVer.contains(verToCheck) || name.contains(runningVer)) {
            appendLog("SECURITY WARNING: Attempted to uninstall currently running kernel (" + runningVer + "). Operation aborted. Reboot into a different kernel before removing it.");
            setStatusMessage("Cannot remove currently running kernel! Reboot into another kernel first.", true);
            return;
        }
    }

    if (!isManual) {
        std::string pkgName = name.toStdString();
        if (!utils::packageInstalled(pkgName)) {
            appendLog("WARNING: Package " + name + " is not actually installed. Switching to manual removal of files.");
            isManual = true;
            setStatusMessage("Package not installed; removing files manually.", false);
        }
    }

    appendLog("Starting removal of kernel: " + name + " (Type: " + QString(isManual ? "Manual /boot" : "XBPS package") + ")");
    setBusy(true);
    setProgress(10);

    auto future = QtConcurrent::run([this, name, type, targetVersion, isManual]() {
        std::string cmd;
        std::string tempScriptPath;
        bool success = false;
        bool dependencyError = false;

        // Función auxiliar para borrar archivos manualmente (reutilizable)
        auto removeFilesManually = [&](const std::string &ver) -> bool {
            char tempPathTemplate[] = "/tmp/neko-kernel-remove-XXXXXX";
            int fd = mkstemp(tempPathTemplate);
            if (fd != -1) {
                tempScriptPath = tempPathTemplate;
                std::string script = "set -e\n"
                                     "rm -f /boot/vmlinuz-" + ver +
                                     " /boot/initramfs-" + ver + ".img /boot/initrd.img-" + ver +
                                     " /boot/config-" + ver + " /boot/System.map-" + ver +
                                     " /boot/vmlinuz-" + ver + ".old /boot/initramfs-" + ver + ".old.img\n"
                                     "rm -rf /usr/lib/modules/" + ver + " /lib/modules/" + ver + "\n"
                                     "(which grub-mkconfig >/dev/null 2>&1 && grub-mkconfig -o /boot/grub/grub.cfg >/dev/null 2>&1 || true)\n";
                ssize_t bytesWritten = write(fd, script.c_str(), script.size());
                (void)bytesWritten;
                (void)fchmod(fd, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
                close(fd);
                cmd = "pkexec sh " + tempScriptPath;
            } else {
                cmd = "pkexec sh -c \"if [ -n '" + ver + "' ]; then "
                      "rm -f /boot/vmlinuz-" + ver +
                      " /boot/initramfs-" + ver + ".img /boot/initrd.img-" + ver +
                      " /boot/config-" + ver + " /boot/System.map-" + ver +
                      " /boot/vmlinuz-" + ver + ".old /boot/initramfs-" + ver + ".old.img; "
                      "rm -rf /usr/lib/modules/" + ver + " /lib/modules/" + ver + "; "
                      "(which grub-mkconfig >/dev/null 2>&1 && grub-mkconfig -o /boot/grub/grub.cfg >/dev/null 2>&1 || true); "
                      "fi\"";
            }
            appendLog("Executing manual removal: " + QString::fromStdString(cmd));
            bool ok = utils::runPrivilegedCommand(cmd);
            if (!tempScriptPath.empty()) {
                unlink(tempScriptPath.c_str());
                tempScriptPath.clear();
            }
            return ok;
        };

        if (isManual) {
            // Ya es manual, borrar archivos directamente
            std::string ver = targetVersion.toStdString();
            success = removeFilesManually(ver);
        } else {
            // Intentar eliminar con xbps-remove
            std::string pkgName = name.toStdString();
            cmd = "pkexec sh -c \"xbps-remove -Rfy " + pkgName + " && (which grub-mkconfig >/dev/null 2>&1 && grub-mkconfig -o /boot/grub/grub.cfg || true)\"";
            appendLog("Executing xbps-remove: " + QString::fromStdString(cmd));
            auto result = utils::execWithOutput(cmd);
            if (!result.stdout.empty()) {
                appendLog(QString::fromStdString(result.stdout));
            }
            if (!result.stderr.empty()) {
                appendLog(QString::fromStdString(result.stderr));
            }
            success = (result.exitCode == 0);

            // Si falló, comprobar si fue por dependencias
            if (!success) {
                std::string output = result.stdout + result.stderr;
                if (output.find("breaks installed pkg") != std::string::npos ||
                    output.find("dependency") != std::string::npos ||
                    output.find("Transaction aborted") != std::string::npos) {
                    dependencyError = true;
                    appendLog("Kernel removal failed due to package dependencies. The package will remain installed, but its files will be removed.");
                    setStatusMessage("Dependency conflict: removing files only, keeping package.", false);
                    // Borrar archivos manualmente
                    std::string ver = targetVersion.toStdString();
                    success = removeFilesManually(ver);
                }
            }
        }

        QMetaObject::invokeMethod(this, [this, success, name, dependencyError]() {
            if (success) {
                if (dependencyError) {
                    appendLog("Kernel files removed successfully. Package " + name + " remains installed (without files).");
                    setStatusMessage("Kernel files removed (package kept due to dependencies)", false);
                } else {
                    appendLog("Kernel removal completed successfully: " + name);
                    setStatusMessage("Kernel removed successfully", false);
                }
            } else {
                appendLog("ERROR: Kernel removal failed for " + name);
                setStatusMessage("Removal failed", true);
            }
            setBusy(false);
            updateKernels();
            updateDkmsModules();
            updateDefaultKernel();
        });
    });
    (void)future;
}

void KernelBridge::vkpurge() {
    appendLog("Initiating vkpurge rm all to remove all old unreferenced kernels...");
    setBusy(true);
    setProgress(10);
    setStatusMessage("Purging all old kernels...", false);
    auto future = QtConcurrent::run([this]() {
        std::string cmd = "pkexec sh -c \"vkpurge rm all && (which grub-mkconfig >/dev/null 2>&1 && grub-mkconfig -o /boot/grub/grub.cfg || true)\"";

        QMetaObject::invokeMethod(this, [this]() {
            setProgress(30);
            setStatusMessage("Running vkpurge...", false);
        });

        auto result = utils::execWithOutput(cmd);
        if (!result.stdout.empty()) {
            appendLog(QString::fromStdString(result.stdout));
        }
        if (!result.stderr.empty()) {
            appendLog(QString::fromStdString(result.stderr));
        }
        bool success = (result.exitCode == 0);

        QMetaObject::invokeMethod(this, [this, success]() {
            setBusy(false);
            setProgress(success ? 100 : 0);
            if (success) {
                appendLog("vkpurge completed successfully.");
                setStatusMessage("Old kernels purged successfully", false);
                emit operationFinished("Kernels purged");
            } else {
                appendLog("ERROR: vkpurge failed.");
                setStatusMessage("vkpurge failed", true);
            }
            setBusy(false);
            updateKernels();
            updateDkmsModules();
            updateDefaultKernel();
        });
    });
    (void)future;
}

// DKMS Management Implementation
QVariantList KernelBridge::getDkmsModulesInternal() {
    QVariantList list;

    auto parseXbpsPackage = [](const std::string &fullPkg, std::string &outName, std::string &outVer) -> bool {
        if (fullPkg.empty()) return false;
        size_t pos = std::string::npos;
        for (size_t i = fullPkg.size(); i > 0; --i) {
            if (fullPkg[i - 1] == '-' && i < fullPkg.size() && std::isdigit(fullPkg[i])) {
                pos = i - 1;
                break;
            }
        }
        if (pos == std::string::npos) return false;
        outName = fullPkg.substr(0, pos);
        outVer = fullPkg.substr(pos + 1);
        return !outName.empty() && !outVer.empty();
    };

    auto normalizeDkmsName = [](const QString &name) -> QString {
        QString normalized = name.toLower();
        if (normalized.endsWith("-dkms")) {
            normalized.chop(5);
            int lastDash = normalized.lastIndexOf('-');
            if (lastDash != -1) {
                QString suffix = normalized.mid(lastDash + 1);
                bool allDigits = true;
                for (int i = 0; i < suffix.size(); ++i) {
                    if (!suffix[i].isDigit()) {
                        allDigits = false;
                        break;
                    }
                }
                if (allDigits) normalized = normalized.left(lastDash);
            }
        }
        return normalized;
    };

    // 1. Get installed/registered DKMS modules from local status
    QMap<QString, QVariantMap> activeModules;
    if (utils::commandExists("dkms")) {
        std::string rawOutput = utils::exec("dkms status 2>&1");
        if (!rawOutput.empty()) {
            std::vector<std::string> lines = utils::split(rawOutput, '\n');
            for (const auto &line_raw : lines) {
                if (line_raw.empty()) continue;
                QString line = QString::fromStdString(line_raw).trimmed();

                int colonIdx = line.indexOf(':');
                if (colonIdx == -1) continue;

                QString infoPart = line.left(colonIdx).trimmed();
                QString statusPart = line.mid(colonIdx + 1).trimmed();

                QStringList parts = infoPart.split(',');
                if (parts.size() < 2) continue;

                QString modAndVer = parts[0].trimmed();
                QString kernelVer = parts[1].trimmed();
                QString arch = parts.size() >= 3 ? parts[2].trimmed() : "all";

                int slashIdx = modAndVer.indexOf('/');
                QString modName = (slashIdx != -1) ? modAndVer.left(slashIdx) : modAndVer;
                QString modVersion = (slashIdx != -1) ? modAndVer.mid(slashIdx + 1) : "unknown";

                QVariantMap map;
                map["name"] = modName;
                map["version"] = modVersion;
                map["kernel"] = kernelVer;
                map["arch"] = arch;
                map["status"] = statusPart; // e.g. "installed", "built", "added"
                map["isDkmsPackage"] = false; // Registered local module
                map["packageInstalled"] = true;

                activeModules[normalizeDkmsName(modName) + "-" + kernelVer] = map;
                list.append(map);
            }
        }
    }

    // 2. Query installed and available DKMS packages from XBPS repositories in Void Linux
    if (utils::commandExists("xbps-query")) {
        std::string installedXbps = utils::exec("xbps-query -l | grep 'dkms' | awk '{print $2}'");
        std::string availableXbps = utils::exec("xbps-query -Rs 'dkms' | awk '{print $2}'");
        std::vector<std::string> xbpsLines;

        for (const auto &line : utils::split(installedXbps, '\n')) {
            if (!line.empty()) xbpsLines.push_back("ii " + line);
        }
        for (const auto &line : utils::split(availableXbps, '\n')) {
            if (!line.empty()) xbpsLines.push_back(line);
        }

        std::set<std::string> processedPkgs;

        for (const auto &xbpsLine : xbpsLines) {
            if (xbpsLine.empty()) continue;
            std::istringstream iss(xbpsLine);
            std::string status;
            std::string fullPkg;
            iss >> status >> fullPkg;
            if (fullPkg.empty()) {
                fullPkg = status;
                status.clear();
            }

            std::string pkgName;
            std::string pkgVersion;
            if (!parseXbpsPackage(fullPkg, pkgName, pkgVersion)) continue;

            if (pkgName.find("dkms") == std::string::npos || pkgName == "dkms") continue;
            if (processedPkgs.count(pkgName)) continue;
            processedPkgs.insert(pkgName);

            bool isInstalled = (status == "[*]" || status == "ii");

            QString modName = QString::fromStdString(pkgName);
            QString normalizedPackage = normalizeDkmsName(modName);

            bool alreadyRegistered = false;
            for (auto it = activeModules.constBegin(); it != activeModules.constEnd(); ++it) {
                if (it.key().startsWith(normalizedPackage + "-")) {
                    alreadyRegistered = true;
                    break;
                }
            }

            QVariantMap map;
            map["name"] = QString::fromStdString(pkgName); // Full package name
            map["version"] = QString::fromStdString(pkgVersion);
            map["kernel"] = ""; // Package entries are repo/package-level, not per-registered kernel
            map["arch"] = "all";
            map["status"] = isInstalled ? "unregistered" : "not installed";
            map["isDkmsPackage"] = true; // Flag for package management
            map["packageInstalled"] = isInstalled;
            if (!alreadyRegistered) {
                list.append(map);
            } else if (!isInstalled) {
                list.append(map);
            }
        }
    }

    return list;
}

void KernelBridge::updateDkmsModules() {
    setBusy(true);
    appendLog("Scanning DKMS modules and packages...");
    if (!utils::commandExists("dkms")) {
        appendLog("WARNING: 'dkms' command not found; local DKMS module detection is disabled.");
    }
    if (!utils::commandExists("xbps-query")) {
        appendLog("WARNING: 'xbps-query' command not found; Void DKMS package repository scan is disabled.");
    }
    auto future = QtConcurrent::run([this]() {
        auto dkmsList = getDkmsModulesInternal();
        QMetaObject::invokeMethod(this, [this, dkmsList]() {
            m_dkmsCache = dkmsList;
            setBusy(false);
            emit dkmsModulesChanged();
            appendLog(QString("DKMS scan completed. Found %1 modules/packages:").arg(dkmsList.size()));
            for (const auto &var : dkmsList) {
                QVariantMap m = var.toMap();
                appendLog(QString("  • %1 v%2 (Kernel: %3) [Status: %4]")
                            .arg(m["name"].toString())
                            .arg(m["version"].toString())
                            .arg(m["kernel"].toString())
                            .arg(m["status"].toString()));
            }
        });
    });
    (void)future;
}

void KernelBridge::installDkmsModule(const QString &name, const QString &version, const QString &kernel) {
    bool isPackage = name.endsWith("-dkms");
    if (isPackage) {
        appendLog("Starting installation of XBPS DKMS package: " + name);
    } else {
        appendLog("Starting installation of DKMS module: " + name + " v" + version + " for kernel " + kernel);
    }
    setBusy(true);

    auto future = QtConcurrent::run([this, name, version, kernel, isPackage]() {
        bool success = true;
        bool isNvidia = name.toLower().contains("nvidia");

        if (isPackage) {
            // Instalar paquete xbps
            std::string cmd = "pkexec xbps-install -y " + name.toStdString();
            appendLog("Executing xbps-install: " + QString::fromStdString(cmd));
            auto result = utils::execWithOutput(cmd);
            if (!result.stdout.empty()) {
                appendLog(QString::fromStdString(result.stdout));
            }
            if (!result.stderr.empty()) {
                appendLog(QString::fromStdString(result.stderr));
            }
            success = (result.exitCode == 0);
            if (success) {
                appendLog("XBPS DKMS package installed: " + name);
            } else {
                appendLog("ERROR: XBPS DKMS package installation failed for " + name);
            }
        } else {
            // Instalar módulo DKMS manualmente
            std::string cmd = "pkexec dkms install -m " + name.toStdString() +
                              " -v " + version.toStdString() +
                              " -k " + kernel.toStdString();
            appendLog("Executing dkms install: " + QString::fromStdString(cmd));
            auto result = utils::execWithOutput(cmd);
            if (!result.stdout.empty()) {
                appendLog(QString::fromStdString(result.stdout));
            }
            if (!result.stderr.empty()) {
                appendLog(QString::fromStdString(result.stderr));
            }
            success = (result.exitCode == 0);
        }

        // Manejo especial para NVIDIA si se instaló el paquete y es NVIDIA
        if (isNvidia && success && isPackage) {
            // Forzar instalación para kernels compatibles (>=6.18)
            std::string nvidiaVersion;
            std::string srcRoot = "/usr/src";
            if (std::filesystem::exists(srcRoot) && std::filesystem::is_directory(srcRoot)) {
                for (const auto &entry : std::filesystem::directory_iterator(srcRoot)) {
                    if (!entry.is_directory()) continue;
                    std::string nameStr = entry.path().filename().string();
                    if (nameStr.rfind("nvidia-", 0) == 0) {
                        nvidiaVersion = nameStr.substr(strlen("nvidia-"));
                        break;
                    }
                }
            }
            if (nvidiaVersion.empty()) {
                nvidiaVersion = version.toStdString();
                size_t underscorePos = nvidiaVersion.find('_');
                if (underscorePos != std::string::npos) {
                    nvidiaVersion = nvidiaVersion.substr(0, underscorePos);
                }
            }
            if (!nvidiaVersion.empty()) {
                std::string modulesRoot = "/usr/lib/modules";
                if (!utils::dirExists(modulesRoot)) {
                    modulesRoot = "/lib/modules";
                }
                if (utils::dirExists(modulesRoot)) {
                    bool installedAny = false;
                    for (const auto &entry : std::filesystem::directory_iterator(modulesRoot)) {
                        if (!entry.is_directory()) continue;
                        std::string kernelDir = entry.path().filename().string();
                        if (kernelDir.empty()) continue;

                        std::string versionStr;
                        for (char c : kernelDir) {
                            if (std::isdigit(c) || c == '.') versionStr.push_back(c);
                            else break;
                        }

                        bool needsForce = false;
                        if (!versionStr.empty()) {
                            std::vector<int> parts;
                            std::stringstream ss(versionStr);
                            std::string part;
                            while (std::getline(ss, part, '.')) {
                                if (part.empty()) break;
                                parts.push_back(std::stoi(part));
                            }
                            if (!parts.empty()) {
                                if (parts[0] > 6 || (parts[0] == 6 && parts.size() > 1 && parts[1] >= 18)) {
                                    needsForce = true;
                                }
                            }
                        }
                        if (!needsForce) continue;

                        std::string dkmsCmd = "pkexec dkms install -m nvidia -v " + nvidiaVersion + " -k " + kernelDir;
                        appendLog("Forcing NVIDIA DKMS install for kernel " + QString::fromStdString(kernelDir));
                        auto result2 = utils::execWithOutput(dkmsCmd);
                        if (!result2.stdout.empty()) {
                            appendLog(QString::fromStdString(result2.stdout));
                        }
                        if (!result2.stderr.empty()) {
                            appendLog(QString::fromStdString(result2.stderr));
                        }
                        if (result2.exitCode == 0) {
                            installedAny = true;
                        }
                    }
                    if (!installedAny) {
                        appendLog("WARNING: No NVIDIA kernel directories requiring forced install were found.");
                    }
                } else {
                    appendLog("ERROR: No kernel modules directory found for forced NVIDIA DKMS install.");
                }
            } else {
                appendLog("ERROR: Could not determine NVIDIA DKMS version.");
            }
        }

        QMetaObject::invokeMethod(this, [this, success, isPackage, name]() {
            if (success) {
                appendLog((isPackage ? "XBPS DKMS package installation finished: " : "DKMS module installation finished: ") + name);
                setStatusMessage(isPackage ? "XBPS DKMS Package installed" : "DKMS module installed", false);
            } else {
                appendLog((isPackage ? "ERROR: XBPS DKMS package installation failed: " : "ERROR: DKMS module installation failed: ") + name);
                setStatusMessage(isPackage ? "XBPS package installation failed" : "DKMS module installation failed", true);
            }
            setBusy(false);
            updateDkmsModules();
        });
    });
    (void)future;
}

void KernelBridge::removeDkmsModule(const QString &name, const QString &version, const QString &kernel) {
    bool isPackage = name.endsWith("-dkms");
    if (isPackage) {
        appendLog("Starting removal of XBPS DKMS package: " + name);
    } else {
        appendLog("Starting removal of DKMS module: " + name + " v" + version + " for kernel " + kernel);
    }
    setBusy(true);

    auto future = QtConcurrent::run([this, name, version, kernel, isPackage]() {
        std::string cmd;
        if (isPackage) {
            cmd = "pkexec xbps-remove -Rfy " + name.toStdString();
        } else {
            if (!kernel.isEmpty()) {
                cmd = "pkexec dkms remove -m " + name.toStdString() +
                      " -v " + version.toStdString() +
                      " -k " + kernel.toStdString();
            } else {
                cmd = "pkexec dkms remove -m " + name.toStdString() +
                      " -v " + version.toStdString() + " --all";
            }
        }
        appendLog("Executing: " + QString::fromStdString(cmd));
        auto result = utils::execWithOutput(cmd);
        if (!result.stdout.empty()) {
            appendLog(QString::fromStdString(result.stdout));
        }
        if (!result.stderr.empty()) {
            appendLog(QString::fromStdString(result.stderr));
        }
        bool success = (result.exitCode == 0);

        QMetaObject::invokeMethod(this, [this, success, isPackage, name]() {
            if (success) {
                appendLog((isPackage ? "XBPS DKMS package removal finished: " : "DKMS module removal finished: ") + name);
                setStatusMessage(isPackage ? "XBPS DKMS Package removed" : "DKMS module removed", false);
            } else {
                appendLog((isPackage ? "ERROR: XBPS DKMS package removal failed: " : "ERROR: DKMS module removal failed: ") + name);
                setStatusMessage(isPackage ? "XBPS package removal failed" : "DKMS module removal failed", true);
            }
            setBusy(false);
            updateDkmsModules();
        });
    });
    (void)future;
}

void KernelBridge::autoinstallDkms() {
    appendLog("Initiating DKMS autoinstall for active kernel: " + activeKernelVersion());
    setBusy(true);

    auto future = QtConcurrent::run([this]() {
        std::string cmd = "pkexec dkms autoinstall";
        appendLog("Executing dkms autoinstall: " + QString::fromStdString(cmd));
        auto result = utils::execWithOutput(cmd);
        if (!result.stdout.empty()) {
            appendLog(QString::fromStdString(result.stdout));
        }
        if (!result.stderr.empty()) {
            appendLog(QString::fromStdString(result.stderr));
        }
        bool success = (result.exitCode == 0);

        QMetaObject::invokeMethod(this, [this, success]() {
            if (success) {
                appendLog("DKMS autoinstall completed successfully.");
                setStatusMessage("DKMS autoinstall completed", false);
            } else {
                appendLog("ERROR: DKMS autoinstall failed.");
                setStatusMessage("DKMS autoinstall failed", true);
            }
            setBusy(false);
            updateDkmsModules();
        });
    });
    (void)future;
}

// Default Kernel Selection Implementation (solo GRUB)
QString KernelBridge::getDefaultKernelInternal() {
    if (QFile::exists("/etc/default/grub")) {
        std::string line = utils::exec("grep '^GRUB_DEFAULT=' /etc/default/grub");
        if (!line.empty()) {
            QString str = QString::fromStdString(line).trimmed();
            int eqIdx = str.indexOf('=');
            if (eqIdx != -1) {
                QString val = str.mid(eqIdx + 1).trimmed();
                val.remove('"');
                val.remove('\'');
                if (val == "saved") {
                    std::string savedLine = utils::exec("grub-editenv list 2>/dev/null | grep '^saved_entry=' || grub2-editenv list 2>/dev/null | grep '^saved_entry='");
                    if (!savedLine.empty()) {
                        QString savedStr = QString::fromStdString(savedLine).trimmed();
                        int savedEqIdx = savedStr.indexOf('=');
                        if (savedEqIdx != -1) {
                            QString savedVal = savedStr.mid(savedEqIdx + 1).trimmed();
                            savedVal.remove('"');
                            savedVal.remove('\'');
                            if (!savedVal.isEmpty()) {
                                QList<QString> markers = {"with Linux ", "con Linux "};
                                for (const QString &marker : markers) {
                                    int idx = savedVal.indexOf(marker, 0, Qt::CaseInsensitive);
                                    if (idx != -1) {
                                        return savedVal.mid(idx + marker.size()).trimmed();
                                    }
                                }
                                return savedVal;
                            }
                        }
                    }
                } else {
                    QList<QString> markers = {"with Linux ", "con Linux "};
                    for (const QString &marker : markers) {
                        if (val.contains(marker, Qt::CaseInsensitive)) {
                            int withLinuxIdx = val.indexOf(marker, 0, Qt::CaseInsensitive);
                            return val.mid(withLinuxIdx + marker.size()).trimmed();
                        }
                    }
                    if (val != "0" && !val.isEmpty()) {
                        return val;
                    }
                }
            }
        }
    }
    return activeKernelVersion();
}

void KernelBridge::updateDefaultKernel() {
    auto future = QtConcurrent::run([this]() {
        QString def = getDefaultKernelInternal();
        QMetaObject::invokeMethod(this, [this, def]() {
            m_defaultKernel = def;
            emit defaultKernelChanged();
            appendLog("Current default boot kernel is set to: " + def);
        });
    });
    (void)future;
}

void KernelBridge::setDefaultKernel(const QString &kernelVersion) {
    // Usar invokeMethod para asegurar que appendLog y setBusy se llaman en el hilo principal
    QMetaObject::invokeMethod(this, [this, kernelVersion]() {
        appendLog("Requesting default boot kernel change to: " + kernelVersion);
        setBusy(true);
    }, Qt::QueuedConnection);

    auto future = QtConcurrent::run([this, kernelVersion]() {
        // Void Linux usa GRUB, así que asumimos GRUB siempre
        std::string cmd;  // <--- DECLARACIÓN AÑADIDA
        std::string distributor = "Void Linux";

        if (QFile::exists("/etc/default/grub")) {
            std::string distLine = utils::exec("grep '^GRUB_DISTRIBUTOR=' /etc/default/grub");
            if (!distLine.empty()) {
                QString distVal = QString::fromStdString(distLine).trimmed();
                int eqIdx = distVal.indexOf('=');
                if (eqIdx != -1) {
                    QString raw = distVal.mid(eqIdx + 1).trimmed();
                    raw.remove('"');
                    raw.remove('\'');
                    if (!raw.isEmpty())
                        distributor = raw.toStdString();
                }
            }
        }

        if (distributor == "Void" || distributor == "Linux") {
            std::string osName = utils::exec("grep '^NAME=' /etc/os-release");
            if (!osName.empty()) {
                QString nameVal = QString::fromStdString(osName).trimmed();
                int eqIdx = nameVal.indexOf('=');
                if (eqIdx != -1) {
                    QString raw = nameVal.mid(eqIdx + 1).trimmed();
                    raw.remove('"');
                    raw.remove('\'');
                    if (!raw.isEmpty())
                        distributor = raw.toStdString();
                }
            }
        }

        QString grubEntry = QString::fromStdString(distributor) + ", with Linux " + kernelVersion;
        std::string entry = grubEntry.toStdString();

        // Función para escapar comillas simples de forma segura en scripts sh
        auto shellEscapeSingleQuotes = [](const std::string &value) -> std::string {
            std::string escaped = "'";
            for (char c : value) {
                if (c == '\'') escaped += "'\\''";
                else escaped += c;
            }
            escaped += "'";
            return escaped;
        };

        std::string grubEditenvBin;
        if (utils::commandExists("grub-editenv")) {
            grubEditenvBin = "grub-editenv";
        } else if (utils::commandExists("grub2-editenv")) {
            grubEditenvBin = "grub2-editenv";
        }

        std::string grubEnvPath = "/boot/grub/grubenv";
        if (!QFile::exists(QString::fromStdString(grubEnvPath))) {
            grubEnvPath = "/boot/grub2/grubenv";
        }

        std::string grubCfgPath = "/boot/grub/grub.cfg";
        bool grubCfgExists = QFile::exists(QString::fromStdString(grubCfgPath));
        if (!grubCfgExists) {
            grubCfgPath = "/boot/grub2/grub.cfg";
            grubCfgExists = QFile::exists(QString::fromStdString(grubCfgPath));
        }

        QString actualEntry;
        bool foundEntry = false;

        if (grubCfgExists) {
            auto parseGrubCfg = [&](const QStringList &lines) -> bool {
                int braceCount = 0;
                QString currentSubmenu;
                int submenuBraceLevel = -1;

                auto extractTitle = [](const QString &l) -> QString {
                    int firstQuote = -1;
                    QChar quoteChar;
                    for (int i = 0; i < l.size(); ++i) {
                        if (l[i] == '\'' || l[i] == '"') {
                            firstQuote = i;
                            quoteChar = l[i];
                            break;
                        }
                    }
                    if (firstQuote != -1) {
                        int secondQuote = l.indexOf(quoteChar, firstQuote + 1);
                        if (secondQuote != -1) {
                            return l.mid(firstQuote + 1, secondQuote - firstQuote - 1).trimmed();
                        }
                    }
                    return "";
                };

                for (const QString &rawLine : lines) {
                    QString line = rawLine.trimmed();

                    if (line.contains("submenu ", Qt::CaseInsensitive)) {
                        QString subTitle = extractTitle(line);
                        if (!subTitle.isEmpty()) {
                            currentSubmenu = subTitle;
                            submenuBraceLevel = braceCount;
                        }
                    }

                    if (line.contains("menuentry ", Qt::CaseInsensitive)
                        && line.contains(kernelVersion, Qt::CaseInsensitive)
                        && !line.contains("recovery", Qt::CaseInsensitive)) {
                        QString entryTitle = extractTitle(line);
                        if (!entryTitle.isEmpty()) {
                            if (!currentSubmenu.isEmpty()) {
                                actualEntry = currentSubmenu + ">" + entryTitle;
                            } else {
                                actualEntry = entryTitle;
                            }
                            foundEntry = true;
                            return true;
                        }
                    }

                    for (int i = 0; i < line.size(); ++i) {
                        if (line[i] == '{') {
                            braceCount++;
                        } else if (line[i] == '}') {
                            braceCount--;
                            if (submenuBraceLevel != -1 && braceCount <= submenuBraceLevel) {
                                currentSubmenu = "";
                                submenuBraceLevel = -1;
                            }
                        }
                    }
                }
                return false;
            };

            QStringList lines;
            QFile cfg(QString::fromStdString(grubCfgPath));
            bool cfgReadable = cfg.open(QIODevice::ReadOnly | QIODevice::Text);

            if (cfgReadable) {
                QTextStream in(&cfg);
                while (!in.atEnd()) {
                    lines.append(in.readLine());
                }
                cfg.close();
                if (!lines.isEmpty()) {
                    foundEntry = parseGrubCfg(lines);
                }
            }

            // Solo intentar con pkexec si no se pudo leer el archivo por permisos
            if (!cfgReadable && !foundEntry) {
                std::string catCmd = "pkexec cat '" + grubCfgPath + "' 2>/dev/null";
                std::string catOutput = utils::exec(catCmd);
                if (!catOutput.empty()) {
                    lines = QString::fromStdString(catOutput).split('\n');
                    foundEntry = parseGrubCfg(lines);
                }
            }
        }

        if (!foundEntry) {
            actualEntry = QString::fromStdString(entry);
        }

        std::string escapedActualEntry = shellEscapeSingleQuotes(actualEntry.toStdString());

        // Construir la cadena de comandos
        std::string commandChain = "export PATH=$PATH:/usr/sbin:/sbin; set -e; ";
        commandChain += "if grep -Eq '^#?[[:space:]]*GRUB_DEFAULT=' /etc/default/grub; then ";
        commandChain += "sed -i -E 's/^#?[[:space:]]*GRUB_DEFAULT=.*/GRUB_DEFAULT=saved/' /etc/default/grub; ";
        commandChain += "else ";
        commandChain += "echo 'GRUB_DEFAULT=saved' >> /etc/default/grub; ";
        commandChain += "fi; ";

        if (!grubEditenvBin.empty() && !grubEnvPath.empty()) {
            if (!QFile::exists(QString::fromStdString(grubEnvPath))) {
                commandChain += grubEditenvBin + " " + grubEnvPath + " create; ";
            }
            commandChain += grubEditenvBin + " " + grubEnvPath + " set saved_entry=" + escapedActualEntry + "; ";
        } else if (utils::commandExists("grub-set-default")) {
            commandChain += "grub-set-default " + escapedActualEntry + "; ";
        } else if (utils::commandExists("grub2-set-default")) {
            commandChain += "grub2-set-default " + escapedActualEntry + "; ";
        } else {
            commandChain += "echo 'No GRUB command available' >&2; false; ";
        }

        if (grubCfgExists) {
            commandChain += "if command -v update-grub >/dev/null 2>&1; then update-grub; ";
            commandChain += "elif command -v grub-mkconfig >/dev/null 2>&1; then grub-mkconfig -o " + grubCfgPath + "; ";
            commandChain += "elif command -v grub2-mkconfig >/dev/null 2>&1; then grub2-mkconfig -o " + grubCfgPath + "; ";
            commandChain += "else echo 'No grub-mkconfig or update-grub command found' >&2; false; fi ";
        }

        // Escribir en un archivo temporal para evitar problemas de escapado con pkexec sh -c
        std::string tempScriptPath;
        {
            char templatePath[] = "/tmp/com.neko.kernelmanager.XXXXXX";
            int fd = mkstemp(templatePath);
            if (fd != -1) {
                tempScriptPath = templatePath;
                std::string content = commandChain;
                ssize_t written = write(fd, content.c_str(), content.size());
                (void)written;
                (void)fchmod(fd, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
                close(fd);
            }
        }

        if (!tempScriptPath.empty()) {
            cmd = "pkexec sh " + shellEscapeSingleQuotes(tempScriptPath);
        } else {
            // Si falla la creación del temporal, usamos un fallback con doble escapado
            auto shellEscapeDouble = [](const std::string &value) {
                std::string escaped = "\"";
                for (char c : value) {
                    if (c == '"' || c == '\\' || c == '$' || c == '`')
                        escaped += "\\";
                    escaped += c;
                }
                escaped += "\"";
                return escaped;
            };

            std::string doubleEscaped = shellEscapeDouble(actualEntry.toStdString());
            size_t pos = commandChain.find(escapedActualEntry);
            while (pos != std::string::npos) {
                commandChain.replace(pos, escapedActualEntry.length(), doubleEscaped);
                pos = commandChain.find(escapedActualEntry, pos + doubleEscaped.length());
            }

            cmd = "pkexec sh -c " + shellEscapeDouble(commandChain);
        }

        if (!tempScriptPath.empty()) {
            cmd += "; rm -f " + shellEscapeSingleQuotes(tempScriptPath);
        }

        appendLog("Executing GRUB update: " + QString::fromStdString(cmd));
        auto result = utils::execWithOutput(cmd);
        if (!result.stdout.empty()) {
            appendLog(QString::fromStdString(result.stdout));
        }
        if (!result.stderr.empty()) {
            appendLog(QString::fromStdString(result.stderr));
        }
        bool success = (result.exitCode == 0);

        QMetaObject::invokeMethod(this, [this, success, kernelVersion]() {
            if (success) {
                appendLog("Default boot kernel changed to " + kernelVersion + " successfully.");
                setStatusMessage("Default kernel updated", false);
                m_defaultKernel = kernelVersion;
                emit defaultKernelChanged();
            } else {
                appendLog("ERROR: Failed to change default kernel to " + kernelVersion);
                setStatusMessage("Failed to update default kernel", true);
            }
            updateDefaultKernel();
            updateKernels();
            setBusy(false);
        });
    });
    (void)future;
}