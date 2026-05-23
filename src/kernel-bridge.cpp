#include "kernel-bridge.hpp"
#include <QtConcurrent/QtConcurrent>
#include <QDateTime>
#include "utils.hpp"
#include <QDir>
#include <QStandardPaths>
#include <QFile>
#include <QTextStream>
#include <QJsonDocument>
#include <QJsonObject>

KernelBridge::KernelBridge(QObject *parent) : QObject(parent) {
    loadConfig();
    updateKernels();
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
        map["type"] = k.category() == "Custom" ? "manual" : "xbps";
        list.append(map);
    }
    return list;
}

void KernelBridge::updateKernels() {
    auto future = QtConcurrent::run([this]() {
        auto newList = getKernels();
        QMetaObject::invokeMethod(this, [this, newList]() {
            m_kernelsCache = newList;
            emit kernelsChanged();
        });
    });
    (void)future;
}

void KernelBridge::appendLog(const QString &line) {
    m_logs += "[" + QDateTime::currentDateTime().toString("hh:mm:ss") + "] " + line + "\n";
    // Keep logs reasonable
    if (m_logs.length() > 20000) m_logs = m_logs.right(15000);
    emit logsChanged();
}

void KernelBridge::installKernel(const QString &name) {
    appendLog("Starting installation of " + name + " and its headers...");
    setBusy(true);
    auto future = QtConcurrent::run([this, name]() {
        // Ensure headers are ALWAYS installed by adding the -headers suffix
        std::string pkgName = name.toStdString();
        // Combine into one pkexec call to reduce prompts and ensure GRUB is updated
        std::string cmd = "pkexec sh -c \"xbps-install -y " + pkgName + " " + pkgName + "-headers && grub-mkconfig -o /boot/grub/grub.cfg\" 2>&1";
        
        appendLog("Executing: " + QString::fromStdString(cmd));

        FILE* pipe = popen(cmd.c_str(), "r");
        bool success = false;
        if (pipe) {
            char buffer[256];
            while (fgets(buffer, sizeof(buffer), pipe)) {
                QString line = QString::fromLocal8Bit(buffer).trimmed();
                QMetaObject::invokeMethod(this, [this, line]() { appendLog(line); });
            }
            success = (pclose(pipe) == 0);
        }

        QMetaObject::invokeMethod(this, [this, name, success]() {
            setBusy(false);
            if (success) {
                appendLog("Installation of " + name + " and headers finished.");
                setStatusMessage("Kernel and headers installed", false);
                emit operationFinished("Kernel installed");
            } else {
                appendLog("ERROR: Installation of " + name + " failed.");
                setStatusMessage("Installation failed (Check logs)", true);
                emit operationFinished("Installation failed");
            }
            updateKernels();
        });
    });
    (void)future;
}

void KernelBridge::removeKernel(const QString &name) {
    QString current = activeKernelVersion();
    appendLog("Requested removal of " + name);
    
    bool isActive = false;
    if (name.startsWith("linux-manual-")) {
        isActive = (name == "linux-manual-" + current);
    } else {
        QString shortName = name;
        if (shortName.startsWith("linux")) shortName.remove(0, 5);
        if (current.startsWith(shortName)) isActive = true;
    }

    if (isActive) {
        appendLog("ERROR: Cannot remove active kernel " + current);
        setStatusMessage("Cannot remove the active kernel (" + current + ")", true);
        return;
    }

    setBusy(true);
    auto future = QtConcurrent::run([this, name, current]() {
        bool isManual = name.startsWith("linux-manual-");
        std::string cmd;
        
        if (isManual) {
            std::string version = name.toStdString().substr(13);
            // Combined all manual removal steps and GRUB update into one pkexec call
            cmd = "pkexec sh -c \"rm -fv /boot/vmlinuz-" + version + 
                  " /boot/initramfs-" + version + ".img /boot/System.map-" + version + 
                  " /boot/config-" + version + " && rm -rfv /lib/modules/" + version + 
                  " && grub-set-default 0 && grub-mkconfig -o /boot/grub/grub.cfg\" 2>&1";
        } else {
            // High-reliability removal logic for XBPS kernels:
            // 1. Identify package and version
            std::string pkgName = name.toStdString();
            std::string version = pkgName;
            if (version.find("linux") == 0) version = version.substr(5); // e.g., "5.15"

            // 2. Build a multi-stage cleanup command:
            //    - Stage A: Remove packages (kernel and headers)
            //    - Stage B: Run vkpurge for the specific version (official cleanup)
            //    - Stage C: Manual fallback cleanup (removes files if vkpurge missed them or if they're orphaned)
            //    - Stage D: Final GRUB update
            cmd = "pkexec sh -c \"";
            cmd += "xbps-remove -Rfy " + pkgName + " " + pkgName + "-headers; ";
            cmd += "vkpurge rm " + version + " || true; ";
            // Fallback: search and destroy any remaining files matching this specific version in critical areas
            cmd += "rm -fv /boot/vmlinuz-" + version + "* /boot/initramfs-" + version + "* ";
            cmd += "/boot/System.map-" + version + "* /boot/config-" + version + "*; ";
            cmd += "rm -rfv /lib/modules/" + version + "*; ";
            cmd += "grub-set-default 0 && grub-mkconfig -o /boot/grub/grub.cfg\" 2>&1";
        }

        appendLog("Executing removal: " + QString::fromStdString(cmd));

        FILE* pipe = popen(cmd.c_str(), "r");
        bool success = false;
        if (pipe) {
            char buffer[256];
            while (fgets(buffer, sizeof(buffer), pipe)) {
                QString line = QString::fromLocal8Bit(buffer).trimmed();
                QMetaObject::invokeMethod(this, [this, line]() { appendLog(line); });
            }
            success = (pclose(pipe) == 0);
        }

        QMetaObject::invokeMethod(this, [this, success]() {
            setBusy(false);
            if (success) {
                appendLog("Kernel removed successfully and GRUB updated.");
                setStatusMessage("Kernel removed successfully", false);
                emit operationFinished("Kernel removed");
            } else {
                appendLog("ERROR: Failed to remove kernel.");
                setStatusMessage("Failed to remove kernel (Check logs)", true);
                emit operationFinished("Failed to remove kernel");
            }
            updateKernels();
        });
    });
    (void)future;
}

void KernelBridge::vkpurge() {
    appendLog("Starting vkpurge rm all...");
    setBusy(true);
    auto future = QtConcurrent::run([this]() {
        // Combined vkpurge and grub-mkconfig into one pkexec call
        std::string cmd = "pkexec sh -c \"vkpurge rm all && grub-mkconfig -o /boot/grub/grub.cfg\" 2>&1";
        
        FILE* pipe = popen(cmd.c_str(), "r");
        bool success = false;
        if (pipe) {
            char buffer[256];
            while (fgets(buffer, sizeof(buffer), pipe)) {
                QString line = QString::fromLocal8Bit(buffer).trimmed();
                QMetaObject::invokeMethod(this, [this, line]() { appendLog(line); });
            }
            success = (pclose(pipe) == 0);
        }
        
        QMetaObject::invokeMethod(this, [this, success]() {
            setBusy(false);
            if (success) {
                appendLog("vkpurge completed and GRUB updated.");
                
                setStatusMessage("Old kernels purged", false);
                emit operationFinished("Kernels purged");
            } else {
                appendLog("vkpurge failed or no kernels to purge.");
                setStatusMessage("vkpurge finished", false);
                emit operationFinished("vkpurge finished");
            }
            updateKernels();
        });
    });
    (void)future;
}

void KernelBridge::fetchKernelOrgVersions(const QString &variant) {
    setBusy(true);
    auto future = QtConcurrent::run([this, variant]() {
        auto versions = utils::fetchKernelOrgVersions(variant.toStdString());
        QStringList qlist;
        for (const auto &v : versions) qlist << QString::fromStdString(v);
        
        QMetaObject::invokeMethod(this, [this, qlist]() {
            m_kernelOrgVersions = qlist;
            setBusy(false);
            emit kernelOrgVersionsChanged();
        });
    });
    (void)future;
}

void KernelBridge::downloadKernelOrg(const QString &variant, const QString &version) {
    setBusy(true);
    auto future = QtConcurrent::run([this, variant, version]() {
        std::string v = version.toStdString();
        std::string var = variant.toStdString();
        std::string pkgName = "kernel-neko-" + var + "-" + v;
        
        // Update UI
        QMetaObject::invokeMethod(this, [this, pkgName]() {
            setTargetTemplate(QString::fromStdString(pkgName));
        });

        std::string home = QDir::homePath().toStdString();
        std::string voidPackagesDir = home + "/.cache/neko-kernel-manager/void-packages";
        
        // Ensure void-packages exists
        if (!utils::dirExists(voidPackagesDir)) {
             utils::runCommand("mkdir -p " + home + "/.cache/neko-kernel-manager");
             utils::gitClone("https://github.com/void-linux/void-packages.git", voidPackagesDir);
             utils::runCommand("cd " + voidPackagesDir + " && ./xbps-src binary-bootstrap");
        }

        // Create template
        std::string srcpkgsDir = voidPackagesDir + "/srcpkgs/" + pkgName;
        utils::runCommand("mkdir -p " + srcpkgsDir);
        
        std::string url = utils::kernelOrgDownloadUrl(var, v);
        
        // We'll create a source template for kernel.org
        std::string templateContent = "pkgname=" + pkgName + "\n"
                                     "version=" + v + "\n"
                                     "revision=1\n"
                                     "short_desc=\"Custom " + var + " kernel " + v + " for Void Linux (Neko)\"\n"
                                     "maintainer=\"NekoKernelManager <admin@neko.local>\"\n"
                                     "license=\"GPL-2.0-only\"\n"
                                     "homepage=\"https://www.kernel.org\"\n"
                                     "distfiles=\"" + url + "\"\n"
                                     "checksum=\"SKIP\"\n" // User might need to run 'xbps-src digest'
                                     "create_wrksrc=yes\n"
                                     "triggers=\"kernel-hooks\"\n"
                                     "kernel_hooks_version=\"" + v + "_1\"\n\n"
                                     "do_build() {\n"
                                     "    make oldconfig\n"
                                     "    make -j$(nproc)\n"
                                     "}\n\n"
                                     "do_install() {\n"
                                     "    vmkdir boot\n"
                                     "    vmkdir usr/lib/modules\n"
                                     "    cp arch/x86/boot/bzImage ${DESTDIR}/boot/vmlinuz-${version}_1\n"
                                     "    make modules_install INSTALL_MOD_PATH=${DESTDIR}/usr\n"
                                     "}\n";

        QFile file(QString::fromStdString(srcpkgsDir + "/template"));
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
            out << QString::fromStdString(templateContent);
            file.close();
        }

        QMetaObject::invokeMethod(this, [this, pkgName]() {
            setBusy(false);
            setStatusMessage("Template prepared: " + QString::fromStdString(pkgName), false);
            emit operationFinished("Template prepared");
        });
    });
    (void)future;
}

void KernelBridge::cloneCustomGit(const QString &url) {
    setBusy(true);
    auto future = QtConcurrent::run([this, url]() {
        bool isTarball = url.endsWith(".tar.gz") || url.endsWith(".tar.xz") || url.endsWith(".tgz") || url.contains("/releases/download/");
        QString message;
        
        std::string home = QDir::homePath().toStdString();
        std::string voidPackagesDir = home + "/.cache/neko-kernel-manager/void-packages";
        
        // Determine a name
        std::string pkgName = "kernel-neko-custom";
        if (url.contains("/")) {
            std::string last = url.toStdString().substr(url.toStdString().find_last_of("/") + 1);
            if (last.find(".") != std::string::npos) last = last.substr(0, last.find("."));
            pkgName = "kernel-neko-" + last;
        }

        QMetaObject::invokeMethod(this, [this, pkgName]() {
            setTargetTemplate(QString::fromStdString(pkgName));
        });

        if (!utils::dirExists(voidPackagesDir)) {
             utils::runCommand("mkdir -p " + home + "/.cache/neko-kernel-manager");
             utils::gitClone("https://github.com/void-linux/void-packages.git", voidPackagesDir);
             utils::runCommand("cd " + voidPackagesDir + " && ./xbps-src binary-bootstrap");
        }

        std::string srcpkgsDir = voidPackagesDir + "/srcpkgs/" + pkgName;
        utils::runCommand("mkdir -p " + srcpkgsDir);

        if (isTarball) {
            // Precompiled binary template style requested by user
            std::string templateContent = "pkgname=" + pkgName + "\n"
                                         "version=1.0.0\n" // Placeholder, should ideally be detected
                                         "revision=1\n"
                                         "short_desc=\"Custom precompiled kernel from tarball (Neko)\"\n"
                                         "maintainer=\"NekoKernelManager <admin@neko.local>\"\n"
                                         "license=\"GPL-2.0-only\"\n"
                                         "homepage=\"" + url.toStdString() + "\"\n"
                                         "distfiles=\"" + url.toStdString() + "\"\n"
                                         "checksum=\"SKIP\"\n"
                                         "create_wrksrc=yes\n"
                                         "triggers=\"kernel-hooks\"\n"
                                         "kernel_hooks_version=\"1.0.0_1\"\n\n"
                                         "do_install() {\n"
                                         "    vmkdir boot\n"
                                         "    vmkdir usr/lib/modules\n"
                                         "    cp -a boot/* ${DESTDIR}/boot/\n"
                                         "    cp -a lib/modules/* ${DESTDIR}/usr/lib/modules/\n"
                                         "}\n";

            QFile file(QString::fromStdString(srcpkgsDir + "/template"));
            if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream out(&file);
                out << QString::fromStdString(templateContent);
                file.close();
            }
            message = "Binary template prepared: " + QString::fromStdString(pkgName);
        } else {
            // Source template for Git
            std::string templateContent = "pkgname=" + pkgName + "\n"
                                         "version=git\n"
                                         "revision=1\n"
                                         "short_desc=\"Custom kernel from Git source (Neko)\"\n"
                                         "maintainer=\"NekoKernelManager <admin@neko.local>\"\n"
                                         "license=\"GPL-2.0-only\"\n"
                                         "homepage=\"" + url.toStdString() + "\"\n"
                                         "distfiles=\"" + url.toStdString() + "\"\n"
                                         "create_wrksrc=yes\n"
                                         "triggers=\"kernel-hooks\"\n"
                                         "kernel_hooks_version=\"git_1\"\n\n"
                                         "do_build() {\n"
                                         "    make oldconfig\n"
                                         "    make -j$(nproc)\n"
                                         "}\n\n"
                                         "do_install() {\n"
                                         "    vmkdir boot\n"
                                         "    vmkdir usr/lib/modules\n"
                                         "    cp arch/x86/boot/bzImage ${DESTDIR}/boot/vmlinuz-git_1\n"
                                         "    make modules_install INSTALL_MOD_PATH=${DESTDIR}/usr\n"
                                         "}\n";

            QFile file(QString::fromStdString(srcpkgsDir + "/template"));
            if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream out(&file);
                out << QString::fromStdString(templateContent);
                file.close();
            }
            message = "Git source template prepared: " + QString::fromStdString(pkgName);
        }
        
        QMetaObject::invokeMethod(this, [this, message]() {
            setBusy(false);
            setStatusMessage(message, false);
            emit operationFinished(message);
            updateKernels();
        });
    });
    (void)future;
}

void KernelBridge::buildKernel(const QString &templateName) {
    setBusy(true);
    auto future = QtConcurrent::run([this, templateName]() {
        std::string home = QDir::homePath().toStdString();
        std::string voidPackagesDir = home + "/.cache/neko-kernel-manager/void-packages";
        std::string name = templateName.toStdString();
        
        // 1. Ensure environment is ready
        if (!utils::dirExists(voidPackagesDir)) {
             utils::runCommand("mkdir -p " + home + "/.cache/neko-kernel-manager");
             utils::gitClone("https://github.com/void-linux/void-packages.git", voidPackagesDir);
             utils::runCommand("cd " + voidPackagesDir + " && ./xbps-src binary-bootstrap");
        }

        // 2. Handle 'neko-' prefix by creating a sub-template if necessary
        std::string realTemplate = m_sourceTemplate.isEmpty() ? name : m_sourceTemplate.toStdString();
        if (name.find("neko-") == 0) {
            if (realTemplate.find("neko-") == 0) realTemplate = realTemplate.substr(5);
            
            std::string srcPath = voidPackagesDir + "/srcpkgs/" + realTemplate;
            std::string destPath = voidPackagesDir + "/srcpkgs/" + name;
            
            if (utils::dirExists(srcPath)) {
                if (utils::dirExists(destPath)) {
                    utils::runCommand("rm -rf " + destPath);
                }
                // Create a custom template that inherits or copies the original
                utils::runCommand("cp -r " + srcPath + " " + destPath);
                utils::runCommand("sed -i 's/^pkgname=.*/pkgname=" + name + "/' " + destPath + "/template");
                
                // Add Neko branding to short_desc so it's visible in some UI/GRUB contexts
                std::string shortDesc = "Neko Kernel (" + name + ")";
                utils::runCommand("sed -i 's/^short_desc=.*/short_desc=\"" + shortDesc + "\"/ ' " + destPath + "/template");
            }
        }

        // 3. Perform build
        std::string buildCmd = "cd " + voidPackagesDir + " && ./xbps-src pkg " + name;
        utils::runInTerminal(buildCmd);

        QMetaObject::invokeMethod(this, [this]() {
            setBusy(false);
            setStatusMessage("Binary compilation complete. Check binpkgs folder.", false);
            emit operationFinished("Build complete");
            updateKernels();
        });
    });
    (void)future;
}

void KernelBridge::exportPackages(const QString &destPath) {
    std::string home = QDir::homePath().toStdString();
    std::string binpkgsDir = home + "/.cache/neko-kernel-manager/void-packages/hostdir/binpkgs";
    
    std::string cmd = "find " + binpkgsDir + " -name '*.xbps' -exec cp {} " + destPath.toStdString() + " \\;";
    utils::runCommand(cmd);
    setStatusMessage("Packages exported", false);
    emit operationFinished("Packages exported");
}

void KernelBridge::saveConfig() {
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir().mkpath(configDir);
    QString configFile = configDir + "/config.json";

    QJsonObject config;
    config["lto"] = m_lto;
    config["preempt"] = m_preempt;
    config["cpu_opt"] = m_cpuOpt;
    config["zfs_support"] = m_zfsSupport;
    config["nvidia_support"] = m_nvidiaSupport;

    QJsonDocument doc(config);
    QFile file(configFile);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        setStatusMessage("Failed to save config", true);
        emit operationFinished("Failed to save config");
        return;
    }

    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    setStatusMessage("Config saved", false);
    emit operationFinished("Config saved");
}

void KernelBridge::loadConfig() {
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QString configFile = configDir + "/config.json";

    if (!QFile::exists(configFile)) {
        emit operationFinished("No config file found");
        return;
    }

    QFile file(configFile);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        setStatusMessage("Failed to open config file", true);
        emit operationFinished("Failed to open config file");
        return;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject()) {
        setStatusMessage("Failed to parse config", true);
        emit operationFinished("Failed to parse config");
        return;
    }

    QJsonObject config = doc.object();
    setLto(config.value("lto").toBool());
    setPreempt(config.value("preempt").toString("voluntary"));
    setCpuOpt(config.value("cpu_opt").toString("generic"));
    setZfsSupport(config.value("zfs_support").toBool());
    setNvidiaSupport(config.value("nvidia_support").toBool());
    setStatusMessage("Config loaded", false);
    emit operationFinished("Config loaded");
}

void KernelBridge::setStatusMessage(const QString &message, bool isError) {
    if (m_statusMessage != message || m_statusIsError != isError) {
        m_statusMessage = message;
        m_statusIsError = isError;
        emit statusMessageChanged();
    }
}

void KernelBridge::setBusy(bool b) {
    if (m_busy != b) {
        m_busy = b;
        emit busyChanged();
    }
}

QString KernelBridge::activeKernelVersion() const {
    return QString::fromStdString(utils::exec("uname -r"));
}